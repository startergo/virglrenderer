/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_kernel.h"
#include "vcomp_queue.h"
#include "vcomp_event.h"
#include "vcomp_context.h"
#include "vcomp_cl_context.h"
#include "vcomp_device.h"
#include "vcomp_platform.h"
#include "vcomp_program.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"
#include "vcl-protocol/vcl_protocol_renderer_kernel.h"

static void
vcomp_dispatch_clCreateKernelMESA(struct vcl_dispatch_context *dispatch,
                                  struct vcl_command_clCreateKernelMESA *args)
{
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_program *program = vcomp_program_from_handle(args->program);
   if (!program)
   {
      args->ret = CL_INVALID_PROGRAM;
      return;
   }

   cl_kernel kernel_handle = clCreateKernel(program->base.handle.program,
                                            args->kernel_name, &args->ret);
   if (!kernel_handle)
   {
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->kernel);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      args->ret = CL_INVALID_VALUE;
      clReleaseKernel(kernel_handle);
      return;
   }

   struct vcomp_kernel *kernel = vcomp_object_alloc(sizeof(*kernel), id);
   if (!kernel)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      clReleaseKernel(kernel_handle);
      return;
   }

   kernel->base.handle.kernel = kernel_handle;
   vcomp_context_add_object(vctx, &kernel->base);
}

static void
vcomp_dispatch_clCreateKernelsInProgram(struct vcl_dispatch_context *dispatch,
                                        struct vcl_command_clCreateKernelsInProgram *args)
{
   struct vcomp_context *vctx = dispatch->data;
   vcl_replace_clCreateKernelsInProgram_args_handle(args);
   if (!args->program)
   {
      args->ret = CL_INVALID_PROGRAM;
      return;
   }

   cl_kernel *handles = NULL;
   struct vcomp_kernel **temp_kernels = NULL;

   // We allocate space for host handles to avoid overwriting incoming kernel IDs
   if (args->num_kernels > 0)
      handles = calloc(args->num_kernels, sizeof(*handles));

   args->ret = clCreateKernelsInProgram(args->program, args->num_kernels,
                                        handles, args->num_kernels_ret);
   // In case of error or no handles, there's nothing else to do
   if (args->ret != CL_SUCCESS || handles == NULL || args->num_kernels == 0)
      goto clean_up;

   size_t actual_num_kernels = args->num_kernels;
   if (args->num_kernels_ret)
      actual_num_kernels = MIN2(actual_num_kernels, *args->num_kernels_ret);
   if (actual_num_kernels == 0)
      goto clean_up;

   uint32_t i;
   temp_kernels = calloc(actual_num_kernels, sizeof(*temp_kernels));
   for (i = 0; i < actual_num_kernels; ++i)
   {
      const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)(args->kernels + i));
      if (!vcomp_context_validate_object_id(vctx, id))
      {
         args->ret = CL_INVALID_VALUE;
         break;
      }

      struct vcomp_kernel *kernel = vcomp_object_alloc(sizeof(*kernel), id);
      if (!kernel)
      {
         args->ret = CL_OUT_OF_HOST_MEMORY;
         break;
      }
      temp_kernels[i] = kernel;
      kernel->base.handle.kernel = handles[i];
      vcomp_context_add_object(vctx, &kernel->base);
   }

   /* remove all devices on errors */
   if (i < actual_num_kernels)
   {
      for (uint32_t j = 0; j < actual_num_kernels; j++)
      {
         struct vcomp_kernel *kernel = temp_kernels[j];
         if (!kernel)
            break;
         vcomp_context_remove_object(vctx, &kernel->base);
      }
      for (uint32_t k = 0; k < actual_num_kernels; k++)
      {
         if (handles && handles[k])
            clReleaseKernel(handles[k]);
      }
   }

clean_up:
   if (temp_kernels)
      free(temp_kernels);
   if (handles)
      free(handles);
}

static void
vcomp_dispatch_clReleaseKernel(struct vcl_dispatch_context *dispatch,
                               struct vcl_command_clReleaseKernel *args)
{
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_kernel *kernel = vcomp_kernel_from_handle(args->kernel);
   if (!kernel)
   {
      vcomp_context_set_fatal(vctx);
      args->ret = CL_INVALID_KERNEL;
      return;
   }
   args->ret = vcomp_kernel_destroy(vctx, kernel);
}

static void
vcomp_dispatch_clSetKernelArg(struct vcl_dispatch_context *dispatch,
                              struct vcl_command_clSetKernelArg *args)
{
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_object *obj = NULL;

