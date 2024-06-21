/*
 * Copyright 2024 Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) OR AUTHOR(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

#include "amdgpu_renderer.h"

#include "util/anon_file.h"
#include "util/bitscan.h"
#include "util/hash_table.h"
#include "util/macros.h"
#include "util/os_file.h"
#include "util/u_atomic.h"
#include "util/u_math.h"
#include "pipe/p_state.h"
#include "amdgpu_virtio_proto.h"
#include "u_thread.h"
#include "virgl_context.h"
#include "virglrenderer.h"
#include "vrend_renderer.h"

#include <stdalign.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>

#include <xf86drm.h>

#include <amdgpu.h>
#include "drm_util.h"
#include "drm_fence.h"

#if defined(__clang__)
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#define print(level, fmt, ...) do {       \
   if (ctx->debug >= level) { \
      unsigned c = (unsigned)((uintptr_t)ctx >> 8) % 256; \
      printf("\033[0;38;5;%dm", c);     \
      printf("[%d|%s] %s: " fmt "\n", ctx->base.ctx_id, ctx->debug_name, __FUNCTION__, ##__VA_ARGS__); \
      printf("\033[0m");     \
   } \
 } while (false)

struct cmd_stat {
   uint64_t total_duration_us;
   uint32_t min_duration_us, max_duration_us;
   uint32_t count;
};

struct amdgpu_context {
   struct virgl_context base;

   const char *debug_name;

   struct amdvgpu_shmem *shmem;
   uint64_t shmem_size;
   uint32_t shm_res_id;
   uint8_t *rsp_mem;
   uint32_t rsp_mem_sz;

   struct amdgpu_ccmd_rsp *current_rsp;

   int fd;

   struct hash_table *blob_table;
   struct hash_table *resource_table;

   int eventfd;

   amdgpu_device_handle dev;
   int debug;

   struct hash_table_u64 *id_to_ctx;

   struct {
      struct cmd_stat s[9];
      uint64_t last_print_ms;
   } statistics;

   uint32_t timeline_count;
   struct drm_timeline timelines[];
};
DEFINE_CAST(virgl_context, amdgpu_context)

static
int close_fd(struct amdgpu_context *ctx, int fd, const char *from) {
   print(2, "close_fd %d (%s)", fd, from);
   return close(fd);
}

int
amdgpu_renderer_probe(int fd, struct virgl_renderer_capset_drm *capset)
{
   amdgpu_device_handle dev;
   uint32_t drm_major, drm_minor;
   int r;

   r = amdgpu_device_initialize2(fd, false, &drm_major, &drm_minor, &dev);
   if (r)
      return -ENOTSUP;
   amdgpu_query_sw_info(dev, amdgpu_sw_info_address32_hi,
                        &capset->u.amdgpu.address32_hi);
   amdgpu_query_buffer_size_alignment(dev,
                                      &capset->u.amdgpu.alignments);
   amdgpu_query_gpu_info(dev,
                         &capset->u.amdgpu.gpu_info);
   strncpy(capset->u.amdgpu.marketing_name,
           amdgpu_get_marketing_name(dev),
           sizeof(capset->u.amdgpu.marketing_name) - 1);

   amdgpu_device_deinitialize(dev);

   return 0;
}

/* Imported objects will use this blob id. */
#define UNKOWN_BLOB_ID 0xffffffff

struct amdgpu_object {
   /* amdgpu_drm handle to the object. */
   amdgpu_bo_handle bo;
   /* Global, assigned by guest kernel. */
   uint32_t res_id;
   /* Context-specific, assigned by guest userspace. It's used to link the bo
    * created via AMDGPU_CCMD_GEM_NEW and the get_blob() callback.
    */
   uint32_t blob_id;
   /* GEM handle, used in ioctl (eg: amdgpu_cs_submit_raw2). */
   uint32_t gem_handle;

   uint32_t flags;
   uint32_t size;

   bool has_metadata    :1;
   bool exported        :1;
   bool enable_cache_wc :1;
};

static void free_amdgpu_object(struct amdgpu_context *ctx, struct amdgpu_object *obj);

static void
amdgpu_renderer_destroy_fini(struct amdgpu_context *ctx)
{
   print(2, "real destroy");

   for (unsigned i = 0; i < ctx->timeline_count; i++) {
      drm_timeline_fini(&ctx->timelines[i]);
      free((char*)ctx->timelines[i].name);
   }

   close_fd(ctx, ctx->eventfd, "amdgpu_renderer_destroy eventfd");

   if (ctx->shmem) {
      print(2, "Unmap shmem %p", (void*)ctx->shmem);
      munmap(ctx->shmem, ctx->shmem_size);
      ctx->shmem = NULL;
   }

   amdgpu_device_deinitialize(ctx->dev);

   free((void*)ctx->debug_name);
   memset(ctx, 0, sizeof(struct amdgpu_context));
   free(ctx);
}

static void
amdgpu_renderer_destroy(struct virgl_context *vctx)
{
   struct amdgpu_context *ctx = to_amdgpu_context(vctx);

   /* Safety check */
   if (_mesa_hash_table_num_entries(ctx->resource_table) != 0) {
      print(2, "%d resources leaked:",
         _mesa_hash_table_num_entries(ctx->resource_table));
      hash_table_foreach (ctx->resource_table, entry) {
         struct amdgpu_object *o = entry->data;
         print(2, "  * blob_id: %u res_id: %u", o->blob_id, o->res_id);
      }
   }
   amdgpu_renderer_destroy_fini(ctx);
}

static void *
amdgpu_context_rsp_noshadow(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   return &ctx->rsp_mem[hdr->rsp_off];
}

static void *
amdgpu_context_rsp(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr,
                   unsigned len)
{
   unsigned rsp_mem_sz = ctx->rsp_mem_sz;
   unsigned off = hdr->rsp_off;

   if ((off > rsp_mem_sz) || (len > rsp_mem_sz - off)) {
      print(0, "invalid shm offset: off=%u, len=%u (shmem_size=%u)", off, len, rsp_mem_sz);
      return NULL;
   }

   struct amdgpu_ccmd_rsp *rsp = amdgpu_context_rsp_noshadow(ctx, hdr);

   assert(len >= sizeof(*rsp));

   /* With newer host and older guest, we could end up wanting a larger rsp struct
    * than guest expects, so allocate a shadow buffer in this case rather than
    * having to deal with this in all the different ccmd handlers.  This is similar
    * in a way to what drm_ioctl() does.
    */
   if (len > rsp->base.len) {
      rsp = malloc(len);
      if (!rsp)
         return NULL;
      rsp->base.len = len;
   }

   ctx->current_rsp = rsp;

   return rsp;
}

static struct amdgpu_object *
amdgpu_object_create(amdgpu_bo_handle handle, uint32_t flags, uint64_t size)
{
   struct amdgpu_object *obj = calloc(1, sizeof(*obj));

   if (!obj)
      return NULL;

   obj->blob_id = UNKOWN_BLOB_ID;
   obj->bo = handle;
   obj->flags = flags;
   obj->size = size;

   return obj;
}

