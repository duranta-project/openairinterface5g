/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include "../nrLDPC_coding_interface.h"
#include "nrLDPC_coding_aal.h"
#include "PHY/sse_intrin.h"
#include "bits.h"
#include <common/utils/LOG/log.h>
#include "common/config/config_paramdesc.h"
#include "common/config/config_userapi.h"
#define NR_LDPC_ENABLE_PARITY_CHECK

#include "PHY/CODING/nrLDPC_decoder/nrLDPCdecoder_defs.h"
#include "PHY/CODING/coding_defs.h"

#include <stdint.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <regex.h>

#include <math.h>

#include <rte_eal.h>
#include <rte_common.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_pdump.h>

#include <rte_dev.h>
#include <rte_launch.h>
#include <rte_bbdev.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_hexdump.h>
#include <rte_interrupts.h>

// this socket is the NUMA socket, so the hardware CPU id (numa is complex)
#define GET_SOCKET(socket_id) (((socket_id) == SOCKET_ID_ANY) ? 0 : (socket_id))
#define MAX_QUEUES 32
#define OPS_CACHE_SIZE 256U
#define OPS_POOL_SIZE_MIN 511U /* 0.5K per queue */
#define SYNC_WAIT 0
#define SYNC_START 1
#define TIME_OUT_POLL 1e8
/* Headroom for filler LLRs insertion in HARQ buffer */
#define FILLER_HEADROOM 1024

pthread_mutex_t encode_mutex;
pthread_mutex_t decode_mutex;

/* we assume one active baseband device only */
struct active_device {
  const char *driver_name;
  uint8_t dev_id;
  struct rte_bbdev_info info;
  bool is_t2;
  uint32_t num_harq_codeblock;
  /* Persistent data structure to keep track of HARQ-related information */
  // Note: This is used to store/keep track of the combined output information across iterations
  struct rte_bbdev_op_data *harq_buffers;
  bool support_internal_harq_memory;
  int dec_queue;
  int enc_queue;
  uint16_t queue_ids[MAX_QUEUES];
  uint16_t nb_queues;
  struct rte_mempool *bbdev_dec_op_pool;
  struct rte_mempool *bbdev_enc_op_pool;
  struct rte_mempool *in_mbuf_pool;
  struct rte_mempool *hard_out_mbuf_pool;
  struct rte_mempool *harq_in_mbuf_pool;
  struct rte_mempool *harq_out_mbuf_pool;
} active_dev;

/* Data buffers used by BBDEV ops */
struct data_buffers {
  struct rte_bbdev_op_data *inputs;
  struct rte_bbdev_op_data *hard_outputs;
  struct rte_bbdev_op_data *harq_outputs;
};

/* Operation parameters specific for given test case */
struct test_op_params {
  uint16_t num_lcores;
  rte_atomic16_t sync;
};

/* Contains per lcore params */
struct thread_params {
  uint8_t dev_id;
  uint16_t queue_id;
  uint32_t lcore_id;
  nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters;
  nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters;
  uint8_t iter_count;
  struct test_op_params *op_params;
  struct data_buffers *data_buffers;
  struct rte_mempool *bbdev_op_pool;
};

static uint16_t nb_segments_decoding(nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters)
{
  uint16_t nb_segments = 0;
  for (int h = 0; h < nrLDPC_slot_decoding_parameters->nb_TBs; ++h) {
    nb_segments += nrLDPC_slot_decoding_parameters->TBs[h].C;
  }
  return nb_segments;
}

static uint16_t nb_segments_encoding(nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters)
{
  uint16_t nb_segments = 0;
  for (int h = 0; h < nrLDPC_slot_encoding_parameters->nb_TBs; ++h) {
    nb_segments += nrLDPC_slot_encoding_parameters->TBs[h].C;
  }
  return nb_segments;
}

/* Read flag value 0/1 from bitmap */
// DPDK BBDEV copy
static inline bool check_bit(uint32_t bitmap, uint32_t bitmask)
{
  return bitmap & bitmask;
}

/* calculates optimal mempool size not smaller than the val */
// DPDK BBDEV copy
static unsigned int optimal_mempool_size(unsigned int val)
{
  return rte_align32pow2(val + 1) - 1;
}

// based on DPDK BBDEV create_mempools
static int create_mempools(struct active_device *ad, int socket_id, uint16_t num_ops, int out_buff_sz, int in_max_sz)
{
  unsigned int ops_pool_size, mbuf_pool_size, data_room_size = 0;
  num_ops = 1;
  uint8_t nb_segments = 1;
  ops_pool_size = optimal_mempool_size(RTE_MAX(
      /* Ops used plus 1 reference op */
      RTE_MAX((unsigned int)(ad->nb_queues * num_ops + 1),
              /* Minimal cache size plus 1 reference op */
              (unsigned int)(1.5 * rte_lcore_count() * OPS_CACHE_SIZE + 1)),
      OPS_POOL_SIZE_MIN));

  /* Decoder ops mempool */
  ad->bbdev_dec_op_pool = rte_bbdev_op_pool_create("bbdev_op_pool_dec",
                                                   RTE_BBDEV_OP_LDPC_DEC,
                                                   /* Encoder ops mempool */ ops_pool_size,
                                                   OPS_CACHE_SIZE,
                                                   socket_id);
  ad->bbdev_enc_op_pool =
      rte_bbdev_op_pool_create("bbdev_op_pool_enc", RTE_BBDEV_OP_LDPC_ENC, ops_pool_size, OPS_CACHE_SIZE, socket_id);

  if ((ad->bbdev_dec_op_pool == NULL) || (ad->bbdev_enc_op_pool == NULL))
    AssertFatal(1 == 0, "ERROR Failed to create %u items ops pool for dev %u on socket %d.", ops_pool_size, ad->dev_id, socket_id);

  /* Inputs */
  mbuf_pool_size = optimal_mempool_size(ops_pool_size * nb_segments);
  data_room_size = RTE_MAX(in_max_sz + RTE_PKTMBUF_HEADROOM + FILLER_HEADROOM, (unsigned int)RTE_MBUF_DEFAULT_BUF_SIZE);
  ad->in_mbuf_pool = rte_pktmbuf_pool_create("in_mbuf_pool", mbuf_pool_size, 0, 0, data_room_size, socket_id);
  AssertFatal(ad->in_mbuf_pool != NULL,
              "ERROR Failed to create %u items input pktmbuf pool for dev %u on socket %d.",
              mbuf_pool_size,
              ad->dev_id,
              socket_id);

  /* Hard outputs */
  data_room_size = RTE_MAX(out_buff_sz + RTE_PKTMBUF_HEADROOM + FILLER_HEADROOM, (unsigned int)RTE_MBUF_DEFAULT_BUF_SIZE);
  ad->hard_out_mbuf_pool = rte_pktmbuf_pool_create("hard_out_mbuf_pool", mbuf_pool_size, 0, 0, data_room_size, socket_id);
  AssertFatal(ad->hard_out_mbuf_pool != NULL,
              "ERROR Failed to create %u items hard output pktmbuf pool for dev %u on socket %d.",
              mbuf_pool_size,
              ad->dev_id,
              socket_id);

  /* HARQ outputs */
  data_room_size = LDPC_MAX_CB_SIZE;
  ad->harq_out_mbuf_pool = rte_pktmbuf_pool_create("harq_out_mbuf_pool", mbuf_pool_size, 0, 0, data_room_size, socket_id);
  AssertFatal(ad->harq_out_mbuf_pool != NULL,
              "ERROR Failed to create %u items harq output pktmbuf pool for dev %u on socket %d.",
              mbuf_pool_size,
              ad->dev_id,
              socket_id);

  /* HARQ inputs */
  // Note: This is used as our harq buffer to store the combined outputs across iterations
  data_room_size = LDPC_MAX_CB_SIZE;
  ad->harq_in_mbuf_pool =
      rte_pktmbuf_pool_create("harq_in_mbuf_pool", active_dev.num_harq_codeblock, 0, 0, data_room_size, socket_id);
  AssertFatal(ad->harq_in_mbuf_pool != NULL,
              "ERROR Failed to create %u items harq input pktmbuf pool for dev %u on socket %d.",
              active_dev.num_harq_codeblock,
              ad->dev_id,
              socket_id);

  return 0;
}

