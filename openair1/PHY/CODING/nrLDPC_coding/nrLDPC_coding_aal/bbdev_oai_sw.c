/*
 * baseband_oai_sw: software BBDev virtual device (vdev) acting as an LDPC LPU behind the DPDK BBDev API,
 * so that libldpc_aal.so (nrLDPC_coding_aal.c) runs without an FPGA.
 *
 * The processing kernels are not reimplemented: the driver dlopen()s (RTLD_LOCAL) the libldpc.so next to it
 * and uses its LDPCencoder, LDPCdecoder, nr_rate_matching_ldpc(_rx), nr_interleaving_ldpc and
 * nr_deinterleaving_ldpc, so the device runs the same code as the CPU path.
 *
 * BBDev semantics (reference: drivers/baseband/turbo_sw, DPDK 24.11):
 *   ENC: input (K-F)/8 bytes per CB, MSB first, CRC included;
 *        output ceil(E/8) bytes, MSB first, after rate matching and interleaving.
 *   DEC: input E int8 LLRs in transmission order;
 *        output (K-F)/8 bytes, or (K-F-24)/8 with CRC_TYPE_24B_DROP, MSB first.
 *   HARQ: internal device memory (advertised in the capabilities); the slot is
 *        harq_combined_output.offset / LDPC_MAX_CB_SIZE, as nrLDPC_coding_aal.c lays out internal memory.
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* dladdr / Dl_info */
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <libgen.h>
#include <limits.h>
#include <stdbool.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_mbuf.h>
#include <bus_vdev_driver.h>

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>

#include "nrLDPC_coding_aal.h"            /* LDPC_MAX_CB_SIZE */
#include "PHY/CODING/coding_defs.h"       /* crcType, lenWithCrc, check_crc, nr_get_R_ldpc_decoder */
#include "PHY/CODING/nrLDPC_defs.h"       /* encoder_implemparams_t, LDPC_*func_t, t_nrLDPC_dec_params */

#define DRIVER_NAME baseband_oai_sw
#define OAI_SW_MAX_QUEUES 8
#define OAI_SW_QUEUE_PRIO_MAX 1 /* add_dev() in nrLDPC_coding_aal.c sets up the DEC queue with priority 1 */
#define OAI_SW_MAX_HARQ_SLOTS 4096
#define OAI_SW_D_LEN (68 * 384) /* largest circular buffer (BG1, Z=384), with margin */

#ifndef OAI_LDPC_DECODER_MAX_NUM_LLR
#define OAI_LDPC_DECODER_MAX_NUM_LLR 27000 /* same as the OAI decoder; >= 68*384 */
#endif

#define OAI_SW_LOG(fmt, ...) fprintf(stderr, "[OAI_SW] " fmt "\n", ##__VA_ARGS__)

/* ------------------------------------------------------------------------ */
/* libldpc.so kernels (dlopen)                                               */
/* ------------------------------------------------------------------------ */
typedef int (*rm_tx_fn)(uint32_t, uint8_t, uint16_t, uint8_t *, uint8_t *, uint8_t, uint32_t, uint32_t, uint8_t, uint32_t);
typedef int (*rm_rx_fn)(uint32_t, uint8_t, uint16_t, int16_t *, int16_t *, uint8_t, uint8_t, uint8_t, uint32_t, uint32_t, uint32_t);
typedef void (*il_fn)(uint32_t, uint8_t, uint8_t *, uint8_t *);
typedef void (*deil_fn)(uint32_t, uint8_t, int16_t *, int16_t *);
typedef int (*getR_fn)(int, int, int, int, int *, int);
typedef int (*crc_fn)(uint8_t *, uint32_t, uint8_t);

static struct {
  void *handle;
  LDPC_encoderfunc_t *encoder;
  LDPC_decoderfunc_t *decoder;
  rm_tx_fn rate_matching;
  rm_rx_fn rate_matching_rx;
  il_fn interleaving;
  deil_fn deinterleaving;
  getR_fn get_R; /* optional, from the executable */
  crc_fn check_crc;
} oai;

