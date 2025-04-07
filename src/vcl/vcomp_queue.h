/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_QUEUE_H
#define VCOMP_QUEUE_H

#include "vcomp_common.h"

struct vcomp_context;

struct vcomp_queue
{
   struct vcomp_object base;
};
VCOMP_DEFINE_OBJECT_CAST(queue, cl_command_queue)


void vcomp_context_init_queue_dispatch(struct vcomp_context *vctx);

cl_int vcomp_queue_destroy(struct vcomp_context *vctx, struct vcomp_queue *queue);

#endif /* VCOMP_QUEUE_H */
