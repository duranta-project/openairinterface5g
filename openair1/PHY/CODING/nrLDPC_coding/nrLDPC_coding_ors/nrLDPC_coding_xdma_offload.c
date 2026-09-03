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

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 500

#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <byteswap.h>
#include <signal.h>
#include <ctype.h>
#include <termios.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

#include "xdma_diag.h"
#include "nrLDPC_coding_xdma_offload.h"

#include "common/utils/assertions.h"
#include "common/utils/nr/nr_common.h"

static void* map_base;
static int fd_mm_interface = -1;
static int fd_dec_write = -1, fd_dec_read = -1;
static char *dev_dec_write, *dev_dec_read;

static void decoder_reset();
static int test_dma_dec_read(char* DecOut, DecIPConf Confparam);
static int test_dma_dec_write(char* data, DecIPConf Confparam);

#ifdef DEBUG_HW_HDR
#define PRINT_ORS_TX_HEADER(tx_hdr)                                                                                              \
  do {                                                                                                                           \
    printf(                                                                                                                      \
        "Tx hdr{max_schedule=%u, mb=%u, id=%u, max_iter=%u, toked_iter=%u, term_no_change=%u, term_pass=%u, include_parity=%u, " \
        "hard_op_o=%u, sc_idx=%u, bg=%u, z_set=%u, z_j=%u, magic=0x%08x, payload_len=%u}\n",                                     \
        (tx_hdr).max_schedule,                                                                                                   \
        (tx_hdr).mb,                                                                                                             \
        (tx_hdr).id,                                                                                                             \
        (tx_hdr).max_iter,                                                                                                       \
        (tx_hdr).toked_iter,                                                                                                     \
        (tx_hdr).term_on_no_change,                                                                                              \
        (tx_hdr).term_on_pass,                                                                                                   \
        (tx_hdr).include_parity_op,                                                                                              \
        (tx_hdr).hard_op_o,                                                                                                      \
        (tx_hdr).sc_idx,                                                                                                         \
        (tx_hdr).bg,                                                                                                             \
        (tx_hdr).z_set,                                                                                                          \
        (tx_hdr).z_j,                                                                                                            \
        (tx_hdr).magic_field,                                                                                                    \
        (tx_hdr).payload_len);                                                                                                   \
  } while (0)
#define PRINT_ORS_RX_HEADER(rx_hdr)                                                                                        \
  do {                                                                                                                     \
    printf(                                                                                                                \
        "Rx hdr{mb=%u, id=%u, toked_iter=%u, term_no_change=%u, term_pass=%u, parity_check_pass=%u, hard_op_o=%u, bg=%u, " \
        "z_set=%u, z_j=%u, magic=0x%08x, payload_len=%u}\n",                                                               \
        (rx_hdr).mb,                                                                                                       \
        (rx_hdr).id,                                                                                                       \
        (rx_hdr).toked_iter,                                                                                               \
        (rx_hdr).term_on_no_change,                                                                                        \
        (rx_hdr).term_pass,                                                                                                \
        (rx_hdr).parity_check_pass,                                                                                        \
        (rx_hdr).hard_op_o,                                                                                                \
        (rx_hdr).bg,                                                                                                       \
        (rx_hdr).z_set,                                                                                                    \
        (rx_hdr).z_j,                                                                                                      \
        (rx_hdr).magic_field,                                                                                              \
        (rx_hdr).payload_len);                                                                                             \
  } while (0)

#else
#define PRINT_ORS_RX_HEADER(rx_hdr) ((void)(rx_hdr))
#define PRINT_ORS_TX_HEADER(tx_hdr) ((void)(tx_hdr))
#endif
typedef struct ors_tx_header_s {
  uint8_t max_schedule;
  uint8_t mb;
  uint8_t id;
  uint8_t max_iter;
  uint8_t toked_iter;
  uint8_t term_on_no_change;
  uint8_t term_on_pass;
  uint8_t include_parity_op;
  uint8_t hard_op_o;
  uint8_t sc_idx;
  uint8_t bg;
  uint8_t z_set;
  uint8_t z_j;
  uint32_t magic_field;
  uint32_t payload_len; //< numb of 16 bytes chunks
} ors_tx_header_t;

typedef struct ors_rx_header_s {
  uint8_t mb;
  uint8_t id;
  uint8_t toked_iter;
  uint8_t term_on_no_change;
  uint8_t term_pass;
  uint8_t parity_check_pass;
  uint8_t hard_op_o;
  uint8_t bg;
  uint8_t z_set;
  uint8_t z_j;
  uint32_t magic_field;
  uint32_t payload_len; //< numb of 16 bytes chunks
} ors_rx_header_t;
#define ORS_MAGIC (0x7E7EU)
/*
 * man 2 write:
 * On Linux, write() (and similar system calls) will transfer at most
 * 	0x7ffff000 (2,147,479,552) bytes, returning the number of bytes
 *	actually transferred.  (This is true on both 32-bit and 64-bit
 *	systems.)
 */
