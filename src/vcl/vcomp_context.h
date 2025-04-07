/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_CONTEXT_H
#define VCOMP_CONTEXT_H

#include "vcomp_cs.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

#include "util/hash_table.h"
#include "virgl_context.h"

struct vcomp_context
{
   struct virgl_context base;
   char debug_name[32];

   struct hash_table *object_table;
   struct hash_table *resource_table;

   bool cs_fatal_error;
   struct vcomp_cs_decoder decoder;
   struct vcomp_cs_encoder encoder;
   struct vcl_dispatch_context dispatch;

   uint32_t platform_count;
   cl_platform_id *platform_handles;
   struct vcomp_platform **platforms;
};

static inline void
vcomp_context_set_fatal(struct vcomp_context *ctx)
{
   ctx->cs_fatal_error = true;
}

static inline bool
vcomp_context_validate_object_id(struct vcomp_context *ctx, vcomp_object_id id)
{
   if (unlikely(!id || _mesa_hash_table_search(ctx->object_table, &id)))
   {
      vcomp_log("invalid object id %" PRIu64, id);
      vcomp_context_set_fatal(ctx);
      return false;
   }

   return true;
}

static inline void
vcomp_context_add_object(struct vcomp_context *vctx, struct vcomp_object *obj)
{
   assert(obj->id);
   assert(!_mesa_hash_table_search(vctx->object_table, &obj->id));
   _mesa_hash_table_insert(vctx->object_table, &obj->id, obj);
}

static inline void
vcomp_context_free_object(struct hash_entry *entry)
{
   struct vcomp_object *obj = entry->data;
   free(obj);
}

static inline void
vcomp_context_remove_object(struct vcomp_context *vctx, struct vcomp_object *obj)
{
   assert(_mesa_hash_table_search(vctx->object_table, &obj->id));

   struct hash_entry *entry = _mesa_hash_table_search(vctx->object_table, &obj->id);
   if (likely(entry))
   {
      vcomp_context_free_object(entry);
      _mesa_hash_table_remove(vctx->object_table, entry);
   }
}

static inline void *
vcomp_context_get_object(struct vcomp_context *vctx, vcomp_object_id obj_id)
{
   const struct hash_entry *entry = _mesa_hash_table_search(vctx->object_table, &obj_id);
   void *obj = likely(entry) ? entry->data : NULL;
   return obj;
}

inline static bool
vcomp_context_contains_platform(struct vcomp_context *vctx, struct vcomp_platform *platform)
{
   for (uint32_t i = 0; i < vctx->platform_count; i++)
   {
      if (vctx->platforms[i] == platform)
      {
         return true;
      }
   }
   return false;
}

#endif /* VCOMP_CONTEXT_H */