const char *ldpcenc_flag_bitmask[] = {
    /** Set for bit-level interleaver bypass on output stream. */
    "RTE_BBDEV_LDPC_INTERLEAVER_BYPASS",
    /** If rate matching is to be performed */
    "RTE_BBDEV_LDPC_RATE_MATCH",
    /** Set for transport block CRC-24A attach */
    "RTE_BBDEV_LDPC_CRC_24A_ATTACH",
    /** Set for code block CRC-24B attach */
    "RTE_BBDEV_LDPC_CRC_24B_ATTACH",
    /** Set for code block CRC-16 attach */
    "RTE_BBDEV_LDPC_CRC_16_ATTACH",
    /** Set if a device supports encoder dequeue interrupts. */
    "RTE_BBDEV_LDPC_ENC_INTERRUPTS",
    /** Set if a device supports scatter-gather functionality. */
    "RTE_BBDEV_LDPC_ENC_SCATTER_GATHER",
    /** Set if a device supports concatenation of non byte aligned output */
    "RTE_BBDEV_LDPC_ENC_CONCATENATION",
};

const char *ldpcdec_flag_bitmask[] = {
    /** Set for transport block CRC-24A checking */
    "RTE_BBDEV_LDPC_CRC_TYPE_24A_CHECK",
    /** Set for code block CRC-24B checking */
    "RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK",
    /** Set to drop the last CRC bits decoding output */
    "RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP"
    /** Set for bit-level de-interleaver bypass on Rx stream. */
    "RTE_BBDEV_LDPC_DEINTERLEAVER_BYPASS",
    /** Set for HARQ combined input stream enable. */
    "RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE",
    /** Set for HARQ combined output stream enable. */
    "RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE",
    /** Set for LDPC decoder bypass.
     *  RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE must be set.
     */
    "RTE_BBDEV_LDPC_DECODE_BYPASS",
    /** Set for soft-output stream enable */
    "RTE_BBDEV_LDPC_SOFT_OUT_ENABLE",
    /** Set for Rate-Matching bypass on soft-out stream. */
    "RTE_BBDEV_LDPC_SOFT_OUT_RM_BYPASS",
    /** Set for bit-level de-interleaver bypass on soft-output stream. */
    "RTE_BBDEV_LDPC_SOFT_OUT_DEINTERLEAVER_BYPASS",
    /** Set for iteration stopping on successful decode condition
     *  i.e. a successful syndrome check.
     */
    "RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE",
    /** Set if a device supports decoder dequeue interrupts. */
    "RTE_BBDEV_LDPC_DEC_INTERRUPTS",
    /** Set if a device supports scatter-gather functionality. */
    "RTE_BBDEV_LDPC_DEC_SCATTER_GATHER",
    /** Set if a device supports input/output HARQ compression. */
    "RTE_BBDEV_LDPC_HARQ_6BIT_COMPRESSION",
    /** Set if a device supports input LLR compression. */
    "RTE_BBDEV_LDPC_LLR_COMPRESSION",
    /** Set if a device supports HARQ input from
     *  device's internal memory.
     */
    "RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE",
    /** Set if a device supports HARQ output to
     *  device's internal memory.
     */
    "RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE",
    /** Set if a device supports loop-back access to
     *  HARQ internal memory. Intended for troubleshooting.
     */
    "RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_LOOPBACK",
    /** Set if a device includes LLR filler bits in the circular buffer
     *  for HARQ memory. If not set, it is assumed the filler bits are not
     *  in HARQ memory and handled directly by the LDPC decoder.
     */
    "RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_FILLERS",
};

void debug_dev_capabilities(uint8_t dev_id, struct rte_bbdev_info *info)
{
  /* Display for debug the capabilities of the card */
  for (int i = 0; info->drv.capabilities[i].type != RTE_BBDEV_OP_NONE; i++) {
    LOG_D(NR_PHY, "device: %d, capability[%d]=%s\n", dev_id, i, rte_bbdev_op_type_str(info->drv.capabilities[i].type));
    if (info->drv.capabilities[i].type == RTE_BBDEV_OP_LDPC_ENC) {
      const struct rte_bbdev_op_cap_ldpc_enc cap = info->drv.capabilities[i].cap.ldpc_enc;
      LOG_D(NR_PHY, "    buffers: src = %d, dst = %d\n   capabilites: ", cap.num_buffers_src, cap.num_buffers_dst);
      for (int j = 0; j < sizeof(cap.capability_flags) * 8; j++)
        if (cap.capability_flags & (1ULL << j))
          LOG_D(NR_PHY, "%s ", ldpcenc_flag_bitmask[j]);
      LOG_D(NR_PHY, "\n");
    }
    if (info->drv.capabilities[i].type == RTE_BBDEV_OP_LDPC_DEC) {
      const struct rte_bbdev_op_cap_ldpc_dec cap = info->drv.capabilities[i].cap.ldpc_dec;
      LOG_D(NR_PHY,
            "    buffers: src = %d, hard out = %d, soft_out %d, llr size %d, llr decimals %d \n   capabilities: ",
            cap.num_buffers_src,
            cap.num_buffers_hard_out,
            cap.num_buffers_soft_out,
            cap.llr_size,
            cap.llr_decimals);
      for (int j = 0; j < sizeof(cap.capability_flags) * 8; j++)
        if (cap.capability_flags & (1ULL << j))
          LOG_D(NR_PHY, "%s ", ldpcdec_flag_bitmask[j]);
      LOG_D(NR_PHY, "\n");
    }
  }
}

void check_required_dev_capabilities(struct rte_bbdev_info *info)
{
  // check ldpc enc/ dec support
  bool ldpc_enc = false;
  bool ldpc_dec = false;
  for (int i = 0; info->drv.capabilities[i].type != RTE_BBDEV_OP_NONE; i++) {
    if (info->drv.capabilities[i].type == RTE_BBDEV_OP_LDPC_ENC) {
      ldpc_enc = true;
    }
    if (info->drv.capabilities[i].type == RTE_BBDEV_OP_LDPC_DEC) {
      ldpc_dec = true;
    }
  }
  AssertFatal(ldpc_enc, "ERROR: bbdev device does not support LDPC encoding\n");
  AssertFatal(ldpc_dec, "ERROR: bbdev device does not support LDPC decoding\n");

  for (int i = 0; info->drv.capabilities[i].type != RTE_BBDEV_OP_NONE; i++) {
    if (info->drv.capabilities[i].type == RTE_BBDEV_OP_LDPC_ENC) {
      // check encoding capabilities
      bool rate_match = check_bit(info->drv.capabilities[i].cap.ldpc_enc.capability_flags, RTE_BBDEV_LDPC_RATE_MATCH);
      AssertFatal(rate_match, "ERROR: bbdev device does not support LDPC encoding with rate matching\n");
    }
    if (info->drv.capabilities[i].type == RTE_BBDEV_OP_LDPC_DEC) {
      // check decoding capabilities
      bool iter_stop = check_bit(info->drv.capabilities[i].cap.ldpc_dec.capability_flags, RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE);
      AssertFatal(iter_stop, "ERROR: bbdev device does not support LDPC decoding with iteration stop\n");

      bool crc_24b_drop = check_bit(info->drv.capabilities[i].cap.ldpc_dec.capability_flags, RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP);
      AssertFatal(crc_24b_drop, "ERROR: bbdev device does not support LDPC decoding with CRC-24B drop\n");

      bool crc_24b_check = check_bit(info->drv.capabilities[i].cap.ldpc_dec.capability_flags, RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK);
      AssertFatal(crc_24b_check, "ERROR: bbdev device does not support LDPC decoding with CRC-24B check\n");
    }
  }
}

bool check_internal_harq_memory_capabilities(struct rte_bbdev_info *info)
{
  for (int i = 0; info->drv.capabilities[i].type != RTE_BBDEV_OP_NONE; i++) {
    if (info->drv.capabilities[i].type == RTE_BBDEV_OP_LDPC_DEC) {
      bool harq_in =
          check_bit(info->drv.capabilities[i].cap.ldpc_dec.capability_flags, RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE);
      bool harq_out =
          check_bit(info->drv.capabilities[i].cap.ldpc_dec.capability_flags, RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE);
      bool internal_harq_memory_support = harq_in & harq_out;
      if (internal_harq_memory_support) {
        LOG_I(NR_PHY, "bbdev device supports internal HARQ memory\n");
      }
      return internal_harq_memory_support;
    }
  }
  return false;
}

