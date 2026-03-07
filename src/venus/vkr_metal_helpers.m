/*
 * Copyright 2026 Lucas Amaral
 * SPDX-License-Identifier: MIT
 */

#ifdef __APPLE__

#include "vkr_metal_helpers.h"

#import <Metal/Metal.h>
#import <mach/vm_page_size.h>

void *
vkr_metal_create_buffer(void *mtl_device, void *ptr, size_t size)
{
   if (!mtl_device || !ptr || size == 0)
      return NULL;

   id<MTLDevice> device = (id<MTLDevice>)mtl_device;
   id<MTLBuffer> buffer = [device newBufferWithBytesNoCopy:ptr
                                                    length:size
                                                   options:MTLResourceStorageModeShared
                                               deallocator:nil];
   if (!buffer)
      return NULL;

   /* Transfer ownership to the caller — caller must use
    * vkr_metal_release_buffer() to release.
    * In MRR (non-ARC), newBuffer… already returns +1 retained.
    * The cast is a simple pointer conversion; CFRelease balances it. */
   return (void *)buffer;
}

void
vkr_metal_release_buffer(void *mtl_buffer)
{
   if (mtl_buffer)
      CFRelease(mtl_buffer);
}

size_t
vkr_metal_get_page_size(void)
{
   return (size_t)vm_page_size;
}

#endif /* __APPLE__ */
