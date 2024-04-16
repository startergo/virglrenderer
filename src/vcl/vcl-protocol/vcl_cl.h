/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCL_CL_H
#define VCL_CL_H

#define CL_USE_DEPRECATED_OPENCL_1_1_APIS
#define CL_USE_DEPRECATED_OPENCL_1_2_APIS
#define CL_API_SUFFIX__VERSION_1_1_DEPRECATED
#define CL_API_SUFFIX__VERSION_1_2_DEPRECATED

#ifdef __APPLE__
#include <opencl.h>
#else
#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#endif

#ifndef CL_VERSION_3_0
typedef cl_ulong cl_properties;
typedef cl_properties cl_queue_properties;
typedef cl_properties cl_mem_properties;
#endif

typedef struct cl_image_desc_MESA
{
   cl_mem_object_type image_type;
   size_t image_width;
   size_t image_height;
   size_t image_depth;
   size_t image_array_size;
   size_t image_row_pitch;
   size_t image_slice_pitch;
   cl_uint num_mip_levels;
   cl_uint num_samples;
   cl_mem mem_object;
} cl_image_desc_MESA;

#endif /* VCL_CL_H */
