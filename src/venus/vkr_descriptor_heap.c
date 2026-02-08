/*
 * Copyright 2026 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include "vkr_descriptor_heap.h"

#include "venus-protocol/vn_protocol_renderer_descriptor_heap.h"

#include "vkr_context.h"
#include "vkr_device.h"

static void
vkr_dispatch_vkWriteSamplerDescriptorsEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkWriteSamplerDescriptorsEXT *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkWriteSamplerDescriptorsEXT_args_handle(args);
   args->ret = vk->WriteSamplerDescriptorsEXT(args->device, args->samplerCount,
                                              args->pSamplers, args->pDescriptors);
}

static void
vkr_dispatch_vkWriteResourceDescriptorsEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkWriteResourceDescriptorsEXT *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkWriteResourceDescriptorsEXT_args_handle(args);
   args->ret = vk->WriteResourceDescriptorsEXT(args->device, args->resourceCount,
                                               args->pResources, args->pDescriptors);
}

static void
vkr_dispatch_vkGetImageOpaqueCaptureDataEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkGetImageOpaqueCaptureDataEXT *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkGetImageOpaqueCaptureDataEXT_args_handle(args);
   args->ret = vk->GetImageOpaqueCaptureDataEXT(args->device, args->imageCount,
                                                args->pImages, args->pDatas);
}

static void
vkr_dispatch_vkRegisterCustomBorderColorEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkRegisterCustomBorderColorEXT *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkRegisterCustomBorderColorEXT_args_handle(args);
   args->ret = vk->RegisterCustomBorderColorEXT(args->device, args->pBorderColor,
                                                args->requestIndex, args->pIndex);
}

static void
vkr_dispatch_vkUnregisterCustomBorderColorEXT(
   UNUSED struct vn_dispatch_context *dispatch,
   struct vn_command_vkUnregisterCustomBorderColorEXT *args)
{
   struct vkr_device *dev = vkr_device_from_handle(args->device);
   struct vn_device_proc_table *vk = &dev->proc_table;

   vn_replace_vkUnregisterCustomBorderColorEXT_args_handle(args);
   vk->UnregisterCustomBorderColorEXT(args->device, args->index);
}

void
vkr_context_init_descriptor_heap_dispatch(struct vkr_context *ctx)
{
   struct vn_dispatch_context *dispatch = &ctx->dispatch;

   dispatch->dispatch_vkWriteSamplerDescriptorsEXT =
      vkr_dispatch_vkWriteSamplerDescriptorsEXT;
   dispatch->dispatch_vkWriteResourceDescriptorsEXT =
      vkr_dispatch_vkWriteResourceDescriptorsEXT;
   dispatch->dispatch_vkGetImageOpaqueCaptureDataEXT =
      vkr_dispatch_vkGetImageOpaqueCaptureDataEXT;
   dispatch->dispatch_vkRegisterCustomBorderColorEXT =
      vkr_dispatch_vkRegisterCustomBorderColorEXT;
   dispatch->dispatch_vkUnregisterCustomBorderColorEXT =
      vkr_dispatch_vkUnregisterCustomBorderColorEXT;
}
