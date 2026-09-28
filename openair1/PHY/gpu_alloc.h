/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Pinned, device-mapped host allocations for the LDPC GPU offload.
 *
 * The GPU offload wants its host-side buffers page-locked and mapped into the
 * device address space. Every such allocation is a hard requirement at init
 * time, so there is nothing sensible to do on failure. These helpers do the
 * allocation, check the status and abort with the call site in the message,
 * which keeps gpuError_t out of the callers.
 *
 * Only compiled into the LDPC_CUDA builds; callers guard their use with
 * #ifdef LDPC_CUDA, as they do for the matching gpuFreeHost().
 */

#ifndef PHY_GPU_ALLOC_H
#define PHY_GPU_ALLOC_H

#include <stddef.h>
#include "PHY/gpu_compat.h"

#include "common/utils/assertions.h"

/** @brief Allocate @p size bytes of pinned, device-mapped host memory, or abort. */
static inline void *gpu_host_alloc_mapped_or_fail(size_t size, const char *file, int line)
{
  void *p = NULL;
  gpuError_t err = gpuHostAlloc(&p, size, gpuHostAllocMapped);
  AssertFatal(err == gpuSuccess, "%s:%d: gpuHostAlloc(%zu) failed: %s\n", file, line, size, gpuGetErrorString(err));
  return p;
}

/** @brief Return the device pointer aliasing the pinned host allocation @p host, or abort. */
static inline void *gpu_host_get_device_pointer_or_fail(void *host, const char *file, int line)
{
  void *dev = NULL;
  gpuError_t err = gpuHostGetDevicePointer(&dev, host, 0);
  AssertFatal(err == gpuSuccess, "%s:%d: gpuHostGetDevicePointer(%p) failed: %s\n", file, line, host, gpuGetErrorString(err));
  return dev;
}

#define gpuHostAlloc_or_fail(size) gpu_host_alloc_mapped_or_fail((size), __FILE__, __LINE__)
#define gpuHostGetDevicePointer_or_fail(host) gpu_host_get_device_pointer_or_fail((host), __FILE__, __LINE__)

#endif /* PHY_GPU_ALLOC_H */