// based on DPDK BBDEV add_bbdev_dev
static int add_dev(uint8_t dev_id, bool is_t2, uint32_t num_harq_codeblock)
{
  int ret;
  unsigned int nb_queues;

  // retrieve device capabilities
  rte_bbdev_info_get(dev_id, &active_dev.info);
  LOG_I(NR_PHY, "using bbdev %d: %s\n", dev_id, active_dev.info.dev_name);

  active_dev.driver_name = active_dev.info.drv.driver_name;
  active_dev.dev_id = dev_id;

  nb_queues = RTE_MIN(rte_lcore_count(), active_dev.info.drv.max_num_queues);
  nb_queues = RTE_MIN(nb_queues, (unsigned int)MAX_QUEUES);

  // debug device capabilities
  debug_dev_capabilities(dev_id, &active_dev.info);

  // check required device capabilities
  check_required_dev_capabilities(&active_dev.info);

  // check internal harq memory capabilities
  active_dev.support_internal_harq_memory = check_internal_harq_memory_capabilities(&active_dev.info);

  // setup harq buffers
  active_dev.num_harq_codeblock = num_harq_codeblock;
  active_dev.harq_buffers = malloc(sizeof(struct rte_bbdev_op_data) * active_dev.num_harq_codeblock);

  // is device T2?
  active_dev.is_t2 = is_t2;

  // device setup
  ret = rte_bbdev_setup_queues(dev_id, nb_queues, active_dev.info.socket_id);
  AssertFatal(ret == 0, "rte_bbdev_setup_queues(%u, %u, %d) ret %i\n", dev_id, nb_queues, active_dev.info.socket_id, ret);

  /* setup device queues */
  struct rte_bbdev_queue_conf qconf = {
      .socket = active_dev.info.socket_id,
      .queue_size = active_dev.info.drv.default_queue_conf.queue_size,
  };

  // Search a queue linked to HW capability ldpc decoding
  qconf.op_type = RTE_BBDEV_OP_LDPC_ENC;
  int queue_id;
  for (queue_id = 0; queue_id < nb_queues; ++queue_id) {
    ret = rte_bbdev_queue_configure(dev_id, queue_id, &qconf);
    if (ret == 0) {
      LOG_I(NR_PHY, "Found LDPC encoding queue (id=%u) at prio%u on dev%u\n", queue_id, qconf.priority, dev_id);
      qconf.priority++;
      active_dev.enc_queue = queue_id;
      active_dev.queue_ids[queue_id] = queue_id;
      break;
    }
  }
  AssertFatal(queue_id != nb_queues, "ERROR Failed to configure encoding queues on dev %u", dev_id);

  // Search a queue linked to HW capability ldpc encoding
  qconf.op_type = RTE_BBDEV_OP_LDPC_DEC;
  for (queue_id++; queue_id < nb_queues; ++queue_id) {
    ret = rte_bbdev_queue_configure(dev_id, queue_id, &qconf);
    if (ret == 0) {
      LOG_I(NR_PHY, "Found LDPC decoding queue (id=%u) at prio%u on dev%u\n", queue_id, qconf.priority, dev_id);
      qconf.priority++;
      active_dev.dec_queue = queue_id;
      active_dev.queue_ids[queue_id] = queue_id;
      break;
    }
  }
  AssertFatal(queue_id != nb_queues, "ERROR Failed to configure encoding queues on dev %u", dev_id);
  active_dev.nb_queues = 2;
  return 0;
}

static int init_op_data_objs_harq(struct rte_bbdev_op_data *bufs, struct rte_mempool *mbuf_pool)
{
  for (int i = 0; i < active_dev.num_harq_codeblock; i++) {
    struct rte_mbuf *m_head = rte_pktmbuf_alloc(mbuf_pool);
    AssertFatal(m_head != NULL,
                "Not enough mbufs in HARQ mbuf pool (needed %u, available %u)",
                active_dev.num_harq_codeblock,
                mbuf_pool->size);
    bufs[i].data = m_head;
    bufs[i].offset = 0;
    bufs[i].length = 0;
  }
  return 0;
}

// based on DPDK BBDEV init_op_data_objs
static int init_op_data_objs_dec(struct rte_bbdev_op_data *bufs,
                                 uint8_t *input,
                                 nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters,
                                 struct rte_mempool *mbuf_pool,
                                 enum op_data_type op_type,
                                 uint16_t min_alignment)
{
  bool large_input = false;
  int j = 0;
  for (int h = 0; h < nrLDPC_slot_decoding_parameters->nb_TBs; ++h) {
    nrLDPC_TB_decoding_parameters_t *p = &nrLDPC_slot_decoding_parameters->TBs[h];
    for (int i = 0; i < p->C; ++i) {
      uint32_t data_len = i < p->first_rE2 ? p->E : p->E2;
      char *data;
      struct rte_mbuf *m_head = rte_pktmbuf_alloc(mbuf_pool);
      AssertFatal(m_head != NULL,
                  "Not enough mbufs in %d data type mbuf pool (needed %u, available %u)",
                  op_type,
                  nb_segments_decoding(nrLDPC_slot_decoding_parameters),
                  mbuf_pool->size);

      if (data_len > RTE_BBDEV_LDPC_E_MAX_MBUF) {
        printf("Warning: Larger input size than DPDK mbuf %u\n", data_len);
        large_input = true;
      }
      bufs[j].data = m_head;
      bufs[j].offset = 0;
      bufs[j].length = 0;

      if (op_type == DATA_INPUT) {
        if (large_input) {
          /* Allocate a fake overused mbuf */
          data = rte_malloc(NULL, data_len, 0);
          AssertFatal(data != NULL, "rte malloc failed with %u bytes", data_len);
          memcpy(data, &input[j * LDPC_MAX_CB_SIZE], data_len);
          m_head->buf_addr = data;
          m_head->buf_iova = rte_malloc_virt2iova(data);
          m_head->data_off = 0;
          m_head->data_len = data_len;
        } else {
          rte_pktmbuf_reset(m_head);
          data = rte_pktmbuf_append(m_head, data_len);
          AssertFatal(data != NULL, "Couldn't append %u bytes to mbuf from %d data type mbuf pool", data_len, op_type);
          AssertFatal(data == RTE_PTR_ALIGN(data, min_alignment),
                      "Data addr in mbuf (%p) is not aligned to device min alignment (%u)",
                      data,
                      min_alignment);
          rte_memcpy(data, &input[j * LDPC_MAX_CB_SIZE], data_len);
        }
        bufs[j].length += data_len;
      }
      ++j;
    }
  }
  return 0;
}

// based on DPDK BBDEV init_op_data_objs
static int init_op_data_objs_enc(struct rte_bbdev_op_data *bufs,
                                 nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters,
                                 struct rte_mempool *mbuf_pool,
                                 enum op_data_type op_type,
                                 uint16_t min_alignment)
{
  bool large_input = false;
  int j = 0;
  for (int h = 0; h < nrLDPC_slot_encoding_parameters->nb_TBs; ++h) {
    for (int i = 0; i < nrLDPC_slot_encoding_parameters->TBs[h].C; ++i) {
      uint32_t data_len = (nrLDPC_slot_encoding_parameters->TBs[h].K - nrLDPC_slot_encoding_parameters->TBs[h].F + 7) / 8;
      char *data;
      struct rte_mbuf *m_head = rte_pktmbuf_alloc(mbuf_pool);
      AssertFatal(m_head != NULL,
                  "Not enough mbufs in %d data type mbuf pool (needed %u, available %u)",
                  op_type,
                  nb_segments_encoding(nrLDPC_slot_encoding_parameters),
                  mbuf_pool->size);

      if (data_len > RTE_BBDEV_LDPC_E_MAX_MBUF) {
        printf("Warning: Larger input size than DPDK mbuf %u\n", data_len);
        large_input = true;
      }
      bufs[j].data = m_head;
      bufs[j].offset = 0;
      bufs[j].length = 0;

      if (op_type == DATA_INPUT) {
        if (large_input) {
          /* Allocate a fake overused mbuf */
          data = rte_malloc(NULL, data_len, 0);
          AssertFatal(data != NULL, "rte malloc failed with %u bytes", data_len);
          memcpy(data, nrLDPC_slot_encoding_parameters->TBs[h].segments[i].c, data_len);
          m_head->buf_addr = data;
          m_head->buf_iova = rte_malloc_virt2iova(data);
          m_head->data_off = 0;
          m_head->data_len = data_len;
        } else {
          rte_pktmbuf_reset(m_head);
          data = rte_pktmbuf_append(m_head, data_len);
          AssertFatal(data != NULL, "Couldn't append %u bytes to mbuf from %d data type mbuf pool", data_len, op_type);
          AssertFatal(data == RTE_PTR_ALIGN(data, min_alignment),
                      "Data addr in mbuf (%p) is not aligned to device min alignment (%u)",
                      data,
                      min_alignment);
          rte_memcpy(data, nrLDPC_slot_encoding_parameters->TBs[h].segments[i].c, data_len);
        }
        bufs[j].length += data_len;
      }
      ++j;
    }
  }
  return 0;
}