static struct hash_entry *
table_search(struct hash_table *ht, uint32_t key)
{
   /* zero is not a valid key for u32_keys hashtable: */
   if (!key)
      return NULL;
   return _mesa_hash_table_search(ht, (void *)(uintptr_t)key);
}

static bool
valid_blob_id(struct amdgpu_context *ctx, uint32_t blob_id)
{
   /* must be non-zero: */
   if (blob_id == 0)
      return false;

   /* must not already be in-use: */
   if (table_search(ctx->blob_table, blob_id))
      return false;

   return true;
}

static void
amdgpu_object_set_blob_id(struct amdgpu_context *ctx, struct amdgpu_object *obj,
                          uint32_t blob_id)
{
   obj->blob_id = blob_id;
   _mesa_hash_table_insert(ctx->blob_table, (void *)(uintptr_t)obj->blob_id, obj);
}

static struct amdgpu_object *
amdgpu_retrieve_object_from_blob_id(struct amdgpu_context *ctx, uint64_t blob_id)
{
   assert((blob_id >> 32) == 0);
   uint32_t id = blob_id;
   struct hash_entry *entry = table_search(ctx->blob_table, id);
   if (!entry)
      return NULL;
   struct amdgpu_object *obj = entry->data;
   _mesa_hash_table_remove(ctx->blob_table, entry);
   return obj;
}

static struct amdgpu_object *
amdgpu_get_object_from_res_id(struct amdgpu_context *ctx, uint32_t res_id, const char *from)
{
   const struct hash_entry *entry = table_search(ctx->resource_table, res_id);
   if (likely(entry)) {
      return entry->data;
   } else {
      if (from) {
         print(0, "Couldn't find res_id: %u [%s]", res_id, from);
         hash_table_foreach (ctx->resource_table, entry) {
            struct amdgpu_object *o = entry->data;
            print(1, "  * blob_id: %u res_id: %u", o->blob_id, o->res_id);
         }
      }
      return NULL;
   }
}

static bool
valid_res_id(struct amdgpu_context *ctx, uint32_t res_id)
{
   return !table_search(ctx->resource_table, res_id);
}

static void
amdgpu_object_set_res_id(struct amdgpu_context *ctx, struct amdgpu_object *obj,
                         uint32_t res_id)
{
   assert(valid_res_id(ctx, res_id));

   obj->res_id = res_id;

   print(2, "blob_id=%u, res_id: %u", obj->blob_id, obj->res_id);

   _mesa_hash_table_insert(ctx->resource_table, (void *)(uintptr_t)obj->res_id, obj);
}

static void
amdgpu_remove_object(struct amdgpu_context *ctx, struct amdgpu_object *obj)
{
   _mesa_hash_table_remove_key(ctx->resource_table, (void *)(uintptr_t)obj->res_id);
}

static void
amdgpu_renderer_attach_resource(struct virgl_context *vctx, struct virgl_resource *res)
{
   struct amdgpu_context *ctx = to_amdgpu_context(vctx);
   struct amdgpu_object *obj = amdgpu_get_object_from_res_id(ctx, res->res_id, NULL);

   if (!obj) {
      if (res->fd_type == VIRGL_RESOURCE_FD_SHM) {
         assert(res->res_id == ctx->shm_res_id);
         return;
      }

      int fd;
      enum virgl_resource_fd_type fd_type = virgl_resource_export_fd(res, &fd);
      if (fd_type == VIRGL_RESOURCE_FD_DMABUF) {
         struct amdgpu_bo_info info = {0};
         struct amdgpu_bo_import_result import;
         int ret;

         ret = amdgpu_bo_import(ctx->dev, amdgpu_bo_handle_type_dma_buf_fd, fd,
                                &import);

         close_fd(ctx, fd, __FUNCTION__);
         if (ret) {
            print(0, "Could not import fd=%d: %s", fd, strerror(errno));
            return;
         }

         ret = amdgpu_bo_query_info(import.buf_handle, &info);
         if (ret) {
            print(0, "amdgpu_bo_query_info failed\n");
            return;
         }

         obj = amdgpu_object_create(import.buf_handle, 0, import.alloc_size);
         if (!obj)
            return;

         obj->bo = import.buf_handle;
         amdgpu_bo_export(obj->bo, amdgpu_bo_handle_type_kms, &obj->gem_handle);
         amdgpu_object_set_res_id(ctx, obj, res->res_id);
         print(1, "imported dmabuf -> res_id=%u" PRIx64, obj->res_id);
      } else {
         print(2, "Ignored res_id: %d (fd_type = %d)", res->res_id, fd_type);
         if (fd_type != VIRGL_RESOURCE_FD_INVALID)
            close_fd(ctx, fd, __FUNCTION__);
         return;
      }
   }
}

static void free_amdgpu_object(struct amdgpu_context *ctx, struct amdgpu_object *obj)
{
   print(2, "free obj res_id: %d", obj->res_id);
   amdgpu_remove_object(ctx, obj);

   amdgpu_bo_free(obj->bo);

   free(obj);
}

static void
amdgpu_renderer_detach_resource(struct virgl_context *vctx, struct virgl_resource *res)
{
   struct amdgpu_context *ctx = to_amdgpu_context(vctx);
   struct amdgpu_object *obj = amdgpu_get_object_from_res_id(ctx, res->res_id, NULL);

   if (!obj) {
      /* If this context doesn't know about this resource id there's nothing to do. */
       return;
   }

   print(2, "res_id=%u (fd_type: %d)", obj->res_id, res->fd_type);

   free_amdgpu_object(ctx, obj);
}

static enum virgl_resource_fd_type
amdgpu_renderer_export_opaque_handle(struct virgl_context *vctx,
                                     struct virgl_resource *res, int *out_fd)
{
   struct amdgpu_context *ctx = to_amdgpu_context(vctx);
   struct amdgpu_object *obj = amdgpu_get_object_from_res_id(ctx, res->res_id, __FUNCTION__);
   int ret;

   print(2, "obj=%p, res_id=%u", (void *)obj, res->res_id);

   if (!obj) {
      print(0, "invalid res_id %u", res->res_id);
      return VIRGL_RESOURCE_FD_INVALID;
   }

   ret = amdgpu_bo_export(obj->bo, amdgpu_bo_handle_type_dma_buf_fd, (uint32_t *)out_fd);

   if (ret) {
      print(0, "failed to get dmabuf fd: %s", strerror(errno));
      return VIRGL_RESOURCE_FD_INVALID;
   }

   char dmabufname[32] = { 0 };
   snprintf(dmabufname, sizeof(dmabufname) - 1, "e:%d-%s",
            obj->res_id, ctx->debug_name);
   set_dmabuf_name(*out_fd, dmabufname);

   if (res->fd_type == VIRGL_RESOURCE_OPAQUE_HANDLE && obj->has_metadata) {
      /* Interpret set_metadata as lazy VIRTGPU_BLOB_FLAG_USE_SHAREABLE. */
      res->fd = os_dupfd_cloexec(*out_fd);
      res->fd_type = VIRGL_RESOURCE_FD_DMABUF;
      print(2, "res_id: %d became VIRGL_RESOURCE_FD_DMABUF", res->res_id);
   } else {
      print(3, "res_id: %d one time export", res->res_id);
   }

   return VIRGL_RESOURCE_FD_DMABUF;
}