static int oai_sw_load_kernels(void)
{
  if (oai.handle != NULL)
    return 0;

  /* libldpc.so lives in the same directory as this libldpc_aal.so */
  Dl_info info;
  char path[PATH_MAX];
  if (dladdr((void *)&oai_sw_load_kernels, &info) == 0 || info.dli_fname == NULL)
    return -1;
  char self[PATH_MAX];
  snprintf(self, sizeof(self), "%s", info.dli_fname);
  snprintf(path, sizeof(path), "%s/libldpc.so", dirname(self));

  oai.handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  if (oai.handle == NULL) {
    OAI_SW_LOG("falha ao carregar %s: %s", path, dlerror());
    return -1;
  }

#define LOAD(field, name)                                       \
  do {                                                          \
    *(void **)(&oai.field) = dlsym(oai.handle, name);           \
    if (oai.field == NULL) {                                    \
      OAI_SW_LOG("símbolo %s ausente em %s", name, path);       \
      return -1;                                                \
    }                                                           \
  } while (0)
  LOAD(encoder, "LDPCencoder");
  LOAD(decoder, "LDPCdecoder");
  LOAD(rate_matching, "nr_rate_matching_ldpc");
  LOAD(rate_matching_rx, "nr_rate_matching_ldpc_rx");
  LOAD(interleaving, "nr_interleaving_ldpc");
  LOAD(deinterleaving, "nr_deinterleaving_ldpc");
#undef LOAD

  /* resolved from the executable, as nrLDPC_coding_aal.c already does for check_crc */
  *(void **)(&oai.get_R) = dlsym(RTLD_DEFAULT, "nr_get_R_ldpc_decoder");
  *(void **)(&oai.check_crc) = dlsym(RTLD_DEFAULT, "check_crc");
  if (oai.check_crc == NULL) {
    OAI_SW_LOG("símbolo check_crc não encontrado no processo");
    return -1;
  }
  if (oai.get_R == NULL)
    OAI_SW_LOG("aviso: nr_get_R_ldpc_decoder ausente; usando taxa mínima (todas as linhas do grafo)");

  OAI_SW_LOG("núcleos carregados de %s", path);
  return 0;
}

/* ------------------------------------------------------------------------ */
/* Internal HARQ memory                                                      */
/* ------------------------------------------------------------------------ */
typedef struct {
  int16_t *d; /* combined circular buffer, same layout as the CPU path */
  int llrLen;
  int round;
} oai_sw_harq_t;

static oai_sw_harq_t harq_mem[OAI_SW_MAX_HARQ_SLOTS];

static oai_sw_harq_t *harq_slot(uint32_t offset)
{
  uint32_t idx = (offset / LDPC_MAX_CB_SIZE) % OAI_SW_MAX_HARQ_SLOTS;
  oai_sw_harq_t *h = &harq_mem[idx];
  if (h->d == NULL)
    h->d = aligned_alloc(64, (OAI_SW_D_LEN + 64) * sizeof(int16_t));
  return h;
}

/* ------------------------------------------------------------------------ */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------ */
static inline uint32_t mother_len(uint8_t BG, uint16_t Z)
{
  return (BG == 1 ? 66 : 50) * Z;
}

/*
 * The BBDev op carries n_cb, while the OAI kernels take Tbslbrm and C. Tbslbrm = 0 makes OAI use Ncb = N
 * (no LBRM); otherwise find Tbslbrm with C = 2 such that floor(3*T/(2*C)) = n_cb (TS 38.212 §5.4.2.1,
 * R_LBRM = 2/3).
 */
static inline void tbslbrm_from_ncb(uint32_t n_cb, uint8_t BG, uint16_t Z, uint32_t *T, uint8_t *C)
{
  *C = 1;
  *T = 0;
  if (n_cb >= mother_len(BG, Z))
    return;
  *C = 2;
  uint32_t guess = (4 * n_cb) / 3;
  for (uint32_t t = (guess > 3 ? guess - 3 : 0); t < guess + 4; t++)
    if ((3 * t) / 4 == n_cb) {
      *T = t;
      return;
    }
  OAI_SW_LOG("aviso: n_cb=%u sem Tbslbrm equivalente; usando Ncb=N", n_cb);
  *C = 1;
}

