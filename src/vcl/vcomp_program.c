/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_program.h"
#include "vcomp_cl_context.h"
#include "vcomp_context.h"
#include "vcomp_device.h"
#include "vcomp_platform.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"
#include "vcl-protocol/vcl_protocol_renderer_program.h"

static void
vcomp_dispatch_clCreateProgramWithSourceMESA(
    struct vcl_dispatch_context *dispatch,
    struct vcl_command_clCreateProgramWithSourceMESA *args)
{
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);

   cl_program prog = clCreateProgramWithSource(
       context->base.handle.cl_context,
       args->count,
       (const char **)args->strings,
       args->lengths,
       &args->ret);

   if (!prog)
   {
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->program);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      args->ret = CL_INVALID_VALUE;
      return;
   }

   struct vcomp_program *program = vcomp_object_alloc(sizeof(*program), id);

   if (!program)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   program->base.handle.program = prog;
   vcomp_context_add_object(vctx, &program->base);
}

static void
vcomp_dispatch_clCreateProgramWithBinaryMESA(
    struct vcl_dispatch_context *dispatch,
    struct vcl_command_clCreateProgramWithBinaryMESA *args)
{
   cl_context guest_handle = args->context;
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(guest_handle);

   unsigned char **binaries = malloc(args->num_devices * sizeof(unsigned char *));
   uint32_t pos = 0;
   for (uint32_t i = 0; i < args->num_devices; i++)
   {
      binaries[i] = malloc(args->lengths[i]);
      memcpy(binaries[i], args->binaries + pos, args->lengths[i]);
      pos += args->lengths[i];
   }

   cl_device_id *handles = calloc(args->num_devices, sizeof(*handles));
   for (uint32_t i = 0; i < args->num_devices; ++i)
   {
      struct vcomp_device *device = vcomp_device_from_handle(args->device_list[i]);
      if (!device || !vcomp_context_contains_platform(vctx, device->platform))
      {
         args->ret = CL_INVALID_DEVICE;
         free(handles);
         return;
      }
      handles[i] = device->base.handle.device;
   }

   cl_program prog = clCreateProgramWithBinary(
       context->base.handle.cl_context,
       args->num_devices,
       handles,
       args->lengths,
       (const unsigned char **)binaries,
       args->binary_status,
       &args->ret);
   free(handles);

   if (!prog)
   {
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->program);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      args->ret = CL_INVALID_VALUE;
      return;
   }

   struct vcomp_program *program = vcomp_object_alloc(sizeof(*program), id);

   if (!program)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   program->base.handle.program = prog;
   vcomp_context_add_object(vctx, &program->base);
}

static void
vcomp_dispatch_clCreateProgramWithILMESA(
    struct vcl_dispatch_context *dispatch,
    struct vcl_command_clCreateProgramWithILMESA *args)
{
#ifdef CL_API_SUFFIX__VERSION_2_1
   cl_context guest_handle = args->context;
   struct vcomp_context *vctx = dispatch->data;
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(guest_handle);

   cl_program prog = clCreateProgramWithIL(
       context->base.handle.cl_context,
       args->il,
       args->length,
       &args->ret);

   if (!prog)
   {
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->program);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      args->ret = CL_INVALID_VALUE;
      return;
   }

   struct vcomp_program *program = vcomp_object_alloc(sizeof(*program), id);

   if (!program)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   program->base.handle.program = prog;
   vcomp_context_add_object(vctx, &program->base);
#else  //
   (void)dispatch;
   (void)args;
#endif // CL_API_SUFFIX__VERSION_1_2
}

static void
vcomp_dispatch_clReleaseProgram(struct vcl_dispatch_context *dispatch,
                                struct vcl_command_clReleaseProgram *args)
{
   struct vcomp_program *program = vcomp_program_from_handle(args->program);
   struct vcomp_context *vctx = dispatch->data;
   if (!program)
   {
      args->ret = CL_INVALID_PROGRAM;
      return;
   }

   args->ret = clReleaseProgram(program->base.handle.program);
   vcomp_context_remove_object(vctx, &program->base);
}

/* The param_value parameter is expected to be a contiguous array of arrays */
static cl_int
vcomp_program_get_binaries(struct vcomp_program *program, uint8_t *param_value,
                           size_t *param_value_size_ret)
{
   /* Query the size of this array */
   size_t binary_count = 0;
   cl_int ret = clGetProgramInfo(program->base.handle.program, CL_PROGRAM_BINARY_SIZES, 0,
                                 NULL, &binary_count);
   if (ret != CL_SUCCESS)
   {
      return ret;
   }
   /* The returned value is in bytes, so adjust it */
   assert(binary_count % sizeof(size_t) == 0);
   binary_count = binary_count / sizeof(size_t);

   /* Query the size of each binary */
   size_t *binary_sizes = calloc(binary_count, sizeof(binary_sizes[0]));
   ret = clGetProgramInfo(program->base.handle.program, CL_PROGRAM_BINARY_SIZES,
                          binary_count * sizeof(binary_sizes[0]), binary_sizes, NULL);
   if (ret != CL_SUCCESS)
   {
      return ret;
   }

   /* Create a new array of pointers to binaries memory in args */
   uint8_t **binaries = calloc(binary_count, sizeof(binaries[0]));
   for (size_t i = 0, offset = 0; i < binary_count; i++)
   {
      binaries[i] = param_value + offset;
      offset += binary_sizes[i];
   }

   /* Copy the binaries directly the args->param_value */
   return clGetProgramInfo(program->base.handle.program, CL_PROGRAM_BINARIES,
                           binary_count * sizeof(binaries[0]), binaries,
                           param_value_size_ret);
}

