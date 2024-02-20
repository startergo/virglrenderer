/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_cl_context.h"
#include "vcomp_context.h"
#include "vcomp_device.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

struct vcomp_cl_context
{
   struct vcomp_object base;

   struct vcomp_device **devices;
};
VCOMP_DEFINE_OBJECT_CAST(cl_context, cl_context)

static void
vcomp_dispatch_clCreateContext(struct vcl_dispatch_context *dispatch,
                               struct vcl_command_clCreateContext *args)
{
   struct vcomp_context *vctx = dispatch->data;
   args->ret = NULL;

   // Collect host handles
   cl_device_id *handles = calloc(args->num_devices, sizeof(*handles));
   for (uint32_t i = 0; i < args->num_devices; ++i)
   {
      struct vcomp_device *device = vcomp_device_from_handle(args->devices[i]);
      if (!device || !vcomp_context_contains_platform(vctx, device->platform))
      {
         args->ret = NULL;
         free(handles);
         return;
      }
      handles[i] = device->base.handle.device;
   }

   cl_context ctx = clCreateContext(args->properties, args->num_devices, handles, NULL, NULL, args->errcode_ret);

   free(handles);

   if (!ctx)
   {
      return;
   }

   // clCreateContext does not take cl_context output handle as input, so we
   // expect our custom clPrepareContextMESA() to be called before this.
   if (!vctx->prepared_context_handle)
   {
      vcomp_log("clPrepareContextMESA() has not been called");
      return;
   }
   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)&vctx->prepared_context_handle);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      return;
   }

   struct vcomp_cl_context *context = vcomp_object_alloc(sizeof(*context), id);
   if (!context)
   {
      return;
   }

   context->devices = calloc(args->num_devices, sizeof(*context->devices));
   context->base.handle.cl_context = ctx;

   vcomp_context_add_object(vctx, &context->base);

   args->ret = vctx->prepared_context_handle;
   // Consume prepared context
   vctx->prepared_context_handle = NULL;
}

static void
vcomp_dispatch_clReleaseContext(struct vcl_dispatch_context *dispatch, struct vcl_command_clReleaseContext *args)
{
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);

   if (!context)
   {
      vcomp_context_set_fatal(vctx);
      return;
   }

   vcomp_cl_context_destroy(vctx, context);
}

void
vcomp_context_init_context_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clCreateContext = vcomp_dispatch_clCreateContext;
   dispatch->dispatch_clReleaseContext = vcomp_dispatch_clReleaseContext;
}

void
vcomp_cl_context_destroy(struct vcomp_context *vctx, struct vcomp_cl_context *context)
{
   clReleaseContext(context->base.handle.cl_context);
   free(context->devices);
   vcomp_context_remove_object(vctx, &context->base);
}
