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

   v_memory->map_table =
       _mesa_hash_table_u64_create(NULL);
   if (!v_memory->map_table)
   {
      free(v_memory);
      *args_ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   vcomp_context_add_object(vctx, &v_memory->base);
}

static void
vcomp_dispatch_clCreateBufferMESA(struct vcl_dispatch_context *ctx,
                                  struct vcl_command_clCreateBufferMESA *args)
{
   struct vcomp_context *vctx = ctx->data;

   vcl_replace_clCreateBufferMESA_args_handle(args);

   cl_mem mem = clCreateBuffer(args->context, args->flags, args->size,
                               (void *)args->host_ptr, &args->ret);
   if (!mem || args->ret != CL_SUCCESS)
      return;

   vcomp_context_add_memory(vctx, mem, args->buffer, &args->ret);
}

static void
vcomp_dispatch_clCreateSubBufferMESA(struct vcl_dispatch_context *ctx,
                                     struct vcl_command_clCreateSubBufferMESA *args)
{
   struct vcomp_context *vctx = ctx->data;

   vcl_replace_clCreateSubBufferMESA_args_handle(args);

   cl_mem mem = clCreateSubBuffer(args->buffer, args->flags,
                                  args->buffer_create_type,
                                  args->buffer_create_info, &args->ret);
   if (!mem || args->ret != CL_SUCCESS)
      return;

   vcomp_context_add_memory(vctx, mem, args->sub_buffer, &args->ret);
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

static void
vcomp_dispatch_clCreateImageMESA(struct vcl_dispatch_context *ctx,
                                 struct vcl_command_clCreateImageMESA *args)
{
   struct vcomp_context *vctx = ctx->data;
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   cl_image_desc image_desc;
   image_desc.image_type = args->image_desc->image_type;
   image_desc.image_width = args->image_desc->image_width;
   image_desc.image_height = args->image_desc->image_height;
   image_desc.image_depth = args->image_desc->image_depth;
   image_desc.image_array_size = args->image_desc->image_array_size;
   image_desc.image_row_pitch = args->image_desc->image_row_pitch;
   image_desc.image_slice_pitch = args->image_desc->image_slice_pitch;
   image_desc.num_mip_levels = args->image_desc->num_mip_levels;
   image_desc.num_samples = args->image_desc->num_samples;
   image_desc.buffer = args->image_desc->mem_object;

   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }

   cl_mem mem = clCreateImage(context->base.handle.cl_context, args->flags, args->image_format, &image_desc, (void *)args->host_ptr, &args->ret);

   if (!mem)
   {
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->image);
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
vcomp_dispatch_clCreateImageWithPropertiesMESA(struct vcl_dispatch_context *ctx,
                                               struct vcl_command_clCreateImageWithPropertiesMESA *args)
{
#ifdef CL_API_SUFFIX__VERSION_3_0
   struct vcomp_context *vctx = ctx->data;
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);

   cl_image_desc image_desc;
   image_desc.image_type = args->image_desc->image_type;
   image_desc.image_width = args->image_desc->image_width;
   image_desc.image_height = args->image_desc->image_height;
   image_desc.image_depth = args->image_desc->image_depth;
   image_desc.image_array_size = args->image_desc->image_array_size;
   image_desc.image_row_pitch = args->image_desc->image_row_pitch;
   image_desc.image_slice_pitch = args->image_desc->image_slice_pitch;
   image_desc.num_mip_levels = args->image_desc->num_mip_levels;
   image_desc.num_samples = args->image_desc->num_samples;
   image_desc.buffer = args->image_desc->mem_object;

   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }

   cl_mem mem = clCreateImageWithProperties(context->base.handle.cl_context, args->properties, args->flags, args->image_format, &image_desc, (void *)args->host_ptr, &args->ret);

   if (!mem)
   {
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->image);
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
#else
   (void)ctx;
   (void)args;
#endif /* CL_API_SUFFIX__VERSION_3_0 */
}

static void
vcomp_dispatch_clEnqueueReadImageMESA(struct vcl_dispatch_context *ctx,
                                      struct vcl_command_clEnqueueReadImageMESA *args)
{
   struct vcomp_context *vctx = ctx->data;

   vcl_replace_clEnqueueReadImageMESA_args_handle(args);

   cl_event host_event;

