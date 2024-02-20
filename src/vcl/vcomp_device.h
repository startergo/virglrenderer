/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_DEVICE_H
#define VCOMP_DEVICE_H

#include "vcomp_common.h"

struct vcomp_context;

struct vcomp_device
{
   struct vcomp_object base;

   struct vcomp_platform *platform;
};
VCOMP_DEFINE_OBJECT_CAST(device, cl_device_id)

void vcomp_context_init_device_dispatch(struct vcomp_context *vctx);

void vcomp_device_destroy(struct vcomp_context *vctx, struct vcomp_device *device);

#endif /* VCOMP_DEVICE_H */
