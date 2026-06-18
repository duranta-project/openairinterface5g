/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
/*! \file nrLDPC_CUDA_mPassKernel.h
 * \brief Defines the kernels for message passing in CUDA version of LDPC decoder
 * \author Qizhi Pan, Raymond Knopp
 * \company EURECOM
 * \email: qizhi.pan@eurecom.fr, raymond.knopp@eurecom.fr
 * \date 2025-12-30
 * \version 1.0
 * \note
 * \warning
 */
#pragma once

#include <cuda_runtime.h>
#include <stdint.h>
#include <stdio.h>
#include "nrLDPC_CUDA_public.h"
#include "nrLDPC_CUDA_shared_param.h"

__device__ __forceinline__ void llrPreProc_Kernel_BG1_int8_Gn_stream(const int8_t *p_llr,
                                                                     int8_t *p_llrProcBuf,
                                                                     int8_t *p_cnProcBuf,
                                                                     uint32_t MsgIdx,
                                                                     uint32_t lane,
                                                                     uint32_t colIdx,
                                                                     uint32_t idxBn,
                                                                     uint32_t GrpIdx,
                                                                     uint32_t circShift,
                                                                     uint32_t Zc,
                                                                     uint32_t R)
{
  {
    uint32_t *p_cnProcBufBit;

    uint8_t bricksLocal[4];
    uint8_t *BricksToBeMoved = bricksLocal;

    p_cnProcBufBit = (uint32_t *)(p_cnProcBuf + d_lut_numCnInCnGroups_BG1_R13[GrpIdx] * NR_LDPC_ZMAX * MsgIdx + lane * 4);

    moveBricks_invget_circ((int8_t *)&p_llr[idxBn * Zc], lane * 4, BricksToBeMoved, Zc, circShift);

    *p_cnProcBufBit = *(uint32_t *)BricksToBeMoved;
  }

  if (colIdx >= 68)
    return;

  const uint8_t numBn2CnG1 =
      (R == 13) ? d_lut_numBnInBnGroups_BG1_R13[0]
                : ((R == 89) ? d_lut_numBnInBnGroups_BG1_R89[0] : d_lut_numBnInBnGroups_BG1_R23[0]); // for R13 is 42
  const uint32_t startColParity = NR_LDPC_START_COL_PARITY_BG1; // 26 for BG1
  const uint32_t colG1 = startColParity * Zc;

  const uint32_t *lut_llr2llrProcBufAddr =
      (R == 13) ? d_llr2llrProcBufAddr_BG1_R13 : ((R == 89) ? d_llr2llrProcBufAddr_BG1_R89 : d_llr2llrProcBufAddr_BG1_R23);
  const uint32_t *lut_llr2llrProcBufBnPos =
      (R == 13) ? d_llr2llrProcBufBnPos_BG1_R13 : ((R == 89) ? d_llr2llrProcBufBnPos_BG1_R89 : d_llr2llrProcBufBnPos_BG1_R23);

  if (colIdx < startColParity) {
    const uint32_t idxBn = lut_llr2llrProcBufAddr[colIdx] + lut_llr2llrProcBufBnPos[colIdx] * NR_LDPC_ZMAX;
    int32_t *dst = (int32_t *)(&p_llrProcBuf[idxBn] + lane * 4);
    int32_t *src = (int32_t *)(&p_llr[colIdx * Zc] + lane * 4);
    *dst = *src;
  } else {
    colIdx = colIdx - startColParity;
    if (numBn2CnG1 > 0 && colIdx < numBn2CnG1) {
      int32_t *dst = (int32_t *)(&p_llrProcBuf[colIdx * NR_LDPC_ZMAX] + lane * 4);
      int32_t *src = (int32_t *)(&p_llr[colG1 + colIdx * Zc] + lane * 4);
      *dst = *src;
    }
  }
}