// DPDK BBEV copy
static int allocate_buffers_on_socket(struct rte_bbdev_op_data **buffers, const int len, const int socket)
{
  int i;

  *buffers = rte_zmalloc_socket(NULL, len, 0, socket);
  if (*buffers == NULL) {
    printf("WARNING: Failed to allocate op_data on socket %d\n", socket);
    /* try to allocate memory on other detected sockets */
    for (i = 0; i < socket; i++) {
      *buffers = rte_zmalloc_socket(NULL, len, 0, i);
      if (*buffers != NULL)
        break;
    }
  }

  return (*buffers == NULL) ? -1 : 0;
}

// DPDK BBDEV copy
static void free_mempools(struct active_device *ad)
{
  rte_mempool_free(ad->bbdev_dec_op_pool);
  rte_mempool_free(ad->bbdev_enc_op_pool);
  rte_mempool_free(ad->in_mbuf_pool);
  rte_mempool_free(ad->hard_out_mbuf_pool);
  rte_mempool_free(ad->harq_in_mbuf_pool);
  rte_mempool_free(ad->harq_out_mbuf_pool);
}

// based on DPDK BBDEV copy_reference_ldpc_dec_op
static void set_ldpc_dec_op(struct rte_bbdev_dec_op **ops,
                            struct rte_bbdev_op_data *inputs,
                            struct rte_bbdev_op_data *outputs,
                            struct rte_bbdev_op_data *harq_outputs,
                            nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters)
{
  int j = 0;
  // The T2 only supports CB mode, and does not TB mode special case handling.
  bool special_case_tb_mode = !active_dev.is_t2 && (nrLDPC_slot_decoding_parameters->nb_TBs == 1)
                              && (nb_segments_decoding(nrLDPC_slot_decoding_parameters) == 1);
  for (int h = 0; h < nrLDPC_slot_decoding_parameters->nb_TBs; ++h) {
    nrLDPC_TB_decoding_parameters_t *p = &nrLDPC_slot_decoding_parameters->TBs[h];
    for (int i = 0; i < p->C; ++i) {
      ops[j]->ldpc_dec.basegraph = p->BG;
      ops[j]->ldpc_dec.z_c = p->Z;
      ops[j]->ldpc_dec.q_m = p->Qm;
      ops[j]->ldpc_dec.n_filler = p->F;
      ops[j]->ldpc_dec.n_cb = (p->BG == 1) ? (66 * p->Z) : (50 * p->Z);
      ops[j]->ldpc_dec.iter_max = p->max_ldpc_iterations;
      ops[j]->ldpc_dec.rv_index = p->rv_index;
      ops[j]->ldpc_dec.op_flags = RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE | RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE;
      if (p->d_to_be_cleared) {
        if (active_dev.is_t2)
          *p->processedSegments = 0;
      } else {
        ops[j]->ldpc_dec.op_flags |= RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE;
        if (active_dev.support_internal_harq_memory) {
          ops[j]->ldpc_dec.op_flags |= RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE;
          ops[j]->ldpc_dec.op_flags |= RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE;
        }
      }

      if (!active_dev.is_t2)
        *p->processedSegments = 0;

      if (!special_case_tb_mode) {
        ops[j]->ldpc_dec.code_block_mode = 1;
        ops[j]->ldpc_dec.cb_params.e = i < p->first_rE2 ? p->E : p->E2;
        if (p->C > 1) {
          ops[j]->ldpc_dec.op_flags |= RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP;
          ops[j]->ldpc_dec.op_flags |= RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK;
        }
      } else {
        /**
         * This is a special case when #TB = 1 and #CB = 1
         * In this case, we must use TB mode
         * Quoted from: https://doc.dpdk.org/guides-23.11/prog_guide/bbdev.html#bbdev-ldpc-decode-operation
         * The case when one CB belongs to TB and is being enqueued individually to BBDEV, this case is considered as a
         * special case of partial TB where its number of CBs is 1. Therefore, it requires to get processed in TB-mode.
         */
        ops[j]->ldpc_dec.code_block_mode = 0;
        ops[j]->ldpc_dec.tb_params.c = 1;
        ops[j]->ldpc_dec.tb_params.r = 0;
        ops[j]->ldpc_dec.tb_params.cab = 1;
        ops[j]->ldpc_dec.tb_params.ea = i < p->first_rE2 ? p->E : p->E2;
        ops[j]->ldpc_dec.tb_params.eb = i < p->first_rE2 ? p->E : p->E2;
      }
      // Calculate offset in the HARQ combined buffers
      // Unique segment offset
      uint32_t segment_offset = (p->harq_unique_pid * NR_LDPC_MAX_NUM_CB) + i;
      // Prune to avoid shooting above maximum id
      uint32_t pruned_segment_offset = segment_offset % active_dev.num_harq_codeblock;
      // Segment offset to byte offset
      uint32_t harq_combined_offset = pruned_segment_offset * LDPC_MAX_CB_SIZE;

      if (active_dev.support_internal_harq_memory) {
        // retrieve corresponding HARQ output information from previous iteration, especially the length
        ops[j]->ldpc_dec.harq_combined_input = active_dev.harq_buffers[pruned_segment_offset];
        // Note: When using INTERNAL_HARQ memory, the "offset" is used to point to a particular address
        // within the BBDEV's onboard memory, and the address should be multiples of 32K.
        harq_outputs[j].offset = harq_combined_offset;
        ops[j]->ldpc_dec.harq_combined_output = harq_outputs[j];
      } else {
        // retrieve corresponding HARQ buffers from previous iteration
        ops[j]->ldpc_dec.harq_combined_input = active_dev.harq_buffers[pruned_segment_offset];
        ops[j]->ldpc_dec.harq_combined_output = harq_outputs[j];
      }
      ops[j]->ldpc_dec.hard_output = outputs[j];
      ops[j]->ldpc_dec.input = inputs[j];
      ++j;
    }
    p->d_to_be_cleared = false;
  }
}

// based on DPDK BBDEV copy_reference_ldpc_enc_op
static void set_ldpc_enc_op(struct rte_bbdev_enc_op **ops,
                            struct rte_bbdev_op_data *inputs,
                            struct rte_bbdev_op_data *outputs,
                            nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters)
{
  int j = 0;
  // The T2 only supports CB mode, and does not TB mode special case handling.
  bool special_case_tb_mode = !active_dev.is_t2 && (nrLDPC_slot_encoding_parameters->nb_TBs == 1)
                              && (nb_segments_encoding(nrLDPC_slot_encoding_parameters) == 1);
  for (int h = 0; h < nrLDPC_slot_encoding_parameters->nb_TBs; ++h) {
    for (int i = 0; i < nrLDPC_slot_encoding_parameters->TBs[h].C; ++i) {
      ops[j]->ldpc_enc.basegraph = nrLDPC_slot_encoding_parameters->TBs[h].BG;
      ops[j]->ldpc_enc.z_c = nrLDPC_slot_encoding_parameters->TBs[h].Z;
      ops[j]->ldpc_enc.q_m = nrLDPC_slot_encoding_parameters->TBs[h].Qm;
      ops[j]->ldpc_enc.n_filler = nrLDPC_slot_encoding_parameters->TBs[h].F;
      ops[j]->ldpc_enc.n_cb = (nrLDPC_slot_encoding_parameters->TBs[h].BG == 1) ? (66 * nrLDPC_slot_encoding_parameters->TBs[h].Z)
                                                                                : (50 * nrLDPC_slot_encoding_parameters->TBs[h].Z);
      if (nrLDPC_slot_encoding_parameters->TBs[h].tbslbrm != 0) {
        uint32_t Nref = 3 * nrLDPC_slot_encoding_parameters->TBs[h].tbslbrm / (2 * nrLDPC_slot_encoding_parameters->TBs[h].C);
        ops[j]->ldpc_enc.n_cb = min(ops[j]->ldpc_enc.n_cb, Nref);
      }
      ops[j]->ldpc_enc.rv_index = nrLDPC_slot_encoding_parameters->TBs[h].rv_index;
      ops[j]->ldpc_enc.op_flags = RTE_BBDEV_LDPC_RATE_MATCH;
      if (!special_case_tb_mode) {
        ops[j]->ldpc_enc.code_block_mode = 1;
        ops[j]->ldpc_enc.cb_params.e = nrLDPC_slot_encoding_parameters->TBs[h].segments[i].E;
      } else {
        /**
         * This is a special case when #TB = 1 and #CB = 1
         * In this case, we must use TB mode
         * Quoted from: https://doc.dpdk.org/guides-23.11/prog_guide/bbdev.html#bbdev-ldpc-decode-operation
         * The case when one CB belongs to TB and is being enqueued individually to BBDEV, this case is considered as a
         * special case of partial TB where its number of CBs is 1. Therefore, it requires to get processed in TB-mode.
         */
        ops[j]->ldpc_enc.code_block_mode = 0;
        ops[j]->ldpc_enc.tb_params.c = 1;
        ops[j]->ldpc_enc.tb_params.r = 0;
        ops[j]->ldpc_enc.tb_params.cab = 1;
        ops[j]->ldpc_enc.tb_params.ea = nrLDPC_slot_encoding_parameters->TBs[h].segments[i].E;
        ops[j]->ldpc_enc.tb_params.eb = nrLDPC_slot_encoding_parameters->TBs[h].segments[i].E;
      }
      ops[j]->ldpc_enc.output = outputs[j];
      ops[j]->ldpc_enc.input = inputs[j];
      ++j;
    }
  }
}

