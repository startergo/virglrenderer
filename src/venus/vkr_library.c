/*
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: MIT
 */

#include <dlfcn.h>

#include "vkr_library.h"
#include "vkr_common.h"

#if defined(ENABLE_VULKAN_DLOAD)

bool
vkr_library_load(struct vulkan_library *lib)
{
   if (lib->handle)
       return true;

   lib->handle = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
   if (lib->handle == NULL)
      lib->handle = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
   if (lib->handle == NULL) {
      vkr_log("failed to open libvulkan: %s", dlerror());
      return false;
   }

   dlerror();    /* Clear any existing error */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
   // ignore error: ISO C forbids conversion of object pointer to function pointer type [-Werror=pedantic]
   lib->GetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)dlsym(lib->handle, "vkGetInstanceProcAddr");
#pragma GCC diagnostic pop

   char *error = dlerror();
   if (error != NULL) {
     fprintf(stderr, "%s\n", error);
     goto error;
   }

   if (lib->GetInstanceProcAddr == NULL) {
      vkr_log("failed to load vkGetInstanceProcAddr: %s", dlerror());
      goto error;
   }

   return true;

error:
   dlclose(lib->handle);
   lib->handle = NULL;
   return false;
}

void
vkr_library_unload(struct vulkan_library *lib)
{
   if (lib->handle) {
      dlclose(lib->handle);
      lib->GetInstanceProcAddr = NULL;
      lib->handle = NULL;
   }
}
#endif // ENABLE_VULKAN_DLOAD
