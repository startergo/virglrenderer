/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCL_CL_H
#define VCL_CL_H

#define CL_USE_DEPRECATED_OPENCL_1_2_APIS

#ifdef __APPLE__
#include <opencl.h>

#define CL_API_SUFFIX__VERSION_1_2_DEPRECATED

typedef cl_ulong cl_properties;
typedef cl_properties cl_queue_properties;
typedef cl_properties cl_mem_properties;

#else
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#endif

#endif /* VCL_CL_H */
