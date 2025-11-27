/*
 * Copyright 2025 Advanced Micro Devices, Inc
 * SPDX-License-Identifier: MIT
 */

#ifndef HSAKMT_HW_H_
#define HSAKMT_HW_H_

#include "drm_hw.h"

#define vhsakmt_shmem_base vdrm_shmem
#define vhsakmt_ccmd_req vdrm_ccmd_req
#define vhsakmt_ccmd_rsp vdrm_ccmd_rsp

/*
 * HSAKMT API Wrapper - Compile-time configurable API selection
 *
 * This provides a unified macro interface to switch between legacy 
 * global HSAKMT API and new context-aware HSAKMT API at compile time:
 *   - Use HSAKMT_CALL(FunctionName) to automatically select the right API
 *   - The macro uses ## token pasting to append "Ctx" suffix when needed
 *   - For APIs with different signatures, use conditional macros at call site
 *   - Define USE_HSAKMT_CTX_API to use new context-aware API
 *   - Leave undefined to use legacy global API
 */

#ifdef USE_HSAKMT_CTX_API
#include <hsakmt/hsakmtctx.h>
#else
#include <hsakmt/hsakmt.h>
#endif

struct vhsakmt_context;
struct vhsakmt_backend;

#ifdef USE_HSAKMT_CTX_API
  #define HSAKMT_CALL(func) func##Ctx
  #define HSAKMT_CTX(ctx)   ((ctx)->kfd_ctx)
  #define HSAKMT_CTX_ARG(ctx) HSAKMT_CTX(ctx),
  #define HSAKMT_BACKEND_CTX(backend) ((backend)->primary_ctx)
  #define HSAKMT_BACKEND_CTX_ARG(backend) HSAKMT_BACKEND_CTX(backend),
#else
  #define HSAKMT_CALL(func) func
  #define HSAKMT_CTX(ctx)   /* nothing */
  #define HSAKMT_CTX_ARG(ctx) /* nothing */
  #define HSAKMT_BACKEND_CTX(backend) /* nothing */
  #define HSAKMT_BACKEND_CTX_ARG(backend) /* nothing */
#endif

/* OpenKFD - Output parameter changes from void to HsaKFDContext** */
#ifdef USE_HSAKMT_CTX_API
  #define HSAKMT_OPEN_KFD(backend) \
    hsaKmtOpenKFDCtx(&((backend)->primary_ctx))
  #define HSAKMT_CLOSE_KFD() \
    hsaKmtCloseKFDCtx()
#else
  #define HSAKMT_OPEN_KFD(backend) \
    hsaKmtOpenKFD()
  #define HSAKMT_CLOSE_KFD(backend) \
    hsaKmtCloseKFD()
#endif

/* OpenSecondaryKFD - Not available in legacy API */
#ifdef USE_HSAKMT_CTX_API
  #define HSAKMT_OPEN_SECONDARY_KFD(ctx) \
    hsaKmtOpenSecondaryKFDCtx(&((ctx)->kfd_ctx))
  #define HSAKMT_CLOSE_SECONDARY_KFD(ctx) \
    hsaKmtCloseSecondaryKFDCtx((ctx)->kfd_ctx)
#else
  #define HSAKMT_OPEN_SECONDARY_KFD(ctx) (-ENOTSUP)
  #define HSAKMT_CLOSE_SECONDARY_KFD(ctx) (-ENOTSUP)
#endif

#endif /* HSAKMT_HW_H_ */
