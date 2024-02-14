/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_device.h"
#include "vcomp_context.h"
#include "vcomp_platform.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

struct vcomp_device
{
   struct vcomp_object base;

   struct vcomp_platform *platform;
};

VCOMP_DEFINE_OBJECT_CAST(device, cl_device_id)

static cl_int
vcomp_platform_get_devices(struct vcomp_platform *platform)
{
   if (platform->device_count)
      return CL_SUCCESS;

   uint32_t count;
   cl_int result = clGetDeviceIDs(platform->base.handle.platform, CL_DEVICE_TYPE_ALL, 0, NULL, &count);
   if (result != CL_SUCCESS)
      return result;

   cl_device_id *handles = calloc(count, sizeof(*handles));
   struct vcomp_device **devices = calloc(count, sizeof(*devices));
   if (!handles || !devices)
   {
      free(devices);
      free(handles);
      return CL_OUT_OF_HOST_MEMORY;
   }

   result = clGetDeviceIDs(platform->base.handle.platform, CL_DEVICE_TYPE_ALL, count, handles, NULL);
   if (result != CL_SUCCESS)
   {
      free(devices);
      free(handles);
      return result;
   }

   platform->device_count = count;
   platform->device_handles = handles;
   platform->devices = devices;

   return CL_SUCCESS;
}

static void
vcomp_dispatch_clGetDeviceIDs(struct vcl_dispatch_context *dispatch,
                              struct vcl_command_clGetDeviceIDs *args)
{
   struct vcomp_context *vctx = dispatch->data;

   struct vcomp_platform *platform = vcomp_platform_from_handle(args->platform);
   args->ret = vcomp_platform_get_devices(platform);
   if (args->ret != CL_SUCCESS)
      return;

   uint32_t count = platform->device_count;
   if (!args->devices)
   {
      *args->num_devices = count;
      args->ret = CL_SUCCESS;
      return;
   }

   count = MIN2(count, args->num_entries);
   args->ret = CL_SUCCESS;

   uint32_t i;
   for (i = 0; i < count; i++)
   {
      struct vcomp_device *device = platform->devices[i];
      const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)&args->devices[i]);

      if (device)
      {
         if (device->base.id != id)
         {
            vcomp_context_set_fatal(vctx);
            break;
         }
         continue;
      }

      if (!vcomp_context_validate_object_id(vctx, id))
         break;

      device = vcomp_object_alloc(sizeof(*device), id);
      if (!device)
      {
         args->ret = CL_OUT_OF_HOST_MEMORY;
         break;
      }

      device->platform = platform;
      device->base.handle.device = platform->device_handles[i];

      platform->devices[i] = device;

      vcomp_context_add_object(vctx, &device->base);
   }
   /* remove all devices on errors */
   if (i < count)
   {
      for (i = 0; i < platform->device_count; i++)
      {
         struct vcomp_device *device = platform->devices[i];
         if (!device)
            break;
         vcomp_context_remove_object(vctx, &device->base);
         platform->devices[i] = NULL;
      }
   }
}

static void
vcomp_dispatch_clGetDeviceInfo(struct vcl_dispatch_context *dispatch,
                               struct vcl_command_clGetDeviceInfo *args)
{
   struct vcomp_context *vctx = dispatch->data;

   struct vcomp_device *device = vcomp_device_from_handle(args->device);
   if (!device ||
       !vcomp_context_contains_platform(vctx, device->platform) ||
       !vcomp_platform_contains_device(device->platform, device))
   {
      args->ret = CL_INVALID_PLATFORM;
      return;
   }

   args->ret = clGetDeviceInfo(device->base.handle.device, args->param_name, args->param_value_size, args->param_value, args->param_value_size_ret);
}

void vcomp_context_init_device_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clGetDeviceIDs = vcomp_dispatch_clGetDeviceIDs;
   dispatch->dispatch_clGetDeviceInfo = vcomp_dispatch_clGetDeviceInfo;
}

void vcomp_device_destroy(struct vcomp_context *vctx, struct vcomp_device *device)
{
   vcomp_context_remove_object(vctx, &device->base);
}
