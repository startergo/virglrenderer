/*
 * Copyright 2023-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_RENDERER_H
#define VCOMP_RENDERER_H

#include "virgl_util.h"

#ifdef ENABLE_VCL

size_t
vcomp_get_capset(void *capset);

#else /* ENABLE_VCL */

static size_t
vcomp_get_capset(UNUSED void *capset)
{
   return 0;
}

#endif /* ENABLE_VCL */

#endif /* VCOMP_RENDERER_H */
