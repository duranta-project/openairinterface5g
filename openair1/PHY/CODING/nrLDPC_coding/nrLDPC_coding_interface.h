/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file openair1/PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h
 * \brief interface for libraries implementing coding/decoding algorithms
 */

#include "common/utils/time_meas.h"
#include "openair1/PHY/CODING/coding_defs.h"
#include "openair1/PHY/CODING/nrLDPC_decoder/nrLDPCdecoder_defs.h"
#include "common/utils/threadPool/thread-pool.h"

#ifndef __NRLDPC_CODING_INTERFACE__H__
#define __NRLDPC_CODING_INTERFACE__H__

/**
 * \typedef nrLDPC_TB_decoding_parameters_t
 * \struct nrLDPC_TB_decoding_parameters_s
 * \brief decoding parameter of transport blocks
 * \var harq_unique_pid unique id of the HARQ process
 * WARNING This id should be unique in the whole instance
 * among the active HARQ processes for the duration of the process
 * \var processedSegments
 * pointer to the number of succesfully decoded segments
 * it initially holds the total number of segments decoded after the previous HARQ round
 * it finally holds the total number of segments decoded after the current HARQ round
 * \var nb_rb number of resource blocks
 * \var Qm modulation order
 * \var mcs MCS
 * \var nb_layers number of layers
 * \var BG LDPC base graph id
 * \var rv_index redundancy version of the current HARQ round
 * \var max_ldpc_iterations maximum number of LDPC iterations
 * \var abort_decode pointer to decode abort flag
 * \var G Available radio resource bits
 * \var tbslbrm Transport block size LBRM
 * \var A Transport block size (This is A from 38.212 V15.4.0 section 5.1)
 * \var K Code block size at decoder output
 * \var Z lifting size
 * \var F filler bits size
 * \var C number of segments 
 * \var segments array of segments parameters
 */
typedef struct nrLDPC_TB_decoding_parameters_s{

  uint32_t harq_unique_pid;
  uint32_t *processedSegments;

  uint16_t nb_rb;
  uint8_t Qm;
  uint8_t mcs;
  uint8_t nb_layers;

  uint8_t BG;
  uint8_t rv_index;
  uint8_t max_ldpc_iterations;
  decode_abort_t *abort_decode;

  uint32_t G;
  uint32_t tbslbrm;
  uint32_t A;
  uint32_t K;
  uint32_t Z;
  uint32_t F;

  uint32_t C;
  int E;
  uint8_t R;
  int E2;
  uint8_t R2;
  int first_rE2;
  short *llr;
  uint8_t *c;
  int16_t *d;
  bool d_to_be_cleared;
  bool decodeSuccess[NR_LDPC_MAX_NUM_CB];
  time_stats_t ts_ldpc_decode;
} nrLDPC_TB_decoding_parameters_t;

/**
 * \typedef nrLDPC_slot_decoding_parameters_t
 * \struct nrLDPC_slot_decoding_parameters_s
 * \brief decoding parameter of slot
 * \var frame frame index
 * \var slot slot index
 * \var nb_TBs number of transport blocks
 * \var threadPool pointer to the thread pool
 * The thread pool can be used by the implementation
 * in order to launch jobs internally
 * DEQUEUING THE JOBS IS DONE WITHIN THE IMPLEMENTATION
 * \var TBs array of TBs decoding parameters
 */
typedef struct nrLDPC_slot_decoding_parameters_s{
  int frame;
  int slot;
  int nb_TBs;
  tpool_t *threadPool;
  nrLDPC_TB_decoding_parameters_t *TBs;
} nrLDPC_slot_decoding_parameters_t;

/**
 * \typedef nrLDPC_segment_encoding_parameters_t
 * \struct nrLDPC_segment_encoding_parameters_s
 * \brief encoding parameter of segments
 * \var E input llr segment size
 * \var c Pointers to code blocks before LDPC encoding (38.212 V15.4.0 section 5.2.2)
 * IT MUST BE FILLED BY THE IMPLEMENTATION
 * \var ts_ldpc_encode encoding time stats
 */
