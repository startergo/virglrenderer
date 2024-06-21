/*
 * Copyright 2020 Google LLC
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_cl_context.h"
#include "vcomp_context.h"
#include "vcomp_common.h"
#include "vcomp_device.h"
#include "vcomp_event.h"
#include "vcomp_memory.h"
#include "vcomp_platform.h"
#include "vcomp_renderer.h"
#include "vcomp_sampler.h"
#include "vcomp_transport.h"
#include "vcomp_queue.h"
#include "vcomp_program.h"
#include "vcomp_kernel.h"

#include "vcl-protocol/vcl_protocol_renderer_dispatches.h"

#define XXH_INLINE_ALL
#include "util/xxhash.h"

#include "util/hash_table.h"
#include "util/u_memory.h"
#include "vrend_renderer.h"

#include <errno.h>

struct vcomp_resource_attachment
{
   struct virgl_resource *res;
};

static void
vcomp_context_free_resource(struct hash_entry *entry)
{
   struct vcomp_resource_attachment *att = entry->data;
   free(att);
}

static void
vcomp_context_destroy(struct virgl_context *ctx)
{
   struct vcomp_context *vctx = (struct vcomp_context *)ctx;
   for (uint32_t i = 0; i < vctx->platform_count; i++)
   {
      struct vcomp_platform *platform = vctx->platforms[i];
      if (!platform)
         break;
      vcomp_platform_destroy(vctx, platform);
   }

   free(vctx->platform_handles);
   free(vctx->platforms);

   _mesa_hash_table_destroy(vctx->object_table, vcomp_context_free_object);
   _mesa_hash_table_destroy(vctx->resource_table, vcomp_context_free_resource);

   free(vctx);
}

static inline struct vcomp_resource_attachment *
vcomp_context_get_resource(struct vcomp_context *vctx, uint32_t res_id)
{
   const struct hash_entry *entry = _mesa_hash_table_search(vctx->resource_table, &res_id);
   return likely(entry) ? entry->data : NULL;
}

static inline void
vcomp_context_add_resource(struct vcomp_context *vctx, struct vcomp_resource_attachment *att)
{
   assert(!_mesa_hash_table_search(vctx->resource_table, &att->res->res_id));
   _mesa_hash_table_insert(vctx->resource_table, &att->res->res_id, att);
}

static inline void
vcomp_context_remove_resource(struct vcomp_context *vctx, uint32_t res_id)
{
   struct hash_entry *entry = _mesa_hash_table_search(vctx->resource_table, &res_id);
   if (likely(entry))
   {
      vcomp_context_free_resource(entry);
      _mesa_hash_table_remove(vctx->resource_table, entry);
   }
}

static void
vcomp_context_attach_resource(struct virgl_context *ctx, struct virgl_resource *res)
{
   struct vcomp_context *vctx = (struct vcomp_context *)ctx;
   struct vcomp_resource_attachment *att = vcomp_context_get_resource(vctx, res->res_id);
   if (att)
   {
      assert(att->res == res);
      return;
   }

   att = CALLOC_STRUCT(vcomp_resource_attachment);
   att->res = res;

   vcomp_context_add_resource(vctx, att);
}

static void
vcomp_context_detach_resource(struct virgl_context *ctx, struct virgl_resource *res)
{
   struct vcomp_context *vctx = (struct vcomp_context *)ctx;
   vcomp_context_remove_resource(vctx, res->res_id);
}

static int
vcomp_context_transfer_send_iov(UNUSED struct vcomp_context *vctx,
                                struct vrend_resource *vres,
                                const struct iovec *iov,
                                int iov_count,
                                const struct vrend_transfer_info *info)
{
   vrend_write_to_iovec(iov, iov_count, info->offset, vres->ptr, info->box->width);
   return 0;
}

static int
vcomp_context_transfer_3d(struct virgl_context *ctx,
                          struct virgl_resource *res,
                          const struct vrend_transfer_info *info,
                          int transfer_mode)
{
   struct vcomp_context *vctx = (struct vcomp_context *)ctx;

   if (!res->pipe_resource)
   {
      vcomp_log("transfer-3d: Failed to find resource %d", res->res_id);
      return EINVAL;
   }

   struct vrend_resource *vres = (struct vrend_resource *)res->pipe_resource;

   // TODO: switch context?

   const struct iovec *iov;
   int iov_count;

   if (info->iovec && info->iovec_cnt)
   {
      iov = info->iovec;
      iov_count = info->iovec_cnt;
   }
   else
   {
      iov = vres->iov;
      iov_count = vres->num_iovs;
   }

   // TODO check transfer and iov bounds

   switch (transfer_mode)
   {
   case VIRGL_TRANSFER_TO_HOST:
      return EINVAL;
   case VIRGL_TRANSFER_FROM_HOST:
      return vcomp_context_transfer_send_iov(vctx, vres, iov, iov_count, info);
   default:
      assert(false);
   }

   return 0;
}

typedef int (*vcomp_decode_callback)(struct vcomp_context *ctx, const uint32_t *buf, uint32_t length);

static int
vcomp_context_submit_cmd(struct virgl_context *base, const void *buffer, size_t size)
{
   struct vcomp_context *ctx = (struct vcomp_context *)base;
   int ret = 0;

   /* CS error is considered fatal (destroy the context?) */
   if (vcomp_cs_decoder_get_fatal(&ctx->decoder))
      return -EINVAL;

   vcomp_cs_decoder_set_stream(&ctx->decoder, buffer, size);

   while (vcomp_cs_decoder_has_command(&ctx->decoder))
   {
      vcl_dispatch_command(&ctx->dispatch);
      if (vcomp_cs_decoder_get_fatal(&ctx->decoder))
      {
         ret = -EINVAL;
         break;
      }
   }

   vcomp_cs_decoder_reset(&ctx->decoder);

   return ret;
}

