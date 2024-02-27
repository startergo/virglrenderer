/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_queue.h"
#include "vcomp_context.h"
#include "vcomp_cl_context.h"
#include "vcomp_device.h"
#include "vcomp_platform.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

static void
vcomp_dispatch_clCreateCommandQueueMESA(struct vcl_dispatch_context *dispatch,
                                        struct vcl_command_clCreateCommandQueueMESA *args)
{
#ifdef CL_USE_DEPRECATED_OPENCL_1_2_APIS
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   struct vcomp_device *device = vcomp_device_from_handle(args->device);

   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }
   if (!device)
   {
      args->ret = CL_INVALID_DEVICE;
      return;
   }

   cl_command_queue comm_queue = clCreateCommandQueue(context->base.handle.cl_context, device->base.handle.device, args->properties, &args->ret);

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->queue);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      args->ret = CL_INVALID_VALUE;
      return;
   }

   struct vcomp_queue *queue = vcomp_object_alloc(sizeof(*queue), id);
   if (!queue)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   queue->base.handle.queue = comm_queue;
   vcomp_context_add_object(vctx, &queue->base);
#else
   (void)dispatch;
   (void)args;
#endif /* CL_USE_DEPRECATED_OPENCL_1_2_APIS */
}

static void
vcomp_dispatch_clCreateCommandQueueWithPropertiesMESA(struct vcl_dispatch_context *dispatch,
                                                      struct vcl_command_clCreateCommandQueueWithPropertiesMESA *args)
{
#ifdef CL_API_SUFFIX__VERSION_2_0
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   struct vcomp_device *device = vcomp_device_from_handle(args->device);

   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }
   if (!device)
   {
      args->ret = CL_INVALID_DEVICE;
      return;
   }

   cl_command_queue comm_queue = clCreateCommandQueueWithProperties(context->base.handle.cl_context, device->base.handle.device, args->properties, &args->ret);

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->queue);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      args->ret = CL_INVALID_VALUE;
      return;
   }

   struct vcomp_queue *queue = vcomp_object_alloc(sizeof(*queue), id);
   if (!queue)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   queue->base.handle.queue = comm_queue;
   vcomp_context_add_object(vctx, &queue->base);
#else
   (void)dispatch;
   (void)args;
#endif /* CL_API_SUFFIX__VERSION_2_0 */
}

static void
vcomp_dispatch_clGetCommandQueueInfo(UNUSED struct vcl_dispatch_context *dispatch,
                                     struct vcl_command_clGetCommandQueueInfo *args)
{
   struct vcomp_queue *queue = vcomp_queue_from_handle(args->command_queue);
   if (!queue)
   {
      args->ret = CL_INVALID_COMMAND_QUEUE;
      return;
   }

   args->ret = clGetCommandQueueInfo(queue->base.handle.queue, args->param_name, args->param_value_size, args->param_value, args->param_value_size_ret);
}

static void
vcomp_dispatch_clReleaseCommandQueue(struct vcl_dispatch_context *dispatch,
                                     struct vcl_command_clReleaseCommandQueue *args)
{
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_queue *queue = vcomp_queue_from_handle(args->command_queue);

   if (!queue)
   {
      vcomp_context_set_fatal(vctx);
      args->ret = CL_INVALID_COMMAND_QUEUE;
      return;
   }
   args->ret = vcomp_queue_destroy(vctx, queue);
}

static void
vcomp_dispatch_clSetCommandQueueProperty(struct vcl_dispatch_context *dispatch,
                                         struct vcl_command_clSetCommandQueueProperty *args)
{
#ifdef CL_USE_DEPRECATED_OPENCL_1_0_APIS
   args->ret = CL_INVALID_OPERATION;

   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_queue *queue = vcomp_queue_from_handle(args->command_queue);
   if (!queue)
   {
      args->ret = CL_INVALID_COMMAND_QUEUE;
      return;
   }

   args->ret = clSetCommandQueueProperty(queue->base.handle.queue, args->properties, args->enable, args->old_properties);
#else
   (void)dispatch;
   (void)args;
#endif /* CL_USE_DEPRECATED_OPENCL_1_0_APIS */
}

static void
vcomp_dispatch_clSetDefaultDeviceCommandQueue(struct vcl_dispatch_context *dispatch,
                                              struct vcl_command_clSetDefaultDeviceCommandQueue *args)
{
#ifdef CL_API_SUFFIX__VERSION_2_1
   struct vcomp_context *vctx = dispatch->data;
   args->ret = CL_SUCCESS;

   struct vcomp_queue *queue = vcomp_queue_from_handle(args->command_queue);
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   struct vcomp_device *device = vcomp_device_from_handle(args->device);

   if (!queue)
   {
      args->ret = CL_INVALID_COMMAND_QUEUE;
      return;
   }
   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }
   if (!device)
   {
      args->ret = CL_INVALID_DEVICE;
      return;
   }
   args->ret = clSetDefaultDeviceCommandQueue(context->base.handle.cl_context, device->base.handle.device, queue->base.handle.queue);
#else
   (void)dispatch;
   (void)args;
#endif /* CL_API_SUFFIX__VERSION_2_1 */
}

void vcomp_context_init_queue_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clCreateCommandQueueMESA = vcomp_dispatch_clCreateCommandQueueMESA;
   dispatch->dispatch_clCreateCommandQueueWithPropertiesMESA = vcomp_dispatch_clCreateCommandQueueWithPropertiesMESA;
   dispatch->dispatch_clGetCommandQueueInfo = vcomp_dispatch_clGetCommandQueueInfo;
   dispatch->dispatch_clReleaseCommandQueue = vcomp_dispatch_clReleaseCommandQueue;
   dispatch->dispatch_clSetCommandQueueProperty = vcomp_dispatch_clSetCommandQueueProperty;
   dispatch->dispatch_clSetDefaultDeviceCommandQueue = vcomp_dispatch_clSetDefaultDeviceCommandQueue;
}

cl_int vcomp_queue_destroy(struct vcomp_context *vctx, struct vcomp_queue *queue)
{
   cl_int ret = clReleaseCommandQueue(queue->base.handle.queue);
   vcomp_context_remove_object(vctx, &queue->base);
   return ret;
}
