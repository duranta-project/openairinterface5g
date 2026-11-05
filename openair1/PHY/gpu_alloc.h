/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Pinned host allocations for the LDPC GPU offload.
 *
 * The GPU offload wants its host-side buffers page-locked and, mostly, mapped
 * into the device address space. Every such allocation is a hard requirement
 * at init time, so there is nothing sensible to do on failure. These helpers
 * do the allocation, check the status and abort with the call site in the
 * message, which keeps gpuError_t out of the callers. gpuMalloc_or_fail() and
 * gpuMemcpyAsync_or_fail() do the same for device memory and asynchronous
 * copies.
 *
 * Only compiled into the LDPC_CUDA builds; callers guard their use with
 * #ifdef LDPC_CUDA, as they do for the matching gpuFreeHost().
 */

#ifndef PHY_GPU_ALLOC_H
#define PHY_GPU_ALLOC_H

#include <stddef.h>
#include "PHY/gpu_compat.h"

#include "common/utils/assertions.h"

/** @brief Allocate @p size bytes of pinned host memory with gpuHostAlloc() @p flags, or abort. */
static inline void *gpu_host_alloc_or_fail(size_t size, unsigned int flags, const char *file, int line)
{
  void *p = NULL;
  gpuError_t err = gpuHostAlloc(&p, size, flags);
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

/** @brief Allocate @p size bytes of device memory, or abort. */
static inline void *gpu_malloc_or_fail(size_t size, const char *file, int line)
{
  void *p = NULL;
  gpuError_t err = gpuMalloc(&p, size);
  AssertFatal(err == gpuSuccess, "%s:%d: gpuMalloc(%zu) failed: %s\n", file, line, size, gpuGetErrorString(err));
  return p;
}

#define gpuHostAlloc_or_fail(size, flags) gpu_host_alloc_or_fail((size), (flags), __FILE__, __LINE__)
#define gpuHostGetDevicePointer_or_fail(host) gpu_host_get_device_pointer_or_fail((host), __FILE__, __LINE__)
#define gpuMalloc_or_fail(size) gpu_malloc_or_fail((size), __FILE__, __LINE__)

/** @brief Enqueue an asynchronous copy of @p count bytes on @p stream, or abort. */
#define gpuMemcpyAsync_or_fail(dst, src, count, kind, stream)                                                      \
  do {                                                                                                             \
    gpuError_t err_ = gpuMemcpyAsync((dst), (src), (count), (kind), (stream));                                     \
    AssertFatal(err_ == gpuSuccess, "gpuMemcpyAsync(%zu) failed: %s\n", (size_t)(count), gpuGetErrorString(err_)); \
  } while (0)

#endif /* PHY_GPU_ALLOC_H */
