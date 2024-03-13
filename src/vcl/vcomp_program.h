/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_PROGRAM_H
#define VCOMP_PROGRAM_H

#include "vcomp_common.h"

struct vcomp_context;

struct vcomp_program
{
    struct vcomp_object base;
};
VCOMP_DEFINE_OBJECT_CAST(program, cl_program)

void vcomp_context_init_program_dispatch(struct vcomp_context *vctx);

#endif /* VCOMP_PROGRAM_H */
