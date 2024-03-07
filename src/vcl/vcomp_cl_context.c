/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_cl_context.h"
#include "vcomp_context.h"
#include "vcomp_device.h"
#include "vcomp_platform.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

static void
vcomp_dispatch_clCreateContextMESA(struct vcl_dispatch_context *dispatch,
                                   struct vcl_command_clCreateContextMESA *args)
{
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_platform *platform;

   // Check platform in properties
   for (const cl_context_properties *prop = args->properties;
        prop != NULL && *prop != 0;
        prop++)
   {
      if (*prop == CL_CONTEXT_PLATFORM)
      {
         const cl_context_properties *next = prop + 1;
         if (!next)
         {
            args->ret = CL_INVALID_PROPERTY;
            return;
         }

         /*
          * This is the guest platform handle which comes within the properties.
          * Unlike the handles in the arguments of the command, this one is not
          * converted to a vcomp object pointer already, so we do it now.
          */
         cl_platform_id *guest_handle = (cl_platform_id *)next;
         platform = vcomp_context_get_object(vctx, (vcomp_object_id)*guest_handle);
         if (!platform || !vcomp_context_contains_platform(vctx, platform))
         {
            args->ret = CL_INVALID_PLATFORM;
            return;
         }

         /*
          * Substitute the guest handle in the properties array with the host
          * handle, so that it will work when calling clCreateContext()
          */
         *guest_handle = platform->base.handle.platform;
         break;
      }
   }

   // Collect host handles
   cl_device_id *handles = calloc(args->num_devices, sizeof(*handles));
   for (uint32_t i = 0; i < args->num_devices; ++i)
   {
      struct vcomp_device *device = vcomp_device_from_handle(args->devices[i]);
      if (!device || !vcomp_context_contains_platform(vctx, device->platform))
      {
         args->ret = CL_INVALID_PLATFORM;
         free(handles);
         return;
      }
      handles[i] = device->base.handle.device;
   }

   cl_context ctx = clCreateContext(args->properties, args->num_devices, handles, NULL, NULL, &args->ret);

   free(handles);

   if (!ctx)
   {
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->context);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      args->ret = CL_INVALID_VALUE;
      return;
   }

   struct vcomp_cl_context *context = vcomp_object_alloc(sizeof(*context), id);
   if (!context)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   context->devices = calloc(args->num_devices, sizeof(*context->devices));
   context->base.handle.cl_context = ctx;

   vcomp_context_add_object(vctx, &context->base);
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

   args->ret = vcomp_cl_context_destroy(vctx, context);
}

static void
vcomp_dispatch_clGetContextInfo(UNUSED struct vcl_dispatch_context *dispatch,
                                struct vcl_command_clGetContextInfo *args)
{
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }

   args->ret = clGetContextInfo(context->base.handle.cl_context, args->param_name, args->param_value_size, args->param_value, args->param_value_size_ret);
}

void vcomp_context_init_context_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clCreateContextMESA = vcomp_dispatch_clCreateContextMESA;
   dispatch->dispatch_clReleaseContext = vcomp_dispatch_clReleaseContext;
   dispatch->dispatch_clGetContextInfo = vcomp_dispatch_clGetContextInfo;
}

cl_int vcomp_cl_context_destroy(struct vcomp_context *vctx, struct vcomp_cl_context *context)
{
   cl_int ret = clReleaseContext(context->base.handle.cl_context);
   free(context->devices);
   vcomp_context_remove_object(vctx, &context->base);
   return ret;
}
