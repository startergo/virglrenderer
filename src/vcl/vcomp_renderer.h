/*
 * Copyright 2023-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_RENDERER_H
#define VCOMP_RENDERER_H

#include "virgl_util.h"

#ifdef ENABLE_VCL

int vcomp_renderer_init(void);

void vcomp_renderer_fini(void);

void vcomp_renderer_reset(void);

size_t
vcomp_get_capset(void *capset);

#else /* ENABLE_VCL */

static inline int
vcomp_renderer_init(void)
{
   virgl_error("OpenCL support was not enabled in virglrenderer\n");
   return -1;
}

static inline void
vcomp_renderer_fini(void)
{
}

static inline void
vcomp_renderer_reset(void)
{
}

static size_t
vcomp_get_capset(UNUSED void *capset)
{
   return 0;
}

#endif /* ENABLE_VCL */

#endif /* VCOMP_RENDERER_H */
