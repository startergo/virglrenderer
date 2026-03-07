/*
 * Copyright 2026 Lucas Amaral
 * SPDX-License-Identifier: MIT
 */

#ifndef VKR_METAL_HELPERS_H
#define VKR_METAL_HELPERS_H

#ifdef __APPLE__

#include <stddef.h>
#include <stdint.h>

#include "venus-protocol/vulkan.h"

/*
 * Vulkan Metal struct definitions from vulkan_metal.h.
 *
 * virglrenderer's venus-protocol headers include the enum values
 * (VK_STRUCTURE_TYPE_IMPORT_MEMORY_METAL_HANDLE_INFO_EXT, etc.) in
 * vulkan_core.h, but not the Metal-specific struct types which live in
 * vulkan_metal.h.  We define only what we need here.
 */

/* VK_EXT_external_memory_metal */
typedef struct VkImportMemoryMetalHandleInfoEXT {
   VkStructureType sType;
   const void *pNext;
   VkExternalMemoryHandleTypeFlagBits handleType;
   void *handle; /* MTLBuffer_id, MTLTexture_id, or MTLHeap_id */
} VkImportMemoryMetalHandleInfoEXT;

typedef struct VkMemoryGetMetalHandleInfoEXT {
   VkStructureType sType;
   const void *pNext;
   VkDeviceMemory memory;
   VkExternalMemoryHandleTypeFlagBits handleType;
} VkMemoryGetMetalHandleInfoEXT;

typedef VkResult(VKAPI_PTR *PFN_vkGetMemoryMetalHandleEXT)(
   VkDevice device,
   const VkMemoryGetMetalHandleInfoEXT *pGetMetalHandleInfo,
   void **pHandle);

typedef VkResult(VKAPI_PTR *PFN_vkGetMemoryMetalHandlePropertiesEXT)(
   VkDevice device,
   VkExternalMemoryHandleTypeFlagBits handleType,
   const void *pHandle,
   void *pMemoryMetalHandleProperties);

/* VK_EXT_metal_objects — for exporting MTLDevice from VkDevice */
typedef enum VkExportMetalObjectTypeFlagBitsEXT {
   VK_EXPORT_METAL_OBJECT_TYPE_METAL_DEVICE_BIT_EXT = 0x00000001,
   VK_EXPORT_METAL_OBJECT_TYPE_METAL_COMMAND_QUEUE_BIT_EXT = 0x00000002,
   VK_EXPORT_METAL_OBJECT_TYPE_METAL_BUFFER_BIT_EXT = 0x00000004,
   VK_EXPORT_METAL_OBJECT_TYPE_METAL_TEXTURE_BIT_EXT = 0x00000008,
   VK_EXPORT_METAL_OBJECT_TYPE_METAL_IOSURFACE_BIT_EXT = 0x00000010,
   VK_EXPORT_METAL_OBJECT_TYPE_METAL_SHARED_EVENT_BIT_EXT = 0x00000020,
} VkExportMetalObjectTypeFlagBitsEXT;

typedef struct VkExportMetalObjectCreateInfoEXT {
   VkStructureType sType;
   const void *pNext;
   VkExportMetalObjectTypeFlagBitsEXT exportObjectType;
} VkExportMetalObjectCreateInfoEXT;

typedef struct VkExportMetalObjectsInfoEXT {
   VkStructureType sType;
   const void *pNext;
} VkExportMetalObjectsInfoEXT;

typedef struct VkExportMetalDeviceInfoEXT {
   VkStructureType sType;
   const void *pNext;
   void *mtlDevice; /* MTLDevice_id */
} VkExportMetalDeviceInfoEXT;

typedef void(VKAPI_PTR *PFN_vkExportMetalObjectsEXT)(
   VkDevice device, VkExportMetalObjectsInfoEXT *pMetalObjectsInfo);

/*
 * Metal helper functions (implemented in vkr_metal_helpers.m)
 */

/* Create an MTLBuffer wrapping existing page-aligned memory.
 * Returns an opaque MTLBuffer pointer (retained), or NULL on failure.
 * The memory must be page-aligned (from mmap/vm_allocate).
 * The size must be a multiple of vkr_metal_get_page_size().
 */
void *
vkr_metal_create_buffer(void *mtl_device, void *ptr, size_t size);

/* Release a previously created MTLBuffer. */
void
vkr_metal_release_buffer(void *mtl_buffer);

/* Return the system VM page size (16384 on Apple Silicon, 4096 on Intel). */
size_t
vkr_metal_get_page_size(void);

#endif /* __APPLE__ */

#endif /* VKR_METAL_HELPERS_H */
