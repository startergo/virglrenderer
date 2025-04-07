/*
 * Copyright 2021 Google LLC
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_cs.h"

static void
vcomp_cs_decoder_sanity_check(const struct vcomp_cs_decoder *dec)
{
   const struct vcomp_cs_decoder_temp_pool *pool = &dec->temp_pool;
   assert(pool->buffer_count <= pool->buffer_max);
   if (pool->buffer_count)
   {
      assert(pool->buffers[pool->buffer_count - 1] <= pool->reset_to);
      assert(pool->reset_to <= pool->cur);
      assert(pool->cur <= pool->end);
   }

   assert(dec->cur <= dec->end);
}

static uint32_t
next_array_size(uint32_t cur_size, uint32_t min_size)
{
   const uint32_t next_size = cur_size ? cur_size * 2 : min_size;
   return next_size > cur_size ? next_size : 0;
}

static size_t
next_buffer_size(size_t cur_size, size_t min_size, size_t need)
{
   size_t next_size = cur_size ? cur_size * 2 : min_size;
   while (next_size < need)
   {
      next_size *= 2;
      if (!next_size)
         return 0;
   }
   return next_size;
}

static bool
vcomp_cs_decoder_grow_temp_pool(struct vcomp_cs_decoder *dec)
{
   struct vcomp_cs_decoder_temp_pool *pool = &dec->temp_pool;
   const uint32_t buf_max = next_array_size(pool->buffer_max, 4);
   if (!buf_max)
      return false;

   uint8_t **bufs = realloc(pool->buffers, sizeof(*pool->buffers) * buf_max);
   if (!bufs)
      return false;

   pool->buffers = bufs;
   pool->buffer_max = buf_max;

   return true;
}

bool vcomp_cs_decoder_alloc_temp_internal(struct vcomp_cs_decoder *dec, size_t size)
{
   struct vcomp_cs_decoder_temp_pool *pool = &dec->temp_pool;

   if (pool->buffer_count >= pool->buffer_max)
   {
      if (!vcomp_cs_decoder_grow_temp_pool(dec))
         return false;
      assert(pool->buffer_count < pool->buffer_max);
   }

   const size_t cur_buf_size =
       pool->buffer_count ? pool->end - pool->buffers[pool->buffer_count - 1] : 0;
   const size_t buf_size = next_buffer_size(cur_buf_size, 4096, size);
   if (!buf_size)
      return false;

   if (buf_size > VCOMP_CS_DECODER_TEMP_POOL_MAX_SIZE - pool->total_size)
      return false;

   uint8_t *buf = malloc(buf_size);
   if (!buf)
      return false;

   pool->total_size += buf_size;
   pool->buffers[pool->buffer_count++] = buf;
   pool->reset_to = buf;
   pool->cur = buf;
   pool->end = buf + buf_size;

   vcomp_cs_decoder_sanity_check(dec);

   return true;
}