static int retrieve_ldpc_dec_op(struct rte_bbdev_dec_op **ops, nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters)
{
  int j = 0;
  for (int h = 0; h < nrLDPC_slot_decoding_parameters->nb_TBs; ++h) {
    nrLDPC_TB_decoding_parameters_t *p = &nrLDPC_slot_decoding_parameters->TBs[h];
    size_t data_off = 0;
    for (int i = 0; i < p->C; ++i) {
      struct rte_bbdev_op_data *hard_output = &ops[j]->ldpc_dec.hard_output;
      struct rte_mbuf *m = hard_output->data;
      uint16_t data_len = rte_pktmbuf_data_len(m) - hard_output->offset;
      uint8_t *data = rte_pktmbuf_mtod_offset(m, uint8_t *, hard_output->offset);
      memcpy(p->c + data_off, data, data_len);

      uint32_t segment_offset = (p->harq_unique_pid * NR_LDPC_MAX_NUM_CB) + i;
      uint32_t pruned_segment_offset = segment_offset % active_dev.num_harq_codeblock;
      struct rte_bbdev_op_data *harq_output = &ops[j]->ldpc_dec.harq_combined_output;
      if (!active_dev.support_internal_harq_memory) {
        struct rte_mbuf *m_src = harq_output->data;
        uint8_t *data_src = rte_pktmbuf_mtod_offset(m_src, uint8_t *, 0);
        struct rte_mbuf *m_dst = active_dev.harq_buffers[pruned_segment_offset].data;
        uint8_t *data_dst = rte_pktmbuf_mtod_offset(m_dst, uint8_t *, 0);
        rte_memcpy(data_dst, data_src, harq_output->length);
      }
      active_dev.harq_buffers[pruned_segment_offset].offset = harq_output->offset;
      active_dev.harq_buffers[pruned_segment_offset].length = harq_output->length;
      ++j;
      data_off += p->K >> 3;
    }
  }
  return 0;
}

static int retrieve_ldpc_enc_op(struct rte_bbdev_enc_op **ops, nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters)
{
  uint8_t *p_out = NULL;
  int j = 0;
  for (int h = 0; h < nrLDPC_slot_encoding_parameters->nb_TBs; ++h) {
    int E_sum = 0;
    int bit_offset = 0;
    int byte_offset = 0;
    p_out = nrLDPC_slot_encoding_parameters->TBs[h].output;
    for (int r = 0; r < nrLDPC_slot_encoding_parameters->TBs[h].C; ++r) {
      struct rte_bbdev_op_data *output = &ops[j]->ldpc_enc.output;
      struct rte_mbuf *m = output->data;
      uint16_t data_len = rte_pktmbuf_data_len(m) - output->offset;
      uint8_t *data = rte_pktmbuf_mtod_offset(m, uint8_t *, output->offset);
      reverse_bits_u8(data, data_len, data);
      if (bit_offset == 0) {
        memcpy(&p_out[byte_offset], data, data_len);
      } else {
        size_t i = 0;
        for (; i < (data_len & ~0x7); i += 8) {
          uint8_t carry = *data << bit_offset;
          p_out[byte_offset + i - 1] |= carry;

          simde__m64 current = *((simde__m64 *)data);
          data += 8;
          current = simde_mm_srli_si64(current, 8 - bit_offset);
          memcpy(&p_out[byte_offset + i], &current, sizeof(simde__m64));
        }
        for (; i < data_len; i++) {
          uint8_t current = *data++;

          uint8_t carry = current << bit_offset;
          p_out[byte_offset + i - 1] |= carry;

          p_out[byte_offset + i] = (current >> (8 - bit_offset));
        }
      }
      E_sum += nrLDPC_slot_encoding_parameters->TBs[h].segments[r].E;
      byte_offset = (E_sum + 7) / 8;
      bit_offset = E_sum % 8;
      ++j;
    }
  }
  return 0;
}

// based on DPDK BBDEV throughput_pmd_lcore_ldpc_dec
static int pmd_lcore_ldpc_dec(void *arg)
{
  struct thread_params *tp = arg;
  nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters = tp->nrLDPC_slot_decoding_parameters;
  int time_out = 0;
  const uint16_t queue_id = tp->queue_id;
  const uint16_t num_segments = nb_segments_decoding(nrLDPC_slot_decoding_parameters);
  struct rte_bbdev_dec_op *ops_enq[num_segments];
  struct rte_bbdev_dec_op *ops_deq[num_segments];
  struct data_buffers *bufs = tp->data_buffers;

  AssertFatal((num_segments < MAX_BURST), "BURST_SIZE should be <= %u", MAX_BURST);

  while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
    rte_pause();

  int ret = rte_bbdev_dec_op_alloc_bulk(tp->bbdev_op_pool, ops_enq, num_segments);
  AssertFatal(ret == 0, "Allocation failed for %d ops", num_segments);
  set_ldpc_dec_op(ops_enq, bufs->inputs, bufs->hard_outputs, bufs->harq_outputs, nrLDPC_slot_decoding_parameters);

  // Start timer
  // We report timing only once in (0,0) since the timers are merged at the end
  time_stats_t *ts_decode = &nrLDPC_slot_decoding_parameters->TBs[0].ts_ldpc_decode;
  if(ts_decode != NULL) start_meas(ts_decode);

  uint16_t enq = 0, deq = 0;
  while (enq < num_segments) {
    uint16_t num_to_enq = num_segments - enq;
    enq += rte_bbdev_enqueue_ldpc_dec_ops(tp->dev_id, queue_id, &ops_enq[enq], num_to_enq);
    deq += rte_bbdev_dequeue_ldpc_dec_ops(tp->dev_id, queue_id, &ops_deq[deq], enq - deq);
  }
  /* dequeue the remaining */
  while (deq < enq) {
    deq += rte_bbdev_dequeue_ldpc_dec_ops(tp->dev_id, queue_id, &ops_deq[deq], enq - deq);
    time_out++;
    DevAssert(time_out <= TIME_OUT_POLL);
  }

  // Stop timer
  if(ts_decode != NULL) stop_meas(ts_decode);

  if (deq == enq) {
    ret = retrieve_ldpc_dec_op(ops_deq, nrLDPC_slot_decoding_parameters);
    AssertFatal(ret == 0, "LDPC offload decoder failed!");
    tp->iter_count = 0;
    /* get the max of iter_count for all dequeued ops */
    int j = 0;
    for (int h = 0; h < nrLDPC_slot_decoding_parameters->nb_TBs; ++h) {
      nrLDPC_TB_decoding_parameters_t *p = &nrLDPC_slot_decoding_parameters->TBs[h];
      for (int i = 0; i < p->C; ++i) {
        bool *status = &p->decodeSuccess[i];
        tp->iter_count = RTE_MAX(ops_enq[j]->ldpc_dec.iter_count, tp->iter_count);

        // Check if CRC is available otherwise rely on ops_enq[j]->status to detect decoding success
        // CRC is NOT available if the CRC type is 24_B which is when C is greater than 1
        if (p->C > 1) {
          *status = (ops_enq[j]->status == 0);
        } else {
          uint8_t *decoded_bytes = p->c;
          uint8_t crc_type = crcType(p->C, p->A);
          uint32_t len_with_crc = lenWithCrc(p->C, p->A);
          *status = check_crc(decoded_bytes, len_with_crc, crc_type);
        }

        if (*status) {
          *p->processedSegments = *p->processedSegments + 1;
        }
        ++j;
      }
    }
  }

  rte_bbdev_dec_op_free_bulk(ops_enq, num_segments);
  // Return the worst decoding number of iterations for all segments
  return tp->iter_count;
}