typedef struct nrLDPC_segment_encoding_parameters_s{
  int E;
  uint8_t *c;
  time_stats_t ts_ldpc_encode;
} nrLDPC_segment_encoding_parameters_t;

/**
 * \typedef nrLDPC_TB_encoding_parameters_t
 * \struct nrLDPC_TB_encoding_parameters_s
 * \brief encoding parameter of transport blocks
 * \var harq_unique_pid unique id of the HARQ process
 * WARNING This id should be unique in the whole instance
 * among the active HARQ processes for the duration of the process
 * \var nb_rb number of resource blocks
 * \var Qm modulation order
 * \var mcs MCS
 * \var nb_layers number of layers
 * \var BG LDPC base graph id
 * \var rv_index
 * \var G Available radio resource bits
 * \var tbslbrm Transport block size LBRM
 * \var A Transport block size (This is A from 38.212 V15.4.0 section 5.1)
 * \var Kb Number of time the lifting size needed to fit the payload of a code block
 * \var K Code block size at input of encoder
 * \var Z lifting size
 * \var F filler bits size
 * \var C number of segments 
 * \var segments array of segments parameters
 * \var output input llr TB array
 */
typedef struct nrLDPC_TB_encoding_parameters_s{

  uint32_t harq_unique_pid;

  uint16_t nb_rb;
  uint8_t Qm;
  uint8_t mcs;
  uint8_t nb_layers;

  uint8_t BG;
  uint8_t rv_index;

  uint32_t G;
  uint32_t tbslbrm;
  uint32_t A;
  uint32_t Kb;
  uint32_t K;
  uint32_t Z;
  uint32_t F;

  uint32_t C;
  nrLDPC_segment_encoding_parameters_t *segments;
  unsigned char *output;
  uint8_t **c_dev;
} nrLDPC_TB_encoding_parameters_t;

/**
 * \typedef nrLDPC_slot_encoding_parameters_t
 * \struct nrLDPC_slot_encoding_parameters_s
 * \brief encoding parameter of slot
 * \var frame frame index
 * \var slot slot index
 * \var nb_TBs number of transport blocks
 * \var threadPool pointer to the thread pool
 * The thread pool can be used by the implementation
 * in order to launch jobs internally
 * DEQUEUING THE JOBS IS DONE WITHIN THE IMPLEMENTATION
 * \var TBs array of TBs decoding parameters
 */
typedef struct nrLDPC_slot_encoding_parameters_s{
  int frame;
  int slot;
  int nb_TBs;
  tpool_t *threadPool;
  nrLDPC_TB_encoding_parameters_t *TBs;
} nrLDPC_slot_encoding_parameters_t;

typedef int32_t(nrLDPC_coding_init_t)(int);
typedef int32_t(nrLDPC_coding_shutdown_t)(void);

/**
 * \brief slot decoding function interface
 * \param nrLDPC_slot_decoding_parameters pointer to the structure holding the parameters necessary for decoding
 */
typedef int32_t(nrLDPC_coding_decoder_t)(nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters);

/**
 * \brief slot encoding function interface
 * \param nrLDPC_slot_encoding_parameters pointer to the structure holding the parameters necessary for encoding
 */
typedef int32_t(nrLDPC_coding_encoder_t)(nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters);

typedef struct nrLDPC_coding_interface_s {
  nrLDPC_coding_init_t *nrLDPC_coding_init;
  nrLDPC_coding_shutdown_t *nrLDPC_coding_shutdown;
  nrLDPC_coding_decoder_t *nrLDPC_coding_decoder;
  nrLDPC_coding_encoder_t *nrLDPC_coding_encoder;
} nrLDPC_coding_interface_t;

int load_nrLDPC_coding_interface(char *version, nrLDPC_coding_interface_t *interface, int);
int free_nrLDPC_coding_interface(nrLDPC_coding_interface_t *interface);

#endif
