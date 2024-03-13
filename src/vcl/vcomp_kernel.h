/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_KERNEL_H
#define VCOMP_KERNEL_H

#include "vcomp_common.h"

struct vcomp_context;

struct vcomp_kernel
{
   struct vcomp_object base;
};
VCOMP_DEFINE_OBJECT_CAST(kernel, cl_kernel)


void vcomp_context_init_kernel_dispatch(struct vcomp_context *vctx);

cl_int vcomp_kernel_destroy(struct vcomp_context *vctx, struct vcomp_kernel *kernel);

#endif /* VCOMP_KERNEL_H */