// based on DPDK BBDEV throughput_pmd_lcore_ldpc_enc
static int pmd_lcore_ldpc_enc(void *arg)
{
  struct thread_params *tp = arg;
  nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters = tp->nrLDPC_slot_encoding_parameters;
  int time_out = 0;
  const uint16_t queue_id = tp->queue_id;
  const uint16_t num_segments = nb_segments_encoding(nrLDPC_slot_encoding_parameters);
  struct rte_bbdev_enc_op *ops_enq[num_segments];
  struct rte_bbdev_enc_op *ops_deq[num_segments];
  struct data_buffers *bufs = tp->data_buffers;

  AssertFatal((num_segments < MAX_BURST), "BURST_SIZE should be <= %u", MAX_BURST);

  while (rte_atomic16_read(&tp->op_params->sync) == SYNC_WAIT)
    rte_pause();

  int ret = rte_bbdev_enc_op_alloc_bulk(tp->bbdev_op_pool, ops_enq, num_segments);
  AssertFatal(ret == 0, "Allocation failed for %d ops", num_segments);
  set_ldpc_enc_op(ops_enq, bufs->inputs, bufs->hard_outputs, nrLDPC_slot_encoding_parameters);

  // We report timing only once in (0,0) since the timers are merged at the end
  time_stats_t *ts_ldpc_encode = &nrLDPC_slot_encoding_parameters->TBs[0].segments[0].ts_ldpc_encode;
  if(ts_ldpc_encode != NULL) start_meas(ts_ldpc_encode);

  uint16_t enq = 0, deq = 0;
  while (enq < num_segments) {
    uint16_t num_to_enq = num_segments - enq;
    enq += rte_bbdev_enqueue_ldpc_enc_ops(tp->dev_id, queue_id, &ops_enq[enq], num_to_enq);
    deq += rte_bbdev_dequeue_ldpc_enc_ops(tp->dev_id, queue_id, &ops_deq[deq], enq - deq);
  }
  /* dequeue the remaining */
  while (deq < enq) {
    deq += rte_bbdev_dequeue_ldpc_enc_ops(tp->dev_id, queue_id, &ops_deq[deq], enq - deq);
    time_out++;
    DevAssert(time_out <= TIME_OUT_POLL);
  }
  if(ts_ldpc_encode != NULL) stop_meas(ts_ldpc_encode);
  // We report timing only once in (0,0) since the timers are merged at the end
  ret = retrieve_ldpc_enc_op(ops_deq, nrLDPC_slot_encoding_parameters);
  AssertFatal(ret == 0, "Failed to retrieve LDPC encoding op!");
  rte_bbdev_enc_op_free_bulk(ops_enq, num_segments);
  return ret;
}

// based on DPDK BBDEV throughput_pmd_lcore_dec
int start_pmd_dec(struct active_device *ad,
                  struct test_op_params *op_params,
                  struct data_buffers *data_buffers,
                  nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters)
{
  unsigned int lcore_id, used_cores = 0;
  /* Set number of lcores */
  int num_lcores = (ad->nb_queues < (op_params->num_lcores)) ? ad->nb_queues : op_params->num_lcores;
  /* Allocate memory for thread parameters structure */
  struct thread_params *t_params = rte_zmalloc(NULL, num_lcores * sizeof(struct thread_params), RTE_CACHE_LINE_SIZE);
  AssertFatal(t_params != 0,
              "Failed to alloc %zuB for t_params",
              RTE_ALIGN(sizeof(struct thread_params) * num_lcores, RTE_CACHE_LINE_SIZE));
  rte_atomic16_set(&op_params->sync, SYNC_WAIT);
  /* Master core is set at first entry */
  t_params[0].dev_id = ad->dev_id;
  t_params[0].lcore_id = rte_lcore_id();
  t_params[0].op_params = op_params;
  t_params[0].data_buffers = data_buffers;
  t_params[0].bbdev_op_pool = ad->bbdev_dec_op_pool;
  t_params[0].queue_id = ad->dec_queue;
  t_params[0].iter_count = 0;
  t_params[0].nrLDPC_slot_decoding_parameters = nrLDPC_slot_decoding_parameters;
  used_cores++;
  // For now, we never enter here, we don't use the DPDK thread pool
  RTE_LCORE_FOREACH_WORKER(lcore_id)
  {
    if (used_cores >= num_lcores)
      break;
    t_params[used_cores].dev_id = ad->dev_id;
    t_params[used_cores].lcore_id = lcore_id;
    t_params[used_cores].op_params = op_params;
    t_params[used_cores].data_buffers = data_buffers;
    t_params[used_cores].bbdev_op_pool = ad->bbdev_dec_op_pool;
    t_params[used_cores].queue_id = ad->queue_ids[used_cores];
    t_params[used_cores].iter_count = 0;
    t_params[used_cores].nrLDPC_slot_decoding_parameters = nrLDPC_slot_decoding_parameters;
    rte_eal_remote_launch(pmd_lcore_ldpc_dec, &t_params[used_cores++], lcore_id);
  }
  rte_atomic16_set(&op_params->sync, SYNC_START);
  int ret = pmd_lcore_ldpc_dec(&t_params[0]);
  /* Master core is always used */
  // for (used_cores = 1; used_cores < num_lcores; used_cores++)
  //	ret |= rte_eal_wait_lcore(t_params[used_cores].lcore_id);
  rte_free(t_params);
  return ret;
}

// based on DPDK BBDEV throughput_pmd_lcore_enc
int32_t start_pmd_enc(struct active_device *ad,
                      struct test_op_params *op_params,
                      struct data_buffers *data_buffers,
                      nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters)
{
  unsigned int lcore_id, used_cores = 0;
  uint16_t num_lcores;
  int ret;
  num_lcores = (ad->nb_queues < (op_params->num_lcores)) ? ad->nb_queues : op_params->num_lcores;
  struct thread_params *t_params = rte_zmalloc(NULL, num_lcores * sizeof(struct thread_params), RTE_CACHE_LINE_SIZE);
  rte_atomic16_set(&op_params->sync, SYNC_WAIT);
  t_params[0].dev_id = ad->dev_id;
  t_params[0].lcore_id = rte_lcore_id() + 1;
  t_params[0].op_params = op_params;
  t_params[0].data_buffers = data_buffers;
  t_params[0].bbdev_op_pool = ad->bbdev_enc_op_pool;
  t_params[0].queue_id = ad->enc_queue;
  t_params[0].iter_count = 0;
  t_params[0].nrLDPC_slot_encoding_parameters = nrLDPC_slot_encoding_parameters;
  used_cores++;
  // For now, we never enter here, we don't use the DPDK thread pool
  RTE_LCORE_FOREACH_WORKER(lcore_id)
  {
    if (used_cores >= num_lcores)
      break;
    t_params[used_cores].dev_id = ad->dev_id;
    t_params[used_cores].lcore_id = lcore_id;
    t_params[used_cores].op_params = op_params;
    t_params[used_cores].data_buffers = data_buffers;
    t_params[used_cores].bbdev_op_pool = ad->bbdev_enc_op_pool;
    t_params[used_cores].queue_id = ad->queue_ids[1];
    t_params[used_cores].iter_count = 0;
    t_params[used_cores].nrLDPC_slot_encoding_parameters = nrLDPC_slot_encoding_parameters;
    rte_eal_remote_launch(pmd_lcore_ldpc_enc, &t_params[used_cores++], lcore_id);
  }
  rte_atomic16_set(&op_params->sync, SYNC_START);
  ret = pmd_lcore_ldpc_enc(&t_params[0]);
  rte_free(t_params);
  return ret;
}

struct test_op_params *op_params = NULL;

