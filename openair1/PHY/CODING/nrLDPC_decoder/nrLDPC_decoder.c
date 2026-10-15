

/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!\file nrLDPC_decoder.c
 * \brief Defines thenrLDPC decoder
*/

#include <stdint.h>
#include "PHY/sse_intrin.h"
#include "nrLDPCdecoder_defs.h"
#include "nrLDPC_types.h"
#include "nrLDPC_init.h"
#include "nrLDPC_mPass.h"
#include "nrLDPC_cnProc.h"
#include "nrLDPC_bnProc.h"
#include "openair1/PHY/CODING/coding_defs.h"
#include "log.h"

//#define NR_LDPC_PROFILER_DETAIL(a) a
#define NR_LDPC_PROFILER_DETAIL(a)

#include "openair1/PHY/CODING/nrLDPC_extern.h"

#ifdef NR_LDPC_DEBUG_MODE
#include "nrLDPC_tools/nrLDPC_debug.h"
#endif

// decoder interface
/**
   \brief LDPC decoder API type definition
   \param p_decParams LDPC decoder parameters
   \param p_llr Input LLRs
   \param p_llrOut Output vector
   \param p_profiler LDPC profiler statistics
*/

static inline uint32_t nrLDPC_decoder_core(int8_t* p_llr,
                                           uint8_t* p_out,
                                           uint32_t numLLR,
                                           t_nrLDPC_lut* p_lut,
                                           t_nrLDPC_dec_params* p_decParams,
                                           t_nrLDPC_time_stats* p_profiler,
                                           decode_abort_t* ab);

int32_t LDPCinit()
{
  return 0;
}

int32_t LDPCshutdown()
{
  return 0;
}

int32_t LDPCdecoder(t_nrLDPC_dec_params* p_decParams,
                    int8_t* p_llr,
                    uint8_t* p_out,
                    t_nrLDPC_time_stats* p_profiler,
                    decode_abort_t* ab)
{
  uint32_t numLLR;
  t_nrLDPC_lut lut;
  t_nrLDPC_lut* p_lut = &lut;

  // Initialize decoder core(s) with correct LUTs
  numLLR = nrLDPC_init(p_decParams, p_lut);

  // Launch LDPC decoder core for one segment
  int numIter = nrLDPC_decoder_core(p_llr, p_out, numLLR, p_lut, p_decParams, p_profiler, ab);
  if (numIter >= p_decParams->numMaxIter) {
    LOG_D(PHY, "set abort: %d, %d\n", numIter, p_decParams->numMaxIter);
    set_abort(ab, true);
  }
    return numIter;
}

/**
   \brief PerformsnrLDPC decoding of one code block
   \param p_llr Input LLRs
   \param p_out Output vector
   \param numLLR Number of LLRs
   \param p_lut Pointer to decoder LUTs
   \param p_decParamsnrLDPC decoder parameters
   \param p_profilernrLDPC profiler statistics
*/
static inline uint32_t nrLDPC_decoder_core(int8_t* p_llr,
                                           uint8_t* p_out,
                                           uint32_t numLLR,
                                           t_nrLDPC_lut* p_lut,
                                           t_nrLDPC_dec_params* p_decParams,
                                           t_nrLDPC_time_stats* p_profiler,
                                           decode_abort_t* ab)
{
    uint16_t Z          = p_decParams->Z;
    uint8_t  BG         = p_decParams->BG;
    uint8_t  numMaxIter = p_decParams->numMaxIter;
    e_nrLDPC_outMode outMode = p_decParams->outMode;
   // int8_t* cnProcBuf=  cnProcBuf;
   // int8_t* cnProcBufRes= cnProcBufRes;

    int8_t cnProcBuf[NR_LDPC_SIZE_CN_PROC_BUF]    __attribute__ ((aligned(64)));
    int8_t cnProcBufRes[NR_LDPC_SIZE_CN_PROC_BUF] __attribute__ ((aligned(64)));
    int8_t bnProcBuf[NR_LDPC_SIZE_BN_PROC_BUF]    __attribute__ ((aligned(64)));
    int8_t bnProcBufRes[NR_LDPC_SIZE_BN_PROC_BUF] __attribute__ ((aligned(64)));
    int8_t llrRes[NR_LDPC_MAX_NUM_LLR]            __attribute__ ((aligned(64)));
    int8_t llrProcBuf[NR_LDPC_MAX_NUM_LLR] __attribute__((aligned(64)));
    // Minimum number of iterations is 1
    // 0 iterations means hard-decision on input LLRs
    // Initialize with parity check fail != 0

    // Initialization
    NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->llr2llrProcBuf));
    nrLDPC_llr2llrProcBuf(p_lut, p_llr, llrProcBuf, Z, BG);
    NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->llr2llrProcBuf));
