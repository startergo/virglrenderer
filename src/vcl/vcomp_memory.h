/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_MEMORY_H
#define VCOMP_MEMORY_H

#include "vcomp_common.h"

struct vcomp_context;

struct vcomp_memory
{
   struct vcomp_object base;
};
VCOMP_DEFINE_OBJECT_CAST(memory, cl_mem)

void vcomp_context_init_memory_dispatch(struct vcomp_context *vctx);

cl_int vcomp_memory_destroy(struct vcomp_context *vctx, struct vcomp_memory *buffer);

#endif // VCOMP_MEMORY_H