static int normalize_dpdk_dev(const char *input, char *output, size_t out_len)
{
  regex_t regex_full, regex_short;
  int reti;

  // patterns
  const char *pattern_full = "^[0]{4}:[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\\.[0-7]$";
  const char *pattern_short = "^[0-9a-fA-F]{2}:[0-9a-fA-F]{2}\\.[0-7]$";
  regcomp(&regex_full, pattern_full, REG_EXTENDED);
  regcomp(&regex_short, pattern_short, REG_EXTENDED);

  // check full format
  reti = regexec(&regex_full, input, 0, NULL, 0);
  if (reti == 0) {
    // already in full format
    strncpy(output, input, out_len - 1);
    output[out_len - 1] = '\0';
  } else {
    // check short format
    reti = regexec(&regex_short, input, 0, NULL, 0);
    if (reti == 0) {
      // convert to full format
      snprintf(output, out_len, "0000:%s", input);
    } else {
      // invalid format
      regfree(&regex_full);
      regfree(&regex_short);
      return -1;
    }
  }
  regfree(&regex_full);
  regfree(&regex_short);
  return 0;
}

// OAI CODE
int32_t nrLDPC_coding_init(int max_num_pxsch)
{
  pthread_mutex_init(&encode_mutex, NULL);
  pthread_mutex_init(&decode_mutex, NULL);

  int ret;
  int dev_id = -1;

  char *dpdk_dev = NULL; // PCI address of the card
  char *dpdk_core_list = NULL; // cores used by DPDK for bbdev
  char *dpdk_file_prefix = NULL;
  char *vfio_vf_token = NULL; // vfio token for the bbdev card
  uint32_t num_harq_codeblock = 0; // size of the HARQ buffer in terms of the number of 32K blocks
  uint32_t is_t2 = 0;
  paramdef_t LoaderParams[] = {
      {"dpdk_dev", NULL, 0, .strptr = &dpdk_dev, .defstrval = NULL, TYPE_STRING, 0, NULL},
      {"dpdk_core_list", NULL, 0, .strptr = &dpdk_core_list, .defstrval = NULL, TYPE_STRING, 0, NULL},
      {"dpdk_file_prefix", NULL, 0, .strptr = &dpdk_file_prefix, .defstrval = "b6", TYPE_STRING, 0, NULL},
      {"vfio_vf_token", NULL, 0, .strptr = &vfio_vf_token, .defstrval = NULL, TYPE_STRING, 0, NULL},
      {"num_harq_codeblock", NULL, 0, .uptr = &num_harq_codeblock, .defintval = 512, TYPE_UINT32, 0, NULL},
      {"is_t2", NULL, 0, .uptr = &is_t2, .defintval = 0, TYPE_UINT8, 0, NULL},
  };
  config_get(config_get_if(), LoaderParams, sizeofArray(LoaderParams), "nrLDPC_coding_aal");
  if (dpdk_dev == NULL)
    LOG_E(NR_PHY,
          "could not find mandatory --nrLDPC_coding_aal.dpdk_dev. If you used --nrLDPC_coding_t2.*, please rename all options to "
          "--nrLDPC_coding_aal.*\n");
  AssertFatal(dpdk_dev != NULL, "nrLDPC_coding_aal.dpdk_dev was not provided");

  char dpdk_dev_full[32];
  if (normalize_dpdk_dev(dpdk_dev, dpdk_dev_full, sizeof(dpdk_dev_full)) != 0) {
    LOG_E(NR_PHY, "invalid DPDK device format: %s\n", dpdk_dev);
    return -1;
  }

  // Detect if EAL was initialized by probing the device
  LOG_I(NR_PHY, "Probing DPDK device %s to know if EAL is initialized. This may generate EAL error messages\n", dpdk_dev_full);
  if (rte_dev_probe(dpdk_dev_full) != 0) {
    LOG_I(NR_PHY, "Probing DPDK device %s failed, initializing EAL before continuing\n", dpdk_dev_full);
    // EAL was not initialized yet
    // We initialize EAL
    AssertFatal(dpdk_core_list != NULL, "nrLDPC_coding_aal.dpdk_core_list was not provided");

    int argc = 7;
    char *argv[11] =
        {"bbdev", "-l", dpdk_core_list, "-a", dpdk_dev_full, "--file-prefix", dpdk_file_prefix, "--", "--", "--", "--"};
    if (vfio_vf_token != NULL) {
      argc += 2;
      argv[7] = "--vfio-vf-token";
      argv[8] = vfio_vf_token;
    }
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
      LOG_E(NR_PHY, "EAL initialization failed\n");
      return (-1);
    }
  } else {
    // EAL was already initialized
    LOG_I(NR_PHY, "Probing DPDK device %s succeeded, skipping EAL initialization\n", dpdk_dev_full);
  }
  uint16_t nb_bbdevs = rte_bbdev_count();
  AssertFatal(nb_bbdevs > 0, "no bbdev found");

  // find the baseband device that matches the dpdk_dev specified in the configurations
  struct rte_bbdev_info info;
  LOG_I(NR_PHY, "detected %u bbdev.\n", nb_bbdevs);
  for (uint16_t device_id = 0; device_id < nb_bbdevs; device_id++) {
    rte_bbdev_info_get(device_id, &info);
    // check if info matches the dpdk_dev that we are looking for
    if (strcmp(info.dev_name, dpdk_dev_full) == 0) {
      LOG_I(NR_PHY, "bbdev %s found.\n", info.dev_name);
      dev_id = device_id;
      break;
    }
  }
  AssertFatal(dev_id != -1, "bbdev %s not found.", dpdk_dev_full);

  AssertFatal(add_dev(dev_id, is_t2, num_harq_codeblock) == 0, "Failed to setup bbdev");
  AssertFatal(rte_bbdev_stats_reset(dev_id) == 0, "Failed to reset stats of bbdev %u", dev_id);
  AssertFatal(rte_bbdev_start(dev_id) == 0, "Failed to start bbdev %u", dev_id);

  // the previous calls have populated this global variable (beurk)
  //  One more global to remove, not thread safe global op_params
  op_params = rte_zmalloc(NULL, sizeof(struct test_op_params), RTE_CACHE_LINE_SIZE);
  AssertFatal(op_params != NULL,
              "Failed to alloc %zuB for op_params",
              RTE_ALIGN(sizeof(struct test_op_params), RTE_CACHE_LINE_SIZE));

  int socket_id = GET_SOCKET(info.socket_id);
  int out_max_sz = 8448; // max code block size (for BG1), 22 * 384
  int in_max_sz = LDPC_MAX_CB_SIZE; // max number of encoded bits (for BG2 and MCS0)
  int num_queues = 1;
  int f_ret = create_mempools(&active_dev, socket_id, num_queues, out_max_sz, in_max_sz);
  if (f_ret != 0) {
    printf("Couldn't create mempools");
    return -1;
  }

  // initialize persistent data structure to keep track of HARQ-related information
  init_op_data_objs_harq(active_dev.harq_buffers, active_dev.harq_in_mbuf_pool);

  op_params->num_lcores = 1;
  return 0;
}

int32_t nrLDPC_coding_shutdown()
{
  int dev_id = 0;
  struct rte_bbdev_stats stats;
  free(active_dev.harq_buffers);
  free_mempools(&active_dev);
  rte_free(op_params);
  rte_bbdev_stats_get(dev_id, &stats);
  rte_bbdev_stop(dev_id);
  rte_bbdev_close(dev_id);
  memset(&active_dev, 0, sizeof(active_dev));
  return 0;
}