/* BG2: Kb is the smallest of {6,8,9,10} with Kb*Z >= K' (TS 38.212 §5.2.2) */
static inline uint32_t kb_from(uint8_t BG, uint16_t Z, uint32_t Kprime)
{
  if (BG == 1)
    return 22;
  const uint32_t kbs[] = {6, 8, 9, 10};
  for (int i = 0; i < 4; i++)
    if (kbs[i] * Z >= Kprime)
      return kbs[i];
  return 10;
}

static inline uint8_t *mbuf_append_bytes(struct rte_mbuf *m, uint32_t offset, uint32_t len)
{
  if (rte_pktmbuf_append(m, len) == NULL)
    return NULL;
  return rte_pktmbuf_mtod_offset(m, uint8_t *, offset);
}

/* ------------------------------------------------------------------------ */
/* Encoder (one code block per op)                                           */
/* ------------------------------------------------------------------------ */
static void process_ldpc_enc(struct rte_bbdev_enc_op *op)
{
  struct rte_bbdev_op_ldpc_enc *enc = &op->ldpc_enc;
  op->status = 0;

  const uint8_t BG = enc->basegraph;
  const uint16_t Z = enc->z_c;
  const uint32_t K = (BG == 1 ? 22 : 10) * Z;
  const uint32_t F = enc->n_filler;
  const uint32_t in_bytes = (K - F + 7) >> 3;
  const uint32_t E = (enc->code_block_mode == RTE_BBDEV_CODE_BLOCK) ? enc->cb_params.e : enc->tb_params.ea;

  /* input is (K-F)/8 bytes; the OAI encoder expects K bits with zeroed fillers */
  uint8_t c[(68 * 384) / 8 + 64] __attribute__((aligned(64)));
  memset(c, 0, (K >> 3) + 64);
  uint8_t *in = rte_pktmbuf_mtod_offset(enc->input.data, uint8_t *, enc->input.offset);
  memcpy(c, in, in_bytes);

  /* output d holds one bit per byte (bit 0 = this code block) */
  uint8_t d[68 * 384 + 64] __attribute__((aligned(64)));
  memset(d, 0, sizeof(d));
  uint8_t *cs[1] = {c};
  time_stats_t t0 = {0}, t1 = {0}, t2 = {0}, t3 = {0};
  encoder_implemparams_t impp = {
      .n_segments = 1,
      .first_seg = 0,
      .gen_code = 0,
      .tinput = &t0,
      .tprep = &t1,
      .tparity = &t2,
      .toutput = &t3,
      .K = K,
      .Kb = kb_from(BG, Z, K - F),
      .Zc = Z,
      .F = F,
      .BG = BG,
      .output = NULL,
      .ans = NULL,
  };
  oai.encoder(cs, d, &impp);

  /* rate matching + interleaving */
  uint32_t T;
  uint8_t C;
  tbslbrm_from_ncb(enc->n_cb, BG, Z, &T, &C);
  uint8_t e[E + 64] __attribute__((aligned(64)));
  uint8_t f[E + 128] __attribute__((aligned(64)));
  memset(e, 0, E + 64);
  memset(f, 0, E + 128);
  if (oai.rate_matching(T, BG, Z, d, e, C, F, K - F - 2 * Z, enc->rv_index, E) != 0) {
    op->status |= 1 << RTE_BBDEV_DRV_ERROR;
    return;
  }
  oai.interleaving(E, enc->q_m, e, f);

  /* output: ceil(E/8) bytes, MSB first (nrLDPC_coding_aal.c reverses the bit order) */
  const uint32_t out_len = (E + 7) >> 3;
  uint8_t *out = mbuf_append_bytes(enc->output.data, enc->output.offset, out_len);
  if (out == NULL) {
    op->status |= 1 << RTE_BBDEV_DATA_ERROR;
    return;
  }
  memset(out, 0, out_len);
  for (uint32_t i = 0; i < E; i++)
    out[i >> 3] |= (f[i] & 1) << (7 - (i & 7));
  enc->output.length += out_len;
}