#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_LLR_PROC);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_LLR_PROC, llrProcBuf);
#endif

    NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->llr2CnProcBuf));
    if (BG == 1)
      nrLDPC_llr2CnProcBuf_BG1(p_lut, p_llr, cnProcBuf, Z);
    else
      nrLDPC_llr2CnProcBuf_BG2(p_lut, p_llr, cnProcBuf, Z);
    NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->llr2CnProcBuf));

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_CN_PROC);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC, cnProcBuf);
#endif

    // First iteration

    // CN processing
    NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->cnProc));
    if (BG==1) {
      nrLDPC_cnProc_BG1_2pass(p_lut, cnProcBuf, cnProcBufRes, Z);
    } else {
      nrLDPC_cnProc_BG2_2pass(p_lut, cnProcBuf, cnProcBufRes, Z);
    }
    NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->cnProc));

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_CN_PROC_RES);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC_RES, cnProcBufRes);
#endif

    NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->cn2bnProcBuf));
    if (BG == 1)
      nrLDPC_cn2bnProcBuf_BG1(p_lut, cnProcBufRes, bnProcBuf, Z);
    else
      nrLDPC_cn2bnProcBuf_BG2(p_lut, cnProcBufRes, bnProcBuf, Z);
    NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->cn2bnProcBuf));

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_BN_PROC);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_BN_PROC, bnProcBuf);
#endif

    // BN processing
    NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->bnProcPc));
    nrLDPC_bnProcPc(p_lut, bnProcBuf, bnProcBufRes, llrProcBuf, llrRes, Z);
    NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->bnProcPc));

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_LLR_RES);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_LLR_RES, llrRes);
#endif

    NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->bnProc));
    nrLDPC_bnProc(p_lut, bnProcBuf, bnProcBufRes, llrRes, Z);
    NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->bnProc));

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_initBuffer2File(nrLDPC_buffers_BN_PROC_RES);
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_BN_PROC_RES, bnProcBufRes);
#endif

    // BN results to CN processing buffer
#ifdef NR_LDPC_PROFILER_DETAIL
    start_meas(&p_profiler->bn2cnProcBuf);
#endif
    if (BG == 1) nrLDPC_bn2cnProcBuf_BG1(p_lut, bnProcBufRes, cnProcBuf, Z);
    else         nrLDPC_bn2cnProcBuf_BG2(p_lut, bnProcBufRes, cnProcBuf, Z);
#ifdef NR_LDPC_PROFILER_DETAIL
    stop_meas(&p_profiler->bn2cnProcBuf);
#endif

#ifdef NR_LDPC_DEBUG_MODE
    nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC, cnProcBuf);
#endif

    // Parity Check not necessary here since it will fail
    // because first 2 cols/BNs in BG are punctured and cannot be
    // estimated after only one iteration

    // First iteration finished
    uint32_t numIter = 1;
    int32_t pcRes = 1; // pcRes is 0 if the ldpc decoder is succesful
    while ((numIter < numMaxIter) && (pcRes != 0)) {
      if (check_abort(ab)) {
        numIter = numMaxIter;
        break;
      }
      // CN processing
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->cnProc);
#endif
        if (BG==1) {
           nrLDPC_cnProc_BG1_2pass(p_lut, cnProcBuf, cnProcBufRes, Z);
        } else {
           nrLDPC_cnProc_BG2_2pass(p_lut, cnProcBuf, cnProcBufRes, Z);
        }
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->cnProc);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC_RES, cnProcBufRes);
#endif

        // Send CN results back to BNs
#ifdef NR_LDPC_PROFILER_DETAIL
        start_meas(&p_profiler->cn2bnProcBuf);
