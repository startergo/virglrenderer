/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_memory.h"
#include "vcomp_cl_context.h"
#include "vcomp_context.h"
#include "vcomp_queue.h"
#include "vcomp_event.h"

#include "vcl-protocol/vcl_protocol_renderer_memory.h"

static void
vcomp_context_add_memory(struct vcomp_context *vctx, cl_mem memory, cl_mem *args_memory,
                         cl_int *args_ret)
{
   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args_memory);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      *args_ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   struct vcomp_memory *v_memory = vcomp_object_alloc(sizeof(*v_memory), id);
   if (!v_memory)
   {
      *args_ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   v_memory->base.handle.memory = memory;

   vcomp_context_add_object(vctx, &v_memory->base);
}

static void
vcomp_dispatch_clCreateBufferMESA(struct vcl_dispatch_context *ctx,
                                  struct vcl_command_clCreateBufferMESA *args)
{
   struct vcomp_context *vctx = ctx->data;

   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }

   cl_mem mem = clCreateBuffer(context->base.handle.cl_context, args->flags, args->size, (void *)args->host_ptr, &args->ret);
   if (!mem)
      return;

   vcomp_context_add_memory(vctx, mem, args->buffer, &args->ret);
}

static void
vcomp_dispatch_clReleaseMemObject(struct vcl_dispatch_context *dispatch, struct vcl_command_clReleaseMemObject *args)
{
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_memory *memory = vcomp_memory_from_handle(args->memobj);

   if (!memory)
   {
      vcomp_context_set_fatal(vctx);
      return;
   }

   args->ret = vcomp_memory_destroy(vctx, memory);
}

static void
vcomp_dispatch_clGetMemObjectInfo(UNUSED struct vcl_dispatch_context *dispatch,
                                  struct vcl_command_clGetMemObjectInfo *args)
{
   vcl_replace_clGetMemObjectInfo_args_handle(args);

   args->ret = clGetMemObjectInfo(args->memobj, args->param_name,
                                  args->param_value_size, args->param_value,
                                  args->param_value_size_ret);
}

static void
vcomp_dispatch_clEnqueueReadBuffer(struct vcl_dispatch_context *dispatch,
                                   struct vcl_command_clEnqueueReadBuffer *args)
{
   struct vcomp_context *vctx = dispatch->data;

   vcl_replace_clEnqueueReadBuffer_args_handle(args);

   cl_event host_event;
   args->ret = clEnqueueReadBuffer(args->command_queue, args->buffer,
                                   args->blocking_read, args->offset, args->size,
                                   args->ptr, args->num_events_in_wait_list,
                                   args->event_wait_list,
                                   args->event ? &host_event : NULL);


   /* Need to create a new vcomp event */
   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clEnqueueWriteBuffer(struct vcl_dispatch_context *dispatch,
                                    struct vcl_command_clEnqueueWriteBuffer *args)
{
   struct vcomp_context *vctx = dispatch->data;

   vcl_replace_clEnqueueWriteBuffer_args_handle(args);

   cl_event host_event;
   args->ret = clEnqueueWriteBuffer(args->command_queue, args->buffer,
                                    args->blocking_write, args->offset, args->size,
                                    args->ptr, args->num_events_in_wait_list,
                                    args->event_wait_list,
                                    args->event ? &host_event : NULL);

   /* Need to create a new vcomp event */
   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clEnqueueCopyBuffer(struct vcl_dispatch_context *dispatch,
                                   struct vcl_command_clEnqueueCopyBuffer *args)
{
   struct vcomp_context *vctx = dispatch->data;

   vcl_replace_clEnqueueCopyBuffer_args_handle(args);

