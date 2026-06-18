/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file ldpc_encoder_cuda32.c
 * \brief Defines the optimized LDPC encoder for NVidia GPUs
 * \email openair_tech@eurecom.fr
 * \date 11-30-2025
 * \version 1.0
 * \note
 * \warning
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

#include <cuda_runtime.h>

//#define DEBUG_LDPC 1

#include "ldpc_encode_parity_check_cuda.c"

uint32_t *c_dev;
uint32_t **c_host;
uint32_t *c_devh[4];
uint32_t *d_dev;
uint32_t **d_host;
uint32_t *d_devh[4];
uint32_t *input_dev;
uint32_t **input_host;
uint32_t *input_devh[128];
int managed = 0, concurrent = 0, uva = 0, pageable = 0, pageable_uses_host = 0, register_host = 0, integrated = 0;

int cuda_support_set = 0;

extern cudaStream_t encoderStreams[4];

int ldpc_input(uint32_t **input,uint32_t *cc[4],int nseg,cudaStream_t *s,int sidx);

void cuda_support_init() {

    int dev = 0;
    struct cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, dev);


    cudaDeviceGetAttribute(&managed, cudaDevAttrManagedMemory, dev);
    cudaDeviceGetAttribute(&concurrent, cudaDevAttrConcurrentManagedAccess, dev);
    cudaDeviceGetAttribute(&uva, cudaDevAttrUnifiedAddressing, dev);
    cudaDeviceGetAttribute(&pageable, cudaDevAttrPageableMemoryAccess, dev);
    cudaDeviceGetAttribute(&pageable_uses_host, cudaDevAttrPageableMemoryAccessUsesHostPageTables, dev);
    cudaDeviceGetAttribute(&register_host, cudaDevAttrHostRegisterSupported,dev);
    cudaDeviceGetAttribute(&integrated, cudaDevAttrIntegrated,dev);

    LOG_I(NR_PHY,"Device: %s (cc %d.%d)\n", prop.name, prop.major, prop.minor);
    LOG_I(NR_PHY,"Unified Virtual Addressing (UVA): %s\n", uva ? "YES" : "NO");
    LOG_I(NR_PHY,"Managed (Unified) Memory:        %s\n", managed ? "YES" : "NO");
    LOG_I(NR_PHY,"Concurrent managed access:       %s\n", concurrent ? "YES" : "NO");
    LOG_I(NR_PHY,"Pageable memory access:          %s\n", pageable ? "YES" : "NO");
    LOG_I(NR_PHY,"Uses host page tables:           %s\n", pageable_uses_host ? "YES" : "NO");
    LOG_I(NR_PHY,"Host Register supported:         %s\n", register_host ? "YES" : "NO");
    LOG_I(NR_PHY,"Integrated (shared) Memory       %s\n", integrated ? "YES" : "NO");

  if (!pageable && !integrated) {
    LOG_I(NR_PHY,"Allocating c,d,cc arrays for GPU \n");
    cudaError_t err=cudaMalloc((void **)&c_dev,4*sizeof(uint32_t*));
    AssertFatal(err == cudaSuccess,"CUDA Error (c_dev): %s\n", cudaGetErrorString(err));
    err=cudaHostAlloc((void **)&c_host,4*sizeof(uint32_t*),cudaHostAllocDefault);
    AssertFatal(err == cudaSuccess,"CUDA Error (c_host): %s\n", cudaGetErrorString(err));
    for (int i=0;i<4;i++) {
      err=cudaMalloc((void**)&c_devh[i],2*22*384*sizeof(uint32_t));
      AssertFatal(err == cudaSuccess,"CUDA Error (c_devh[%d]): %s\n", i,cudaGetErrorString(err));
      err=cudaHostAlloc((void**)&c_host[i],2*22*384*sizeof(uint32_t),cudaHostAllocDefault);
      AssertFatal(err == cudaSuccess,"CUDA Error (chost[%d]): %s\n", i,cudaGetErrorString(err));
    }
    err = cudaMemcpy(c_dev,c_devh,4*sizeof(uint32_t*),cudaMemcpyHostToDevice);
    AssertFatal(err == cudaSuccess,"CUDA Error (memcpy c_devh -> c_dev): %s\n", cudaGetErrorString(err));
    err=cudaMalloc((void**)&d_dev,4*sizeof(uint32_t*));
    AssertFatal(err == cudaSuccess,"CUDA Error: %s\n", cudaGetErrorString(err));
    err=cudaHostAlloc((void **)&d_host,4*sizeof(uint32_t*),cudaHostAllocDefault);
    AssertFatal(err == cudaSuccess,"CUDA Error (d_host): %s\n", cudaGetErrorString(err));
    for (int i=0;i<4;i++) {
      err=cudaMalloc((void**)&d_devh[i],68*384*sizeof(uint32_t));
      AssertFatal(err == cudaSuccess,"CUDA Error (d_devh[%d]: %s\n", i,cudaGetErrorString(err));
      err=cudaHostAlloc((void**)&d_host[i],68*384*sizeof(uint32_t),cudaHostAllocDefault);
      AssertFatal(err == cudaSuccess,"CUDA Error (d_host[%d]): %s\n", i,cudaGetErrorString(err));
    }
    err=cudaMemcpy(d_dev,d_devh,4*sizeof(uint32_t*),cudaMemcpyHostToDevice);
    AssertFatal(err == cudaSuccess,"CUDA Error (memcpy d_devh -> d_dev): %s\n", cudaGetErrorString(err));
    err=cudaMalloc((void**)&input_dev,128*sizeof(uint8_t*));
    AssertFatal(err == cudaSuccess,"CUDA Error: %s\n", cudaGetErrorString(err));
    err=cudaHostAlloc((void **)&input_host,128*sizeof(uint8_t*),cudaHostAllocDefault);
    AssertFatal(err == cudaSuccess,"CUDA Error (cc_host): %s\n", cudaGetErrorString(err));
    for (int i=0;i<128;i++) {
      err=cudaMalloc((void**)&input_devh[i],(8448/8)*sizeof(uint8_t));
      AssertFatal(err == cudaSuccess,"CUDA Error (input_devh[%d]: %s\n", i,cudaGetErrorString(err));
      err=cudaHostAlloc((void**)&input_host[i],(8448/8)*sizeof(uint8_t),cudaHostAllocDefault);
      AssertFatal(err == cudaSuccess,"CUDA Error (input_host[%d]): %s\n", i,cudaGetErrorString(err));
    }
    err=cudaMemcpy(input_dev,input_devh,128*sizeof(uint8_t*),cudaMemcpyHostToDevice);
    AssertFatal(err == cudaSuccess,"CUDA Error (memcpy cc_devh -> d_dev): %s\n", cudaGetErrorString(err));
  }
  else {
    LOG_I(NR_PHY,"Allocating c,d,cc arrays for CPU/GPU shared-memory\n");
    cudaError_t err=cudaHostAlloc((void **)&c_host,4*sizeof(uint32_t*),cudaHostAllocMapped|cudaHostAllocPortable);
    AssertFatal(err == cudaSuccess,"CUDA Error (c_host): %s\n", cudaGetErrorString(err));
    err = cudaHostGetDevicePointer((void**)&c_dev, c_host, 0);
    AssertFatal(err == cudaSuccess,"CUDA Error (c_dev): %s\n", cudaGetErrorString(err));
    LOG_I(NR_PHY,"c_host %p, c_dev %p\n",c_host,c_dev);
    for (int i=0;i<4;i++) {
      err=cudaHostAlloc((void**)&c_host[i],2*22*384*sizeof(uint32_t),cudaHostAllocMapped);
      AssertFatal(err == cudaSuccess,"CUDA Error (c_host[%d]): %s\n", i,cudaGetErrorString(err));
      err = cudaHostGetDevicePointer((void**)&c_devh[i], c_host[i], 0);
      AssertFatal(err == cudaSuccess,"CUDA Error (c_devh[%d]): %s\n", i,cudaGetErrorString(err));
    }
    err=cudaMemcpy(c_dev,c_devh,4*sizeof(uint32_t*),cudaMemcpyHostToDevice);
    AssertFatal(err == cudaSuccess,"CUDA Error (memcpy c_devh -> c_dev): %s\n", cudaGetErrorString(err));
    err=cudaHostAlloc((void **)&d_host,4*sizeof(uint32_t*),cudaHostAllocMapped);
    AssertFatal(err == cudaSuccess,"CUDA Error (d_host): %s\n", cudaGetErrorString(err));
    err=cudaHostGetDevicePointer((void**)&d_dev, d_host, 0);
    AssertFatal(err == cudaSuccess,"CUDA Error cudaHostGetDevicePointer(d_dev): %s\n", cudaGetErrorString(err));
    LOG_I(NR_PHY,"d_host %p, d_dev %p\n",d_host,d_dev);
    for (int i=0;i<4;i++) {
      err=cudaHostAlloc((void**)&d_host[i],68*384*sizeof(uint32_t),cudaHostAllocMapped);
      AssertFatal(err == cudaSuccess,"CUDA Error (d_host[%d]): %s\n", i,cudaGetErrorString(err));
      err=cudaHostGetDevicePointer((void**)&d_devh[i], d_host[i], 0);
      AssertFatal(err == cudaSuccess,"CUDA Error (cudaHostGetDevicePointer) d_devh[%d]: %s\n", i,cudaGetErrorString(err));
      LOG_I(NR_PHY,"d_host[%d] %p, d_devh[%d] %p\n",i,d_host[i],i,d_devh[i]);
    }
    err=cudaMemcpy(d_dev,d_devh,4*sizeof(uint32_t*),cudaMemcpyHostToDevice);
    AssertFatal(err == cudaSuccess,"CUDA Error (memcpy d_devh -> d_dev): %s\n", cudaGetErrorString(err));
    err=cudaHostAlloc((void **)&input_host,128*sizeof(uint8_t*),cudaHostAllocMapped);
    AssertFatal(err == cudaSuccess,"CUDA Error (input_host): %s\n", cudaGetErrorString(err));
    err=cudaHostGetDevicePointer((void**)&input_dev, input_host, 0);
    AssertFatal(err == cudaSuccess,"CUDA Error cudaHostGetDevicePointer(cc_host): %s\n", cudaGetErrorString(err));
    LOG_I(NR_PHY,"input_host %p, input_dev %p\n",input_host,input_dev);
  }


  cuda_support_set=1;
}