/* ------------------------------------------------------------------------ */
/* Decoder (one code block per op)                                           */
/* ------------------------------------------------------------------------ */
static void process_ldpc_dec(struct rte_bbdev_dec_op *op)
{
  struct rte_bbdev_op_ldpc_dec *dec = &op->ldpc_dec;
  op->status = 0;

  const uint8_t BG = dec->basegraph;
  const uint16_t Z = dec->z_c;
  const uint32_t K = (BG == 1 ? 22 : 10) * Z;
  const uint32_t F = dec->n_filler;
  const uint32_t Kprime = K - F;
  const uint8_t Kc = (BG == 2) ? 52 : 68;
  const bool cb_mode = (dec->code_block_mode == RTE_BBDEV_CODE_BLOCK);
  const uint32_t E = cb_mode ? dec->cb_params.e : dec->tb_params.ea;
  const bool crc24b = (dec->op_flags & RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK) != 0;
  const bool crc_drop = (dec->op_flags & RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP) != 0;
  const bool combine_in = (dec->op_flags & RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE) != 0;

  /* 1. int8 -> int16 LLRs and deinterleaving */
  int8_t *in = rte_pktmbuf_mtod_offset(dec->input.data, int8_t *, dec->input.offset);
  int16_t llr[E + 64] __attribute__((aligned(64)));
  int16_t harq_e[E + 64] __attribute__((aligned(64)));
  for (uint32_t i = 0; i < E; i++)
    llr[i] = in[i];
  memset(llr + E, 0, 64 * sizeof(int16_t));
  oai.deinterleaving(E, dec->q_m, harq_e, llr);

  /* 2. rate recovery with HARQ combining in internal memory */
  oai_sw_harq_t *h = harq_slot(dec->harq_combined_output.offset);
  if (h->d == NULL) {
    op->status |= 1 << RTE_BBDEV_DRV_ERROR;
    return;
  }
  if (!combine_in) {
    h->llrLen = 0;
    h->round = 0;
  } else {
    h->round++;
  }
  uint32_t T;
  uint8_t C;
  tbslbrm_from_ncb(dec->n_cb, BG, Z, &T, &C);
  if (oai.rate_matching_rx(T, BG, Z, h->d, harq_e, C, dec->rv_index, combine_in ? 0 : 1, E, F, K - F - 2 * Z) != 0) {
    op->status |= 1 << RTE_BBDEV_DRV_ERROR;
    return;
  }

  /* 3. decoder parameters, as in nrLDPC_prepare_TB_decoding() */
  t_nrLDPC_dec_params p = {0};
  p.BG = BG;
  p.Z = Z;
  p.numMaxIter = dec->iter_max;
  p.outMode = nrLDPC_outMode_BIT;
  p.check_crc = oai.check_crc;
  if (oai.get_R)
    p.R = oai.get_R(dec->rv_index, E, BG, Z, &h->llrLen, h->round);
  else
    p.R = (BG == 1) ? 13 : 15;
  if (cb_mode && crc24b) {
    p.crc_type = CRC24_B;
    p.Kprime = Kprime;
  } else {
    /* single-CB TB: CRC24A if K' > 3840, else CRC16 (TS 38.212 §6.2.1, §7.2.1) */
    p.crc_type = (Kprime > 3840) ? CRC24_A : CRC16;
    p.Kprime = Kprime;
  }

  /* 4. decoder input vector, as in nr_process_decode_segment() */
  int16_t z[68 * 384 + 16] __attribute__((aligned(16)));
  memset(z, 0, 2 * Z * sizeof(*z));
  for (uint32_t i = 0; i < F; i++)
    z[Kprime + i] = 127;
  memcpy(z + 2 * Z, h->d, (Kprime - 2 * Z) * sizeof(*z));
  memcpy(z + K, h->d + (K - 2 * Z), (Kc * Z - K) * sizeof(*z));
  int8_t l[68 * 384 + 16] __attribute__((aligned(16)));
  for (uint32_t i = 0; i < (uint32_t)Kc * Z; i++)
    l[i] = (int8_t)RTE_MAX(-128, RTE_MIN(127, (int)z[i]));
  memset(l + Kc * Z, 0, 16);

  /* 5. decoding */
  int8_t out_buf[OAI_LDPC_DECODER_MAX_NUM_LLR] __attribute__((aligned(32)));
  t_nrLDPC_time_stats ts;
  memset(&ts, 0, sizeof(ts));
  decode_abort_t ab;
  memset(&ab, 0, sizeof(ab));
  int iters = oai.decoder(&p, l, (uint8_t *)out_buf, &ts, &ab);
  dec->iter_count = RTE_MAX((uint8_t)RTE_MIN(iters, 255), dec->iter_count);
  if (iters >= p.numMaxIter) {
    op->status |= 1 << RTE_BBDEV_SYNDROME_ERROR;
    if (crc24b)
      op->status |= 1 << RTE_BBDEV_CRC_ERROR;
  }

  /* 6. hard output: (K-F)/8 bytes, or (K-F-24)/8 with CRC24B dropped */
  const uint32_t out_len = (Kprime - (crc_drop ? 24 : 0)) >> 3;
  uint8_t *out = mbuf_append_bytes(dec->hard_output.data, dec->hard_output.offset, out_len);
  if (out == NULL) {
    op->status |= 1 << RTE_BBDEV_DATA_ERROR;
    return;
  }
  memcpy(out, out_buf, out_len);
  dec->hard_output.length += out_len;

  /* internal HARQ memory: nothing to copy out, only report the buffer length */
  dec->harq_combined_output.length = mother_len(BG, Z);
}

