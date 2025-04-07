/*
 * Copyright 2021 Google LLC
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#ifndef VCOMP_CS_H
#define VCOMP_CS_H

#include "vcomp_common.h"

#include "vcl-protocol/vcl_cl.h"

#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "util/hash_table.h"
#include "util/macros.h"
#include "util/u_math.h"

/*
 * This is to avoid integer overflows and to catch bogus allocations (e.g.,
 * the guest driver encodes an uninitialized value).
 */
#define VCOMP_CS_DECODER_TEMP_POOL_MAX_SIZE (1u * 1024 * 1024 * 1024)

typedef uint64_t vcomp_object_id;

struct vcomp_cs_encoder
{
   bool *fatal_error;

   uint8_t *cur;
   const uint8_t *end;
};

/*
 * We usually need many small allocations during decoding.  Those allocations
 * are suballocated from the temp pool.
 *
 * After a command is decoded, vcomp_cs_decoder_reset_temp_pool is called to
 * reset pool->cur.  After an entire command stream is decoded,
 * vcomp_cs_decoder_gc_temp_pool is called to garbage collect pool->buffers.
 */
struct vcomp_cs_decoder_temp_pool
{
   uint8_t **buffers;
   uint32_t buffer_count;
   uint32_t buffer_max;
   size_t total_size;

   uint8_t *reset_to;

   uint8_t *cur;
   const uint8_t *end;
};

struct vcomp_cs_decoder
{
   const struct hash_table *object_table;
   const struct hash_table *resource_table;

   bool fatal_error;

   struct vcomp_cs_decoder_temp_pool temp_pool;

   const uint8_t *cur;
   const uint8_t *end;
};

static inline void
vcomp_cs_decoder_init(struct vcomp_cs_decoder *dec,
                      struct hash_table *object_table,
                      struct hash_table *resource_table)
{
   memset(dec, 0, sizeof(*dec));
   dec->object_table = object_table;
   dec->resource_table = resource_table;
}

static inline void
vcomp_cs_encoder_init(struct vcomp_cs_encoder *enc, bool *fatal_error)
{
   memset(enc, 0, sizeof(*enc));
   enc->fatal_error = fatal_error;
}

static inline bool
vcomp_cs_decoder_get_fatal(const struct vcomp_cs_decoder *dec)
{
   return dec->fatal_error;
}

static inline void
vcomp_cs_encoder_set_fatal(const struct vcomp_cs_encoder *enc)
{
   *enc->fatal_error = true;
}

static inline void
vcomp_cs_decoder_set_stream(struct vcomp_cs_decoder *dec, const void *data, size_t size)
{
   dec->cur = data;
   dec->end = dec->cur + size;
}

static inline void
vcomp_cs_encoder_set_stream(struct vcomp_cs_encoder *enc, void *data, size_t size)
{
   enc->cur = data;
   enc->end = enc->cur + size;
}

static inline bool
vcomp_cs_decoder_has_command(const struct vcomp_cs_decoder *dec)
{
   return dec->cur < dec->end;
}

static inline void
vcomp_cs_decoder_reset(struct vcomp_cs_decoder *dec)
{
   /* dec->fatal_error is sticky */

   dec->cur = NULL;
   dec->end = NULL;
}

static inline void
vcomp_cs_decoder_set_fatal(const struct vcomp_cs_decoder *dec)
{
   ((struct vcomp_cs_decoder *)dec)->fatal_error = true;
}

static inline struct vcomp_object *
vcomp_cs_decoder_lookup_object(const struct vcomp_cs_decoder *dec,
                               vcomp_object_id id)
{
   struct vcomp_object *obj;

   if (!id)
      return NULL;

   const struct hash_entry *entry =
       _mesa_hash_table_search((struct hash_table *)dec->object_table, &id);
   obj = likely(entry) ? entry->data : NULL;
   if (unlikely(!obj))
   {
      vcomp_log("failed to look up object %" PRIu64, id);
      vcomp_cs_decoder_set_fatal(dec);
   }

   return obj;
}

