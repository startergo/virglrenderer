/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCL_CL_H
#define VCL_CL_H

#ifdef __APPLE__
#include <opencl.h>

typedef cl_ulong cl_properties;
typedef cl_properties cl_queue_properties;
typedef cl_properties cl_mem_properties;

#else
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#endif

#endif /* VCL_CL_H */
