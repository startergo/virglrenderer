/*
 * Copyright 2021 Google LLC
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_common.h"

#include "virgl_util.h"

#include <string.h>
#include <stdlib.h>

void vcomp_log(const char *fmt, ...)
{
   va_list va;
   char *tmp_fmt = NULL;

   if (asprintf(&tmp_fmt, "%s\n", fmt) < 0)
      return;

   va_start(va, fmt);
   virgl_prefixed_logv("vcomp", VIRGL_LOG_LEVEL_INFO, tmp_fmt, va);
   va_end(va);
   free(tmp_fmt);
}
