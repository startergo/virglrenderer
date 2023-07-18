/**************************************************************************
 *
 * Copyright (C) 2014 Red Hat Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/
#ifndef VREND_IOV_H
#define VREND_IOV_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#ifdef HAVE_SYS_UIO_H
#include <sys/uio.h>
#else
struct iovec {
    void *iov_base;
    size_t iov_len;
};
#endif

struct vrend_transfer_info {
   unsigned int level;
   uint32_t stride;
   uint32_t layer_stride;
   unsigned int iovec_cnt;
   const struct iovec *iovec;
   uint64_t offset;
   struct pipe_box *box;
   bool synchronized;
};

struct vrend_iovec_iter {
   const struct iovec *iov_begin;
   const struct iovec *iov_end;
   const struct iovec *iov;
   // Offset in current iov.
   size_t current_offset;
   // Sum of sizes of all previous iovecs in the iov array.
   // Total offset to current byte = previous_offset + current_offset.
   size_t previous_offset;
   // Sum of all iov sizes. Valid after call to vrend_get_iovec_iter_size.
   // Invalid value is (size_t)-1 .
   size_t cached_total_size;
};

typedef void (*iov_cb)(void *cookie, unsigned int doff, void *src, int len);

size_t vrend_get_iovec_size(const struct iovec *iov, int iovlen);
// Note: this seeks through the iovec from the beginning, so it is slow
// when used for many consecutive reads. See vrend_iovec_iter API below.
size_t vrend_read_from_iovec(const struct iovec *iov, int iov_cnt,
                             size_t offset, char *buf, size_t bytes);
// Note: this seeks through the iovec from the beginning, so it is slow
// when used for many consecutive writes. See vrend_iovec_iter API below.
size_t vrend_write_to_iovec(const struct iovec *iov, int iov_cnt,
                            size_t offset, const char *buf, size_t bytes);

size_t vrend_read_from_iovec_cb(const struct iovec *iov, int iov_cnt,
                          size_t offset, size_t bytes, iov_cb iocb, void *cookie);

void vrend_init_iovec_iter(struct vrend_iovec_iter* restrict iov_iter,
                           const struct iovec *iov_begin, int iovlen);
void vrend_clear_iovec_iter(struct vrend_iovec_iter* restrict iov_iter);
size_t vrend_get_iovec_iter_size(struct vrend_iovec_iter *iov_iter);
// Move current iovec offset to given absolute target_offset.
void vrend_seek_iovec_iter(struct vrend_iovec_iter* restrict iov_iter,
                           size_t target_offset);
// num: num blocks to read.
// skip_bytes: for strides, skip skip_bytes forward after each block.
// buf_skip_bytes: after writing a block to buf, offset buf by buf_skip_bytes.
//                 May be negative for writing blocks in reverse order.
size_t vrend_read_mult_from_iovec_iter(struct vrend_iovec_iter* restrict iov_iter,
                                       char *buf, size_t bytes,
                                       int num, uint32_t skip_bytes,
                                       int buf_skip_bytes);
// Reads one block of data starting from given absolute byte offset.
// If iov_iter is NULL, falls back on non-iterator API.
static inline size_t vrend_read_from_iovec_iter_compat(struct vrend_iovec_iter* restrict iov_iter,
                                                       const struct iovec *iov, int iov_cnt,
                                                       size_t offset, char *buf, size_t bytes)
{
   if (iov_iter) {
      vrend_seek_iovec_iter(iov_iter, offset);
      return vrend_read_mult_from_iovec_iter(iov_iter, buf, bytes, 1, 0, 0);
   } else {
      return vrend_read_from_iovec(iov, iov_cnt, offset, buf, bytes);
   }
}

int vrend_copy_iovec(const struct iovec *src_iov, int src_iovlen, size_t src_offset,
                     const struct iovec *dst_iov, int dst_iovlen, size_t dst_offset,
                     size_t count, char *buf);

#endif