__device__ void llr2bit_Kernel_BG1_int8(uint32_t R,
                                        uint8_t *__restrict__ out,
                                        const int8_t *__restrict__ llrRes,
                                        uint32_t numLLR,
                                        uint32_t Zc)
{
  uint32_t lane = threadIdx.x;
  uint32_t outColIdx = (blockIdx.x << 2) + threadIdx.y;

  if (outColIdx >= num_TotalBlocks_llr_llrRes)
    return;

  // Constants Setup
  const uint8_t numBn2CnG1 = (R == 13) ? d_lut_numBnInBnGroups_BG1_R13[0]
                                       : ((R == 89) ? d_lut_numBnInBnGroups_BG1_R89[0] : d_lut_numBnInBnGroups_BG1_R23[0]);
  const uint32_t startColParity = NR_LDPC_START_COL_PARITY_BG1;
  const uint32_t *lut_Addr =
      (R == 13) ? d_llr2llrProcBufAddr_BG1_R13 : ((R == 89) ? d_llr2llrProcBufAddr_BG1_R89 : d_llr2llrProcBufAddr_BG1_R23);
  const uint32_t *lut_Pos =
      (R == 13) ? d_llr2llrProcBufBnPos_BG1_R13 : ((R == 89) ? d_llr2llrProcBufBnPos_BG1_R89 : d_llr2llrProcBufBnPos_BG1_R23);

  int32_t raw_llrs;

  // Load Data: Handle Systematic (Scatter/Gather) vs Parity (Linear) mapping
  if (outColIdx < startColParity) {
    uint32_t idxBn = lut_Addr[outColIdx] + lut_Pos[outColIdx] * NR_LDPC_ZMAX;
    raw_llrs = *(const int32_t *)(&llrRes[idxBn] + lane * 4);
  } else {
    uint32_t srcParityIdx = outColIdx - startColParity;
    if (numBn2CnG1 > 0 && outColIdx < numBn2CnG1) {
      raw_llrs = *(const int32_t *)(llrRes + srcParityIdx * NR_LDPC_ZMAX + lane * 4);
    } else {
      raw_llrs = 0;
    }
  }

  // Hard Decision: Convert 4 int8 LLRs -> 4 bytes (0 or 1)
  int8_t *p_val = (int8_t *)&raw_llrs;
  uint32_t my_word = 0;

#pragma unroll
  for (int i = 0; i < 4; i++) {
    // Hard decision: LLR < 0 implies bit 1
    uint32_t byte_val = (p_val[i] < 0) ? 1 : 0;
    my_word |= (byte_val << (i * 8));
  }

  // Store output linearly
  *(uint32_t *)(&out[outColIdx * Zc + lane * 4]) = my_word;
}

__device__ void llr2bitPacked_Kernel_BG1_int8(uint32_t R,
                                              uint8_t *__restrict__ out,
                                              const int8_t *__restrict__ llrRes,
                                              uint32_t numLLR,
                                              uint32_t Zc)
{
  uint32_t lane = threadIdx.x;
  uint32_t outColIdx = (blockIdx.x << 2) + threadIdx.y;

  if (outColIdx >= num_TotalBlocks_llr_llrRes)
    return;

  const uint8_t numBn2CnG1 = (R == 13) ? d_lut_numBnInBnGroups_BG1_R13[0]
                                       : ((R == 89) ? d_lut_numBnInBnGroups_BG1_R89[0] : d_lut_numBnInBnGroups_BG1_R23[0]);
  const uint32_t startColParity = NR_LDPC_START_COL_PARITY_BG1;
  const uint32_t *lut_Addr =
      (R == 13) ? d_llr2llrProcBufAddr_BG1_R13 : ((R == 89) ? d_llr2llrProcBufAddr_BG1_R89 : d_llr2llrProcBufAddr_BG1_R23);
  const uint32_t *lut_Pos =
      (R == 13) ? d_llr2llrProcBufBnPos_BG1_R13 : ((R == 89) ? d_llr2llrProcBufBnPos_BG1_R89 : d_llr2llrProcBufBnPos_BG1_R23);

  int32_t raw_llrs;

  // Load Data: Handle Systematic vs Parity mapping
  if (outColIdx < startColParity) {
    uint32_t idxBn = lut_Addr[outColIdx] + lut_Pos[outColIdx] * NR_LDPC_ZMAX;
    raw_llrs = *(const int32_t *)(&llrRes[idxBn] + lane * 4);
  } else {
    uint32_t srcParityIdx = outColIdx - startColParity;
    if (numBn2CnG1 > 0 && outColIdx < numBn2CnG1) {
      raw_llrs = *(const int32_t *)(llrRes + srcParityIdx * NR_LDPC_ZMAX + lane * 4);
    } else {
      raw_llrs = 0;
    }
  }

  // Extract 4 bits from 4 LLRs (Local Packing)
  // Assuming MSB order within nibble: [3][2][1][0]
  uint32_t my_4_bits = 0;
  int8_t *p_val = (int8_t *)&raw_llrs;

#pragma unroll
  for (int i = 0; i < 4; i++) {
    if (p_val[i] < 0) {
      my_4_bits |= (1 << (3 - i));
    }
  }

  // Thread Cooperation: Pair threads to pack 8 bits (1 byte)
  // Even thread (0,2..) takes low nibble, Odd thread (1,3..) takes high nibble.
  uint32_t neighbor_bits = __shfl_xor_sync(0xffffffff, my_4_bits, 1);

  if ((lane & 1) == 0) {
    // Combine: [Odd Thread Bits (High)] | [My Bits (Low)]
    uint8_t packed_byte = (neighbor_bits & 0xF) | ((my_4_bits & 0xF) << 4);

    // Output addr: lane steps by 4 LLRs, but we output 1 byte per 8 LLRs -> lane >> 1
    uint32_t outAddr = outColIdx * (Zc >> 3) + (lane >> 1);
    out[outAddr] = packed_byte;
  }
}
