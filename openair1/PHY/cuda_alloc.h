/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

/*
 * Pinned, device-mapped host allocations for the LDPC CUDA offload.
 *
 * The GPU offload wants its host-side buffers page-locked and mapped into the
 * device address space. Every such allocation is a hard requirement at init
 * time, so there is nothing sensible to do on failure. These helpers do the
 * allocation, check the status and abort with the call site in the message,
 * which keeps cudaError_t out of the callers.
 *
 * Only compiled into the LDPC_CUDA builds; callers guard their use with
 * #ifdef LDPC_CUDA, as they do for the matching cudaFreeHost().
 */

#ifndef PHY_CUDA_ALLOC_H
#define PHY_CUDA_ALLOC_H

#include <stddef.h>
#include <cuda_runtime.h>

#include "common/utils/assertions.h"

/** @brief Allocate @p size bytes of pinned, device-mapped host memory, or abort. */
static inline void *cuda_host_alloc_mapped_or_fail(size_t size, const char *file, int line)
{
  void *p = NULL;
  cudaError_t err = cudaHostAlloc(&p, size, cudaHostAllocMapped);
  AssertFatal(err == cudaSuccess, "%s:%d: cudaHostAlloc(%zu) failed: %s\n", file, line, size, cudaGetErrorString(err));
  return p;
}

/** @brief Return the device pointer aliasing the pinned host allocation @p host, or abort. */
static inline void *cuda_host_get_device_pointer_or_fail(void *host, const char *file, int line)
{
  void *dev = NULL;
  cudaError_t err = cudaHostGetDevicePointer(&dev, host, 0);
  AssertFatal(err == cudaSuccess, "%s:%d: cudaHostGetDevicePointer(%p) failed: %s\n", file, line, host, cudaGetErrorString(err));
  return dev;
}

#define cudaHostAlloc_or_fail(size) cuda_host_alloc_mapped_or_fail((size), __FILE__, __LINE__)
#define cudaHostGetDevicePointer_or_fail(host) cuda_host_get_device_pointer_or_fail((host), __FILE__, __LINE__)

#endif /* PHY_CUDA_ALLOC_H */
