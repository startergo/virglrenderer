/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_MEMORY_H
#define VCOMP_MEMORY_H

#include "vcomp_common.h"
#include "util/hash_table.h"

struct vcomp_context;

struct vcomp_memory
{
   struct vcomp_object base;

   /*
    * clEnqueueMapBuffer and clEnqueueMapImage increment the mapped count of the memory
    * object. The initial mapped count value of the memory object is zero. Multiple calls
    * to clEnqueueMapBuffer, or clEnqueueMapImage on the same memory object will increment
    * this mapped count by appropriate number of calls. clEnqueueUnmapMemObject decrements
    * the mapped count of the memory object.
    */
   struct hash_table_u64 *map_table;
};
VCOMP_DEFINE_OBJECT_CAST(memory, cl_mem)

void vcomp_context_init_memory_dispatch(struct vcomp_context *vctx);

cl_int vcomp_memory_destroy(struct vcomp_context *vctx, struct vcomp_memory *buffer);

#endif // VCOMP_MEMORY_H
