/*
 * Copyright (c) 2016-present,  Xilinx, Inc.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license
 * the terms of the BSD Licence are reported below:
 *
 * BSD License
 *
 * For Xilinx DMA IP software
 *
 * Copyright (c) 2016-present, Xilinx, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 *  * Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 *  * Neither the name Xilinx nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MODULES_TXCTRL_INC_XDMA_DIAG_H_
#define MODULES_TXCTRL_INC_XDMA_DIAG_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/**
 * \brief Structure for controlling the LDPC IP core
 * \param Z_val Shifting size
 * \param kb K_b value
 * \param max_schedule Scheduling behaviour of the CBs in the IP core
 * \param CB_num Number of CBs
 * \param BGSel BG
 * \param z_set Z_set
 * \param z_j Z_j
 * \param max_iter Maximum possible LDPC iteration
 * \param SetIdx Scaling index
 * \param numb_of_parity_bits_per_cb Ptr to the number of parity bits per CB. Every entry is a multiple of Z_c. Array size is
 * MAX_CB, but just CB_num are set
 */
typedef struct {
  const uint32_t *numb_of_parity_bits_per_cb;
  uint32_t Z_val;
  uint8_t kb;
  uint8_t max_schedule;
  uint8_t CB_num;
  uint8_t BGSel;
  uint8_t z_set;
  uint8_t z_j;
  uint8_t max_iter;
  uint8_t SetIdx;
} DecIPConf;

typedef struct {
  char *user_device, *dec_write_device, *dec_read_device;
} devices_t;

#define MAP_SIZE (1024UL * 1024UL)

#define OFFSET_RESET 0x0004U

int32_t test_dma_init(devices_t devices);
void dma_close();

#ifdef __cplusplus
}
#endif
#endif