/* ------------------------------------------------------------------------ */
/* Advertised capabilities                                                   */
/* ------------------------------------------------------------------------ */
static const struct rte_bbdev_op_cap oai_sw_capabilities[] = {
    {
        .type = RTE_BBDEV_OP_LDPC_ENC,
        .cap.ldpc_enc =
            {
                .capability_flags = RTE_BBDEV_LDPC_RATE_MATCH,
                .num_buffers_src = RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
                .num_buffers_dst = RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
            },
    },
    {
        .type = RTE_BBDEV_OP_LDPC_DEC,
        .cap.ldpc_dec =
            {
                .capability_flags = RTE_BBDEV_LDPC_ITERATION_STOP_ENABLE | RTE_BBDEV_LDPC_CRC_TYPE_24B_DROP
                                    | RTE_BBDEV_LDPC_CRC_TYPE_24B_CHECK | RTE_BBDEV_LDPC_HQ_COMBINE_IN_ENABLE
                                    | RTE_BBDEV_LDPC_HQ_COMBINE_OUT_ENABLE | RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_IN_ENABLE
                                    | RTE_BBDEV_LDPC_INTERNAL_HARQ_MEMORY_OUT_ENABLE,
                .llr_size = 8,
                .llr_decimals = 0,
                .num_buffers_src = RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
                .num_buffers_hard_out = RTE_BBDEV_LDPC_MAX_CODE_BLOCKS,
                .num_buffers_soft_out = 0,
            },
    },
    RTE_BBDEV_END_OF_CAPABILITIES_LIST()};

/* ------------------------------------------------------------------------ */
/* Control path                                                              */
/* ------------------------------------------------------------------------ */
struct oai_sw_private {
  unsigned int max_nb_queues;
};

struct __rte_cache_aligned oai_sw_queue {
  struct rte_ring *ring; /* processed ops waiting for dequeue */
};