#define RW_MAX_SIZE 0x7ffff000

static inline void write_headers_to_buffer(const ors_tx_header_t* hs, void* buffer, const size_t CB_num, const uint32_t* offsets)
{
  uint64_t tmp[2] = {0};
  for (size_t r = 0; r < CB_num; ++r) {
    tmp[0] = (((uint64_t)hs[r].max_schedule & 0xF) << 38) | (((uint64_t)hs[r].mb & 0x3F) << 32)
             | (((uint64_t)hs[r].id & 0xFF) << 24) | (((uint64_t)hs[r].max_iter & 0x3F) << 18)
             | (((uint64_t)hs[r].term_on_no_change & 0x1) << 17) | (((uint64_t)hs[r].term_on_pass & 0x1) << 16)
             | (((uint64_t)hs[r].include_parity_op & 0x1) << 15) | (((uint64_t)hs[r].hard_op_o & 0x1) << 14)
             | (((uint64_t)hs[r].sc_idx & 0xF) << 9) | (((uint64_t)hs[r].bg & 0x7) << 6) | (((uint64_t)hs[r].z_set & 0x7) << 3)
             | (((uint64_t)hs[r].z_j & 0x7));

    tmp[1] = (((uint64_t)hs[r].magic_field & 0xFFFF) << 48) | (((uint64_t)hs[r].payload_len & 0xFFFF) << 32);

    memcpy(buffer + offsets[r], tmp, sizeof(tmp));
    memset(tmp, 0, sizeof(tmp));
  }
}

static inline void read_headers_from_buffer(const void* buffer, ors_rx_header_t* headers, const size_t CB_num, const size_t offset)
{
  for (size_t r = 0; r < CB_num; ++r) {
    const size_t local_offset = offset * r;
    const uint64_t* w0 = (const uint64_t*)(buffer + local_offset);
    const uint64_t* w1 = (const uint64_t*)(buffer + local_offset + sizeof(uint64_t));
    ors_rx_header_t hd = {};

    hd.mb = (*w0 >> 32) & 0x3F;
    hd.id = (*w0 >> 24) & 0xFF;
    hd.toked_iter = (*w0 >> 18) & 0x3F;
    hd.term_on_no_change = (*w0 >> 17) & 0x1;
    hd.term_pass = (*w0 >> 16) & 0x1;
    hd.parity_check_pass = (*w0 >> 15) & 0x1;
    hd.hard_op_o = (*w0 >> 14) & 0x1;
    hd.bg = (*w0 >> 6) & 0x7;
    hd.z_set = (*w0 >> 3) & 0x7;
    hd.z_j = *w0 & 0x7;

    hd.magic_field = (*w1 >> 48) & 0xFFFF;
    hd.payload_len = (*w1 >> 32) & 0xFFFF;
    headers[r] = hd;
  }
}

static ssize_t read_to_buffer(char* fname, int fd, char* buffer, uint64_t size)
{
  DevAssert(size <= RW_MAX_SIZE);
  /* read data from file into memory buffer */
  ssize_t rc = read(fd, buffer, size);
  if (rc != size) {
    fprintf(stderr, "%s: %s, R, 0x%lx != 0x%lx.\n", __FUNCTION__, fname, rc, size);
    perror("read file");
    return -EIO;
  }
  return rc;
}

static ssize_t write_from_buffer(char* fname, int fd, char* buffer, uint64_t size)
{
  DevAssert(size <= RW_MAX_SIZE);
  /* write data to file from memory buffer */
  ssize_t rc = write(fd, buffer, size);
  if (rc != size) {
    fprintf(stderr, "%s: %s, W, 0x%lx != 0x%lx.\n", __FUNCTION__, fname, rc, size);
    perror("write file");
    return -EIO;
  }

  return rc;
}

