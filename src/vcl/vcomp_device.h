/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_DEVICE_H
#define VCOMP_DEVICE_H

struct vcomp_context;
struct vcomp_device;

void vcomp_context_init_device_dispatch(struct vcomp_context *vctx);

void vcomp_device_destroy(struct vcomp_context *vctx, struct vcomp_device *device);

#endif /* VCOMP_DEVICE_H */
