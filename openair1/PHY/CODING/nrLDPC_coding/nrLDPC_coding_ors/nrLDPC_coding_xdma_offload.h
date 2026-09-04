/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \briefFPGA accelerator integrated into OAI (for one and multi code block)
 */

#ifndef __NRLDPC_CODING_XDMA_OFFLOAD__H_

#define __NRLDPC_CODING_XDMA_OFFLOAD__H_

#include <stdint.h>
#include "PHY/CODING/nrLDPC_decoder/nrLDPCdecoder_defs.h"

#define DEVICE_NAME_DEFAULT_USER "/dev/xdma0_user"
#define DEVICE_NAME_DEFAULT_DEC_READ "/dev/xdma0_c2h_0"
#define DEVICE_NAME_DEFAULT_DEC_WRITE "/dev/xdma0_h2c_0"
#define HEADER_SIZE (16U)
#define MAX_CB (NR_LDPC_MAX_NUM_CB)
#define GET_PADDING(X, M) (((M) - ((X) % (M))) % (M))
#define CEIL_UP(X, M) ((X) + (GET_PADDING(X, M)))
#define CEIL_UP_16B(X) CEIL_UP(X, 16)

/**
    \brief LDPC input parameter
    \param Zc shifting size
    \param BG base graph
    \param numCB number of code block
    \param max_iter Max decoder iteration
    \param numb_of_parity_bits_per_CB  Number of parity bits per CB
    \param SetIdx Scaling factor index
*/
typedef struct {
  unsigned char max_schedule;
  unsigned char SetIdx;
  int Zc;
  unsigned char numCB;
  unsigned char BG;
  unsigned char max_iter;
  uint32_t numb_of_parity_bits_per_CB[MAX_CB];
} DecIFConf;

int nrLDPC_decoder_FPGA(uint8_t* buf_in, uint8_t* buf_out, DecIFConf dec_conf);

#endif // __NRLDPC_CODING_XDMA_OFFLOAD__H_