uint32_t **LDPCencoder32(uint8_t **input, encoder_implemparams_t *impp)
{
  //set_log(PHY, 4);

  int Zc = impp->Zc;
  int Kb = impp->Kb;
  short block_length = impp->K;
  short BG = impp->BG;
  int ncols=22;

  int encoder_stream=0;

  AssertFatal(BG==1,"BG %d is not supported for CUDA version\n",BG);
  AssertFatal(Zc==384,"Zc %d is not supported for CUDA version \n", Zc);
 
  if(impp->tinput != NULL) start_meas(impp->tinput);

#ifdef DEBUG_LDPC
  LOG_I(PHY,"ldpc_encoder_cuda32: BG %d, Zc %d, Kb %d, block_length %d, segments %d\n",BG,Zc,Kb,block_length,impp->n_segments);
  LOG_I(PHY,"ldpc_encoder_cuda32: PDU (seg 0) %x %x %x %x\n",input[0][0],input[0][1],input[0][2],input[0][3]);
#endif

  int n_inputs = (impp->n_segments/32)+(((impp->n_segments&31) > 0) ? 1: 0);
//  uint32_t  cc[4][22*Zc]; //padded input, unpacked, max size

  if (!pageable&&!integrated) { // this means we are not on shared memory
    for (int r=0;r<impp->n_segments;r++) {
        cudaMemcpyAsync(input_devh[r],input[r],block_length>>3,cudaMemcpyHostToDevice,encoderStreams[encoder_stream]);
    }
  }
  ldpc_input(pageable||integrated? (uint32_t**)input : (uint32_t**)input_dev,(uint32_t**)c_dev,impp->n_segments,encoderStreams,encoder_stream);
  if(impp->tinput != NULL) stop_meas(impp->tinput);
  //parity check part
  if(impp->tparity != NULL) start_meas(impp->tparity);
  encode_parity_check_part_cuda((uint32_t**)c_dev, (uint32_t**)d_dev, BG, Zc, Kb, ncols,n_inputs,encoderStreams,encoder_stream);
  if (!pageable&&!integrated) { // this means we are not on shared memory
     for (int r=0; r<n_inputs;r++) cudaMemcpyAsync(d_host[r],d_devh[r],68*384*sizeof(uint32_t),cudaMemcpyDeviceToHost,encoderStreams[encoder_stream]);  
  }
  cudaStreamSynchronize(encoderStreams[encoder_stream]);
  if(impp->tparity != NULL) stop_meas(impp->tparity);
  
  return d_host;
}