static void oai_sw_info_get(struct rte_bbdev *dev, struct rte_bbdev_driver_info *dev_info)
{
  struct oai_sw_private *priv = dev->data->dev_private;

  static struct rte_bbdev_queue_conf default_queue_conf = {
      .queue_size = RTE_BBDEV_QUEUE_SIZE_LIMIT,
  };
  default_queue_conf.socket = dev->data->socket_id;

  dev_info->driver_name = RTE_STR(DRIVER_NAME);
  dev_info->max_num_queues = priv->max_nb_queues;
  dev_info->num_queues[RTE_BBDEV_OP_LDPC_ENC] = priv->max_nb_queues;
  dev_info->num_queues[RTE_BBDEV_OP_LDPC_DEC] = priv->max_nb_queues;
  dev_info->queue_priority[RTE_BBDEV_OP_LDPC_ENC] = OAI_SW_QUEUE_PRIO_MAX;
  dev_info->queue_priority[RTE_BBDEV_OP_LDPC_DEC] = OAI_SW_QUEUE_PRIO_MAX;
  dev_info->max_dl_queue_priority = OAI_SW_QUEUE_PRIO_MAX;
  dev_info->max_ul_queue_priority = OAI_SW_QUEUE_PRIO_MAX;
  dev_info->queue_size_lim = RTE_BBDEV_QUEUE_SIZE_LIMIT;
  dev_info->hardware_accelerated = false;
  dev_info->default_queue_conf = default_queue_conf;
  dev_info->capabilities = oai_sw_capabilities;
  dev_info->cpu_flag_reqs = NULL;
  dev_info->min_alignment = 1;
  dev_info->harq_buffer_size = 0;
  dev_info->data_endianness = RTE_LITTLE_ENDIAN;
  dev_info->device_status = RTE_BBDEV_DEV_NOSTATUS;
}

static int oai_sw_queue_setup(struct rte_bbdev *dev, uint16_t q_id, const struct rte_bbdev_queue_conf *conf)
{
  char ring_name[RTE_RING_NAMESIZE];
  snprintf(ring_name, sizeof(ring_name), "oai_sw_%u_%u", dev->data->dev_id, q_id);

  struct oai_sw_queue *q = rte_zmalloc_socket(ring_name, sizeof(*q), RTE_CACHE_LINE_SIZE, conf->socket);
  if (q == NULL)
    return -ENOMEM;

  q->ring = rte_ring_create(ring_name, conf->queue_size, conf->socket, RING_F_SP_ENQ | RING_F_SC_DEQ);
  if (q->ring == NULL) {
    rte_free(q);
    return -EFAULT;
  }

  dev->data->queues[q_id].queue_private = q;
  OAI_SW_LOG("fila %u configurada (op_type=%s, tamanho=%u)", q_id, rte_bbdev_op_type_str(conf->op_type), conf->queue_size);
  return 0;
}

static int oai_sw_queue_release(struct rte_bbdev *dev, uint16_t q_id)
{
  struct oai_sw_queue *q = dev->data->queues[q_id].queue_private;
  if (q != NULL) {
    rte_ring_free(q->ring);
    rte_free(q);
    dev->data->queues[q_id].queue_private = NULL;
  }
  return 0;
}

static const struct rte_bbdev_ops oai_sw_ops = {
    .info_get = oai_sw_info_get,
    .queue_setup = oai_sw_queue_setup,
    .queue_release = oai_sw_queue_release,
};

/* ------------------------------------------------------------------------ */
/* Data path: process on enqueue, return on dequeue                          */
/* ------------------------------------------------------------------------ */
/* enqueue call/op counts, printed at exit: evidence that a run went through this device */
static uint64_t stats_dec_calls, stats_dec_ops, stats_enc_calls, stats_enc_ops;
static void stats_report(void)
{
  OAI_SW_LOG("resumo: DEC %lu chamadas / %lu ops | ENC %lu chamadas / %lu ops",
             stats_dec_calls,
             stats_dec_ops,
             stats_enc_calls,
             stats_enc_ops);
}
RTE_INIT(stats_register)
{
  atexit(stats_report);
}

static uint16_t ring_enqueue(struct rte_bbdev_queue_data *q_data, void **ops, uint16_t nb_ops)
{
  struct oai_sw_queue *q = q_data->queue_private;
  uint16_t n = rte_ring_enqueue_burst(q->ring, ops, nb_ops, NULL);
  q_data->queue_stats.enqueue_err_count += nb_ops - n;
  q_data->queue_stats.enqueued_count += n;
  return n;
}

