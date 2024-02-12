/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_PLATFORM_H
#define VCOMP_PLATFORM_H

#include "vcomp_common.h"

struct vcomp_context;

struct vcomp_platform
{
   struct vcomp_object base;
};

VCOMP_DEFINE_OBJECT_CAST(platform, cl_platform_id)

void vcomp_context_init_platform_dispatch(struct vcomp_context *vctx);

void vcomp_platform_destroy(struct vcomp_context *vctx, struct vcomp_platform *platform);

#endif /* VCOMP_PLATFORM_H */