static int test_dma_dec_read(char* DecOut, DecIPConf Confparam)
{
  ssize_t rc = 0;

  const uint32_t Z_val = Confparam.Z_val;
  const uint32_t kb = Confparam.kb;
  const uint32_t CB_num = Confparam.CB_num;

  const uint32_t OutDataNUM = Z_val * kb + HEADER_SIZE * 8;
  const uint32_t Out_dwNumItems_p128 = CEIL_UP(OutDataNUM, 128);
  // bits to bytes
  const uint32_t Out_dwNumItems = Out_dwNumItems_p128 / 8;

  const uint64_t size = Out_dwNumItems * CB_num;

  if (fd_dec_read < 0) {
    fprintf(stderr, "%s: Unable to open device %s, %d.\n", __FUNCTION__, dev_dec_read, fd_dec_read);
    perror("open device");
    return -EINVAL;
  }

  /* read data from AXI ST into buffer using SGDMA */
  rc = read_to_buffer(dev_dec_read, fd_dec_read, DecOut, size);
  ors_rx_header_t headers[MAX_CB];
  read_headers_from_buffer(DecOut, &headers[0], CB_num, Out_dwNumItems);
  PRINT_ORS_RX_HEADER(headers[0]);
  size_t mx_iter = 0;
  for (size_t r = 0; r < CB_num; ++r) {
    const uint16_t previous_set_id = min((CB_num - 1) - r, 255);
    if (headers[r].magic_field != ORS_MAGIC) {
      fprintf(stderr,
              "%s: Header magic field is wrong. Magic field should be %x got %x.\n",
              __FUNCTION__,
              ORS_MAGIC,
              headers[r].magic_field);
      return -(EINVAL);
    } else if (headers[r].id != previous_set_id) {
      fprintf(stderr, "%s: Header ID mismatch. ID should be %u got %u.\n", __FUNCTION__, previous_set_id, headers[r].id);
      return -(EINVAL);
    }

    mx_iter = max(mx_iter, headers[r].toked_iter);
  }

  rc = mx_iter;

  return rc;
}

// int test_dma_dec_write(unsigned int *data, DecIPConf Confparam)
static int test_dma_dec_write(char* data, DecIPConf Confparam)
{
  ssize_t rc = 0;

  uint64_t size = 0;

  const uint32_t Z_val = Confparam.Z_val;
  const uint8_t max_schedule = Confparam.max_schedule;
  const uint8_t bg = Confparam.BGSel - 1;
  const uint8_t z_j = Confparam.z_j;
  const uint32_t kb = Confparam.kb;
  const uint8_t max_iter = Confparam.max_iter;
  const uint8_t sc_idx = Confparam.SetIdx;
  const uint8_t z_set = Confparam.z_set - 1;
  const uint32_t CB_num = Confparam.CB_num;

  uint16_t id = CB_num - 1;

  ors_tx_header_t headers[MAX_CB] = {};
  uint32_t offsets[MAX_CB] = {};
  for (uint32_t r = 0; r < CB_num; ++r) {
    const uint8_t mb = Confparam.numb_of_parity_bits_per_cb[r] / Z_val;
    // kb (number of informations bits)
    size_t local_size = HEADER_SIZE + kb * Z_val + Confparam.numb_of_parity_bits_per_cb[r];
    // ceil up to a multiple of 16B
    local_size = CEIL_UP_16B(local_size);
    offsets[r] = size;
    size += local_size;
    const size_t numb_of_16B_units = (local_size - HEADER_SIZE) / 16; //< header isn't part of data
    headers[r].max_schedule = max_schedule;
    headers[r].mb = mb;
    headers[r].id = min(255, id);
    headers[r].max_iter = max_iter;
    headers[r].term_on_no_change = 1;
    headers[r].term_on_pass = 1;
    headers[r].include_parity_op = 0;
    headers[r].hard_op_o = 1;
    headers[r].sc_idx = sc_idx;
    headers[r].bg = bg;
    headers[r].z_set = z_set;
    headers[r].z_j = z_j;
    headers[r].magic_field = ORS_MAGIC;
    headers[r].payload_len = numb_of_16B_units; //< payload size in 16 Byte units
    PRINT_ORS_TX_HEADER(headers[r]);
    id--;
  }
  // insert header infront of data
  write_headers_to_buffer(headers, data, CB_num, offsets);

  if (fd_dec_write < 0) {
    fprintf(stderr, "%s: Unable to open device %s, %d.\n", __FUNCTION__, dev_dec_write, fd_dec_write);
    perror("open device");
    return -EINVAL;
  }

  rc = write_from_buffer(dev_dec_write, fd_dec_write, data, size);
  return rc;
}