static inline void
vcomp_cs_encoder_write(struct vcomp_cs_encoder *enc,
                       size_t size,
                       const void *val,
                       size_t val_size)
{
   assert(val_size <= size);

   if (unlikely(size > (size_t)(enc->end - enc->cur)))
   {
      vcomp_log("failed to write the reply stream");
      vcomp_cs_encoder_set_fatal(enc);
      return;
   }

   /* we should not rely on the compiler to optimize away memcpy... */
   memcpy(enc->cur, val, val_size);
   enc->cur += size;
}

static inline bool
vcomp_cs_decoder_peek_internal(const struct vcomp_cs_decoder *dec,
                               size_t size,
                               void *val,
                               size_t val_size)
{
   assert(val_size <= size);

   if (unlikely(size > (size_t)(dec->end - dec->cur)))
   {
      vcomp_log("failed to peek %zu bytes", size);
      vcomp_cs_decoder_set_fatal(dec);
      memset(val, 0, val_size);
      return false;
   }

   /* we should not rely on the compiler to optimize away memcpy... */
   memcpy(val, dec->cur, val_size);
   return true;
}

static inline void
vcomp_cs_decoder_read(struct vcomp_cs_decoder *dec, size_t size, void *val, size_t val_size)
{
   if (vcomp_cs_decoder_peek_internal(dec, size, val, val_size))
      dec->cur += size;
}

static inline void
vcomp_cs_decoder_peek(const struct vcomp_cs_decoder *dec,
                      size_t size,
                      void *val,
                      size_t val_size)
{
   vcomp_cs_decoder_peek_internal(dec, size, val, val_size);
}

static inline vcomp_object_id
vcomp_cs_handle_load_id(const void **handle)
{
   const vcomp_object_id *p = (const vcomp_object_id *)handle;
   return *p;
}

static inline void
vcomp_cs_handle_store_id(void **handle, vcomp_object_id id)
{
   vcomp_object_id *p = (vcomp_object_id *)handle;
   *p = id;
}

static inline void
vcomp_cs_decoder_reset_temp_pool(struct vcomp_cs_decoder *dec)
{
   struct vcomp_cs_decoder_temp_pool *pool = &dec->temp_pool;
   pool->cur = pool->reset_to;
}

bool vcomp_cs_decoder_alloc_temp_internal(struct vcomp_cs_decoder *dec, size_t size);

static inline void *
vcomp_cs_decoder_alloc_temp(struct vcomp_cs_decoder *dec, size_t size)
{
   struct vcomp_cs_decoder_temp_pool *pool = &dec->temp_pool;

   if (unlikely(size > (size_t)(pool->end - pool->cur)))
   {
      if (!vcomp_cs_decoder_alloc_temp_internal(dec, size))
      {
         vcomp_log("failed to suballocate %zu bytes from the temp pool", size);
         vcomp_cs_decoder_set_fatal(dec);
         return NULL;
      }
   }

   /* align to 64-bit after we know size is at most
    * Vcomp_CS_DECODER_TEMP_POOL_MAX_SIZE and cannot overflow
    */
   size = align64(size, 8);
   assert(size <= (size_t)(pool->end - pool->cur));

   void *ptr = pool->cur;
   pool->cur += size;
   return ptr;
}

static inline void *
vcomp_cs_decoder_alloc_temp_array(struct vcomp_cs_decoder *dec, size_t size, size_t count)
{
   size_t alloc_size;
   if (unlikely(__builtin_mul_overflow(size, count, &alloc_size)))
   {
      vcomp_log("overflow in array allocation of %zu * %zu bytes", size, count);
      vcomp_cs_decoder_set_fatal(dec);
      return NULL;
   }

   return vcomp_cs_decoder_alloc_temp(dec, alloc_size);
}

#endif /* VCOMP_CS_H */