#endif
        if (BG == 1) nrLDPC_cn2bnProcBuf_BG1(p_lut, cnProcBufRes, bnProcBuf, Z);
        else         nrLDPC_cn2bnProcBuf_BG2(p_lut, cnProcBufRes, bnProcBuf, Z);
#ifdef NR_LDPC_PROFILER_DETAIL
        stop_meas(&p_profiler->cn2bnProcBuf);
#endif

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_BN_PROC, bnProcBuf);
#endif

        // BN Processing
        NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->bnProcPc));
        nrLDPC_bnProcPc(p_lut, bnProcBuf, bnProcBufRes, llrProcBuf, llrRes, Z);
        NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->bnProcPc));

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_LLR_RES, llrRes);
#endif

        NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->bnProc));
        nrLDPC_bnProc(p_lut, bnProcBuf, bnProcBufRes, llrRes, Z);
        NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->bnProc));

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_BN_PROC_RES, bnProcBufRes);
#endif

        // BN results to CN processing buffer
        NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->bn2cnProcBuf));
        if (BG == 1)
          nrLDPC_bn2cnProcBuf_BG1(p_lut, bnProcBufRes, cnProcBuf, Z);
        else
          nrLDPC_bn2cnProcBuf_BG2(p_lut, bnProcBufRes, cnProcBuf, Z);
        NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->bn2cnProcBuf));

#ifdef NR_LDPC_DEBUG_MODE
        nrLDPC_debug_writeBuffer2File(nrLDPC_buffers_CN_PROC, cnProcBuf);
#endif

   // Parity Check
        if (!p_decParams->check_crc) {
          NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->cnProcPc));
          if (BG == 1)
            pcRes = nrLDPC_cnProcPc_BG1(p_lut, cnProcBuf, cnProcBufRes, Z);
          else
            pcRes = nrLDPC_cnProcPc_BG2(p_lut, cnProcBuf, cnProcBufRes, Z);
          NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->cnProcPc));
        } else {
          if (numIter > 0) {
            int8_t llrOut[NR_LDPC_MAX_NUM_LLR] __attribute__((aligned(64))) = {0};
            int8_t* p_llrOut = outMode == nrLDPC_outMode_LLRINT8 ? (int8_t*)p_out : llrOut;
            nrLDPC_llrRes2llrOut(p_lut, p_llrOut, llrRes, Z, BG);
            if (outMode == nrLDPC_outMode_BIT)
              nrLDPC_llr2bitPacked(p_out, p_llrOut, numLLR);
            else // if (outMode == nrLDPC_outMode_BITINT8)
              nrLDPC_llr2bit(p_out, p_llrOut, numLLR);
            if (p_decParams->check_crc((uint8_t*)p_out, p_decParams->Kprime, p_decParams->crc_type)) {
              numIter++;
              LOG_D(PHY, "Segment CRC OK, exiting LDPC decoder\n");
              break;
            } else {
              LOG_D(PHY, "Segment CRC NOK, Kprime %d, BG %d, Z %d\n", p_decParams->Kprime, BG, Z);
            }
          }
        }
      // Increase iteration counter
      numIter++;
    }
    if (!p_decParams->check_crc) {
      int8_t llrOut[NR_LDPC_MAX_NUM_LLR] __attribute__((aligned(64))) = {0};
      int8_t* p_llrOut = outMode == nrLDPC_outMode_LLRINT8 ? (int8_t*)p_out : llrOut;
      // Assign results from processing buffer to output
      NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->llrRes2llrOut));
      nrLDPC_llrRes2llrOut(p_lut, p_llrOut, llrRes, Z, BG);
      NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->llrRes2llrOut));
      // Hard-decision
      NR_LDPC_PROFILER_DETAIL(start_meas(&p_profiler->llr2bit));
      if (outMode == nrLDPC_outMode_BIT)
        nrLDPC_llr2bitPacked(p_out, p_llrOut, numLLR);
      else // if (outMode == nrLDPC_outMode_BITINT8)
        nrLDPC_llr2bit(p_out, p_llrOut, numLLR);
      NR_LDPC_PROFILER_DETAIL(stop_meas(&p_profiler->llr2bit));
    }
    return numIter;
}




