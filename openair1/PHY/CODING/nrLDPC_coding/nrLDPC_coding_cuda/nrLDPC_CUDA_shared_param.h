/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
/*! \file nrLDPC_CUDA_shared_param.h
 * \brief Shared parameters in CUDA implementation of LDPC decoder 
 * \author Qizhi Pan, Raymond Knopp
 * \company EURECOM
 * \email: qizhi.pan@eurecom.fr, raymond.knopp@eurecom.fr
 * \date 2025-12-30
 * \version 1.0
 * \note 
 * \warning
 */

#ifndef NRLDPC_CUDA_SHARED_PARAM_H_
#define NRLDPC_CUDA_SHARED_PARAM_H_

#include <cuda_runtime.h>

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

#define JETSON_ORIN 
//#define GH200

#if defined(GH200)
#define NodeEdge_Switch_Cn_R13 32
#define NodeEdge_Switch_Bn_R13 10
#define NodeEdge_Switch_Cn_R23 32
#define NodeEdge_Switch_Bn_R23 12
#define NodeEdge_Switch_Cn_R89 48
#define NodeEdge_Switch_Bn_R89 24
#elif defined(JETSON_ORIN)
#define NodeEdge_Switch_Cn_R13 3
#define NodeEdge_Switch_Bn_R13 1
#define NodeEdge_Switch_Cn_R23 3
#define NodeEdge_Switch_Bn_R23 1
#define NodeEdge_Switch_Cn_R89 4
#define NodeEdge_Switch_Bn_R89 2
#endif

#ifdef __cplusplus
}
#endif

typedef struct KernelLaunchConfig {
    dim3 grid;
    dim3 block;
}KernelLaunchConfig;


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
