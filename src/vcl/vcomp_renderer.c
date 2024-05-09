/*
 * Copyright 2023-2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_common.h"
#include "vcomp_renderer.h"

#include "vcl_hw.h"

#include <string.h>

int vcomp_renderer_init(void)
{
   vcomp_log("initialized");
   return 0;
}

void vcomp_renderer_fini(void)
{
}

void vcomp_renderer_reset(void)
{
}

size_t
vcomp_get_capset(void *capset)
{
   vcomp_log("getting capset");
   struct virgl_renderer_capset_vcl *c = capset;
   const char *platform_name = "virglrenderer vcomp";

   if (c)
   {
      strncpy(c->platform_name, platform_name, sizeof(c->platform_name));
   }

   return sizeof(*c);
}