   struct vcomp_kernel *kernel = vcomp_kernel_from_handle(args->kernel);
   if (!kernel)
   {
      args->ret = CL_INVALID_KERNEL;
      return;
   }

   /* In case this is an OpenCL handle, we need to replace it with the host handle */
   const void *arg_value = args->arg_value;
   if (arg_value && args->arg_size == sizeof(vcomp_handle))
   {
      const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->arg_value);
      obj = vcomp_context_get_object(vctx, id);
      if (obj)
      {
         /* clSetKernelArg expects a pointer to the handle */
         arg_value = &obj->handle.u64;
      }
   }

   args->ret = clSetKernelArg(kernel->base.handle.kernel, args->arg_index,
                              args->arg_size, arg_value);
}

static void
vcomp_dispatch_clGetKernelInfo(UNUSED struct vcl_dispatch_context *dispatch,
                               struct vcl_command_clGetKernelInfo *args)
{
   struct vcomp_kernel *kernel = vcomp_kernel_from_handle(args->kernel);
   if (!kernel)
   {
      args->ret = CL_INVALID_KERNEL;
      return;
   }
   args->ret = clGetKernelInfo(kernel->base.handle.kernel, args->param_name,
                               args->param_value_size, args->param_value,
                               args->param_value_size_ret);
}

static void
vcomp_dispatch_clGetKernelWorkGroupInfo(UNUSED struct vcl_dispatch_context *dispatch,
                                        struct vcl_command_clGetKernelWorkGroupInfo *args)
{
   struct vcomp_kernel *kernel = vcomp_kernel_from_handle(args->kernel);
   struct vcomp_device *device = vcomp_device_from_handle(args->device);
   if (!kernel)
   {
      args->ret = CL_INVALID_KERNEL;
      return;
   }
   if (!device)
   {
      args->ret = CL_INVALID_DEVICE;
      return;
   }

   args->ret = clGetKernelWorkGroupInfo(kernel->base.handle.kernel,
                                        device->base.handle.device, args->param_name,
                                        args->param_value_size, args->param_value,
                                        args->param_value_size_ret);
}

static void
vcomp_dispatch_clEnqueueNDRangeKernel(struct vcl_dispatch_context *dispatch,
                                      struct vcl_command_clEnqueueNDRangeKernel *args)
{
   struct vcomp_context *vctx = dispatch->data;

   vcl_replace_clEnqueueNDRangeKernel_args_handle(args);

   cl_event host_event = NULL;
   args->ret = clEnqueueNDRangeKernel(args->command_queue,
                                      args->kernel,
                                      args->work_dim,
                                      args->global_work_offset,
                                      args->global_work_size,
                                      args->local_work_size,
                                      args->num_events_in_wait_list,
                                      args->event_wait_list,
                                      args->event ? &host_event : NULL);

   if (args->event && args->ret == CL_SUCCESS)
      vcomp_context_add_event(vctx, host_event, args->event, &args->ret);
}

static void
vcomp_dispatch_clGetKernelArgInfo(UNUSED struct vcl_dispatch_context *dispatch,
                                  struct vcl_command_clGetKernelArgInfo *args)
{
   struct vcomp_kernel *kernel = vcomp_kernel_from_handle(args->kernel);
   if (!kernel)
   {
      args->ret = CL_INVALID_KERNEL;
      return;
   }

   args->ret = clGetKernelArgInfo(kernel->base.handle.kernel, args->arg_index,
                                  args->param_name, args->param_value_size,
                                  args->param_value, args->param_value_size_ret);
}

static void
vcomp_dispatch_clSetKernelArgSVMPointer(UNUSED struct vcl_dispatch_context *dispatch,
                                        struct vcl_command_clSetKernelArgSVMPointer *args)
{
#ifdef CL_API_SUFFIX__VERSION_2_0
   struct vcomp_kernel *kernel = vcomp_kernel_from_handle(args->kernel);
   if (!kernel)
   {
      args->ret = CL_INVALID_KERNEL;
      return;
   }

   args->ret = clSetKernelArgSVMPointer(kernel->base.handle.kernel, args->arg_index,
                                        args->arg_value);
#else
   (void)dispatch;
   (void)args;
#endif /* CL_API_SUFFIX__VERSION_2_0 */
}

