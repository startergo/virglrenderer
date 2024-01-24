/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_transport.h"
#include "vcomp_context.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

#include "vrend_renderer.h"

struct vcomp_context;

static void
vcomp_dispatch_clSetReplyBufferMESA(struct vcl_dispatch_context *dispatch,
                                    struct vcl_command_clSetReplyBufferMESA *args)
{
   struct vcomp_context *vctx = dispatch->data;

   struct virgl_resource *res = virgl_resource_lookup(args->resource_id);
   if (!res)
   {
      vcomp_log("Failed to find virgl resource %u", args->resource_id);
      vcomp_cs_decoder_set_fatal(&vctx->decoder);
      return;
   }

   struct vrend_resource *vres = (struct vrend_resource *)res->pipe_resource;
   if (!vres)
   {
      vcomp_log("No pipe resource attached to virgl resource %u", args->resource_id);
      vcomp_cs_decoder_set_fatal(&vctx->decoder);
      return;
   }

   vcomp_cs_encoder_set_stream(&vctx->encoder, vres->ptr, vres->base.width0);
}

void vcomp_context_init_transport_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clSetReplyBufferMESA = vcomp_dispatch_clSetReplyBufferMESA;
}
