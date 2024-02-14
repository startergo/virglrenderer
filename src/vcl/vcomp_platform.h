/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_PLATFORM_H
#define VCOMP_PLATFORM_H

#include "vcomp_common.h"

struct vcomp_context;
struct vcomp_device;

struct vcomp_platform
{
   struct vcomp_object base;

   uint32_t device_count;
   cl_device_id *device_handles;
   struct vcomp_device **devices;
};

VCOMP_DEFINE_OBJECT_CAST(platform, cl_platform_id)

void vcomp_context_init_platform_dispatch(struct vcomp_context *vctx);

void vcomp_platform_destroy(struct vcomp_context *vctx, struct vcomp_platform *platform);

inline static bool
vcomp_platform_contains_device(struct vcomp_platform *platform, struct vcomp_device *device)
{
   for (uint32_t i = 0; i < platform->device_count; i++)
   {
      if (platform->devices[i] == device)
      {
         return true;
      }
   }
   return false;
}

#endif /* VCOMP_PLATFORM_H */