   args->ret = clEnqueueReadImage(args->command_queue,
                                  args->image,
                                  args->blocking_read,
                                  args->origin, args->region,
                                  args->row_pitch,
                                  args->slice_pitch,
                                  args->ptr,
                                  args->num_events_in_wait_list,
                                  args->event_wait_list,
                                  args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clEnqueueWriteImageMESA(struct vcl_dispatch_context *ctx,
                                       struct vcl_command_clEnqueueWriteImageMESA *args)
{
   struct vcomp_context *vctx = ctx->data;

   vcl_replace_clEnqueueWriteImageMESA_args_handle(args);

   cl_event host_event;

   args->ret = clEnqueueWriteImage(args->command_queue,
                                   args->image,
                                   args->blocking_write,
                                   args->origin, args->region,
                                   args->input_row_pitch,
                                   args->input_slice_pitch,
                                   args->ptr,
                                   args->num_events_in_wait_list,
                                   args->event_wait_list,
                                   args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clEnqueueCopyImage(struct vcl_dispatch_context *ctx,
                                  struct vcl_command_clEnqueueCopyImage *args)
{
   struct vcomp_context *vctx = ctx->data;

   vcl_replace_clEnqueueCopyImage_args_handle(args);

   cl_event host_event;

   args->ret = clEnqueueCopyImage(args->command_queue,
                                  args->src_image,
                                  args->dst_image,
                                  args->src_origin,
                                  args->dst_origin,
                                  args->region,
                                  args->num_events_in_wait_list,
                                  args->event_wait_list,
                                  args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clEnqueueCopyImageToBuffer(struct vcl_dispatch_context *ctx,
                                          struct vcl_command_clEnqueueCopyImageToBuffer *args)
{
   struct vcomp_context *vctx = ctx->data;

   vcl_replace_clEnqueueCopyImageToBuffer_args_handle(args);

   cl_event host_event;

   args->ret = clEnqueueCopyImageToBuffer(args->command_queue,
                                          args->src_image,
                                          args->dst_buffer,
                                          args->src_origin,
                                          args->region,
                                          args->dst_offset,
                                          args->num_events_in_wait_list,
                                          args->event_wait_list,
                                          args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clEnqueueCopyBufferToImage(struct vcl_dispatch_context *ctx,
                                          struct vcl_command_clEnqueueCopyBufferToImage *args)
{
   struct vcomp_context *vctx = ctx->data;

   vcl_replace_clEnqueueCopyBufferToImage_args_handle(args);

   cl_event host_event;

   args->ret = clEnqueueCopyBufferToImage(args->command_queue,
                                          args->src_buffer,
                                          args->dst_image,
                                          args->src_offset,
                                          args->dst_origin,
                                          args->region,
                                          args->num_events_in_wait_list,
                                          args->event_wait_list,
                                          args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clEnqueueFillImageMESA(struct vcl_dispatch_context *ctx,
                                      struct vcl_command_clEnqueueFillImageMESA *args)
{
   struct vcomp_context *vctx = ctx->data;

   vcl_replace_clEnqueueFillImageMESA_args_handle(args);

   cl_event host_event;

   args->ret = clEnqueueFillImage(args->command_queue,
                                  args->image,
                                  args->fill_color,
                                  args->origin,
                                  args->region,
                                  args->num_events_in_wait_list,
                                  args->event_wait_list,
                                  args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static inline void *
vcomp_memory_get_mapping(const struct vcomp_memory *mem, const void *ptr)
{
   return _mesa_hash_table_u64_search(mem->map_table, (uint64_t)ptr);
}

static inline void
vcomp_memory_add_mapping(struct vcomp_memory *mem, const void *ptr, void *mapped_ptr)
{
   assert(ptr && mapped_ptr);
   assert(!_mesa_hash_table_u64_search(mem->map_table, (uint64_t)ptr));
   _mesa_hash_table_u64_insert(mem->map_table, (uint64_t)ptr, mapped_ptr);
}

static void *
vcomp_memory_remove_mapping(struct vcomp_memory *mem, void *ptr)
{
   void *mapped_ptr = _mesa_hash_table_u64_search(mem->map_table, (uint64_t)ptr);
   assert(mapped_ptr);
   _mesa_hash_table_u64_remove(mem->map_table, (uint64_t)ptr);
   return mapped_ptr;
}

static cl_int
vcomp_memory_force_unmap(struct vcomp_memory *mem, cl_command_queue queue, void *ptr)
{
   void *mapped_ptr = vcomp_memory_remove_mapping(mem, ptr);
   return clEnqueueUnmapMemObject(queue,
                                  mem->base.handle.memory,
                                  mapped_ptr,
                                  0,
                                  NULL,
                                  NULL);
}

static void
vcomp_dispatch_clEnqueueMapBufferMESA(struct vcl_dispatch_context *ctx,
                                      struct vcl_command_clEnqueueMapBufferMESA *args)
{
   struct vcomp_context *vctx = ctx->data;

   struct vcomp_memory *buffer = vcomp_memory_from_handle(args->buffer);
   if (!buffer)
   {
      args->ret = CL_INVALID_MEM_OBJECT;
      return;
   }

   vcl_replace_clEnqueueMapBufferMESA_args_handle(args);

   void *mapped_ptr = vcomp_memory_get_mapping(buffer, args->ptr);
   if (mapped_ptr)
   {
      /* Guest pointer already mapped, remove current mapping before mapping again */
      args->ret = vcomp_memory_force_unmap(buffer, args->command_queue, mapped_ptr);
      if (args->ret != CL_SUCCESS)
      {
         return;
      }
      mapped_ptr = NULL;
   }

   cl_event host_event;

   mapped_ptr = clEnqueueMapBuffer(args->command_queue,
                                   args->buffer,
                                   args->blocking_map,
                                   args->map_flags,
                                   args->offset,
                                   args->size,
                                   args->num_events_in_wait_list,
                                   args->event_wait_list,
                                   args->event ? &host_event : NULL,
                                   &args->ret);

   if (args->ret != CL_SUCCESS)
      return;

   vcomp_memory_add_mapping(buffer, args->ptr, mapped_ptr);

   if (args->event)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);

   if (mapped_ptr && args->size > 0)
      memcpy(args->ptr, mapped_ptr, args->size);
}

static void
vcomp_dispatch_clEnqueueUnmapMemObjectMESA(struct vcl_dispatch_context *ctx,
                                           struct vcl_command_clEnqueueUnmapMemObjectMESA *args)
{

   struct vcomp_context *vctx = ctx->data;

   struct vcomp_memory *memobj = vcomp_memory_from_handle(args->memobj);
   if (!memobj)
   {
      args->ret = CL_INVALID_MEM_OBJECT;
      return;
   }

   void *mapped_ptr = vcomp_memory_get_mapping(memobj, args->mapped_ptr);
   if (mapped_ptr == NULL)
   {
      /* not a valid pointer returned by clEnqueueMap* */
      args->ret = CL_INVALID_VALUE;
      return;
   }

   /* Copy memory from host before unmapping */
   memcpy(mapped_ptr, args->mapped_ptr, args->size);

   vcl_replace_clEnqueueUnmapMemObjectMESA_args_handle(args);

   cl_event host_event;

   args->ret = clEnqueueUnmapMemObject(args->command_queue,
                                       args->memobj,
                                       mapped_ptr,
                                       args->num_events_in_wait_list,
                                       args->event_wait_list,
                                       args->event ? &host_event : NULL);

   if (args->ret != CL_SUCCESS)
      return;

   vcomp_memory_remove_mapping(memobj, (void *)args->mapped_ptr);

   if (args->event)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
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
   vctx->dispatch.dispatch_clGetSupportedImageFormats = vcomp_dispatch_clGetSupportedImageFormats;
   vctx->dispatch.dispatch_clGetImageInfo = vcomp_dispatch_clGetImageInfo;
   vctx->dispatch.dispatch_clCreateSubBufferMESA = vcomp_dispatch_clCreateSubBufferMESA;
   vctx->dispatch.dispatch_clCreateImageMESA = vcomp_dispatch_clCreateImageMESA;
   vctx->dispatch.dispatch_clCreateImageWithPropertiesMESA = vcomp_dispatch_clCreateImageWithPropertiesMESA;
   vctx->dispatch.dispatch_clEnqueueReadImageMESA = vcomp_dispatch_clEnqueueReadImageMESA;
   vctx->dispatch.dispatch_clEnqueueWriteImageMESA = vcomp_dispatch_clEnqueueWriteImageMESA;
   vctx->dispatch.dispatch_clEnqueueCopyImage = vcomp_dispatch_clEnqueueCopyImage;
   vctx->dispatch.dispatch_clEnqueueCopyImageToBuffer = vcomp_dispatch_clEnqueueCopyImageToBuffer;
   vctx->dispatch.dispatch_clEnqueueCopyBufferToImage = vcomp_dispatch_clEnqueueCopyBufferToImage;
   vctx->dispatch.dispatch_clEnqueueFillImageMESA = vcomp_dispatch_clEnqueueFillImageMESA;
   vctx->dispatch.dispatch_clEnqueueMapBufferMESA = vcomp_dispatch_clEnqueueMapBufferMESA;
   vctx->dispatch.dispatch_clEnqueueUnmapMemObjectMESA = vcomp_dispatch_clEnqueueUnmapMemObjectMESA;
}

cl_int vcomp_memory_destroy(struct vcomp_context *vctx, struct vcomp_memory *memory)
{
   _mesa_hash_table_u64_destroy(memory->map_table, NULL);
   cl_int ret = clReleaseMemObject(memory->base.handle.memory);
   vcomp_context_remove_object(vctx, &memory->base);
   return ret;
}
