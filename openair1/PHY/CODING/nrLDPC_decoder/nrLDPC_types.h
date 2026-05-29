/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Defines all types for the LDPC decoder
 */

#ifndef __NR_LDPC_TYPES__H__
#define __NR_LDPC_TYPES__H__
#ifndef CODEGEN
#include "time_meas.h"
#endif
#include "nrLDPCdecoder_defs.h"
typedef struct {
  uint8_t* d;
  int dim1;
  int dim2;
} arr8_t;
typedef struct {
  uint16_t* d;
  int dim1;
  int dim2;
} arr16_t;
typedef struct {
  uint32_t* d;
  int dim1;
  int dim2;
} arr32_t;
// ==============================================================================
// TYPES

/**
   Structure containing the pointers to the LUTs.
 */
typedef struct nrLDPC_lut {
    const uint32_t* startAddrCnGroups; /**< Start addresses for CN groups in CN processing buffer */
    const uint8_t*  numCnInCnGroups; /**< Number of CNs in every CN group */
    const uint8_t*  numBnInBnGroups; /**< Number of CNs in every BN group */
    const uint32_t* startAddrBnGroups; /**< Start addresses for BN groups in BN processing buffer  */
    const uint16_t* startAddrBnGroupsLlr; /**< Start addresses for BN groups in LLR processing buffer  */
    arr16_t circShift[NR_LDPC_NUM_CN_GROUPS_BG1]; /**< LUT for circular shift values for all CN groups and Zs */
    arr32_t startAddrBnProcBuf[NR_LDPC_NUM_CN_GROUPS_BG1]; /**< LUT of start addresses of CN groups in BN proc buffer */
    arr8_t bnPosBnProcBuf[NR_LDPC_NUM_CN_GROUPS_BG1]; /**< LUT of BN positions in BG for CN groups */
    const uint16_t* llr2llrProcBufAddr; /**< LUT for transferring input LLRs to LLR processing buffer */
    const uint8_t*  llr2llrProcBufBnPos; /**< LUT BN position in BG */
    arr8_t posBnInCnProcBuf[NR_LDPC_NUM_CN_GROUPS_BG1]; /**< LUT for llr2cnProcBuf */
} t_nrLDPC_lut;

/**
   Enum with possible LDPC output formats.
 */
typedef enum nrLDPC_outMode {
    nrLDPC_outMode_BIT, /**< 32 bits per uint32_t output */
    nrLDPC_outMode_BITINT8, /**< 1 bit per int8_t output */
    nrLDPC_outMode_LLRINT8 /**< Single LLR value per int8_t output */
} e_nrLDPC_outMode;

/**
   Structure containing LDPC decoder parameters.
 */
typedef struct nrLDPC_dec_params {
    uint8_t BG; /**< Base graph */
    uint16_t Z; /**< Lifting size */
    uint8_t R; /**< Decoding rate: Format 15,13,... for code rates 1/5, 1/3,... */
    uint8_t numMaxIter; /**< Maximum number of iterations */
    int Kprime; /**< Size of the payload bits and CRC bits in the code block */
    e_nrLDPC_outMode outMode; /**< Output format */
    int crc_type; /**< Size and type of the parity check bits (16, 24A or 24B) */
    int (*check_crc)(uint8_t* decoded_bytes, uint32_t n, uint8_t crc_type); /**< Parity check function */
    int n_segments; /**Infomation for cuda streaming*/
    int LastTrial; /**Infomation for cuda streaming to create and destroy*/
} t_nrLDPC_dec_params;

/**
   Structure containing LDPC decoder processing time statistics.
 */
#ifndef CODEGEN
typedef struct nrLDPC_time_stats {
    time_stats_t llr2llrProcBuf; /**< Statistics for function llr2llrProcBuf */
    time_stats_t llr2CnProcBuf; /**< Statistics for function llr2CnProcBuf */
    time_stats_t cnProc; /**< Statistics for function cnProc */
    time_stats_t cnProcPc; /**< Statistics for function cnProcPc */
    time_stats_t bnProcPc; /**< Statistics for function bnProcPc */
    time_stats_t bnProc; /**< Statistics for function bnProc */
    time_stats_t cn2bnProcBuf; /**< Statistics for function cn2bnProcBuf */
    time_stats_t bn2cnProcBuf; /**< Statistics for function bn2cnProcBuf */
    time_stats_t llrRes2llrOut; /**< Statistics for function llrRes2llrOut */
    time_stats_t llr2bit; /**< Statistics for function llr2bit */
    time_stats_t total; /**< Statistics for total processing time */
} t_nrLDPC_time_stats;
#endif
/**
   Structure containing the processing buffers
 */
typedef struct nrLDPC_procBuf {
    int8_t* cnProcBuf; /**< CN processing buffer */
    int8_t* cnProcBufRes; /**< Buffer for CN processing results */
    int8_t* bnProcBuf; /**< BN processing buffer */
    int8_t* bnProcBufRes; /**< Buffer for BN processing results */
    int8_t* llrRes; /**< Buffer for LLR results */
    int8_t* llrProcBuf; /**< LLR processing buffer */
} t_nrLDPC_procBuf;



#endif