static void
vcomp_dispatch_clGetProgramInfo(UNUSED struct vcl_dispatch_context *dispatch,
                                struct vcl_command_clGetProgramInfo *args)
{
   struct vcomp_program *program = vcomp_program_from_handle(args->program);
   if (!program)
   {
      args->ret = CL_INVALID_PROGRAM;
      return;
   }

   /* Special handling is required for this */
   if (args->param_name == CL_PROGRAM_BINARIES && args->param_value != NULL)
   {
      args->ret = vcomp_program_get_binaries(program, args->param_value,
                                             args->param_value_size_ret);
   }
   else
   {
      args->ret = clGetProgramInfo(
          program->base.handle.program,
          args->param_name,
          args->param_value_size,
          args->param_value,
          args->param_value_size_ret);
   }
}

static void
vcomp_dispatch_clGetProgramBuildInfo(struct vcl_dispatch_context *dispatch,
                                     struct vcl_command_clGetProgramBuildInfo *args)
{
   struct vcomp_program *program = vcomp_program_from_handle(args->program);
   struct vcomp_context *vctx = dispatch->data;

   if (!program)
   {
      args->ret = CL_INVALID_PROGRAM;
      return;
   }

   struct vcomp_device *device = vcomp_device_from_handle(args->device);
   if (!device || !vcomp_context_contains_platform(vctx, device->platform))
   {
      args->ret = CL_INVALID_DEVICE;
      return;
   }

   args->ret = clGetProgramBuildInfo(
       program->base.handle.program,
       device->base.handle.device,
       args->param_name,
       args->param_value_size,
       args->param_value,
       args->param_value_size_ret);
}

static void
vcomp_dispatch_clBuildProgram(struct vcl_dispatch_context *dispatch,
                              struct vcl_command_clBuildProgram *args)
{
   struct vcomp_program *program = vcomp_program_from_handle(args->program);
   struct vcomp_context *vctx = dispatch->data;

   cl_device_id *handles = calloc(args->num_devices, sizeof(*handles));
   for (uint32_t i = 0; i < args->num_devices; ++i)
   {
      struct vcomp_device *device = vcomp_device_from_handle(args->device_list[i]);
      if (!device || !vcomp_context_contains_platform(vctx, device->platform))
      {
         args->ret = CL_INVALID_DEVICE;
         free(handles);
         return;
      }
      handles[i] = device->base.handle.device;
   }

   args->ret = clBuildProgram(
       program->base.handle.program,
       args->num_devices,
       handles,
       args->options,
       NULL,
       NULL);

   free(handles);
}

static void
vcomp_dispatch_clCompileProgram(UNUSED struct vcl_dispatch_context *dispatch,
                                struct vcl_command_clCompileProgram *args)
{
   vcl_replace_clCompileProgram_args_handle(args);
   args->ret = clCompileProgram(
       args->program,
       args->num_devices,
       args->device_list,
       args->options,
       args->num_input_headers,
       args->input_headers,
       args->header_include_names,
       NULL,
       NULL);
}

static void
vcomp_dispatch_clLinkProgramMESA(struct vcl_dispatch_context *dispatch,
                                 struct vcl_command_clLinkProgramMESA *args)
{
   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   struct vcomp_context *vctx = dispatch->data;

   cl_device_id *handles = calloc(args->num_devices, sizeof(*handles));
   for (uint32_t i = 0; i < args->num_devices; ++i)
   {
      struct vcomp_device *device = vcomp_device_from_handle(args->device_list[i]);
      if (!device || !vcomp_context_contains_platform(vctx, device->platform))
      {
         args->ret = CL_INVALID_DEVICE;
         free(handles);
         return;
      }
      handles[i] = device->base.handle.device;
   }

   cl_program *prog_handles = calloc(args->num_input_programs, sizeof(*prog_handles));
   for (uint32_t i = 0; i < args->num_input_programs; ++i)
   {
      struct vcomp_program *program = vcomp_program_from_handle(args->input_programs[i]);
      if (!program)
      {
         args->ret = CL_INVALID_PROGRAM;
         free(handles);
         free(prog_handles);
         return;
      }
      prog_handles[i] = program->base.handle.program;
   }

   cl_program prog = clLinkProgram(
       context->base.handle.cl_context,
       args->num_devices,
       handles,
       args->options,
       args->num_input_programs,
       prog_handles,
       NULL,
       NULL,
       &args->ret);

   free(handles);
   free(prog_handles);

   if (!prog)
   {
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args->program);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      args->ret = CL_INVALID_VALUE;
      return;
   }

   struct vcomp_program *program = vcomp_object_alloc(sizeof(*program), id);

   if (!program)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   program->base.handle.program = prog;
   vcomp_context_add_object(vctx, &program->base);
}

void vcomp_context_init_program_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clCreateProgramWithSourceMESA =
       vcomp_dispatch_clCreateProgramWithSourceMESA;
   dispatch->dispatch_clCreateProgramWithBinaryMESA =
       vcomp_dispatch_clCreateProgramWithBinaryMESA;
   dispatch->dispatch_clCreateProgramWithILMESA =
       vcomp_dispatch_clCreateProgramWithILMESA;
   dispatch->dispatch_clReleaseProgram = vcomp_dispatch_clReleaseProgram;
   dispatch->dispatch_clGetProgramInfo = vcomp_dispatch_clGetProgramInfo;
   dispatch->dispatch_clGetProgramBuildInfo = vcomp_dispatch_clGetProgramBuildInfo;
   dispatch->dispatch_clBuildProgram = vcomp_dispatch_clBuildProgram;
   dispatch->dispatch_clCompileProgram = vcomp_dispatch_clCompileProgram;
   dispatch->dispatch_clLinkProgramMESA = vcomp_dispatch_clLinkProgramMESA;
}
