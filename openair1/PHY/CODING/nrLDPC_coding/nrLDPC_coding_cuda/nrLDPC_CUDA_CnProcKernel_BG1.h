/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
/*! \file nrLDPC_CUDA_CnProcKernel_BG1.h
 * \brief Defines the kernels for check node processing
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
#include "nrLDPC_CUDA_lut.h"

__device__ __forceinline__ void cnProcKernel_BG1_int8_Gn_R13_node(const int8_t *__restrict__ d_cnProcBuf,
                                                                  int8_t *__restrict__ d_bnProcBuf,
                                                                  uint32_t lane,
                                                                  uint32_t CnIdx,
                                                                  uint32_t CnNumInGrp,
                                                                  uint32_t CnGrpIdxNum,
                                                                  uint32_t Cn2MsgStartIdx,
                                                                  uint32_t Zc,
                                                                  uint32_t ZcIdx)
{
  uint32_t min1 = 0x7F7F7F7F;
  uint32_t min2 = 0x7F7F7F7F;
  uint32_t total_xor = 0;

  uint32_t cache_raw[19];
  uint32_t cache_abs[19];

  const int32_t *cnProcBufPtr = (const int32_t *)(d_cnProcBuf) + lane;
  const int32_t *currPtr = cnProcBufPtr;
  uint32_t offset = (CnNumInGrp * NR_LDPC_ZMAX) >> 2;
#pragma unroll
  for (int MsgIdx = 0; MsgIdx < CnGrpIdxNum; MsgIdx++) {
    uint32_t val = *currPtr;

    cache_raw[MsgIdx] = val;
    uint32_t v_abs = __vabs4(val);
    cache_abs[MsgIdx] = v_abs;

    total_xor = __vxor4(total_xor, val);

    uint32_t old_min1 = min1;

    min1 = __vminu4(old_min1, v_abs);

    uint32_t candidate = __vmaxu4(old_min1, v_abs);

    min2 = __vminu4(min2, candidate);
    currPtr += offset;
  }

#pragma unroll
  for (int temp_MsgIdx = 0; temp_MsgIdx < CnGrpIdxNum; temp_MsgIdx++) {
    uint32_t target_sign = __vxor4(total_xor, cache_raw[temp_MsgIdx]);

    uint32_t my_abs = cache_abs[temp_MsgIdx];

    uint32_t is_min_mask = __vcmpeq4(my_abs, min1);

    uint32_t final_mag = (min2 & is_min_mask) | (min1 & ~is_min_mask);

    // final_mag = scale_int8x4(final_mag, 0.75);

    uint32_t BricksToBeMoved = __vsign4(final_mag, target_sign);

    uint32_t MsgIdx = Cn2MsgStartIdx + temp_MsgIdx;
    uint32_t circShift = cn_bn_map_BG1_Z_R13[MsgIdx][ZcIdx];
    int8_t *p_bnProcBuf = (int8_t *)(d_bnProcBuf + cn_bn_map_BG1_Z_R13[MsgIdx][0]);

    moveBricks_invput_circ(p_bnProcBuf, lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
  }
}
__device__ __forceinline__ void cnProcKernel_BG1_int8_Gn_R23_node(const int8_t *__restrict__ d_cnProcBuf,
                                                                  int8_t *__restrict__ d_bnProcBuf,
                                                                  uint32_t lane,
                                                                  uint32_t CnIdx,
                                                                  uint32_t CnNumInGrp,
                                                                  uint32_t CnGrpIdxNum,
                                                                  uint32_t Cn2MsgStartIdx,
                                                                  uint32_t Zc,
                                                                  uint32_t ZcIdx)
{

  uint32_t min1 = 0x7F7F7F7F;
  uint32_t min2 = 0x7F7F7F7F;
  uint32_t total_xor = 0;

  uint32_t cache_raw[19];
  uint32_t cache_abs[19];

  const int32_t *cnProcBufPtr = (const int32_t *)(d_cnProcBuf) + lane;
  const int32_t *currPtr = cnProcBufPtr;
  uint32_t offset = (CnNumInGrp * NR_LDPC_ZMAX) >> 2;
#pragma unroll
  for (int MsgIdx = 0; MsgIdx < CnGrpIdxNum; MsgIdx++) {
    uint32_t val = *currPtr;

    cache_raw[MsgIdx] = val;
    uint32_t v_abs = __vabs4(val);
    cache_abs[MsgIdx] = v_abs;

    total_xor = __vxor4(total_xor, val);

    uint32_t old_min1 = min1;

    min1 = __vminu4(old_min1, v_abs);

    uint32_t candidate = __vmaxu4(old_min1, v_abs);

    min2 = __vminu4(min2, candidate);
    currPtr += offset;
  }

#pragma unroll
  for (int temp_MsgIdx = 0; temp_MsgIdx < CnGrpIdxNum; temp_MsgIdx++) {
    uint32_t target_sign = __vxor4(total_xor, cache_raw[temp_MsgIdx]);

    uint32_t my_abs = cache_abs[temp_MsgIdx];

    uint32_t is_min_mask = __vcmpeq4(my_abs, min1);

    uint32_t final_mag = (min2 & is_min_mask) | (min1 & ~is_min_mask);

    // final_mag = scale_int8x4(final_mag, 0.75);

    uint32_t BricksToBeMoved = __vsign4(final_mag, target_sign);

    uint32_t MsgIdx = Cn2MsgStartIdx + temp_MsgIdx;
    uint32_t circShift = cn_bn_map_BG1_Z_R23[MsgIdx][ZcIdx];
    int8_t *p_bnProcBuf = (int8_t *)(d_bnProcBuf + cn_bn_map_BG1_Z_R23[MsgIdx][0]);

    moveBricks_invput_circ(p_bnProcBuf, lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
  }
}
__device__ __forceinline__ void cnProcKernel_BG1_int8_Gn_R89_node(const int8_t *__restrict__ d_cnProcBuf,
                                                                  int8_t *__restrict__ d_bnProcBuf,
                                                                  uint32_t lane,
                                                                  uint32_t CnIdx,
                                                                  uint32_t CnNumInGrp,
                                                                  uint32_t CnGrpIdxNum,
                                                                  uint32_t Cn2MsgStartIdx,
                                                                  uint32_t Zc,
                                                                  uint32_t ZcIdx)
{

  uint32_t min1 = 0x7F7F7F7F;
  uint32_t min2 = 0x7F7F7F7F;
  uint32_t total_xor = 0;

  uint32_t cache_raw[19];
  uint32_t cache_abs[19];

  const int32_t *cnProcBufPtr = (const int32_t *)(d_cnProcBuf) + lane;
  const int32_t *currPtr = cnProcBufPtr;
  uint32_t offset = (CnNumInGrp * NR_LDPC_ZMAX) >> 2;
#pragma unroll
  for (int MsgIdx = 0; MsgIdx < CnGrpIdxNum; MsgIdx++) {
    uint32_t val = *currPtr;

    cache_raw[MsgIdx] = val;
    uint32_t v_abs = __vabs4(val);
    cache_abs[MsgIdx] = v_abs;

    total_xor = __vxor4(total_xor, val);

    uint32_t old_min1 = min1;

    min1 = __vminu4(old_min1, v_abs);

    uint32_t candidate = __vmaxu4(old_min1, v_abs);

    min2 = __vminu4(min2, candidate);
    currPtr += offset;
  }

#pragma unroll
  for (int temp_MsgIdx = 0; temp_MsgIdx < CnGrpIdxNum; temp_MsgIdx++) {
    uint32_t target_sign = __vxor4(total_xor, cache_raw[temp_MsgIdx]);

    uint32_t my_abs = cache_abs[temp_MsgIdx];

    uint32_t is_min_mask = __vcmpeq4(my_abs, min1);

    uint32_t final_mag = (min2 & is_min_mask) | (min1 & ~is_min_mask);

    // final_mag = scale_int8x4(final_mag, 0.75);

    uint32_t BricksToBeMoved = __vsign4(final_mag, target_sign);

    uint32_t MsgIdx = Cn2MsgStartIdx + temp_MsgIdx;
    uint32_t circShift = cn_bn_map_BG1_Z_R89[MsgIdx][ZcIdx];
    int8_t *p_bnProcBuf = (int8_t *)(d_bnProcBuf + cn_bn_map_BG1_Z_R89[MsgIdx][0]);

    moveBricks_invput_circ(p_bnProcBuf, lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
  }
}
__device__ __forceinline__ void cnProcKernel_BG1_int8_G3(const int8_t *__restrict__ p_cnProcBuf,
                                                         int8_t *__restrict__ p_bnProcBuf,
                                                         uint32_t row,
                                                         uint32_t lane,
                                                         uint32_t idxBn,
                                                         uint32_t circShift,
                                                         uint32_t Zc)
{
  uint32_t ymm0, sgn, min;
  const uint32_t ones = 0x01010101;

  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG3[row][0] * 4);
  sgn = __vxor4(ones, ymm0);
  min = __vabs4(ymm0);

  // loop starts here
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG3[row][1] * 4);

  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);

  uint32_t BricksToBeMoved = __vsign4(min, sgn);

  moveBricks_invput_circ((int8_t *)&p_bnProcBuf[idxBn], lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
}

__device__ __forceinline__ void cnProcKernel_BG1_int8_G4(const int8_t *__restrict__ p_cnProcBuf,
                                                         int8_t *__restrict__ p_bnProcBuf,
                                                         uint32_t row,
                                                         uint32_t lane,
                                                         uint32_t idxBn,
                                                         uint32_t circShift,
                                                         uint32_t Zc)
{
  uint32_t ymm0, sgn, min;
  const uint32_t ones = 0x01010101;

  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG4[row][0] * 4);

  sgn = __vxor4(ones, ymm0);
  min = __vabs4(ymm0);
  //-------------------------loop starts here-------------------------------
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG4[row][1] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG4[row][2] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  //-------------------------------------------------------------------------

  uint32_t BricksToBeMoved = __vsign4(min, sgn);

  moveBricks_invput_circ((int8_t *)&p_bnProcBuf[idxBn], lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
}

__device__ __forceinline__ void cnProcKernel_BG1_int8_G5(const int8_t *__restrict__ p_cnProcBuf,
                                                         int8_t *__restrict__ p_bnProcBuf,
                                                         uint32_t row,
                                                         uint32_t lane,
                                                         uint32_t idxBn,
                                                         uint32_t circShift,
                                                         uint32_t Zc)
{
  uint32_t ymm0, sgn, min;
  const uint32_t ones = 0x01010101;

  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG5[row][0] * 4);
  sgn = __vxor4(ones, ymm0);
  min = __vabs4(ymm0);

  //-------------------------loop starts here-------------------------------
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG5[row][1] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG5[row][2] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG5[row][3] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  //-------------------------------------------------------------------------
  uint32_t BricksToBeMoved = __vsign4(min, sgn);

  moveBricks_invput_circ((int8_t *)&p_bnProcBuf[idxBn], lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
}

__device__ __forceinline__ void cnProcKernel_BG1_int8_G6(const int8_t *__restrict__ p_cnProcBuf,
                                                         int8_t *__restrict__ p_bnProcBuf,
                                                         uint32_t row,
                                                         uint32_t lane,
                                                         uint32_t idxBn,
                                                         uint32_t circShift,
                                                         uint32_t Zc)
{
  uint32_t ymm0, sgn, min;
  const uint32_t ones = 0x01010101;

  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG6[row][0] * 4);
  sgn = __vxor4(ones, ymm0);
  min = __vabs4(ymm0);

  //-------------------------loop starts here-------------------------------
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG6[row][1] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG6[row][2] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG6[row][3] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG6[row][4] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  //-------------------------------------------------------------------------

  uint32_t BricksToBeMoved = __vsign4(min, sgn);

  moveBricks_invput_circ((int8_t *)&p_bnProcBuf[idxBn], lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
}

__device__ __forceinline__ void cnProcKernel_BG1_int8_G7(const int8_t *__restrict__ p_cnProcBuf,
                                                         int8_t *__restrict__ p_bnProcBuf,
                                                         uint32_t row,
                                                         uint32_t lane,
                                                         uint32_t idxBn,
                                                         uint32_t circShift,
                                                         uint32_t Zc)
{
  uint32_t ymm0, sgn, min;
  const uint32_t ones = 0x01010101;

  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG7[row][0] * 4);
  sgn = __vxor4(ones, ymm0);
  min = __vabs4(ymm0);

  //-------------------------loop starts here-------------------------------
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG7[row][1] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG7[row][2] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG7[row][3] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG7[row][4] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG7[row][5] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  //-------------------------------------------------------------------------
  uint32_t BricksToBeMoved = __vsign4(min, sgn);

  moveBricks_invput_circ((int8_t *)&p_bnProcBuf[idxBn], lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
}

__device__ __forceinline__ void cnProcKernel_BG1_int8_G8(const int8_t *__restrict__ p_cnProcBuf,
                                                         int8_t *__restrict__ p_bnProcBuf,
                                                         uint32_t row,
                                                         uint32_t lane,
                                                         uint32_t idxBn,
                                                         uint32_t circShift,
                                                         uint32_t Zc)
{
  uint32_t ymm0, sgn, min;
  const uint32_t ones = 0x01010101;

  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG8[row][0] * 4);
  sgn = __vxor4(ones, ymm0);
  min = __vabs4(ymm0);

  //-------------------------loop starts here-------------------------------
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG8[row][1] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG8[row][2] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG8[row][3] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG8[row][4] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG8[row][5] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG8[row][6] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  //-------------------------------------------------------------------------
  uint32_t BricksToBeMoved = __vsign4(min, sgn);

  moveBricks_invput_circ((int8_t *)&p_bnProcBuf[idxBn], lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
}

__device__ __forceinline__ void cnProcKernel_BG1_int8_G9(const int8_t *__restrict__ p_cnProcBuf,
                                                         int8_t *__restrict__ p_bnProcBuf,
                                                         uint32_t row,
                                                         uint32_t lane,
                                                         uint32_t idxBn,
                                                         uint32_t circShift,
                                                         uint32_t Zc)
{
  uint32_t ymm0, sgn, min;
  const uint32_t ones = 0x01010101;

  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG9[row][0] * 4);
  sgn = __vxor4(ones, ymm0);
  min = __vabs4(ymm0);

  //-------------------------loop starts here-------------------------------
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG9[row][1] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG9[row][2] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG9[row][3] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG9[row][4] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG9[row][5] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG9[row][6] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG9[row][7] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  //-------------------------------------------------------------------------
  uint32_t BricksToBeMoved = __vsign4(min, sgn);

  moveBricks_invput_circ((int8_t *)&p_bnProcBuf[idxBn], lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
}

__device__ __forceinline__ void cnProcKernel_BG1_int8_G10(const int8_t *__restrict__ p_cnProcBuf,
                                                          int8_t *__restrict__ p_bnProcBuf,
                                                          uint32_t row,
                                                          uint32_t lane,
                                                          uint32_t idxBn,
                                                          uint32_t circShift,
                                                          uint32_t Zc)
{
  uint32_t ymm0, sgn, min;
  const uint32_t ones = 0x01010101;

  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG10[row][0] * 4);
  sgn = __vxor4(ones, ymm0);
  min = __vabs4(ymm0);

  //-------------------------loop starts here-------------------------------
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG10[row][1] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG10[row][2] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG10[row][3] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG10[row][4] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG10[row][5] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG10[row][6] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG10[row][7] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG10[row][8] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  //-------------------------------------------------------------------------
  uint32_t BricksToBeMoved = __vsign4(min, sgn);

  moveBricks_invput_circ((int8_t *)&p_bnProcBuf[idxBn], lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
}

__device__ __forceinline__ void cnProcKernel_BG1_int8_G19(const int8_t *__restrict__ p_cnProcBuf,
                                                          int8_t *__restrict__ p_bnProcBuf,
                                                          uint32_t row,
                                                          uint32_t lane,
                                                          uint32_t idxBn,
                                                          uint32_t circShift,
                                                          uint32_t Zc)
{
  uint32_t ymm0, sgn, min;
  const uint32_t ones = 0x01010101;

  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][0] * 4);

  sgn = __vxor4(ones, ymm0);
  min = __vabs4(ymm0);

  //-------------------------loop starts here-------------------------------
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][1] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][2] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][3] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][4] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][5] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][6] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][7] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][8] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][9] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][10] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][11] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][12] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][13] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][14] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][15] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][16] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  ymm0 = *(const uint32_t *)(p_cnProcBuf + lane * 4 + c_lut_idxG19[row][17] * 4);
  min = __vminu4(min, __vabs4(ymm0));
  sgn = __vxor4(sgn, ymm0);
  //-------------------------------------------------------------------------
  uint32_t BricksToBeMoved = __vsign4(min, sgn);

  moveBricks_invput_circ((int8_t *)&p_bnProcBuf[idxBn], lane * 4, (uint8_t *)&BricksToBeMoved, Zc, circShift);
}
