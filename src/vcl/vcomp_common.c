/*
 * Copyright 2021 Google LLC
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_common.h"

#include "virgl_util.h"

#include <string.h>

void
vcomp_log(const char *fmt, ...)
{
   va_list va;

   va_start(va, fmt);
   virgl_prefixed_logv("vcomp", VIRGL_LOG_LEVEL_INFO, fmt, va);
   va_end(va);
}
