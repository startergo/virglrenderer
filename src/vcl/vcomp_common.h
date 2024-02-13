/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_COMMON_H
#define VCOMP_COMMON_H

#include <assert.h>

#include <vcl-protocol/vcl_cl.h>

typedef uint64_t vcomp_object_id;

struct vcomp_object
{
   vcomp_object_id id;

   union
   {
      uint64_t u64;
      cl_platform_id platform_id;
   } handle;
};

void vcomp_log(const char *fmt, ...);

static inline void *
vcomp_object_alloc(size_t size, vcomp_object_id id)
{
   assert(size >= sizeof(struct vcomp_object));

   struct vcomp_object *obj = calloc(1, size);
   if (!obj)
      return NULL;

   /* obj is only half-initialized */
   obj->id = id;

   return obj;
}

#endif /* VCOMP_COMMON_H */
