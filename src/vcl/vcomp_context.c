/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_common.h"
#include "vcomp_renderer.h"

#include "util/u_memory.h"
#include "virgl_context.h"

struct vcomp_context
{
   struct virgl_context base;
   char debug_name[32];
};

static void
vcomp_context_destroy(struct virgl_context *ctx)
{
   struct vcomp_context *vctx = (struct vcomp_context *)ctx;
   free(vctx);
}

static void
vcomp_context_init_base(struct vcomp_context *vctx,
                        uint32_t ctx_id)
{
   struct virgl_context *ctx = &vctx->base;

   ctx->ctx_id = ctx_id;
   ctx->destroy = vcomp_context_destroy;
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

   vcomp_context_init_base(vctx, id);

   vcomp_log("context %d created: `%s`", vctx->base.ctx_id, vctx->debug_name);

   return &vctx->base;
}