static int
amdgpu_renderer_transfer_3d(UNUSED struct virgl_context *vctx,
                            UNUSED struct virgl_resource *res,
                            UNUSED const struct vrend_transfer_info *info,
                            UNUSED int transfer_mode)
{
   struct amdgpu_context *ctx = to_amdgpu_context(vctx);
   print(0, "unsupported");
   return -1;
}

static void
update_heap_info_in_shmem(struct amdgpu_context *ctx)
{
   amdgpu_query_heap_info(ctx->dev, AMDGPU_GEM_DOMAIN_VRAM,
                       AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED,
                       &ctx->shmem->vis_vram);
   amdgpu_query_heap_info(ctx->dev, AMDGPU_GEM_DOMAIN_VRAM,
                          0,
                          &ctx->shmem->vram);
   amdgpu_query_heap_info(ctx->dev, AMDGPU_GEM_DOMAIN_GTT,
                          0,
                          &ctx->shmem->gtt);
}

static int
amdgpu_renderer_get_blob(struct virgl_context *vctx, uint32_t res_id, uint64_t blob_id,
                         uint64_t blob_size, uint32_t blob_flags,
                         struct virgl_context_blob *blob)
{
   struct amdgpu_context *ctx = to_amdgpu_context(vctx);

   print(2, "blob_id=%" PRIu64 ", res_id=%u, blob_size=%" PRIu64
         ", blob_flags=0x%x",
           blob_id, res_id, blob_size, blob_flags);

   if ((blob_id >> 32) != 0) {
      print(0, "invalid blob_id: %" PRIu64, blob_id);
      return -EINVAL;
   }

