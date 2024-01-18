/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_common.h"
#include "vcomp_renderer.h"

#include "util/hash_table.h"
#include "util/u_memory.h"
#include "virgl_context.h"
#include "vrend_renderer.h"

#include <errno.h>

struct vcomp_context
{
   struct virgl_context base;
   char debug_name[32];

   struct hash_table *resource_table;
};

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
   return vrend_write_to_iovec(iov, iov_count, info->offset, vres->ptr, info->box->width);
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

   vctx->resource_table =
       _mesa_hash_table_create(NULL, _mesa_hash_u32, _mesa_key_u32_equal);
   if (!vctx->resource_table)
      goto err_ctx_resource_table;

   vcomp_context_init_base(vctx, id);

   vcomp_log("context %d created: `%s`", vctx->base.ctx_id, vctx->debug_name);

   return &vctx->base;

err_ctx_resource_table:
   free(vctx);
   return NULL;
}
