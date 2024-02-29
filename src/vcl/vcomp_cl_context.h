/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_CL_CONTEXT_H
#define VCOMP_CL_CONTEXT_H

#include "vcomp_common.h"

struct vcomp_context;

struct vcomp_cl_context
{
   struct vcomp_object base;

   struct vcomp_device **devices;
};
VCOMP_DEFINE_OBJECT_CAST(cl_context, cl_context)

void vcomp_context_init_context_dispatch(struct vcomp_context *vctx);

void vcomp_cl_context_destroy(struct vcomp_context *vctx, struct vcomp_cl_context *context);

#endif /* VCOMP_CL_CONTEXT_H */