int32_t test_dma_init(devices_t devices)
{
  int32_t ret = 0;
  // device files already opened
  if (fd_dec_write > 0 && fd_dec_read > 0 && fd_mm_interface > 0) {
    return ret;
  }

  dev_dec_write = devices.dec_write_device;
  dev_dec_read = devices.dec_read_device;

  fd_dec_write = open(dev_dec_write, O_WRONLY);
  if (fd_dec_write < 0) {
    ret = fd_dec_write;
    fprintf(stderr, "%s: Failed to open %s!\n", __FUNCTION__, dev_dec_write);
    goto write_open_failed;
  }
  fd_dec_read = open(dev_dec_read, O_RDONLY);
  if (fd_dec_read < 0) {
    ret = fd_dec_read;
    fprintf(stderr, "%s: Failed to open %s!\n", __FUNCTION__, dev_dec_read);
    goto read_open_failed;
  }

  fd_mm_interface = open(devices.user_device, O_RDWR | O_SYNC);
  if (fd_mm_interface < 0) {
    ret = fd_mm_interface;
    fprintf(stderr, "%s: Failed to open %s!\n", __FUNCTION__, devices.user_device);
    goto reg_open_failed;
  }

  map_base = mmap(0, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd_mm_interface, 0);
  if (map_base == MAP_FAILED) {
    ret = -(errno);
    fprintf(stderr, "%s: Failed to map!\n", __FUNCTION__);
    goto mmap_failed;
  }
  decoder_reset();

  fflush(stdout);
  return ret;
mmap_failed:
  close(fd_mm_interface);
  fd_mm_interface = -1;
reg_open_failed:
  close(fd_dec_read);
  fd_dec_read = -1;
read_open_failed:
  close(fd_dec_write);
  fd_dec_write = -1;
write_open_failed:
  return ret;
}

void dma_close()
{
  decoder_reset();
  munmap(map_base, MAP_SIZE);
  if (fd_dec_write > 0)
    close(fd_dec_write);
  if (fd_dec_read > 0)
    close(fd_dec_read);
  if (fd_mm_interface > 0)
    close(fd_mm_interface);
  fd_dec_write = -1;
  fd_dec_read = -1;
  fd_mm_interface = -1;
}

int nrLDPC_decoder_FPGA(uint8_t* buf_in, uint8_t* buf_out, DecIFConf dec_conf)
{
  DecIPConf Confparam = {.max_iter = dec_conf.max_iter,
                         .SetIdx = dec_conf.SetIdx,
                         .numb_of_parity_bits_per_cb = dec_conf.numb_of_parity_bits_per_CB,
                         .Z_val = dec_conf.Zc,
                         .CB_num = dec_conf.numCB,
                         .BGSel = dec_conf.BG};
  int z_a, z_tmp;
  int z_j = 0;

  uint8_t i_LS;

  // LDPC input parameter
  const int Zc = dec_conf.Zc; // shifting size
  const int baseGraph = dec_conf.BG; // base graph

  // calc xdma LDPC parameter
  // calc i_LS
  if ((Zc % 15) == 0)
    i_LS = 7;
  else if ((Zc % 13) == 0)
    i_LS = 6;
  else if ((Zc % 11) == 0)
    i_LS = 5;
  else if ((Zc % 9) == 0)
    i_LS = 4;
  else if ((Zc % 7) == 0)
    i_LS = 3;
  else if ((Zc % 5) == 0)
    i_LS = 2;
  else if ((Zc % 3) == 0)
    i_LS = 1;
  else
    i_LS = 0;

  // calc z_a
  if (i_LS == 0)
    z_a = 2;
  else
    z_a = i_LS * 2 + 1;

  // calc z_j
  z_tmp = Zc / z_a;
  while (z_tmp % 2 == 0) {
    z_j = z_j + 1;
    z_tmp = z_tmp / 2;
  }

  // set kb value
  if (baseGraph == 1) {
    Confparam.kb = 22;
  } else if (baseGraph == 2) {
    Confparam.kb = 10;
  } else if (baseGraph == 3) {
    Confparam.kb = 9;
  } else if (baseGraph == 4) {
    Confparam.kb = 8;
  } else {
    Confparam.kb = 6;
  }

  // set z_set, z_j
  Confparam.z_set = i_LS + 1;
  Confparam.z_j = z_j;
  // LDPC accelerator start
  // write into accelerator
  if (test_dma_dec_write((char*)buf_in, Confparam) < 0) {
    exit(1);
    fprintf(stderr, "%s: Write exit!\n", __FUNCTION__);
  }
  const int numb_of_iter_or_err = test_dma_dec_read((char*)buf_out, Confparam);
  if (numb_of_iter_or_err < 0) {
    exit(1);
    fprintf(stderr, "%s: Read exit!\n", __FUNCTION__);
  }

  return numb_of_iter_or_err;
}

static void decoder_reset()
{
  *(volatile uint32_t*)(map_base + OFFSET_RESET) |= (1 << 8);
  usleep(10);
  *(volatile uint32_t*)(map_base + OFFSET_RESET) &= ~(1 << 8);
  usleep(10);
}