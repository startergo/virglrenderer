/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_cl_context.h"
#include "vcomp_common.h"
#include "vcomp_context.h"
#include "vcomp_sampler.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

struct vcomp_sampler
{
   struct vcomp_object base;
};
VCOMP_DEFINE_OBJECT_CAST(sampler, cl_sampler)

static void
vcomp_context_add_sampler(struct vcomp_context *vctx, cl_sampler sampler,
                          cl_sampler *args_sampler, cl_int *ret)
{
   if (!sampler)
   {
      *ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args_sampler);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      *ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   struct vcomp_sampler *v_sampler = vcomp_object_alloc(sizeof(*v_sampler), id);
   if (!v_sampler)
   {
      *ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   v_sampler->base.handle.sampler = sampler;

   vcomp_context_add_object(vctx, &v_sampler->base);
}

static void
vcomp_dispatch_clCreateSamplerMESA(struct vcl_dispatch_context *dispatch,
                                   struct vcl_command_clCreateSamplerMESA *args)
{
#ifdef CL_API_SUFFIX__VERSION_1_2_DEPRECATED
   struct vcomp_context *vctx = dispatch->data;

   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }

   cl_sampler sampler = clCreateSampler(context->base.handle.cl_context,
                                        args->normalized_coords,
                                        args->addressing_mode,
                                        args->filter_mode, &args->ret);

   vcomp_context_add_sampler(vctx, sampler, args->sampler, &args->ret);
#else
   (void)dispatch;
   (void)args;
#endif /* CL_API_SUFFIX__VERSION_1_2_DEPRECATED */
}

static void
vcomp_dispatch_clCreateSamplerWithPropertiesMESA(struct vcl_dispatch_context *dispatch,
                                                 struct vcl_command_clCreateSamplerWithPropertiesMESA *args)
{
#ifdef CL_API_SUFFIX__VERSION_2_0
   struct vcomp_context *vctx = dispatch->data;

   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }

   cl_sampler sampler = clCreateSamplerWithProperties(context->base.handle.cl_context,
                                                      args->sampler_properties,
                                                      &args->ret);

   vcomp_context_add_sampler(vctx, sampler, args->sampler, &args->ret);
#else
   (void)dispatch;
   (void)args;
#endif /* CL_API_SUFFIX__VERSION_2_0 */
}

static void
vcomp_dispatch_clReleaseSampler(struct vcl_dispatch_context *dispatch,
                                struct vcl_command_clReleaseSampler *args)
{
   struct vcomp_context *vctx = dispatch->data;

   struct vcomp_sampler *sampler = vcomp_sampler_from_handle(args->sampler);
   if (!sampler)
   {
      args->ret = CL_INVALID_SAMPLER;
      return;
   }

   args->ret = clReleaseSampler(sampler->base.handle.sampler);

   vcomp_context_remove_object(vctx, &sampler->base);
}

static void
vcomp_dispatch_clGetSamplerInfo(UNUSED struct vcl_dispatch_context *dispatch,
                                struct vcl_command_clGetSamplerInfo *args)
{
   struct vcomp_sampler *sampler = vcomp_sampler_from_handle(args->sampler);
   if (!sampler)
   {
      args->ret = CL_INVALID_SAMPLER;
      return;
   }

   args->ret = clGetSamplerInfo(sampler->base.handle.sampler,
                                args->param_name, args->param_value_size,
                                args->param_value, args->param_value_size_ret);
}

void
vcomp_context_init_sampler_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clCreateSamplerMESA = vcomp_dispatch_clCreateSamplerMESA;
   dispatch->dispatch_clCreateSamplerWithPropertiesMESA = vcomp_dispatch_clCreateSamplerWithPropertiesMESA;
   dispatch->dispatch_clReleaseSampler = vcomp_dispatch_clReleaseSampler;
   dispatch->dispatch_clGetSamplerInfo = vcomp_dispatch_clGetSamplerInfo;
}
