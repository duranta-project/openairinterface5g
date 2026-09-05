/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Defines the optimized LDPC encoder for NVidia GPUs
 */

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "time_meas.h"
#include "openair1/PHY/CODING/nrLDPC_defs.h"
#include "PHY/sse_intrin.h"
#include "openair1/PHY/CODING/nrLDPC_extern.h"

#include "PHY/gpu_compat.h"

//#define DEBUG_LDPC 1

#include "ldpc_encode_parity_check_cuda.c"

#define MAX_SEGx32 4

uint32_t *c_dev;
uint32_t **c_host;
uint32_t *c_devh[4];
uint32_t *d_dev;
uint32_t **d_host;
uint32_t *d_devh[MAX_SEGx32];
uint32_t *input_dev;
uint32_t **input_host;
uint32_t *input_devh[144];
int managed = 0, concurrent = 0, uva = 0, pageable = 0, pageable_uses_host = 0, register_host = 0, integrated = 0;

int cuda_support_set = 0;

extern gpuStream_t encoderStreams[4];

int ldpc_input(uint32_t **input,uint32_t *cc[4],int nseg,int Zc,int ncols,gpuStream_t *s,int sidx);

void cuda_support_init()
{
  int dev = 0;
  gpuDeviceProp_t prop;
  gpuGetDeviceProperties(&prop, dev);

  gpuDeviceGetAttribute(&managed, gpuDevAttrManagedMemory, dev);
  gpuDeviceGetAttribute(&concurrent, gpuDevAttrConcurrentManagedAccess, dev);
  gpuDeviceGetAttribute(&uva, gpuDevAttrUnifiedAddressing, dev);
  gpuDeviceGetAttribute(&pageable, gpuDevAttrPageableMemoryAccess, dev);
  gpuDeviceGetAttribute(&pageable_uses_host, gpuDevAttrPageableMemoryAccessUsesHostPageTables, dev);
  gpuDeviceGetAttribute(&register_host, gpuDevAttrHostRegisterSupported, dev);
  gpuDeviceGetAttribute(&integrated, gpuDevAttrIntegrated, dev);

  LOG_I(NR_PHY, "Device: %s (cc %d.%d)\n", prop.name, prop.major, prop.minor);
  LOG_I(NR_PHY, "Unified Virtual Addressing (UVA): %s\n", uva ? "YES" : "NO");
  LOG_I(NR_PHY, "Managed (Unified) Memory:        %s\n", managed ? "YES" : "NO");
  LOG_I(NR_PHY, "Concurrent managed access:       %s\n", concurrent ? "YES" : "NO");
  LOG_I(NR_PHY, "Pageable memory access:          %s\n", pageable ? "YES" : "NO");
  LOG_I(NR_PHY, "Uses host page tables:           %s\n", pageable_uses_host ? "YES" : "NO");
  LOG_I(NR_PHY, "Host Register supported:         %s\n", register_host ? "YES" : "NO");
  LOG_I(NR_PHY, "Integrated (shared) Memory       %s\n", integrated ? "YES" : "NO");

  if (!pageable && !integrated) {
    LOG_I(NR_PHY, "Allocating c,d,cc arrays for GPU \n");
    gpuError_t err = gpuMalloc((void**)&c_dev, 4 * sizeof(uint32_t*));
    AssertFatal(err == gpuSuccess, "CUDA Error (c_dev): %s\n", gpuGetErrorString(err));
    err = gpuHostAlloc((void**)&c_host, 4 * sizeof(uint32_t*), gpuHostAllocDefault);
    AssertFatal(err == gpuSuccess, "CUDA Error (c_host): %s\n", gpuGetErrorString(err));
    for (int i = 0; i < 4; i++) {
      err = gpuMalloc((void**)&c_devh[i], 2 * 22 * 384 * sizeof(uint32_t));
      AssertFatal(err == gpuSuccess, "CUDA Error (c_devh[%d]): %s\n", i, gpuGetErrorString(err));
      err = gpuHostAlloc((void**)&c_host[i], 2 * 22 * 384 * sizeof(uint32_t), gpuHostAllocDefault);
      AssertFatal(err == gpuSuccess, "CUDA Error (chost[%d]): %s\n", i, gpuGetErrorString(err));
    }
    err = gpuMemcpy(c_dev, c_devh, 4 * sizeof(uint32_t*), gpuMemcpyHostToDevice);
    AssertFatal(err == gpuSuccess, "CUDA Error (memcpy c_devh -> c_dev): %s\n", gpuGetErrorString(err));
    err = gpuMalloc((void**)&d_dev, MAX_SEGx32 * sizeof(uint32_t*));
    AssertFatal(err == gpuSuccess, "CUDA Error: %s\n", gpuGetErrorString(err));
    err = gpuHostAlloc((void**)&d_host, MAX_SEGx32 * sizeof(uint32_t*), gpuHostAllocDefault);
    AssertFatal(err == gpuSuccess, "CUDA Error (d_host): %s\n", gpuGetErrorString(err));
    for (int i = 0; i < MAX_SEGx32; i++) {
      err = gpuMalloc((void**)&d_devh[i], 68 * 384 * sizeof(uint32_t));
      AssertFatal(err == gpuSuccess, "CUDA Error (d_devh[%d]: %s\n", i, gpuGetErrorString(err));
      err = gpuHostAlloc((void**)&d_host[i], 68 * 384 * sizeof(uint32_t), gpuHostAllocDefault);
      AssertFatal(err == gpuSuccess, "CUDA Error (d_host[%d]): %s\n", i, gpuGetErrorString(err));
    }
    err = gpuMemcpy(d_dev, d_devh, MAX_SEGx32 * sizeof(uint32_t*), gpuMemcpyHostToDevice);
    AssertFatal(err == gpuSuccess, "CUDA Error (memcpy d_devh -> d_dev): %s\n", gpuGetErrorString(err));
    err = gpuMalloc((void**)&input_dev, 144 * sizeof(uint8_t*));
    AssertFatal(err == gpuSuccess, "CUDA Error: %s\n", gpuGetErrorString(err));
    err = gpuHostAlloc((void**)&input_host, 144 * sizeof(uint8_t*), gpuHostAllocDefault);
    AssertFatal(err == gpuSuccess, "CUDA Error (cc_host): %s\n", gpuGetErrorString(err));
    for (int i = 0; i < 144; i++) {
      err = gpuMalloc((void**)&input_devh[i], (8448 / 8) * sizeof(uint8_t));
      AssertFatal(err == gpuSuccess, "CUDA Error (input_devh[%d]: %s\n", i, gpuGetErrorString(err));
      err = gpuHostAlloc((void**)&input_host[i], (8448 / 8) * sizeof(uint8_t), gpuHostAllocDefault);
      AssertFatal(err == gpuSuccess, "CUDA Error (input_host[%d]): %s\n", i, gpuGetErrorString(err));
    }
    err = gpuMemcpy(input_dev, input_devh, 144 * sizeof(uint8_t*), gpuMemcpyHostToDevice);
    AssertFatal(err == gpuSuccess, "CUDA Error (memcpy cc_devh -> d_dev): %s\n", gpuGetErrorString(err));
  } else {
    LOG_I(NR_PHY, "Allocating c,d,cc arrays for CPU/GPU shared-memory\n");
    gpuError_t err = gpuHostAlloc((void**)&c_host, 4 * sizeof(uint32_t*), gpuHostAllocMapped | gpuHostAllocPortable);
    AssertFatal(err == gpuSuccess, "CUDA Error (c_host): %s\n", gpuGetErrorString(err));
    err = gpuHostGetDevicePointer((void**)&c_dev, c_host, 0);
    AssertFatal(err == gpuSuccess, "CUDA Error (c_dev): %s\n", gpuGetErrorString(err));
    LOG_I(NR_PHY, "c_host %p, c_dev %p\n", c_host, c_dev);
    for (int i = 0; i < 4; i++) {
      err = gpuHostAlloc((void**)&c_host[i], 2 * 22 * 384 * sizeof(uint32_t), gpuHostAllocMapped);
      AssertFatal(err == gpuSuccess, "CUDA Error (c_host[%d]): %s\n", i, gpuGetErrorString(err));
      err = gpuHostGetDevicePointer((void**)&c_devh[i], c_host[i], 0);
      AssertFatal(err == gpuSuccess, "CUDA Error (c_devh[%d]): %s\n", i, gpuGetErrorString(err));
    }
    err = gpuMemcpy(c_dev, c_devh, 4 * sizeof(uint32_t*), gpuMemcpyHostToDevice);
    AssertFatal(err == gpuSuccess, "CUDA Error (memcpy c_devh -> c_dev): %s\n", gpuGetErrorString(err));
    err = gpuHostAlloc((void**)&d_host, 4 * sizeof(uint32_t*), gpuHostAllocMapped);
    AssertFatal(err == gpuSuccess, "CUDA Error (d_host): %s\n", gpuGetErrorString(err));
    err = gpuHostGetDevicePointer((void**)&d_dev, d_host, 0);
    AssertFatal(err == gpuSuccess, "CUDA Error gpuHostGetDevicePointer(d_dev): %s\n", gpuGetErrorString(err));
    LOG_I(NR_PHY, "d_host %p, d_dev %p\n", d_host, d_dev);
    for (int i = 0; i < MAX_SEGx32; i++) {
      err = gpuHostAlloc((void**)&d_host[i], 68 * 384 * sizeof(uint32_t), gpuHostAllocMapped);
      AssertFatal(err == gpuSuccess, "CUDA Error (d_host[%d]): %s\n", i, gpuGetErrorString(err));
      err = gpuHostGetDevicePointer((void**)&d_devh[i], d_host[i], 0);
      AssertFatal(err == gpuSuccess, "CUDA Error (gpuHostGetDevicePointer) d_devh[%d]: %s\n", i, gpuGetErrorString(err));
      LOG_I(NR_PHY, "d_host[%d] %p, d_devh[%d] %p\n", i, d_host[i], i, d_devh[i]);
    }
    err = gpuMemcpy(d_dev, d_devh, MAX_SEGx32 * sizeof(uint32_t*), gpuMemcpyHostToDevice);
    AssertFatal(err == gpuSuccess, "CUDA Error (memcpy d_devh -> d_dev): %s\n", gpuGetErrorString(err));
    err = gpuHostAlloc((void**)&input_host, 144 * sizeof(uint8_t*), gpuHostAllocMapped);
    AssertFatal(err == gpuSuccess, "CUDA Error (input_host): %s\n", gpuGetErrorString(err));
    err = gpuHostGetDevicePointer((void**)&input_dev, input_host, 0);
    AssertFatal(err == gpuSuccess, "CUDA Error gpuHostGetDevicePointer(cc_host): %s\n", gpuGetErrorString(err));
    LOG_I(NR_PHY, "input_host %p, input_dev %p\n", input_host, input_dev);
  }

  cuda_support_set = 1;
}

