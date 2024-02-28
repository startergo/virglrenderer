/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_EVENT_H
#define VCOMP_EVENT_H

struct vcomp_context;
struct vcomp_event;

void vcomp_context_init_event_dispatch(struct vcomp_context *vctx);
void vcomp_context_add_event(struct vcomp_context *vctx, cl_event event,
                             cl_event *args_event, cl_int *args_ret);

#endif /* VCOMP_EVENT_H */