   cl_event host_event;
   args->ret = clEnqueueCopyBuffer(args->command_queue, args->src_buffer,
                                   args->dst_buffer, args->src_offset,
                                   args->dst_offset, args->size,
                                   args->num_events_in_wait_list,
                                   args->event_wait_list,
                                   args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clEnqueueCopyBufferRect(struct vcl_dispatch_context *dispatch,
                                       struct vcl_command_clEnqueueCopyBufferRect *args)
{
   struct vcomp_context *vctx = dispatch->data;

   vcl_replace_clEnqueueCopyBufferRect_args_handle(args);

   cl_event host_event;
   args->ret = clEnqueueCopyBufferRect(args->command_queue, args->src_buffer,
                                       args->dst_buffer, args->src_origin,
                                       args->dst_origin, args->region,
                                       args->src_row_pitch,
                                       args->src_slice_pitch,
                                       args->dst_row_pitch,
                                       args->dst_slice_pitch,
                                       args->num_events_in_wait_list,
                                       args->event_wait_list,
                                       args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clEnqueueFillBuffer(struct vcl_dispatch_context *dispatch,
                                   struct vcl_command_clEnqueueFillBuffer *args)
{
   struct vcomp_context *vctx = dispatch->data;

   vcl_replace_clEnqueueFillBuffer_args_handle(args);

   cl_event host_event;
   args->ret = clEnqueueFillBuffer(args->command_queue, args->buffer,
                                   args->pattern, args->pattern_size,
                                   args->offset, args->size,
                                   args->num_events_in_wait_list,
                                   args->event_wait_list,
                                   args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clEnqueueMigrateMemObjects(struct vcl_dispatch_context *dispatch,
                                          struct vcl_command_clEnqueueMigrateMemObjects *args)
{
   struct vcomp_context *vctx = dispatch->data;

   vcl_replace_clEnqueueMigrateMemObjects_args_handle(args);

   cl_event host_event;
   args->ret = clEnqueueMigrateMemObjects(args->command_queue,
                                          args->num_mem_objects,
                                          args->mem_objects,
                                          args->flags,
                                          args->num_events_in_wait_list,
                                          args->event_wait_list,
                                          args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clCreateImage2DMESA(struct vcl_dispatch_context *ctx,
                                   struct vcl_command_clCreateImage2DMESA *args)
{
   struct vcomp_context *vctx = ctx->data;

   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }

   cl_mem mem = clCreateImage2D(context->base.handle.cl_context, args->flags,
                                args->image_format, args->image_width,
                                args->image_height, args->image_row_pitch,
                                (void *)args->host_ptr, &args->ret);
   if (!mem)
      return;

   vcomp_context_add_memory(vctx, mem, args->image, &args->ret);
}

static void
vcomp_dispatch_clCreateImage3DMESA(struct vcl_dispatch_context *ctx,
                                   struct vcl_command_clCreateImage3DMESA *args)
{
   struct vcomp_context *vctx = ctx->data;

   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }

   cl_mem mem = clCreateImage3D(context->base.handle.cl_context, args->flags, args->image_format, args->image_width,
                                args->image_height, args->image_depth, args->image_row_pitch, args->image_slice_pitch,
                                (void *)args->host_ptr, &args->ret);
   if (!mem)
      return;

   vcomp_context_add_memory(vctx, mem, args->image, &args->ret);
}

static void
vcomp_dispatch_clGetSupportedImageFormats(
    UNUSED struct vcl_dispatch_context *ctx,
    struct vcl_command_clGetSupportedImageFormats *args)
{
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }

   args->ret = clGetSupportedImageFormats(context->base.handle.cl_context, args->flags,
                                          args->image_type, args->num_entries,
                                          args->image_formats, args->num_image_formats);
}

static void
vcomp_dispatch_clGetImageInfo(UNUSED struct vcl_dispatch_context *ctx,
                              struct vcl_command_clGetImageInfo *args)
{
   struct vcomp_memory *image = vcomp_memory_from_handle(args->image);
   if (!image)
   {
      args->ret = CL_INVALID_MEM_OBJECT;
      return;
   }

   args->ret = clGetImageInfo(image->base.handle.memory, args->param_name,
                              args->param_value_size, args->param_value,
                              args->param_value_size_ret);
}

void vcomp_context_init_memory_dispatch(struct vcomp_context *vctx)
{
   vctx->dispatch.dispatch_clCreateBufferMESA = vcomp_dispatch_clCreateBufferMESA;
   vctx->dispatch.dispatch_clReleaseMemObject = vcomp_dispatch_clReleaseMemObject;
   vctx->dispatch.dispatch_clGetMemObjectInfo = vcomp_dispatch_clGetMemObjectInfo;
   vctx->dispatch.dispatch_clEnqueueReadBuffer = vcomp_dispatch_clEnqueueReadBuffer;
   vctx->dispatch.dispatch_clEnqueueWriteBuffer = vcomp_dispatch_clEnqueueWriteBuffer;
   vctx->dispatch.dispatch_clEnqueueCopyBuffer = vcomp_dispatch_clEnqueueCopyBuffer;
   vctx->dispatch.dispatch_clEnqueueCopyBufferRect = vcomp_dispatch_clEnqueueCopyBufferRect;
   vctx->dispatch.dispatch_clEnqueueFillBuffer = vcomp_dispatch_clEnqueueFillBuffer;
   vctx->dispatch.dispatch_clEnqueueMigrateMemObjects = vcomp_dispatch_clEnqueueMigrateMemObjects;
   vctx->dispatch.dispatch_clCreateImage2DMESA = vcomp_dispatch_clCreateImage2DMESA;
   vctx->dispatch.dispatch_clCreateImage3DMESA = vcomp_dispatch_clCreateImage3DMESA;
   vctx->dispatch.dispatch_clGetSupportedImageFormats =
       vcomp_dispatch_clGetSupportedImageFormats;
   vctx->dispatch.dispatch_clGetImageInfo = vcomp_dispatch_clGetImageInfo;
}

cl_int vcomp_memory_destroy(struct vcomp_context *vctx, struct vcomp_memory *memory)
{
   cl_int ret = clReleaseMemObject(memory->base.handle.memory);
   vcomp_context_remove_object(vctx, &memory->base);
   return ret;
}