static void
vcomp_dispatch_debug_log(UNUSED struct vcl_dispatch_context *dispatch, const char *msg)
{
   vcomp_log(msg);
}

static void
vcomp_context_init_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->data = vctx;
   dispatch->debug_log = vcomp_dispatch_debug_log;

   dispatch->encoder = (struct vcl_cs_encoder *)&vctx->encoder;
   dispatch->decoder = (struct vcl_cs_decoder *)&vctx->decoder;

   vcomp_context_init_transport_dispatch(vctx);
   vcomp_context_init_platform_dispatch(vctx);
   vcomp_context_init_device_dispatch(vctx);
   vcomp_context_init_context_dispatch(vctx);
   vcomp_context_init_queue_dispatch(vctx);
   vcomp_context_init_memory_dispatch(vctx);
   vcomp_context_init_event_dispatch(vctx);
   vcomp_context_init_program_dispatch(vctx);
   vcomp_context_init_sampler_dispatch(vctx);
   vcomp_context_init_kernel_dispatch(vctx);
}

static void
vcomp_retire_fences(UNUSED struct virgl_context *ctx)
{
}

static int
vcomp_get_fencing_fd(UNUSED struct virgl_context *ctx)
{
   return 0;
}

static void
vcomp_context_init_base(struct vcomp_context *vctx,
                        uint32_t ctx_id)
{
   struct virgl_context *ctx = &vctx->base;

   ctx->ctx_id = ctx_id;
   ctx->destroy = vcomp_context_destroy;
   ctx->attach_resource = vcomp_context_attach_resource;
   ctx->detach_resource = vcomp_context_detach_resource;
   ctx->transfer_3d = vcomp_context_transfer_3d;
   ctx->submit_cmd = vcomp_context_submit_cmd;
   ctx->retire_fences = vcomp_retire_fences;
   ctx->get_fencing_fd = vcomp_get_fencing_fd;
}

static uint32_t
vcomp_hash_u64(const void *key)
{
   return XXH32(key, sizeof(uint64_t), 0);
}

static bool
vcomp_key_u64_equal(const void *key1, const void *key2)
{
   return *(const uint64_t *)key1 == *(const uint64_t *)key2;
}

struct virgl_context *
vcomp_context_create(int id, uint32_t nlen, const char *debug_name)
{
   struct vcomp_context *vctx = CALLOC_STRUCT(vcomp_context);

   if (!vctx)
      return NULL;

   if (nlen && debug_name)
   {
      uint32_t max_nlen = sizeof(vctx->debug_name) - 1;
      uint32_t name_len = nlen < max_nlen ? nlen : max_nlen;
      strncpy(vctx->debug_name, debug_name, name_len);
      vctx->debug_name[max_nlen] = 0;
   }

   vctx->object_table =
       _mesa_hash_table_create(NULL, vcomp_hash_u64, vcomp_key_u64_equal);
   if (!vctx->object_table)
      goto err_ctx_object_table;

   vctx->resource_table =
       _mesa_hash_table_create(NULL, _mesa_hash_u32, _mesa_key_u32_equal);
   if (!vctx->resource_table)
      goto err_ctx_resource_table;

   vcomp_cs_decoder_init(&vctx->decoder, vctx->object_table, vctx->resource_table);
   vcomp_cs_encoder_init(&vctx->encoder, &vctx->decoder.fatal_error);

   vcomp_context_init_base(vctx, id);
   vcomp_context_init_dispatch(vctx);

   return &vctx->base;

err_ctx_resource_table:
   _mesa_hash_table_destroy(vctx->object_table, vcomp_context_free_object);
err_ctx_object_table:
   free(vctx);
   return NULL;
}
