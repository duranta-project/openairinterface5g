/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Shared parameters in CUDA implementation of LDPC decoder 
 */

#ifndef NRLDPC_CUDA_SHARED_PARAM_H_
#define NRLDPC_CUDA_SHARED_PARAM_H_

#include <stdint.h>
#include "PHY/gpu_compat.h"

#ifndef MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER
#define MAX_NUM_NR_DLSCH_SEGMENTS_PER_LAYER 36
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define RowLength 96 //Zc = 384/4 = 96

#define num_TotalBlocks_BG1_R13_Edge 316//based on number of Cn2Bn Msgs
#define num_TotalBlocks_BG1_R23_Edge 144
#define num_TotalBlocks_BG1_R89_Edge 79
#define num_TotalBlocks_llr_llrRes 22 //Only includes systematic bits

#define num_TotalBlocks_cn_BG1_R13_Node 46 //based on number of CNs
#define num_TotalBlocks_bn_BG1_R13_Node 68 //based on number of BNs
#define num_TotalBlocks_cn_BG1_R23_Node 13
#define num_TotalBlocks_bn_BG1_R23_Node 35
#define num_TotalBlocks_cn_BG1_R89_Node 5
#define num_TotalBlocks_bn_BG1_R89_Node 27

// Segment counts above which the node kernels are used instead of the edge kernels. They depend on
// how many SMs the GPU has, so cuda_support_init_decoder() picks them at runtime from the SM count
typedef struct {
  uint32_t cn_r13, bn_r13, cn_r23, bn_r23, cn_r89, bn_r89;
} nodeEdge_switch_t;
extern nodeEdge_switch_t nodeEdge_switch;

#define NodeEdge_Switch_Cn_R13 nodeEdge_switch.cn_r13
#define NodeEdge_Switch_Bn_R13 nodeEdge_switch.bn_r13
#define NodeEdge_Switch_Cn_R23 nodeEdge_switch.cn_r23
#define NodeEdge_Switch_Bn_R23 nodeEdge_switch.bn_r23
#define NodeEdge_Switch_Cn_R89 nodeEdge_switch.cn_r89
#define NodeEdge_Switch_Bn_R89 nodeEdge_switch.bn_r89

#ifdef __cplusplus
}
#endif

typedef struct KernelLaunchConfig {
  dim3 grid;
  dim3 block;
} KernelLaunchConfig;

typedef struct {
  int idxBn;
  int idxCn;
  int preBuf;
  int circShift;
  int8_t dd;
} DumpEntry;

typedef struct {
  int8_t* p_llr_ptr;
  int8_t* p_out_ptr;
} ldpc_cuda_bridge_t;

#endif /* NRLDPC_CUDA_SHARED_PARAM_H_ */