static void
llr_scaling(int16_t *llr, int llr_len, uint8_t *llr_scaled, int8_t llr_size, int8_t llr_decimal, int8_t nb_layers, int8_t Qm)
{
  const int16_t llr_max = (1 << (llr_size - 1)) - 1;
  const int16_t llr_min = -llr_max;

  // Step 1: Find the max absolute LLR
  int16_t max_abs = 1; // prevent divide-by-zero
  // SCALAR IMPLEMENTATION
  // for (int i = 0; i < llr_len; i++) {
  //     int16_t abs_val = abs(llr[i]);
  //     if (abs_val > max_abs) max_abs = abs_val;
  // }
  // VECTORIZED IMPLEMENTATION
  simde__m128i max_vec = simde_mm_set1_epi16(1);
  for (int i = 0; i < llr_len; i += 8) {
    simde__m128i llr_vec = simde_mm_loadu_si128((simde__m128i *)&llr[i]);
    simde__m128i abs_vec = simde_mm_abs_epi16(llr_vec);
    max_vec = simde_mm_max_epi16(max_vec, abs_vec);
  }
  // reduce max_vec to single max_abs
  int16_t temp[8];
  simde_mm_storeu_si128((simde__m128i *)temp, max_vec);
  for (int i = 0; i < 8; i++) {
    if (temp[i] > max_abs)
      max_abs = temp[i];
  }

  // Step 2: Compute dynamic scale factor
  float fixed_point_range = (float)llr_max / (1 << llr_decimal);
  float scale = fixed_point_range / (float)max_abs;
  if (max_abs < fixed_point_range) {
    scale = 1.0f;
  }

  // Step 3: Scale and saturate
  // SCALAR IMPLEMENTATION
  // for (int i = 0; i < llr_len; i++) {
  //     float scaled = (float)llr[i] * scale; // map into fixed-point domain
  //     scaled = (int8_t)roundf(scaled * (1 << llr_decimal));
  //     // Clamp to [-128, 127]
  //     if (scaled > llr_max) llr_scaled[i] = llr_max;
  //     else if (scaled < llr_min) llr_scaled[i] = llr_min;
  // }
  // VECTORIZED IMPLEMENTATION
  simde__m128 scale_vec = simde_mm_set1_ps(scale);
  simde__m128i llr_max_vec = simde_mm_set1_epi16(llr_max);
  simde__m128i llr_min_vec = simde_mm_set1_epi16(llr_min);
  simde__m128i decimal_shift = simde_mm_set1_epi16(1 << llr_decimal);
  for (int i = 0; i < llr_len; i += 8) {
    // load LLR values
    simde__m128i llr_vec = simde_mm_loadu_si128((simde__m128i *)&llr[i]);

    // convert to float for scaling
    simde__m128i llr_lo = simde_mm_cvtepi16_epi32(llr_vec);
    simde__m128i llr_hi = simde_mm_cvtepi16_epi32(simde_mm_srli_si128(llr_vec, 8));
    simde__m128 float_lo = simde_mm_cvtepi32_ps(llr_lo);
    simde__m128 float_hi = simde_mm_cvtepi32_ps(llr_hi);

    // scale
    float_lo = simde_mm_mul_ps(float_lo, scale_vec);
    float_hi = simde_mm_mul_ps(float_hi, scale_vec);

    // convert back to int16 with saturation
    llr_lo = simde_mm_cvtps_epi32(float_lo);
    llr_hi = simde_mm_cvtps_epi32(float_hi);
    simde__m128i scaled_vec = simde_mm_packs_epi32(llr_lo, llr_hi);

    scaled_vec = simde_mm_mullo_epi16(scaled_vec, decimal_shift);

    // clamp to [llr_min, llr_max]
    scaled_vec = simde_mm_min_epi16(scaled_vec, llr_max_vec);
    scaled_vec = simde_mm_max_epi16(scaled_vec, llr_min_vec);

    // Pack to int8 and store
    simde__m128i result = simde_mm_packs_epi16(scaled_vec, simde_mm_setzero_si128());
    simde_mm_storeu_si128((simde__m128i *)&llr_scaled[i], result);
  }
}

int32_t nrLDPC_coding_decoder(nrLDPC_slot_decoding_parameters_t *nrLDPC_slot_decoding_parameters)
{
  pthread_mutex_lock(&decode_mutex);

  int ret;
  int socket_id = active_dev.info.socket_id;

  const uint16_t num_segments = nb_segments_decoding(nrLDPC_slot_decoding_parameters);

  /* It is not unlikely that l_ol becomes big enough to overflow the stack
   * If you observe this behavior then move it to the heap
   * Then you would better do a persistent allocation to limit the overhead
   */
  uint8_t l_ol[num_segments * LDPC_MAX_CB_SIZE] __attribute__((aligned(16)));

  // fill_queue_buffers -> init_op_data_objs
  struct rte_mempool *mbuf_pools[DATA_NUM_TYPES] = {active_dev.in_mbuf_pool,
                                                    active_dev.hard_out_mbuf_pool,
                                                    active_dev.harq_out_mbuf_pool};
  struct data_buffers data_buffers;
  struct rte_bbdev_op_data **queue_ops[DATA_NUM_TYPES] = {&data_buffers.inputs,
                                                          &data_buffers.hard_outputs,
                                                          &data_buffers.harq_outputs};
  int8_t llr_size = (active_dev.info.drv.capabilities)[RTE_BBDEV_OP_LDPC_DEC].cap.ldpc_dec.llr_size;
  int8_t llr_decimal = (active_dev.info.drv.capabilities)[RTE_BBDEV_OP_LDPC_DEC].cap.ldpc_dec.llr_decimals;
  int offset = 0;
  for (int h = 0; h < nrLDPC_slot_decoding_parameters->nb_TBs; ++h) {
    nrLDPC_TB_decoding_parameters_t *p = &nrLDPC_slot_decoding_parameters->TBs[h];
    size_t data_off = 0;
    for (int r = 0; r < p->C; r++) {
      uint32_t E = r < p->first_rE2 ? p->E : p->E2;
      if (active_dev.is_t2) {
        // For the T2, we simply saturate the LLRs.
        uint16_t z_ol[LDPC_MAX_CB_SIZE] __attribute__((aligned(16)));
        memcpy(z_ol, p->llr + data_off, E * sizeof(uint16_t));
        simde__m128i *pv_ol128 = (simde__m128i *)z_ol;
        simde__m128i *pl_ol128 = (simde__m128i *)&l_ol[offset];
        for (int i = 0, j = 0; j < ((E + 15) >> 4); i += 2, j++) {
          pl_ol128[j] = simde_mm_packs_epi16(pv_ol128[i], pv_ol128[i + 1]);
        }
      } else {
        llr_scaling(p->llr + data_off, E, &l_ol[offset], llr_size, llr_decimal, p->nb_layers, p->Qm);
      }
      offset += LDPC_MAX_CB_SIZE;
      data_off += E;
    }
  }

  for (enum op_data_type type = DATA_INPUT; type < DATA_NUM_TYPES; ++type) {
    ret = allocate_buffers_on_socket(queue_ops[type], num_segments * sizeof(struct rte_bbdev_op_data), socket_id);
    AssertFatal(ret == 0, "Couldn't allocate memory for rte_bbdev_op_data structs");
    ret = init_op_data_objs_dec(*queue_ops[type],
                                l_ol,
                                nrLDPC_slot_decoding_parameters,
                                mbuf_pools[type],
                                type,
                                active_dev.info.drv.min_alignment);
    AssertFatal(ret == 0, "Couldn't init rte_bbdev_op_data structs");
  }

  ret = start_pmd_dec(&active_dev, op_params, &data_buffers, nrLDPC_slot_decoding_parameters);
  if (ret < 0) {
    LOG_E(NR_PHY, "Couldn't start pmd dec\n");
  }

  for (enum op_data_type type = DATA_INPUT; type < DATA_NUM_TYPES; ++type) {
    for (int segment = 0; segment < num_segments; ++segment)
      rte_pktmbuf_free((*queue_ops[type])[segment].data);
    rte_free(*queue_ops[type]);
  }

  pthread_mutex_unlock(&decode_mutex);
  return 0;
}

int32_t nrLDPC_coding_encoder(nrLDPC_slot_encoding_parameters_t *nrLDPC_slot_encoding_parameters)
{
  pthread_mutex_lock(&encode_mutex);

  const uint16_t num_segments = nb_segments_encoding(nrLDPC_slot_encoding_parameters);

  int ret;
  int socket_id = active_dev.info.socket_id;

  // fill_queue_buffers -> init_op_data_objs
  struct rte_mempool *mbuf_pools[2] = {active_dev.in_mbuf_pool, active_dev.hard_out_mbuf_pool};
  struct data_buffers data_buffers;
  struct rte_bbdev_op_data **queue_ops[2] = {&data_buffers.inputs, &data_buffers.hard_outputs};

  for (enum op_data_type type = DATA_INPUT; type < 2; ++type) {
    ret = allocate_buffers_on_socket(queue_ops[type], num_segments * sizeof(struct rte_bbdev_op_data), socket_id);
    AssertFatal(ret == 0, "Couldn't allocate memory for rte_bbdev_op_data structs");
    ret = init_op_data_objs_enc(*queue_ops[type],
                                nrLDPC_slot_encoding_parameters,
                                mbuf_pools[type],
                                type,
                                active_dev.info.drv.min_alignment);
    AssertFatal(ret == 0, "Couldn't init rte_bbdev_op_data structs");
  }

  ret = start_pmd_enc(&active_dev, op_params, &data_buffers, nrLDPC_slot_encoding_parameters);

  for (enum op_data_type type = DATA_INPUT; type < 2; ++type) {
    for (int segment = 0; segment < num_segments; ++segment)
      rte_pktmbuf_free((*queue_ops[type])[segment].data);
    rte_free(*queue_ops[type]);
  }

  pthread_mutex_unlock(&encode_mutex);
  return ret;
}
