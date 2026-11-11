/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Parity check function used by ldpc encoders
 */

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "PHY/gpu_compat.h"



int ldpc_BG1_Zc176_cuda32(uint32_t **c,uint32_t **d,int n_inputs, gpuStream_t *stream,int sidx);
int ldpc_BG1_Zc192_cuda32(uint32_t **c,uint32_t **d,int n_inputs, gpuStream_t *stream,int sidx);
int ldpc_BG1_Zc208_cuda32(uint32_t **c,uint32_t **d,int n_inputs, gpuStream_t *stream,int sidx);
int ldpc_BG1_Zc224_cuda32(uint32_t **c,uint32_t **d,int n_inputs, gpuStream_t *stream,int sidx);
int ldpc_BG1_Zc240_cuda32(uint32_t **c,uint32_t **d,int n_inputs, gpuStream_t *stream,int sidx);
int ldpc_BG1_Zc256_cuda32(uint32_t **c,uint32_t **d,int n_inputs, gpuStream_t *stream,int sidx);
int ldpc_BG1_Zc288_cuda32(uint32_t **c,uint32_t **d,int n_inputs, gpuStream_t *stream,int sidx);
int ldpc_BG1_Zc320_cuda32(uint32_t **c,uint32_t **d,int n_inputs, gpuStream_t *stream,int sidx);
int ldpc_BG1_Zc352_cuda32(uint32_t **c,uint32_t **d,int n_inputs, gpuStream_t *stream,int sidx);
int ldpc_BG1_Zc384_cuda32(uint32_t **c,uint32_t **d,int n_inputs, gpuStream_t *stream,int sidx);

void encode_parity_check_part_cuda(uint32_t **c,
                                   uint32_t **d,
                                   short BG,
                                   short Zc,
                                   short Kb,
                                   int ncols,
                                   int n_inputs,
                                   gpuStream_t *stream,
                                   int sidx)
{
  if (BG == 1) {
    switch (Zc) {
      case 176:
        ldpc_BG1_Zc176_cuda32(c, d, n_inputs, stream, sidx);
        break;
      case 192:
        ldpc_BG1_Zc192_cuda32(c, d, n_inputs, stream, sidx);
        break;
      case 208:
        ldpc_BG1_Zc208_cuda32(c, d, n_inputs, stream, sidx);
        break;
      case 224:
        ldpc_BG1_Zc224_cuda32(c, d, n_inputs, stream, sidx);
        break;
      case 240:
        ldpc_BG1_Zc240_cuda32(c, d, n_inputs, stream, sidx);
        break;
      case 256:
        ldpc_BG1_Zc256_cuda32(c, d, n_inputs, stream, sidx);
        break;
      case 288:
        ldpc_BG1_Zc288_cuda32(c, d, n_inputs, stream, sidx);
        break;
      case 320:
        ldpc_BG1_Zc320_cuda32(c, d, n_inputs, stream, sidx);
        break;
      case 352:
        ldpc_BG1_Zc352_cuda32(c, d, n_inputs, stream, sidx);
        break;
      case 384:
        ldpc_BG1_Zc384_cuda32(c, d, n_inputs, stream, sidx);
        break;
      default:
        AssertFatal(false, "BG %d Zc %d is not supported yet\n", BG, Zc);
    }
  } else if (BG == 2) {
    switch (Zc) {
      case 72:
      case 80:
      case 88:
      case 96:
      case 104:
      case 112:
      case 120:
      case 128:
      case 144:
      case 160:
      case 176:
      case 192:
      case 208:
      case 224:
      case 240:
      case 256:
      case 288:
      case 320:
      case 352:
      case 384:
      default:
        AssertFatal(false, "BG %d Zc %d is not supported yet\n", BG, Zc);
    }
  } else
    AssertFatal(false, "BG %d is not supported\n", BG);
}