   /* blob_id of zero is reserved for the shmem buffer: */
   if (blob_id == 0) {
      int fd;

      if (blob_flags != VIRGL_RENDERER_BLOB_FLAG_USE_MAPPABLE) {
         print(0, "invalid blob_flags: 0x%x", blob_flags);
         return -EINVAL;
      }

      if (ctx->shmem) {
         print(0, "There can be only one!");
         return -EINVAL;
      }

      char name[64];
      snprintf(name, 64, "amdgpu-shmem-%s", ctx->debug_name);
      fd = os_create_anonymous_file(blob_size, name);
      if (fd < 0) {
         print(0, "Failed to create shmem file: %s", strerror(errno));
         return -ENOMEM;
      }

      int ret = fcntl(fd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW);
      if (ret) {
         print(0, "fcntl failed: %s", strerror(errno));
         close_fd(ctx, fd, __FUNCTION__);
         return -ENOMEM;
      }

      ctx->shmem = mmap(NULL, blob_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
      if (ctx->shmem == MAP_FAILED) {
         print(0, "shmem mmap failed: %s", strerror(errno));
         close_fd(ctx, fd, __FUNCTION__);
         ctx->shmem = NULL;
         return -ENOMEM;
      }
      ctx->shmem_size = blob_size;
      ctx->shm_res_id = res_id;
      print(1, "shmem: %p (size: %lu vs %lu)", (void *)ctx->shmem, blob_size, sizeof(*ctx->shmem));

      ctx->shmem->base.rsp_mem_offset = sizeof(*ctx->shmem);

      uint8_t *ptr = (uint8_t *)ctx->shmem;
      ctx->rsp_mem = &ptr[ctx->shmem->base.rsp_mem_offset];
      ctx->rsp_mem_sz = blob_size - ctx->shmem->base.rsp_mem_offset;

      blob->type = VIRGL_RESOURCE_FD_SHM;
      blob->u.fd = fd;
      blob->map_info = VIRGL_RENDERER_MAP_CACHE_WC;

      update_heap_info_in_shmem(ctx);

      return 0;
   }

   if (!valid_res_id(ctx, res_id)) {
      print(0, "Invalid res_id %u", res_id);
      return -EINVAL;
   }

   struct amdgpu_object *obj = amdgpu_retrieve_object_from_blob_id(ctx, blob_id);

   /* If GEM_NEW fails, we can end up here without a backing obj or if it's a dumb buffer. */
   if (!obj) {
      print(0, "No object with blob_id=%ld", blob_id);
      return -ENOENT;
   }

   if (obj->enable_cache_wc)
      blob->map_info = VIRGL_RENDERER_MAP_CACHE_WC;
   else
      blob->map_info = VIRGL_RENDERER_MAP_CACHE_CACHED;

   /* a memory can only be exported once; we don't want two resources to point
    * to the same storage.
    */
   if (obj->exported) {
      print(0, "Already exported! blob_id:%ld", blob_id);
      return -EINVAL;
   }

   amdgpu_object_set_res_id(ctx, obj, res_id);

   if (blob_flags & VIRGL_RENDERER_BLOB_FLAG_USE_SHAREABLE) {
      int fd, ret;

      ret = amdgpu_bo_export(obj->bo, amdgpu_bo_handle_type_dma_buf_fd, (uint32_t *)&fd);

      if (ret) {
         print(0, "Export to fd failed for blob_id:%ld r=%d (%s)", blob_id, ret, strerror(errno));
         return ret;
      }

      char dmabufname[32] = { 0 };
      snprintf(dmabufname, sizeof(dmabufname) - 1, "r:%d-%s",
               obj->res_id, ctx->debug_name);
      set_dmabuf_name(fd, dmabufname);

      print(2, "dmabuf created: %d for res_id: %d", fd, obj->res_id);

      blob->type = VIRGL_RESOURCE_FD_DMABUF;
      blob->u.fd = fd;
   } else {
      blob->type = VIRGL_RESOURCE_OPAQUE_HANDLE;
      blob->u.opaque_handle = obj->gem_handle;
   }

   obj->exported = true;

   /* Update usage (should probably be done on alloc/import instead). */
   update_heap_info_in_shmem(ctx);

   return 0;
}

static int
amdgpu_ccmd_query_info(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_query_info_req *req = to_amdgpu_ccmd_query_info_req(hdr);
   struct amdgpu_ccmd_query_info_rsp *rsp;
   unsigned rsp_len;
   if (__builtin_add_overflow(sizeof(*rsp), req->info.return_size, &rsp_len)) {
      print(1, "%s: Request size overflow: %zu + %" PRIu32 " > %u",
            __FUNCTION__, sizeof(*rsp), req->info.return_size, UINT_MAX);
      return -EINVAL;
   }

   rsp = amdgpu_context_rsp(ctx, hdr, rsp_len);

   if (!rsp)
      return -ENOMEM;

   size_t return_size = req->info.return_size;
   void *value = calloc(return_size, 1);
   struct drm_amdgpu_info request;
   memcpy(&request, &req->info, sizeof(request));
   request.return_pointer = (uintptr_t)value;

   int r = drmCommandWrite(amdgpu_device_get_fd(ctx->dev), DRM_AMDGPU_INFO, &request, sizeof(request));

   rsp->hdr.ret = r;

   if (rsp->hdr.ret < 0 && request.query != AMDGPU_INFO_HW_IP_INFO)
      print(0, "ioctl error: fd: %d r: %d|%d %s",
            amdgpu_device_get_fd(ctx->dev), rsp->hdr.ret, r,strerror(errno));

   memcpy(rsp->payload, value, req->info.return_size);
   free(value);

   return 0;
}

static int
amdgpu_ccmd_gem_new(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_gem_new_req *req = to_amdgpu_ccmd_gem_new_req(hdr);
   int ret = 0;
   int64_t va_map_size = 0;

   if (!valid_blob_id(ctx, req->blob_id)) {
      print(0, "Invalid blob_id %ld", req->blob_id);
      ret = -EINVAL;
      goto alloc_failed;
   }

   struct amdgpu_bo_alloc_request r = {
      .alloc_size = req->r.alloc_size,
      .phys_alignment = req->r.phys_alignment,
      .preferred_heap = req->r.preferred_heap,
      .flags = req->r.flags,
   };
   amdgpu_bo_handle bo_handle;
   ret = amdgpu_bo_alloc(ctx->dev, &r, &bo_handle);

   if (ret) {
      print(0, "amdgpu_bo_alloc failed: %d (%s)", ret, strerror(errno));
      goto alloc_failed;
   }

   /* If a VA address was requested, assign it to the BO now. */
   if (req->va) {
      va_map_size = align64(req->vm_map_size, getpagesize());

      ret = amdgpu_bo_va_op_raw(ctx->dev, bo_handle, 0, va_map_size, req->va,
                                req->vm_flags, AMDGPU_VA_OP_MAP);
      if (ret) {
         print(0, "amdgpu_bo_va_op_raw failed: %d va: %" PRIx64 " va_map_size: %ld (%s)",
               ret, req->va, va_map_size, strerror(errno));
         goto va_map_failed;
      }
   }

   uint32_t gem_handle;
   ret = amdgpu_bo_export(bo_handle, amdgpu_bo_handle_type_kms, &gem_handle);
   if (ret) {
      print(0, "Failed to get kms handle");
      goto export_failed;
   }

   struct amdgpu_object *obj =
      amdgpu_object_create(bo_handle, req->r.flags, req->r.alloc_size);

   if (obj == NULL)
      goto export_failed;

   obj->gem_handle = gem_handle;
   /* Enable Write-Combine except for GTT buffers with WC disabled. */
   obj->enable_cache_wc =
      (req->r.preferred_heap != AMDGPU_GEM_DOMAIN_GTT) ||
      (req->r.flags & AMDGPU_GEM_CREATE_CPU_GTT_USWC);

   amdgpu_object_set_blob_id(ctx, obj, req->blob_id);

   print(2, "new object blob_id: %ld heap: %08x flags: %lx vm_flags: %x",
         req->blob_id, req->r.preferred_heap, req->r.flags, req->vm_flags);

   return 0;

export_failed:
   if (req->vm_flags)
      amdgpu_bo_va_op(bo_handle, 0, va_map_size, req->va, 0, AMDGPU_VA_OP_UNMAP);

va_map_failed:
   amdgpu_bo_free(bo_handle);

alloc_failed:
   print(2, "ERROR blob_id: %ld heap: %08x flags: %lx vm_flags: %x va: %" PRIx64,
         req->blob_id, req->r.preferred_heap, req->r.flags, req->vm_flags, req->va);
   if (ctx->shmem)
      ctx->shmem->async_error++;
   return ret;
}

static int
amdgpu_ccmd_bo_va_op(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_bo_va_op_req *req = to_amdgpu_ccmd_bo_va_op_req(hdr);
   struct amdgpu_object *obj;
   struct amdgpu_ccmd_rsp *rsp;
   rsp = amdgpu_context_rsp(ctx, hdr, sizeof(struct amdgpu_ccmd_rsp));

   if (req->is_sparse_bo) {
      rsp->ret = amdgpu_bo_va_op_raw(
         ctx->dev, NULL, req->offset, req->vm_map_size, req->va,
         req->flags, req->op);
      if (rsp->ret && ctx->shmem) {
         ctx->shmem->async_error++;
         print(0, "amdgpu_bo_va_op_raw for sparse bo failed: offset: 0x%lx size: 0x%lx va: %" PRIx64, req->offset, req->vm_map_size, req->va);
      }
      return 0;
   }

   obj = amdgpu_get_object_from_res_id(ctx, req->res_id, __FUNCTION__);
   if (!obj) {
      /* This is ok. This means the guest closed the GEM already. */
      return -EINVAL;
   }

   rsp->ret = amdgpu_bo_va_op_raw(
      ctx->dev, obj->bo, req->offset, req->vm_map_size, req->va,
      req->flags,
      req->op);
   if (rsp->ret) {
      if (ctx->shmem)
         ctx->shmem->async_error++;

      print(0, "amdgpu_bo_va_op_raw failed: op: %d res_id: %d offset: 0x%lx size: 0x%lx va: %" PRIx64 " r=%d",
         req->op, obj->res_id, req->offset, req->vm_map_size, req->va, rsp->ret);
   } else {
      print(2, "va_op %d res_id: %u va: 0x%" PRIx64 " @offset 0x%" PRIx64,
            req->op, req->res_id, req->va, req->offset);
   }

   return 0;
}

static int
amdgpu_ccmd_set_metadata(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_set_metadata_req *req = to_amdgpu_ccmd_set_metadata_req(hdr);
   struct amdgpu_ccmd_rsp *rsp = amdgpu_context_rsp(ctx, hdr, sizeof(struct amdgpu_ccmd_rsp));

   struct amdgpu_object *obj = amdgpu_get_object_from_res_id(ctx, req->res_id, __FUNCTION__);
   if (!obj) {
      print(0, "Cannot find object with res_id=%d",
              req->res_id);
      rsp->ret = -EINVAL;
      return -1;
   }

   /* We could also store the metadata here instead of passing them to the host kernel =>
    * actually no because this would only work if the desktop runs on radeonsi-virtio.
    */
   struct amdgpu_bo_metadata metadata = {0};
   metadata.flags = req->flags;
   metadata.tiling_info = req->tiling_info;
   metadata.size_metadata = req->size_metadata;
   if (req->size_metadata) {
      if (req->size_metadata > sizeof(metadata.umd_metadata)) {
         print(0, "Metadata size is too large for target buffer: %" PRIu32 " > %zu",
               req->size_metadata, sizeof(metadata.umd_metadata));
         rsp->ret = -EINVAL;
         return -1;
      }
      size_t requested_size = size_add(req->size_metadata,
                                       offsetof(struct amdgpu_ccmd_set_metadata_req,
                                                umd_metadata));
      if (requested_size > hdr->len || requested_size == SIZE_MAX) {
         print(0, "Metadata size is too large for source buffer: %zu > %" PRIu32,
               requested_size, hdr->len);
         rsp->ret = -EINVAL;
         return -1;
      }
      memcpy(metadata.umd_metadata, req->umd_metadata, req->size_metadata);
   }

   rsp->ret = amdgpu_bo_set_metadata(obj->bo, &metadata);
   if (rsp->ret) {
      print(0, "amdgpu_bo_set_metadata failed for res: %d", req->res_id);
   }

   obj->has_metadata = true;

   return 0;
}

static int
amdgpu_ccmd_bo_query_info(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_bo_query_info_req *req =
      to_amdgpu_ccmd_bo_query_info_req(hdr);
   struct amdgpu_ccmd_bo_query_info_rsp *rsp;
   rsp = amdgpu_context_rsp(ctx, hdr, sizeof(struct amdgpu_ccmd_bo_query_info_rsp));

   struct amdgpu_object *obj = amdgpu_get_object_from_res_id(ctx, req->res_id, __FUNCTION__);
   if (!obj) {
      print(0, "Cannot find object");
      rsp->hdr.ret = -EINVAL;
      return -1;
   }

   struct amdgpu_bo_info info = {0};
   rsp->hdr.ret = amdgpu_bo_query_info(obj->bo, &info);
   if (rsp->hdr.ret) {
      print(0, "amdgpu_bo_query_info failed");
      rsp->hdr.ret = -EINVAL;
      return -1;
   }

   memcpy(&rsp->info, &info, sizeof(info));

   return 0;
}

static int
amdgpu_ccmd_create_ctx(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_create_ctx_req *req = to_amdgpu_ccmd_create_ctx_req(hdr);
   struct amdgpu_ccmd_create_ctx_rsp *rsp;
   rsp = amdgpu_context_rsp(ctx, hdr, sizeof(struct amdgpu_ccmd_create_ctx_rsp));

   if (req->create) {
      amdgpu_context_handle ctx_handle;

      int r = amdgpu_cs_ctx_create2(ctx->dev, req->priority, &ctx_handle);
      rsp->hdr.ret = r;
      if (r) {
         print(0, "amdgpu_cs_ctx_create2(prio=%d) failed (%s)", req->priority, strerror(errno));
         return r;
      }

      print(1, "amdgpu_cs_ctx_create2 dev: %p -> %p", (void*)ctx->dev, (void*)ctx_handle);

      if (!rsp)
         return -ENOMEM;

      /* We need the ctx_id in the guest */
      struct amdgpu_cs_fence f = {
         .context = ctx_handle
      };
      struct drm_amdgpu_cs_chunk_dep d = { 0 };
      amdgpu_cs_chunk_fence_to_dep(&f, &d);
      rsp->ctx_id = d.ctx_id;

      _mesa_hash_table_u64_insert(ctx->id_to_ctx, d.ctx_id,
                                  (void*)(uintptr_t)ctx_handle);

   } else {
      amdgpu_context_handle actx = _mesa_hash_table_u64_search(ctx->id_to_ctx, req->id);
      if (actx == NULL) {
         print(0, "Failed to find ctx_id: %d", req->id);
      } else {
         amdgpu_cs_ctx_free(actx);
         rsp->hdr.ret = 0;
        _mesa_hash_table_u64_remove(ctx->id_to_ctx, req->id);
      }

      print(1, "amdgpu_cs_ctx_free dev: %p -> %p", (void*)ctx->dev, (void*)actx);
   }

   return 0;
}

/* Check that 'offset + len' fits in buffer of size 'max_len', that
 * 'len' is correct for 'count' objects of size 'size',
 * and that 'offset' is aligned to 'align'.
 */
static bool validate_chunk_inputs(size_t offset, size_t len, struct amdgpu_context *ctx,
                                  size_t count, size_t size, size_t align)
{
   if (offset % align != 0) {
      print(0, "Offset 0x%zx is misaligned (needed 0x%zx)", offset, align);
      return false; /* misaligned */
   }
   size_t total_len = size_mul(size, count);
   if (total_len == SIZE_MAX || total_len > len) {
      print(0, "Length 0x%zx cannot hold 0x%zx entries of size 0x%zx",
            len, count, size);
      return false;
   }
   return true;
}

static int
amdgpu_ccmd_cs_submit(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_cs_submit_req *req = to_amdgpu_ccmd_cs_submit_req(hdr);
   struct drm_amdgpu_bo_list_in bo_list_in;
   struct drm_amdgpu_cs_chunk_fence user_fence;
   struct drm_amdgpu_cs_chunk_sem syncobj_in = { 0 };
   struct drm_amdgpu_bo_list_entry *bo_handles_in = NULL;
   struct drm_amdgpu_bo_list_entry *bo_list = NULL;
   struct drm_amdgpu_cs_chunk *chunks;
   unsigned num_chunks = 0;
   uint64_t seqno = 0;
   int r;

   struct amdgpu_ccmd_rsp *rsp;
   rsp = amdgpu_context_rsp(ctx, hdr, sizeof(struct amdgpu_ccmd_rsp));
   /* Do not allocate arbitrarily large buffer. */
   if (req->num_chunks > AMDGPU_CCMD_CS_SUBMIT_MAX_NUM_CHUNKS) {
      print(1, "%s: Invalid num_chunks: %" PRIu32 " > %d",
            __FUNCTION__, req->num_chunks, AMDGPU_CCMD_CS_SUBMIT_MAX_NUM_CHUNKS);
      rsp->ret = -EINVAL;
      return -1;
   }
   if (req->ring_idx == 0 || ctx->timeline_count < req->ring_idx) {
      print(0, "Invalid ring_idx value: %d (must be in [1, %d] range)",
         req->ring_idx,
         ctx->timeline_count);
      rsp->ret = -EINVAL;
      return -1;
   }
   chunks = malloc((req->num_chunks + 1 /* syncobj_in */ + 1 /* syncobj_out */) *
                   sizeof(*chunks));
   if (chunks == NULL) {
      print(0, "Failed to allocate %" PRIu32 " chunks", req->num_chunks + 2);
      r = -EINVAL;
      goto end;
   }

   amdgpu_context_handle actx = _mesa_hash_table_u64_search(ctx->id_to_ctx,
                                                            (uintptr_t)req->ctx_id);

   struct desc {
      uint16_t chunk_id;
      uint16_t length_dw;
      uint32_t offset;
   };
   size_t descriptors_len = size_add(offsetof(struct amdgpu_ccmd_cs_submit_req, payload),
                                     size_mul(req->num_chunks, sizeof(struct desc)));
   if (descriptors_len == SIZE_MAX || descriptors_len > hdr->len) {
      print(0, "Descriptors are out of bounds: %zu + %zu * %" PRIu32 " > %" PRIu32,
            offsetof(struct amdgpu_ccmd_cs_submit_req, payload),
            sizeof(struct desc), req->num_chunks, hdr->len);
      r = -EINVAL;
      goto end;
   }
   const struct desc *descriptors = (void*) req->payload;

   for (size_t i = 0; i < req->num_chunks; i++) {
      unsigned chunk_id = descriptors[i].chunk_id;
      size_t offset = size_add(descriptors_len, descriptors[i].offset);
      size_t len = size_mul(descriptors[i].length_dw, 4);
      size_t end = size_add(offset, len);

      chunks[num_chunks].chunk_id = chunk_id;
      /* Validate input. */
      if (end == SIZE_MAX || end > hdr->len) {
         print(0, "Descriptors are out of bounds: %zu > %" PRIu32, end, hdr->len);
         r = -EINVAL;
         goto end;
      }
#define validate_chunk_inputs(count, type) \
   validate_chunk_inputs(offset, len, ctx, count, sizeof(type), alignof(type))

      void *input = (char *)req + offset;

      if (chunk_id == AMDGPU_CHUNK_ID_BO_HANDLES) {
         uint32_t bo_count = req->bo_number;
         if (!validate_chunk_inputs(bo_count, typeof(*bo_handles_in))) {
            r = -EINVAL;
            goto end;
         }

         bo_handles_in = input;
         bo_list = malloc(bo_count * sizeof(struct drm_amdgpu_bo_list_entry));

         bo_list_in.operation = ~0;
         bo_list_in.list_handle = ~0;
         bo_list_in.bo_number = bo_count;
         bo_list_in.bo_info_size = sizeof(struct drm_amdgpu_bo_list_entry);
         bo_list_in.bo_info_ptr = (uint64_t)(uintptr_t)bo_list;

         for (uint32_t j = 0; j < bo_count; j++) {
            struct amdgpu_object *obj =
               amdgpu_get_object_from_res_id(ctx, bo_handles_in[j].bo_handle, __FUNCTION__);
            if (!obj) {
               print(0, "Couldn't retrieve bo with res_id %d", bo_handles_in[j].bo_handle);
               r = -EINVAL;
               goto end;
            }
            bo_list[j].bo_handle = obj->gem_handle;
            bo_list[j].bo_priority = bo_handles_in[j].bo_priority;
         }

         chunks[num_chunks].length_dw = sizeof(bo_list_in) / 4;
         chunks[num_chunks].chunk_data = (uintptr_t)&bo_list_in;
      } else if (chunk_id == AMDGPU_CHUNK_ID_FENCE) {
         const struct drm_amdgpu_cs_chunk_fence *in;
         if (!validate_chunk_inputs(1, typeof(*in))) {
            r = -EINVAL;
            goto end;
         }
         in = input;
         if (in->offset % sizeof(uint64_t)) {
            print(0, "Invalid chunk offset %" PRIu32 " (not multiple of 8)", in->offset);
            r = -EINVAL;
            goto end;
         }
         struct amdgpu_object *obj =
               amdgpu_get_object_from_res_id(ctx, in->handle, __FUNCTION__);
         if (!obj) {
            print(0, "Couldn't retrieve user_fence bo with res_id %d", in->handle);
            r = -EINVAL;
            goto end;
         }
         struct amdgpu_cs_fence_info info;
         info.offset = in->offset / sizeof(int64_t);
         info.handle = obj->bo;
         amdgpu_cs_chunk_fence_info_to_data(&info, (void*) &user_fence);
         chunks[num_chunks].length_dw = sizeof(struct drm_amdgpu_cs_chunk_fence) / 4;
         chunks[num_chunks].chunk_data = (uintptr_t)&user_fence;
      } else if (chunk_id == AMDGPU_CHUNK_ID_DEPENDENCIES) {
         chunks[num_chunks].length_dw = descriptors[i].length_dw;
         chunks[num_chunks].chunk_data = (uintptr_t)input;
      } else if (chunk_id == AMDGPU_CHUNK_ID_IB) {
         chunks[num_chunks].length_dw = descriptors[i].length_dw;
         chunks[num_chunks].chunk_data = (uintptr_t)input;
         if (chunks[num_chunks].length_dw != sizeof(struct drm_amdgpu_cs_chunk_ib) / 4) {
            r = -EINVAL;
            goto end;
         }
      } else {
         print(0, "Unsupported chunk_id %d received", chunk_id);
         r = -EINVAL;
         goto end;
      }

      num_chunks++;
   }

   int in_fence_fd = virgl_context_take_in_fence_fd(&ctx->base);
   if (in_fence_fd >= 0) {
      chunks[num_chunks].chunk_id = AMDGPU_CHUNK_ID_SYNCOBJ_IN;
      chunks[num_chunks].length_dw = sizeof(syncobj_in) / 4;
      chunks[num_chunks].chunk_data = (uintptr_t)&syncobj_in;
      r = drmSyncobjCreate(amdgpu_device_get_fd(ctx->dev), 0, &syncobj_in.handle);
      if (r != 0) {
         print(0, "input syncobj creation failed");
         goto end;
      }
      r = drmSyncobjImportSyncFile(
         amdgpu_device_get_fd(ctx->dev), syncobj_in.handle, in_fence_fd);
      if (r == 0)
         num_chunks++;
   }

   struct drm_amdgpu_cs_chunk_sem syncobj_out = { 0 };
   chunks[num_chunks].chunk_id = AMDGPU_CHUNK_ID_SYNCOBJ_OUT;
   chunks[num_chunks].length_dw = sizeof(syncobj_out) / 4;
   chunks[num_chunks].chunk_data = (uintptr_t)&syncobj_out;
   r = drmSyncobjCreate(amdgpu_device_get_fd(ctx->dev), 0, &syncobj_out.handle);
   if (r == 0)
      num_chunks++;
   else
      print(0, "out syncobj creation failed");

   r = amdgpu_cs_submit_raw2(ctx->dev, actx, 0, num_chunks, chunks, &seqno);

   if (in_fence_fd >= 0) {
      close(in_fence_fd);
      drmSyncobjDestroy(amdgpu_device_get_fd(ctx->dev), syncobj_in.handle);
   }

   if (r == 0) {
      int submit_fd;
      r = drmSyncobjExportSyncFile(amdgpu_device_get_fd(ctx->dev), syncobj_out.handle, &submit_fd);
      if (r == 0) {
         drm_timeline_set_last_fence_fd(&ctx->timelines[req->ring_idx - 1], submit_fd);
         print(3, "Set last fd ring_idx: %d: %d", req->ring_idx, submit_fd);
      } else {
         print(0, "Failed to create a FD from the syncobj (%d)", r);
      }
   } else {
      if (ctx->shmem)
         ctx->shmem->async_error++;
      print(0, "command submission failed failed (ring: %d, num_chunks: %d)", req->ring_idx, num_chunks);
   }

   if (r != 0 || ctx->debug >= 4) {
      print(1, "GPU submit used %d BOs:", req->bo_number);
      print(1, "Used | Resource ID ");
      print(1, "-----|-------------");
      hash_table_foreach (ctx->resource_table, entry) {
         const struct amdgpu_object *o = entry->data;
         bool used = false;
         for (unsigned j = 0; j < req->bo_number && !used; j++) {
            if (bo_handles_in[j].bo_handle == o->res_id)
               used = true;
         }

         if (r == 0 && !used)
            continue;

         print(1, "%s | %*u ", used ? "  x " : "    ",
               (int)strlen("Resource ID"), o->res_id);
      }
   }

   drmSyncobjDestroy(amdgpu_device_get_fd(ctx->dev), syncobj_out.handle);

   print(3, "ctx: %d -> seqno={v=%d a=%ld} r=%d", req->ctx_id, hdr->seqno, seqno, r);

end:
   if (bo_list)
      free(bo_list);
   free(chunks);
   rsp->ret = r;
   return r;
}

static int amdgpu_ccmd_reserve_vmid(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_reserve_vmid_req *req = to_amdgpu_ccmd_reserve_vmid_req(hdr);
   struct amdgpu_ccmd_rsp *rsp;
   rsp = amdgpu_context_rsp(ctx, hdr, sizeof(struct amdgpu_ccmd_rsp));
   rsp->ret = req->enable ?
         amdgpu_vm_reserve_vmid(ctx->dev, 0) : amdgpu_vm_unreserve_vmid(ctx->dev, 0);
   return 0;
}

static int amdgpu_ccmd_set_pstate(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   const struct amdgpu_ccmd_set_pstate_req *req = to_amdgpu_ccmd_set_pstate_req(hdr);
   struct amdgpu_ccmd_set_pstate_rsp *rsp;
   rsp = amdgpu_context_rsp(ctx, hdr, sizeof(*rsp));
   amdgpu_context_handle actx = _mesa_hash_table_u64_search(ctx->id_to_ctx,
                                                            (uintptr_t)req->ctx_id);
   if (actx == NULL) {
      print(0, "Couldn't find amdgpu_context with id %d", req->ctx_id);
      rsp->hdr.ret = -EINVAL;
      return -1;
   }
   rsp->hdr.ret = amdgpu_cs_ctx_stable_pstate(actx, req->op, req->flags, &rsp->out_flags);
   return 0;
}

static const struct ccmd {
   const char *name;
   int (*handler)(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr);
   size_t size;
} ccmd_dispatch[] = {
#define HANDLER(N, n)                                                                    \
   [AMDGPU_CCMD_##N] = {#N, amdgpu_ccmd_##n, sizeof(struct amdgpu_ccmd_##n##_req)}
   HANDLER(QUERY_INFO, query_info),
   HANDLER(GEM_NEW, gem_new),
   HANDLER(BO_VA_OP, bo_va_op),
   HANDLER(CS_SUBMIT, cs_submit),
   HANDLER(SET_METADATA, set_metadata),
   HANDLER(BO_QUERY_INFO, bo_query_info),
   HANDLER(CREATE_CTX, create_ctx),
   HANDLER(RESERVE_VMID, reserve_vmid),
   HANDLER(SET_PSTATE, set_pstate),
};

static int
submit_cmd_dispatch(struct amdgpu_context *ctx, const struct vdrm_ccmd_req *hdr)
{
   int ret;

   if (!ctx->shmem) {
      print(0, "shmem not inited");
      return -EINVAL;
   }

   if (hdr->cmd >= ARRAY_SIZE(ccmd_dispatch)) {
      print(0, "invalid cmd: %u", hdr->cmd);
      return -EINVAL;
   }

   const struct ccmd *ccmd = &ccmd_dispatch[hdr->cmd];

   if (!ccmd->handler) {
      print(0, "no handler: %u", hdr->cmd);
      return -EINVAL;
   }

   struct cmd_stat * const stat = &ctx->statistics.s[hdr->cmd - 1];
   print(2, "command: %s (seqno: %u, size:%zu)",
         ccmd->name, hdr->seqno, ccmd->size);

   uint64_t start = util_current_thread_get_time_nano();

   /* If the request length from the guest is smaller than the expected
    * size, ie. newer host and older guest, we need to make a copy of
    * the request with the new fields at the end zero initialized.
    */
   if (ccmd->size > hdr->len) {
      uint8_t buf[ccmd->size];

      memcpy(&buf[0], hdr, hdr->len);
      memset(&buf[hdr->len], 0, ccmd->size - hdr->len);

      ret = ccmd->handler(ctx, (struct vdrm_ccmd_req *)buf);
   } else {
      ret = ccmd->handler(ctx, hdr);
   }

   uint64_t end = util_current_thread_get_time_nano();

   stat->count++;
   uint64_t dt = (end - start) / 1000;
   stat->min_duration_us = MIN2(stat->min_duration_us, dt);
   stat->max_duration_us = MAX2(stat->max_duration_us, dt);
   stat->total_duration_us += dt;

   uint64_t ms = end / 1000000;
   if (ms - ctx->statistics.last_print_ms >= 1000 && ctx->debug >= 1) {
      unsigned c = (unsigned)((uintptr_t)ctx >> 8) % 256;
      printf("\033[0;38;5;%dm", c);

      uint64_t dt = ms - ctx->statistics.last_print_ms;
      uint32_t total = 0;
      int align = 1;
      for (unsigned i = 0; i < ARRAY_SIZE(ctx->statistics.s); i++) {
         total += ctx->statistics.s[i].count;
         if (total >= 10000)
            align = 5;
         else if (total >= 1000)
            align = 4;
         else if (total >= 100)
            align = 3;
      }

      printf("====== Stats for %s (%d cmd, dt: %ld ms) =====\n", ctx->debug_name, total, dt);
      for (unsigned i = 0; i < ARRAY_SIZE(ctx->statistics.s); i++) {
         if (ctx->statistics.s[i].count) {
            int n = (int)roundf(100 * ctx->statistics.s[i].count / (float)total);
            printf("\t%s %3d%% (n: %*d, min: %.3f ms, max: %.3f ms, avg: %.3f ms)\n",
                   ccmd_dispatch[i + 1].name,
                   n,
                   align,
                   ctx->statistics.s[i].count,
                   ctx->statistics.s[i].min_duration_us / 1000.f,
                   ctx->statistics.s[i].max_duration_us / 1000.f,
                   ctx->statistics.s[i].total_duration_us / (ctx->statistics.s[i].count * 1000.f));
         }
      }
      printf("\033[0m");
      memset(ctx->statistics.s, 0, sizeof(ctx->statistics.s));
      ctx->statistics.last_print_ms = ms;
   }

   if (ret)
      print(0, "%s: dispatch failed: %d (%s)", ccmd->name, ret, strerror(errno));

   /* If the response length from the guest is smaller than the
    * expected size, ie. newer host and older guest, then a shadow
    * copy is used, and we need to copy back to the actual rsp
    * buffer.
    */
   struct amdgpu_ccmd_rsp *rsp = amdgpu_context_rsp_noshadow(ctx, hdr);
   if (ctx->current_rsp && (ctx->current_rsp != rsp)) {
      unsigned len = rsp->base.len;
      memcpy(rsp, ctx->current_rsp, len);
      rsp->base.len = len;
      free(ctx->current_rsp);
   }
   ctx->current_rsp = NULL;

   /* Note that commands with no response, like SET_DEBUGINFO, could
    * be sent before the shmem buffer is allocated:
    */
   if (ctx->shmem) {
      /* TODO better way to do this?  We need ACQ_REL semantics (AFAIU)
       * to ensure that writes to response buffer are visible to the
       * guest process before the update of the seqno.  Otherwise we
       * could just use p_atomic_set.
       */
      uint32_t seqno = hdr->seqno;
      p_atomic_xchg(&ctx->shmem->base.seqno, seqno);
      print(3, "seqno: %u", seqno);
   }

   return ret;
}

static int
amdgpu_renderer_submit_cmd(struct virgl_context *vctx, const void *_buffer, size_t size)
{
   struct amdgpu_context *ctx = to_amdgpu_context(vctx);
   const uint8_t *buffer = _buffer;

   while (size >= sizeof(struct vdrm_ccmd_req)) {
      const struct vdrm_ccmd_req *hdr = (const struct vdrm_ccmd_req *)buffer;

      /* Sanity check first: */
      if ((hdr->len > size) || (hdr->len < sizeof(*hdr)) || (hdr->len % 4)) {
         print(0, "bad size, %u vs %zu (%u)", hdr->len, size, hdr->cmd);
         return -EINVAL;
      }

      if (hdr->rsp_off % 4) {
         print(0, "bad rsp_off, %u", hdr->rsp_off);
         return -EINVAL;
      }

      int ret = submit_cmd_dispatch(ctx, hdr);
      if (ret) {
         print(0, "dispatch failed: %d (%u)", ret, hdr->cmd);
         return ret;
      }

      buffer += hdr->len;
      size -= hdr->len;
   }

   if (size > 0) {
      print(0, "bad size, %zu trailing bytes", size);
      return -EINVAL;
   }

   return 0;
}

static int
amdgpu_renderer_get_fencing_fd(struct virgl_context *vctx)
{
   assert(false); /* Unused ? */
   struct amdgpu_context *mctx = to_amdgpu_context(vctx);
   return mctx->eventfd;
}

static void
amdgpu_renderer_retire_fences(UNUSED struct virgl_context *vctx)
{
   /* No-op as VIRGL_RENDERER_ASYNC_FENCE_CB is required */
}

static int
amdgpu_renderer_submit_fence(struct virgl_context *vctx, uint32_t flags,
                             uint32_t ring_idx, uint64_t fence_id)
{
   struct amdgpu_context *ctx = to_amdgpu_context(vctx);

   /* timeline is ring_idx-1 (because ring_idx 0 is host CPU timeline) */
   if (ring_idx > AMDGPU_HW_IP_NUM) {
      print(0, "invalid ring_idx: %" PRIu32, ring_idx);
      return -EINVAL;
   }
   /* ring_idx zero is used for the guest to synchronize with host CPU,
    * meaning by the time ->submit_fence() is called, the fence has
    * already passed.. so just immediate signal:
    */
   if (ring_idx == 0 || ctx->timelines[ring_idx - 1].last_fence_fd < 0) {
      vctx->fence_retire(vctx, ring_idx, fence_id);
      return 0;
   }

   print(3, "ring_idx: %d fence_id: %lu", ring_idx, fence_id);
   return drm_timeline_submit_fence(&ctx->timelines[ring_idx - 1], flags, fence_id);
}

static void
amdgpu_renderer_fence_retire(struct virgl_context *vctx,
                             uint32_t ring_idx,
                             uint64_t fence_id)
{
   struct amdgpu_context *ctx = to_amdgpu_context(vctx);
   print(3, "ring_idx: %d fence_id: %lu", ring_idx, fence_id);
   vctx->fence_retire(vctx, ring_idx, fence_id);
}

struct virgl_context *
amdgpu_renderer_create(int fd, size_t debug_len, const char *debug_name)
{
   struct amdgpu_context *ctx;
   amdgpu_device_handle dev;

   int r;
   uint32_t drm_major, drm_minor;

   /* Don't use libdrm_amdgpu device deduplication logic. The goal is to
    * get a different drm_file for each application inside the guest so we
    * get inter-application implicit synchronisation handled for us by
    * the kernel.
    * This also makes the application completely separate (eg: each one gets
    * its own VM space).
    * Using dlopen wouldn't work because it returns the same refcounted handle.
    * dlmopen(LM_ID_NEWLM, ...) would work but there can only be 16 namespaces.
    */
   r = amdgpu_device_initialize2(fd, false, &drm_major, &drm_minor, &dev);
   if (r) {
      printf("amdgpu_device_initialize failed (fd=%d, %s)\n", fd, strerror(errno));
      close(fd);
      return NULL;
   }

   uint32_t timeline_count = 0;
   for (unsigned ip_type = 0; ip_type < AMDGPU_HW_IP_NUM; ip_type++) {
      struct drm_amdgpu_info_hw_ip ip_info = {0};

      int r = amdgpu_query_hw_ip_info(dev, ip_type, 0, &ip_info);
      if (r < 0)
         continue;

      if (ip_info.available_rings)
         timeline_count += util_bitcount(ip_info.available_rings);
   }
   if (timeline_count == 0) {
      printf("ERR: No available_rings for dev %d\n", fd);
      amdgpu_device_deinitialize(dev);
      return NULL;
   }

   ctx = calloc(1, sizeof(struct amdgpu_context) + timeline_count * sizeof(ctx->timelines[0]));
   if (ctx == NULL)
      goto fail;
   ctx->debug_name = strndup(debug_name, debug_len);
   ctx->debug = -1;
   ctx->dev = dev;
   const char *d = getenv("DEBUG");
   if (d)
      ctx->debug = atoi(d);

   print(1, "amdgpu_renderer_create name=%s fd=%d (from %d) -> dev=%p", ctx->debug_name, fd,
         amdgpu_device_get_fd(ctx->dev), (void*)ctx->dev);

   /* Indexed by blob_id, but only lower 32b of blob_id are used: */
   ctx->blob_table = _mesa_hash_table_create_u32_keys(NULL);
   /* Indexed by res_id: */
   ctx->resource_table = _mesa_hash_table_create_u32_keys(NULL);

   ctx->base.destroy = amdgpu_renderer_destroy;
   ctx->base.attach_resource = amdgpu_renderer_attach_resource;
   ctx->base.detach_resource = amdgpu_renderer_detach_resource;
   ctx->base.export_opaque_handle = amdgpu_renderer_export_opaque_handle;
   ctx->base.transfer_3d = amdgpu_renderer_transfer_3d;
   ctx->base.get_blob = amdgpu_renderer_get_blob;
   ctx->base.submit_cmd = amdgpu_renderer_submit_cmd;
   ctx->base.get_fencing_fd = amdgpu_renderer_get_fencing_fd;
   ctx->base.retire_fences = amdgpu_renderer_retire_fences;
   ctx->base.submit_fence = amdgpu_renderer_submit_fence;
   ctx->base.supports_fence_sharing = true;

   ctx->id_to_ctx = _mesa_hash_table_u64_create(NULL);

   ctx->eventfd = create_eventfd(0);

   /* Ring 0 is for CPU execution. */
   /* TODO: add a setting to control which queues are exposed to the
    * guest.
    */
   uint32_t ring_idx = 1;
   for (unsigned ip_type = 0; ip_type < AMDGPU_HW_IP_NUM; ip_type++) {
      struct drm_amdgpu_info_hw_ip ip_info = {0};

      int r = amdgpu_query_hw_ip_info(dev, ip_type, 0, &ip_info);
      if (r < 0)
         continue;

      int cnt = util_bitcount(ip_info.available_rings);
      for (int i = 0; i < cnt; i++) {
         char *name;
         if (asprintf(&name, "a-%s-%d", ctx->debug_name, ring_idx) < 0)
            goto fail;
         drm_timeline_init(&ctx->timelines[ring_idx - 1], &ctx->base,
                           name,
                           ctx->eventfd, ring_idx,
                           amdgpu_renderer_fence_retire);
         ring_idx += 1;
      }
   }
   ctx->timeline_count = timeline_count;
   assert(ring_idx == timeline_count + 1);

   ctx->statistics.last_print_ms = util_current_thread_get_time_nano() / 1000000;

   close(fd);

   return &ctx->base;

fail:
   amdgpu_device_deinitialize(dev);
   free(ctx);
   close(fd);
   return NULL;
}
