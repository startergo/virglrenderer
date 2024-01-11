/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_CONTEXT_H
#define VCOMP_CONTEXT_H

#include "vcomp_cs.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

#include "virgl_context.h"

struct vcomp_context
{
   struct virgl_context base;
   char debug_name[32];

   struct hash_table *resource_table;

   struct vcomp_cs_decoder decoder;
   struct vcomp_cs_encoder encoder;
   struct vcl_dispatch_context dispatch;
};

#endif /* VCOMP_CONTEXT_H */
