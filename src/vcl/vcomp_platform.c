/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_platform.h"
#include "vcomp_context.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

struct vcomp_context;

static void
vcomp_dispatch_clGetPlatformIDs(UNUSED struct vcl_dispatch_context *dispatch,
                                struct vcl_command_clGetPlatformIDs *args)
{
   args->ret = clGetPlatformIDs(args->num_entries, args->platforms, args->num_platforms);
}

void vcomp_context_init_platform_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clGetPlatformIDs =
       vcomp_dispatch_clGetPlatformIDs;
}