pthread_mutex_t encoder_mutex = PTHREAD_MUTEX_INITIALIZER;
uint32_t** LDPCencoder32(uint8_t** input, encoder_implemparams_t* impp)
{
  // set_log(PHY, 4);

  int Zc = impp->Zc;
  int Kb = impp->Kb;
  short block_length = impp->K;
  short BG = impp->BG;
  int ncols = 22;

  int encoder_stream = 0;

  AssertFatal(BG == 1, "BG %d is not supported for CUDA version\n", BG);
  AssertFatal(Zc == 384 || Zc == 176, "Zc %d is not supported for CUDA version \n", Zc);

  if (impp->tinput != NULL)
    start_meas(impp->tinput);

#ifdef DEBUG_LDPC
  LOG_I(PHY,
        "ldpc_encoder_cuda32: BG %d, Zc %d, Kb %d, block_length %d, segments %d\n",
        BG,
        Zc,
        Kb,
        block_length,
        impp->n_segments);
  LOG_I(PHY, "ldpc_encoder_cuda32: PDU (seg 0) %x %x %x %x\n", input[0][0], input[0][1], input[0][2], input[0][3]);
#endif

  int n_inputs = (impp->n_segments / 32) + (((impp->n_segments & 31) > 0) ? 1 : 0);
  //  uint32_t  cc[4][22*Zc]; //padded input, unpacked, max size

  int ret = pthread_mutex_lock(&encoder_mutex);
  AssertFatal(ret == 0, "pthread_mutex_lock(): ret %d, errno %d, %s\n", ret, errno, strerror(errno));
  if (!pageable && !integrated) { // this means we are not on shared memory
    for (int r = 0; r < impp->n_segments; r++) {
      gpuMemcpyAsync(input_devh[r], input[r], block_length >> 3, gpuMemcpyHostToDevice, encoderStreams[encoder_stream]);
    }
  }
  ldpc_input(pageable || integrated ? (uint32_t**)input : (uint32_t**)input_dev,
             (uint32_t**)c_dev,
             impp->n_segments,
             Zc,
             ncols,
             encoderStreams,
             encoder_stream);
  if (impp->tinput != NULL)
    stop_meas(impp->tinput);
  // parity check part
  if (impp->tparity != NULL)
    start_meas(impp->tparity);
  encode_parity_check_part_cuda((uint32_t**)c_dev, (uint32_t**)d_dev, BG, Zc, Kb, ncols, n_inputs, encoderStreams, encoder_stream);
  if (!pageable && !integrated) { // this means we are not on shared memory
    AssertFatal(n_inputs <= MAX_SEGx32, "d_devh only allocated till %d, but requested %d\n", MAX_SEGx32, n_inputs);
    for (int r = 0; r < n_inputs; r++)
      gpuMemcpyAsync(d_host[r], d_devh[r], 68 * Zc * sizeof(uint32_t), gpuMemcpyDeviceToHost, encoderStreams[encoder_stream]);
  }
  gpuStreamSynchronize(encoderStreams[encoder_stream]);
  if (impp->tparity != NULL)
    stop_meas(impp->tparity);
  ret = pthread_mutex_unlock(&encoder_mutex);
  AssertFatal(ret == 0, "pthread_mutex_unlock(): ret %d, errno %d, %s\n", ret, errno, strerror(errno));

  return d_host;
}
