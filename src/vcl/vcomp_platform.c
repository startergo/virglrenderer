/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_platform.h"
#include "vcomp_context.h"
#include "vcomp_common.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

static cl_int
vcomp_context_get_platforms(struct vcomp_context *vctx)
{
   if (vctx->platform_count)
      return CL_SUCCESS;

   uint32_t count;
   cl_uint result = clGetPlatformIDs(0, NULL, &count);
   if (result != CL_SUCCESS)
      return result;

   cl_platform_id *handles = calloc(count, sizeof(*handles));
   struct vcomp_platform **platforms = calloc(count, sizeof(*platforms));
   if (!handles || !platforms)
   {
      free(platforms);
      free(handles);
      return CL_OUT_OF_HOST_MEMORY;
   }

   result = clGetPlatformIDs(count, handles, &count);
   if (result != CL_SUCCESS)
   {
      free(platforms);
      free(handles);
      return result;
   }

   vctx->platform_count = count;
   vctx->platform_handles = handles;
   vctx->platforms = platforms;

   return CL_SUCCESS;
}

static void
vcomp_dispatch_clGetPlatformIDs(struct vcl_dispatch_context *dispatch,
                                struct vcl_command_clGetPlatformIDs *args)
{
   struct vcomp_context *vctx = dispatch->data;

   args->ret = vcomp_context_get_platforms(vctx);
   if (args->ret != CL_SUCCESS)
      return;

   uint32_t count = vctx->platform_count;
   if (!args->platforms)
   {
      if (args->num_platforms)
      {
         *args->num_platforms = count;
         args->ret = CL_SUCCESS;
         return;
      }
   }

   if (count > args->num_entries)
   {
      count = args->num_entries;
   }
   else
   {
      if (args->num_platforms)
      {
         *args->num_platforms = count;
      }
   }

   uint32_t i;
   for (i = 0; i < count; i++)
   {
      struct vcomp_platform *platform = vctx->platforms[i];
      const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)&args->platforms[i]);

      if (platform)
      {
         if (platform->base.id != id)
         {
            vcomp_context_set_fatal(vctx);
            break;
         }
         continue;
      }

      if (!vcomp_context_validate_object_id(vctx, id))
         break;

      platform = vcomp_object_alloc(sizeof(*platform), id);
      if (!platform)
      {
         args->ret = CL_OUT_OF_HOST_MEMORY;
         break;
      }

      platform->base.handle.platform = vctx->platform_handles[i];

      vctx->platforms[i] = platform;

      vcomp_context_add_object(vctx, &platform->base);
   }
   /* remove platform on errors */
   if (i < count)
   {
      for (i = 0; i < vctx->platform_count; i++)
      {
         struct vcomp_platform *platform = vctx->platforms[0];
         if (!platform)
            break;
         vcomp_context_remove_object(vctx, &platform->base);
         vctx->platforms[i] = NULL;
      }
   }
}

void vcomp_context_init_platform_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clGetPlatformIDs =
       vcomp_dispatch_clGetPlatformIDs;
}

void vcomp_platform_destroy(struct vcomp_context *vctx, struct vcomp_platform *platform)
{
   vcomp_context_remove_object(vctx, &platform->base);
}