static void
vcomp_dispatch_clSetKernelExecInfo(UNUSED struct vcl_dispatch_context *dispatch,
                                   struct vcl_command_clSetKernelExecInfo *args)
{
#ifdef CL_API_SUFFIX__VERSION_2_0
   struct vcomp_kernel *kernel = vcomp_kernel_from_handle(args->kernel);
   if (!kernel)
   {
      args->ret = CL_INVALID_KERNEL;
      return;
   }

   args->ret = clSetKernelExecInfo(kernel->base.handle.kernel, args->param_name,
                                   args->param_value_size, args->param_value);
#else
   (void)dispatch;
   (void)args;
#endif /* CL_API_SUFFIX__VERSION_2_0 */
}

static void
vcomp_dispatch_clCloneKernelMESA(struct vcl_dispatch_context *dispatch,
                                 struct vcl_command_clCloneKernelMESA *args)
{
#ifdef CL_API_SUFFIX__VERSION_2_1
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_kernel *source_kernel = vcomp_kernel_from_handle(args->source_kernel);
   if (!source_kernel)
   {
      args->ret = CL_INVALID_KERNEL;
      return;
   }

   cl_kernel cloned_kernel = clCloneKernel(source_kernel->base.handle.kernel, &args->ret);
   if (!cloned_kernel)
   {
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->kernel);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      args->ret = CL_INVALID_VALUE;
      clReleaseKernel(cloned_kernel);
      return;
   }

   struct vcomp_kernel *kernel = vcomp_object_alloc(sizeof(*kernel), id);
   if (!kernel)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      clReleaseKernel(cloned_kernel);
      return;
   }

   kernel->base.handle.kernel = cloned_kernel;
   vcomp_context_add_object(vctx, &kernel->base);
#else
   (void)dispatch;
   (void)args;
#endif /* CL_API_SUFFIX__VERSION_2_1 */
}

static void
vcomp_dispatch_clGetKernelSubGroupInfo(UNUSED struct vcl_dispatch_context *dispatch,
                                       struct vcl_command_clGetKernelSubGroupInfo *args)
{
#ifdef CL_API_SUFFIX__VERSION_2_1
   struct vcomp_kernel *kernel = vcomp_kernel_from_handle(args->kernel);
   struct vcomp_device *device = vcomp_device_from_handle(args->device);
   if (!kernel)
   {
      args->ret = CL_INVALID_KERNEL;
      return;
   }
   if (!device)
   {
      args->ret = CL_INVALID_DEVICE;
      return;
   }

   args->ret = clGetKernelWorkGroupInfo(kernel->base.handle.kernel,
                                        device->base.handle.device, args->param_name,
                                        args->param_value_size, args->param_value,
                                        args->param_value_size_ret);
#else
   (void)dispatch;
   (void)args;
#endif /* CL_API_SUFFIX__VERSION_2_1 */
}

void vcomp_context_init_kernel_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clCreateKernelMESA = vcomp_dispatch_clCreateKernelMESA;
   dispatch->dispatch_clCreateKernelsInProgram = vcomp_dispatch_clCreateKernelsInProgram;
   dispatch->dispatch_clReleaseKernel = vcomp_dispatch_clReleaseKernel;
   dispatch->dispatch_clSetKernelArg = vcomp_dispatch_clSetKernelArg;
   dispatch->dispatch_clGetKernelInfo = vcomp_dispatch_clGetKernelInfo;
   dispatch->dispatch_clGetKernelWorkGroupInfo = vcomp_dispatch_clGetKernelWorkGroupInfo;
   dispatch->dispatch_clEnqueueNDRangeKernel = vcomp_dispatch_clEnqueueNDRangeKernel;
   dispatch->dispatch_clGetKernelArgInfo = vcomp_dispatch_clGetKernelArgInfo;
   dispatch->dispatch_clSetKernelArgSVMPointer = vcomp_dispatch_clSetKernelArgSVMPointer;
   dispatch->dispatch_clSetKernelExecInfo = vcomp_dispatch_clSetKernelExecInfo;
   dispatch->dispatch_clCloneKernelMESA = vcomp_dispatch_clCloneKernelMESA;
   dispatch->dispatch_clGetKernelSubGroupInfo = vcomp_dispatch_clGetKernelSubGroupInfo;
}

cl_int vcomp_kernel_destroy(struct vcomp_context *vctx, struct vcomp_kernel *kernel)
{
   cl_int ret = clReleaseKernel(kernel->base.handle.kernel);
   vcomp_context_remove_object(vctx, &kernel->base);
   return ret;
}