static uint16_t ring_dequeue(struct rte_bbdev_queue_data *q_data, void **ops, uint16_t nb_ops)
{
  struct oai_sw_queue *q = q_data->queue_private;
  uint16_t n = rte_ring_dequeue_burst(q->ring, ops, nb_ops, NULL);
  q_data->queue_stats.dequeued_count += n;
  return n;
}

static uint16_t oai_sw_enqueue_ldpc_dec(struct rte_bbdev_queue_data *q_data, struct rte_bbdev_dec_op **ops, uint16_t nb_ops)
{
  stats_dec_calls++;
  stats_dec_ops += nb_ops;
  for (uint16_t i = 0; i < nb_ops; i++)
    process_ldpc_dec(ops[i]);
  return ring_enqueue(q_data, (void **)ops, nb_ops);
}

static uint16_t oai_sw_dequeue_ldpc_dec(struct rte_bbdev_queue_data *q_data, struct rte_bbdev_dec_op **ops, uint16_t nb_ops)
{
  return ring_dequeue(q_data, (void **)ops, nb_ops);
}

static uint16_t oai_sw_enqueue_ldpc_enc(struct rte_bbdev_queue_data *q_data, struct rte_bbdev_enc_op **ops, uint16_t nb_ops)
{
  stats_enc_calls++;
  stats_enc_ops += nb_ops;
  for (uint16_t i = 0; i < nb_ops; i++)
    process_ldpc_enc(ops[i]);
  return ring_enqueue(q_data, (void **)ops, nb_ops);
}

static uint16_t oai_sw_dequeue_ldpc_enc(struct rte_bbdev_queue_data *q_data, struct rte_bbdev_enc_op **ops, uint16_t nb_ops)
{
  return ring_dequeue(q_data, (void **)ops, nb_ops);
}

/* ------------------------------------------------------------------------ */
/* vdev probe / remove                                                       */
/* ------------------------------------------------------------------------ */
static int oai_sw_probe(struct rte_vdev_device *vdev)
{
  const char *name = rte_vdev_device_name(vdev);
  if (name == NULL)
    return -EINVAL;

  if (oai_sw_load_kernels() != 0)
    return -ENOENT;

  struct rte_bbdev *bbdev = rte_bbdev_allocate(name);
  if (bbdev == NULL)
    return -ENODEV;

  int socket_id = rte_socket_id();
  struct oai_sw_private *priv = rte_zmalloc_socket(name, sizeof(*priv), RTE_CACHE_LINE_SIZE, socket_id);
  if (priv == NULL) {
    rte_bbdev_release(bbdev);
    return -ENOMEM;
  }
  priv->max_nb_queues = OAI_SW_MAX_QUEUES;

  bbdev->data->dev_private = priv;
  bbdev->dev_ops = &oai_sw_ops;
  bbdev->device = &vdev->device;
  bbdev->data->socket_id = socket_id;
  bbdev->intr_handle = NULL;

  bbdev->enqueue_ldpc_dec_ops = oai_sw_enqueue_ldpc_dec;
  bbdev->dequeue_ldpc_dec_ops = oai_sw_dequeue_ldpc_dec;
  bbdev->enqueue_ldpc_enc_ops = oai_sw_enqueue_ldpc_enc;
  bbdev->dequeue_ldpc_enc_ops = oai_sw_dequeue_ldpc_enc;

  OAI_SW_LOG("dispositivo %s criado (LDPC_ENC + LDPC_DEC, %u filas, HARQ interna)", name, priv->max_nb_queues);
  return 0;
}

static int oai_sw_remove(struct rte_vdev_device *vdev)
{
  const char *name = rte_vdev_device_name(vdev);
  if (name == NULL)
    return -EINVAL;
  struct rte_bbdev *bbdev = rte_bbdev_get_named_dev(name);
  if (bbdev == NULL)
    return -EINVAL;
  rte_free(bbdev->data->dev_private);
  return rte_bbdev_release(bbdev);
}

static struct rte_vdev_driver oai_sw_drv = {
    .probe = oai_sw_probe,
    .remove = oai_sw_remove,
};

RTE_PMD_REGISTER_VDEV(DRIVER_NAME, oai_sw_drv);