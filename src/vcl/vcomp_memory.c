/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_memory.h"
#include "vcomp_cl_context.h"
#include "vcomp_context.h"
#include "vcomp_queue.h"

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
   {
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->buffer);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   struct vcomp_memory *memory = vcomp_object_alloc(sizeof(*memory), id);
   if (!memory)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   memory->base.handle.memory = mem;
   vcomp_context_add_object(vctx, &memory->base);
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
vcomp_dispatch_clEnqueueReadBuffer(UNUSED struct vcl_dispatch_context *dispatch,
                                   struct vcl_command_clEnqueueReadBuffer *args)
{
   struct vcomp_queue *queue = vcomp_queue_from_handle(args->command_queue);
   if (!queue)
   {
      args->ret = CL_INVALID_COMMAND_QUEUE;
      return;
   }
   struct vcomp_memory *mem = vcomp_memory_from_handle(args->buffer);
   if (!mem)
   {
      args->ret = CL_INVALID_MEM_OBJECT;
      return;
   }

   args->ret = clEnqueueReadBuffer(queue->base.handle.queue,
                                   mem->base.handle.memory,
                                   args->blocking_read, args->offset,
                                   args->size, args->ptr, 0, NULL, NULL);
}

static void
vcomp_dispatch_clEnqueueWriteBuffer(UNUSED struct vcl_dispatch_context *dispatch,
                                    struct vcl_command_clEnqueueWriteBuffer *args)
{
   struct vcomp_queue *queue = vcomp_queue_from_handle(args->command_queue);
   if (!queue)
   {
      args->ret = CL_INVALID_COMMAND_QUEUE;
      return;
   }
   struct vcomp_memory *mem = vcomp_memory_from_handle(args->buffer);
   if (!mem)
   {
      args->ret = CL_INVALID_MEM_OBJECT;
      return;
   }

   args->ret = clEnqueueWriteBuffer(queue->base.handle.queue,
                                    mem->base.handle.memory,
                                    args->blocking_write, args->offset,
                                    args->size, args->ptr, 0, NULL, NULL);
}

void vcomp_context_init_memory_dispatch(struct vcomp_context *vctx)
{
   vctx->dispatch.dispatch_clCreateBufferMESA = vcomp_dispatch_clCreateBufferMESA;
   vctx->dispatch.dispatch_clReleaseMemObject = vcomp_dispatch_clReleaseMemObject;
   vctx->dispatch.dispatch_clEnqueueReadBuffer = vcomp_dispatch_clEnqueueReadBuffer;
   vctx->dispatch.dispatch_clEnqueueWriteBuffer = vcomp_dispatch_clEnqueueWriteBuffer;
}

cl_int vcomp_memory_destroy(struct vcomp_context *vctx, struct vcomp_memory *memory)
{
   cl_int ret = clReleaseMemObject(memory->base.handle.memory);
   vcomp_context_remove_object(vctx, &memory->base);
   return ret;
}
