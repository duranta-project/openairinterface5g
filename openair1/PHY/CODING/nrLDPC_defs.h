/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
//============================================================================================================================
// encoder interface
#ifndef __NRLDPC_DEFS__H__
#define __NRLDPC_DEFS__H__
#include <openair1/PHY/defs_nr_common.h>
#include "openair1/PHY/CODING/nrLDPC_decoder/nrLDPC_types.h"
#include "common/utils/threadPool/task_ans.h"

/**
   \brief LDPC encoder parameter structure
   \var n_segments number of segments in the transport block
   \var first_seg index of the first segment of the subset to encode
   within the transport block
   \var gen_code flag to generate parity check code
   0 -> encoding
   1 -> generate parity check code with AVX2
   2 -> generate parity check code without AVX2
   \var tinput time statistics for data input in the encoder
   \var tprep time statistics for data preparation in the encoder
   \var tparity time statistics for adding parity bits
   \var toutput time statistics for data output from the encoder
   \var K size of the complete code segment before encoding
   including payload, CRC bits and filler bit
   (also known as Kr, see 38.212-5.2.2)
   \var Kb number of lifting sizes to fit the payload (see 38.212-5.2.2)
   \var Zc lifting size (see 38.212-5.2.2)
   \var F number of filler bits (see 38.212-5.2.2)
   \var harq pointer to the HARQ process structure
   \var BG base graph index
   \var output output buffer after INTERLEAVING
   \var ans pointer to the task answer
   to notify thread pool about completion of the task
*/
typedef struct {
  unsigned int n_segments; // optim8seg
  unsigned int first_seg; // optim8segmulti
  unsigned char gen_code; // orig
  time_stats_t *tinput;
  time_stats_t *tinput_memcpy;
  time_stats_t *tprep;
  time_stats_t *tparity;
  time_stats_t *toutput;
  time_stats_t *tconcat;
  /// Size in bits of the code segments
  uint32_t K;
  /// Number of lifting sizes to fit the payload
  uint32_t Kb;
  /// Lifting size
  uint32_t Zc;
  /// Number of "Filler" bits
  uint32_t F;
  /// Encoder BG
  uint8_t BG;
  /// Interleaver outputs
  unsigned char *output;
  task_ans_t *ans;
} encoder_implemparams_t;

typedef int32_t(LDPC_initfunc_t)(void);
typedef int32_t(LDPC_shutdownfunc_t)(void);

// decoder interface
/**
   \brief LDPC decoder API type definition
   \param p_decParams LDPC decoder parameters
   \param p_llr Input LLRs
   \param p_llrOut Output vector
   \param time_stats time statistics
   \param ab structure shared between tasks to stop all the tasks if one fails
*/
typedef int32_t(LDPC_decoderfunc_t)(t_nrLDPC_dec_params *p_decParams,
                                    int8_t *p_llr,
                                    uint8_t *p_out,
                                    t_nrLDPC_time_stats *time_stats,
                                    decode_abort_t *ab);
typedef int32_t(LDPC_encoderfunc_t)(uint8_t **, uint8_t *, encoder_implemparams_t *);
typedef uint32_t**(LDPC_encoderfunc32_t)(uint8_t **, encoder_implemparams_t *);

#endif
