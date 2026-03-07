/*
 * Copyright 2026 Lucas Amaral
 * SPDX-License-Identifier: MIT
 */

#ifndef VKR_METAL_HELPERS_H
#define VKR_METAL_HELPERS_H

#include <stddef.h>

/*
 * Metal shared memory (opaque, allocated/freed in vkr_metal_helpers.m)
 */
struct vkr_mtl_shm {
   int shm_fd;
   void *shm_ptr;
   size_t shm_size;
   void *mtl_buffer;
};

#ifdef __APPLE__

#include <stdint.h>

/*
 * Metal helper functions (implemented in vkr_metal_helpers.m)
 *
 * Vulkan Metal struct types come from venus-protocol/vulkan_metal.h,
 * included via VK_USE_PLATFORM_METAL_EXT (set in meson.build).
 */

/* Get the system default MTLDevice.  Caches the result internally.
 * Returns an opaque MTLDevice pointer, or NULL on failure.
 *
 * TODO: For multi-GPU Macs, match against the physical device.
 */
void *
vkr_metal_get_default_device(void);

/* Allocate Metal shared memory: create anonymous SHM file, mmap it,
 * wrap as MTLBuffer.  Returns a populated vkr_mtl_shm, or NULL on failure.
 * Caller must free with vkr_mtl_shm_free().
 */
struct vkr_mtl_shm *
vkr_mtl_shm_alloc(void *mtl_device, uint64_t size);

/* Release all resources held by a vkr_mtl_shm and free the struct. */
void
vkr_mtl_shm_free(struct vkr_mtl_shm *shm);

#else /* !__APPLE__ */

static inline void *
vkr_metal_get_default_device(void)
{
   return NULL;
}

static inline struct vkr_mtl_shm *
vkr_mtl_shm_alloc(void *mtl_device, uint64_t size)
{
   (void)mtl_device;
   (void)size;
   return NULL;
}

static inline void
vkr_mtl_shm_free(struct vkr_mtl_shm *shm)
{
   (void)shm;
}

#endif /* __APPLE__ */

#endif /* VKR_METAL_HELPERS_H */
