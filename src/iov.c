/*
 * this code is taken from Michael - the qemu code is GPLv2 so I don't want
 * to reuse it.
 * I've adapted it to handle offsets and callback
 */

//
// iovec.c
//
// Scatter/gather utility routines
//
// Copyright (C) 2002 Michael Ringgaard. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
// 3. Neither the name of the project nor the names of its contributors
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
// OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
// OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE.
//

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "vrend_iov.h"

size_t vrend_get_iovec_size(const struct iovec *iov, int iovlen) {
  size_t size = 0;

  while (iovlen > 0) {
    size += iov->iov_len;
    iov++;
    iovlen--;
  }

  return size;
}

size_t vrend_read_from_iovec(const struct iovec *iov, int iovlen,
			     size_t offset,
			     char *buf, size_t count)
{
  size_t read = 0;
  size_t len;

  while (count > 0 && iovlen > 0) {
    if (iov->iov_len > offset) {
      len = iov->iov_len - offset;

      if (count < len) len = count;

      memcpy(buf, (char*)iov->iov_base + offset, len);
      read += len;

      buf += len;
      count -= len;
      offset = 0;
    } else {
      offset -= iov->iov_len;
    }

    iov++;
    iovlen--;
  }
    assert(offset == 0);
  return read;
}

size_t vrend_write_to_iovec(const struct iovec *iov, int iovlen,
			 size_t offset, const char *buf, size_t count)
{
  size_t written = 0;
  size_t len;

  while (count > 0 && iovlen > 0) {
    if (iov->iov_len > offset) {
      len = iov->iov_len - offset;

      if (count < len) len = count;

      memcpy((char*)iov->iov_base + offset, buf, len);
      written += len;

      offset = 0;
      buf += len;
      count -= len;
    } else {
      offset -= iov->iov_len;
    }
    iov++;
    iovlen--;
  }
    assert(offset == 0);
  return written;
}

size_t vrend_read_from_iovec_cb(const struct iovec *iov, int iovlen,
				size_t offset, size_t count,
				iov_cb iocb, void *cookie)
{
  size_t read = 0;
  size_t len;

  while (count > 0 && iovlen > 0) {
    if (iov->iov_len > offset) {
      len = iov->iov_len - offset;

      if (count < len) len = count;

      (*iocb)(cookie, read, (char*)iov->iov_base + offset, len);
      read += len;

      count -= len;
      offset = 0;
    } else {
      offset -= iov->iov_len;
    }
    iov++;
    iovlen--;
  }
    assert(offset == 0);
  return read;


}

static void vrend_advance_iovec_iter(struct vrend_iovec_iter* restrict it, size_t relative_offset)
{
  it->current_offset += relative_offset;
  while(it->current_offset >= it->iov->iov_len && it->iov != it->iov_end) {
    it->current_offset -= it->iov->iov_len;
    it->previous_offset += it->iov->iov_len;
    ++it->iov;
  }
}

void vrend_init_iovec_iter(struct vrend_iovec_iter* restrict it,
                           const struct iovec *iov_begin, int iovlen)
{
  it->iov_begin = it->iov = iov_begin;
  it->iov_end = iov_begin + iovlen;
  it->current_offset = 0;
  it->previous_offset = 0;
  it->previous_offset = (size_t)-1;
}

void vrend_clear_iovec_iter(struct vrend_iovec_iter* restrict it) {
  vrend_init_iovec_iter(it, NULL, 0);
}

size_t vrend_get_iovec_iter_size(struct vrend_iovec_iter *it) {
  if (it->cached_total_size != (size_t)-1)
    return it->cached_total_size;
  it->cached_total_size = 0;
  for (const struct iovec* iov = it->iov_begin; iov != it->iov_end; ++iov) {
    it->cached_total_size += iov->iov_len;
  }
  return it->cached_total_size;
}

void vrend_seek_iovec_iter(struct vrend_iovec_iter* restrict it,
                           size_t target_offset)
{
  size_t total_offset = it->previous_offset + it->current_offset;
  if (target_offset >= total_offset)
    vrend_advance_iovec_iter(it, target_offset - total_offset);
  else {
    // Assume starting over
    it->current_offset = 0;
    it->iov = it->iov_begin;
    it->previous_offset = 0;
    vrend_advance_iovec_iter(it, target_offset);
  }
}

size_t vrend_read_mult_from_iovec_iter(struct vrend_iovec_iter* restrict it,
                                      char *buf, size_t bytes,
                                      int num, uint32_t skip_bytes,
                                      int buf_skip_bytes)
{
  size_t read = 0;
  size_t len;
  size_t item_bytes;
  int num_countdown = num;

  while (num_countdown > 0 && it->iov != it->iov_end) {
    item_bytes = bytes;
    char* block_buf = buf;
    while (item_bytes > 0 && it->iov != it->iov_end) {
      len = it->iov->iov_len - it->current_offset;

      if (item_bytes < len)
        len = item_bytes;

      memcpy(block_buf, (char*)it->iov->iov_base + it->current_offset, len);
      read += len;
      block_buf += len;
      item_bytes -= len;
      vrend_advance_iovec_iter(it, len);
    }
    --num_countdown;
    if (num_countdown > 0) {
      vrend_advance_iovec_iter(it, skip_bytes);
      buf += buf_skip_bytes;
    }
  }
  assert(read == num * bytes);
  return read;
}

/**
 * Copy data from one iovec to another iovec.
 *
 * TODO: Implement iovec copy without copy to intermediate buffer.
 *
 * \param src_iov    The source iov.
 * \param src_iovlen The number of memory regions in the source iov.
 * \param src_offset The byte offset in the source iov to start reading from.
 * \param dst_iov    The destination iov.
 * \param dst_iovlen The number of memory regions in the destination iov.
 * \param dst_offset The byte offset in the destination iov to start writing to.
 * \param count      The number of bytes to copy
 * \param buf        If not NULL, a pointer to a buffer of at least count size
 *                   to use a temporary storage for the copy operation.
 * \return           -1 on failure, 0 on success
 */
int vrend_copy_iovec(const struct iovec *src_iov, int src_iovlen, size_t src_offset,
		     const struct iovec *dst_iov, int dst_iovlen, size_t dst_offset,
		     size_t count, char *buf)
{
  int ret = 0;
  bool needs_free;
  size_t nread;
  size_t nwritten;

  if (src_iov == NULL || dst_iov == NULL)
    return -1;

  if (src_iov == dst_iov && src_offset == dst_offset)
    return 0;

  if (!buf) {
    buf = malloc(count);
    needs_free = true;
  } else {
    needs_free = false;
  }

  if (!buf)
    return -1;

  nread = vrend_read_from_iovec(src_iov, src_iovlen, src_offset, buf, count);
  if (nread != count) {
    ret = -1;
    goto out;
  }

  nwritten = vrend_write_to_iovec(dst_iov, dst_iovlen, dst_offset, buf, count);
  if (nwritten != count) {
    ret = -1;
    goto out;
  }

out:
  if (needs_free)
    free(buf);

  return ret;
}
