/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Boundary and regression tests for NR P7 message unpackers.
 *
 * Verifies:
 * 1. Safe rejection of oversized counts and invalid enums (e.g. SRS num_symbols > 2,
 *    PUSCH num_cb resulting in cb_len > sizeofArray(cb_present_and_position)).
 * 2. Acceptance of valid maximum-capacity counts (count == sizeofArray(array)) to ensure
 *    protocol behavior at standard boundaries is preserved without off-by-one errors.
 * 3. Safe rejection of truncated network buffers without buffer over-reads or crashes.
 */

#include "nfapi/tests/nr_fapi_test.h"
#include "nfapi.h"
#include "nr_fapi_p7.h"
#include "common/config/config_userapi.h"

configmodule_interface_t *uniqCfg = NULL;

void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  (void)assert;
  printf("exit at %s:%d:%s: %s\n", file, line, function, s ? s : "");
  abort();
}

typedef struct {
  uint8_t buf[2048];
  size_t pos;
} body_t;

static void put8(body_t *b, uint8_t v)
{
  uint8_t *p = b->buf + b->pos;
  if (push8(v, &p, b->buf + sizeof(b->buf)))
    b->pos = p - b->buf;
}

static void put16(body_t *b, uint16_t v)
{
  uint8_t *p = b->buf + b->pos;
  if (push16(v, &p, b->buf + sizeof(b->buf)))
    b->pos = p - b->buf;
}

static void put32(body_t *b, uint32_t v)
{
  uint8_t *p = b->buf + b->pos;
  if (push32(v, &p, b->buf + sizeof(b->buf)))
    b->pos = p - b->buf;
}

static void pad(body_t *b, size_t n)
{
  for (size_t i = 0; i < n && b->pos < sizeof(b->buf); i++)
    b->buf[b->pos++] = 0;
}

#define EXPECT_REJECT(unpack_fn, body, type, cleanup)                  \
  do {                                                                 \
    type *msg = calloc(1, sizeof(*msg));                               \
    uint8_t *read = (body).buf;                                        \
    uint8_t *end = (body).buf + (body).pos;                            \
    uint8_t rc = unpack_fn(&read, end, msg);                           \
    cleanup;                                                           \
    free(msg);                                                         \
    if (rc != 0) {                                                     \
      printf("FAIL: %s accepted an invalid/oversized input\n", #unpack_fn); \
      return 1;                                                        \
    }                                                                  \
    printf("OK: %s rejected invalid/oversized input\n", #unpack_fn);   \
  } while (0)

#define EXPECT_ACCEPT(unpack_fn, body, type, cleanup)                  \
  do {                                                                 \
    type *msg = calloc(1, sizeof(*msg));                               \
    uint8_t *read = (body).buf;                                        \
    uint8_t *end = (body).buf + (body).pos;                            \
    uint8_t rc = unpack_fn(&read, end, msg);                           \
    cleanup;                                                           \
    free(msg);                                                         \
    if (rc == 0) {                                                     \
      printf("FAIL: %s rejected valid maximum-capacity count\n", #unpack_fn); \
      return 1;                                                        \
    }                                                                  \
    printf("OK: %s accepted valid maximum-capacity count\n", #unpack_fn); \
  } while (0)

/* Test 1: SRS num_symbols > 2 (TS 38.211 §6.4.1.4.1 invalid enum, causing shift overflow and symbol_list[4] OOB) */
static int test_srs_num_symbols_oversized(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  put8(&b, 1);  /* n_pdus = 1 */
  put8(&b, 0);  /* rach_present */
  put8(&b, 0);  /* n_ulsch */
  put8(&b, 0);  /* n_ulcch */
  put8(&b, 0);  /* n_group */

  /* PDU header */
  put16(&b, NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE);
  put16(&b, 100); /* pdu_size */

  /* SRS PDU */
  put16(&b, 0); /* rnti */
  put32(&b, 0); /* handle */
  put16(&b, 0); /* bwp_size */
  put16(&b, 0); /* bwp_start */
  put8(&b, 0);  /* subcarrier_spacing */
  put8(&b, 0);  /* cyclic_prefix */
  put8(&b, 0);  /* num_ant_ports */
  put8(&b, 3);  /* num_symbols = 3 (INVALID, > 2) */
  put8(&b, 0);  /* num_repetitions */
  put8(&b, 0);  /* time_start_position */
  put8(&b, 0);  /* config_index */
  put16(&b, 0); /* sequence_id */
  put8(&b, 0);  /* bandwidth_index */
  put8(&b, 0);  /* comb_size */
  put8(&b, 0);  /* comb_offset */
  put8(&b, 0);  /* cyclic_shift */
  put8(&b, 0);  /* frequency_position */
  put16(&b, 0); /* frequency_shift */
  put8(&b, 0);  /* frequency_hopping */
  put8(&b, 0);  /* group_or_sequence_hopping */
  put8(&b, 0);  /* resource_type */
  put16(&b, 0); /* t_srs */
  put16(&b, 0); /* t_offset */
  pad(&b, 200);

  EXPECT_REJECT(unpack_ul_tti_request, b, nfapi_nr_ul_tti_request_t, (void)0);
  return 0;
}

/* Test 2: PUSCH num_cb = 16 => cb_len = 2 > sizeofArray(cb_present_and_position) */
static int test_pusch_cb_len_oversized(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  put8(&b, 1);  /* n_pdus = 1 */
  put8(&b, 0);  /* rach_present */
  put8(&b, 0);  /* n_ulsch */
  put8(&b, 0);  /* n_ulcch */
  put8(&b, 0);  /* n_group */

  /* PDU header */
  put16(&b, NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE);
  put16(&b, 150); /* pdu_size */

  /* PUSCH PDU fixed fields */
  put16(&b, PUSCH_PDU_BITMAP_PUSCH_DATA); /* pdu_bit_map */
  put16(&b, 0); /* rnti */
  put32(&b, 0); /* handle */
  put16(&b, 0); /* bwp_size */
  put16(&b, 0); /* bwp_start */
  put8(&b, 0);  /* subcarrier_spacing */
  put8(&b, 0);  /* cyclic_prefix */
  put16(&b, 0); /* target_code_rate */
  put8(&b, 0);  /* qam_mod_order */
  put8(&b, 0);  /* mcs_index */
  put8(&b, 0);  /* mcs_table */
  put8(&b, 0);  /* transform_precoding */
  put16(&b, 0); /* data_scrambling_id */
  put8(&b, 0);  /* nrOfLayers */
  put16(&b, 0); /* ul_dmrs_symb_pos */
  put8(&b, 0);  /* dmrs_config_type */
  put16(&b, 0); /* ul_dmrs_scrambling_id */
  put16(&b, 0); /* pusch_identity */
  put8(&b, 0);  /* scid */
  put8(&b, 0);  /* num_dmrs_cdm_grps_no_data */
  put16(&b, 0); /* dmrs_ports */
  put8(&b, 0);  /* resource_alloc */
  pad(&b, 36);  /* rb_bitmap */
  put16(&b, 0); /* rb_start */
  put16(&b, 0); /* rb_size */
  put8(&b, 0);  /* vrb_to_prb_mapping */
  put8(&b, 0);  /* frequency_hopping */
  put16(&b, 0); /* tx_direct_current_location */
  put8(&b, 0);  /* uplink_frequency_shift_7p5khz */
  put8(&b, 0);  /* start_symbol_index */
  put8(&b, 0);  /* nr_of_symbols */

  /* PUSCH Data */
  put8(&b, 0);  /* rv_index */
  put8(&b, 0);  /* harq_process_id */
  put8(&b, 0);  /* new_data_indicator */
  put32(&b, 0); /* tb_size */
  put16(&b, 16); /* num_cb = 16 => cb_len = 2, exceeds 1-byte cb_present_and_position */
  pad(&b, 20);

  EXPECT_REJECT(unpack_ul_tti_request, b, nfapi_nr_ul_tti_request_t, (void)0);
  return 0;
}

/* Test 3: Valid maximum-capacity groups in UL_TTI: n_group = NFAPI_MAX_NUM_GROUPS */
static int test_ul_tti_n_group_max_capacity(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  put8(&b, 0);  /* n_pdus */
  put8(&b, 0);  /* rach_present */
  put8(&b, 0);  /* n_ulsch */
  put8(&b, 0);  /* n_ulcch */
  put8(&b, NFAPI_MAX_NUM_GROUPS); /* exact maximum capacity (8) */
  for (int i = 0; i < NFAPI_MAX_NUM_GROUPS; i++) {
    put8(&b, 0); /* n_ue = 0 for each group */
  }

  EXPECT_ACCEPT(unpack_ul_tti_request, b, nfapi_nr_ul_tti_request_t, (void)0);
  return 0;
}

/* Test 4: Valid maximum SRS symbols: num_symbols = 2 (4 symbols, exact symbol_list[4] capacity) */
static int test_srs_valid_max_symbols(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  put8(&b, 1);  /* n_pdus = 1 */
  put8(&b, 0);  /* rach_present */
  put8(&b, 0);  /* n_ulsch */
  put8(&b, 0);  /* n_ulcch */
  put8(&b, 0);  /* n_group */

  /* PDU header */
  put16(&b, NFAPI_NR_UL_CONFIG_SRS_PDU_TYPE);
  put16(&b, 100); /* pdu_size */

  /* SRS PDU */
  put16(&b, 0); /* rnti */
  put32(&b, 0); /* handle */
  put16(&b, 0); /* bwp_size */
  put16(&b, 0); /* bwp_start */
  put8(&b, 0);  /* subcarrier_spacing */
  put8(&b, 0);  /* cyclic_prefix */
  put8(&b, 0);  /* num_ant_ports */
  put8(&b, 2);  /* num_symbols = 2 => 1 << 2 = 4 symbols (VALID MAX CAPACITY) */
  put8(&b, 0);  /* num_repetitions */
  put8(&b, 0);  /* time_start_position */
  put8(&b, 0);  /* config_index */
  put16(&b, 0); /* sequence_id */
  put8(&b, 0);  /* bandwidth_index */
  put8(&b, 0);  /* comb_size */
  put8(&b, 0);  /* comb_offset */
  put8(&b, 0);  /* cyclic_shift */
  put8(&b, 0);  /* frequency_position */
  put16(&b, 0); /* frequency_shift */
  put8(&b, 0);  /* frequency_hopping */
  put8(&b, 0);  /* group_or_sequence_hopping */
  put8(&b, 0);  /* resource_type */
  put16(&b, 0); /* t_srs */
  put16(&b, 0); /* t_offset */

  /* RX Beamforming PDU */
  put8(&b, 0);  /* trp_scheme */
  put16(&b, 0); /* num_prgs = 0 */
  put16(&b, 0); /* prg_size */
  put8(&b, 0);  /* dig_bf_interface = 0 */

  /* SRS Parameters v4 */
  put16(&b, 1); /* srs_bandwidth_size */
  for (int s = 0; s < 4; s++) {
    put16(&b, 0); /* srs_bandwidth_start */
    put8(&b, 0);  /* sequence_group */
    put8(&b, 0);  /* sequence_number */
  }
  put32(&b, 0); /* usage = 0 => nUsage = 0 */
  put8(&b, 0);  /* singular_Value_representation */
  put8(&b, 0);  /* iq_representation */
  put16(&b, 0); /* prg_size */
  put8(&b, 0);  /* num_total_ue_antennas */
  put32(&b, 0); /* ue_antennas_in_this_srs_resource_set */
  put32(&b, 0); /* sampled_ue_antennas */
  put8(&b, 0);  /* report_scope */
  put8(&b, 0);  /* num_ul_spatial_streams_ports = 0 */

  EXPECT_ACCEPT(unpack_ul_tti_request, b, nfapi_nr_ul_tti_request_t, (void)0);
  return 0;
}

/* Test 5: Valid maximum codeblocks in PUSCH: num_cb = 8 (cb_len = 1, exact capacity) */
static int test_pusch_valid_max_cb(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  put8(&b, 1);  /* n_pdus = 1 */
  put8(&b, 0);  /* rach_present */
  put8(&b, 0);  /* n_ulsch */
  put8(&b, 0);  /* n_ulcch */
  put8(&b, 0);  /* n_group */

  /* PDU header */
  put16(&b, NFAPI_NR_UL_CONFIG_PUSCH_PDU_TYPE);
  put16(&b, 150); /* pdu_size */

  /* PUSCH PDU fixed fields */
  put16(&b, PUSCH_PDU_BITMAP_PUSCH_DATA); /* pdu_bit_map */
  put16(&b, 0); /* rnti */
  put32(&b, 0); /* handle */
  put16(&b, 0); /* bwp_size */
  put16(&b, 0); /* bwp_start */
  put8(&b, 0);  /* subcarrier_spacing */
  put8(&b, 0);  /* cyclic_prefix */
  put16(&b, 0); /* target_code_rate */
  put8(&b, 0);  /* qam_mod_order */
  put8(&b, 0);  /* mcs_index */
  put8(&b, 0);  /* mcs_table */
  put8(&b, 0);  /* transform_precoding */
  put16(&b, 0); /* data_scrambling_id */
  put8(&b, 0);  /* nrOfLayers */
  put16(&b, 0); /* ul_dmrs_symb_pos */
  put8(&b, 0);  /* dmrs_config_type */
  put16(&b, 0); /* ul_dmrs_scrambling_id */
  put16(&b, 0); /* pusch_identity */
  put8(&b, 0);  /* scid */
  put8(&b, 0);  /* num_dmrs_cdm_grps_no_data */
  put16(&b, 0); /* dmrs_ports */
  put8(&b, 0);  /* resource_alloc */
  pad(&b, 36);  /* rb_bitmap */
  put16(&b, 0); /* rb_start */
  put16(&b, 0); /* rb_size */
  put8(&b, 0);  /* vrb_to_prb_mapping */
  put8(&b, 0);  /* frequency_hopping */
  put16(&b, 0); /* tx_direct_current_location */
  put8(&b, 0);  /* uplink_frequency_shift_7p5khz */
  put8(&b, 0);  /* start_symbol_index */
  put8(&b, 0);  /* nr_of_symbols */

  /* PUSCH Data */
  put8(&b, 0);  /* rv_index */
  put8(&b, 0);  /* harq_process_id */
  put8(&b, 0);  /* new_data_indicator */
  put32(&b, 0); /* tb_size */
  put16(&b, 8); /* num_cb = 8 => cb_len = 1 (VALID MAX CAPACITY) */
  put8(&b, 0xff); /* cb_present_and_position[0] */

  /* Beamforming */
  put8(&b, 0);  /* trp_scheme */
  put16(&b, 0); /* num_prgs = 0 */
  put16(&b, 0); /* prg_size */
  put8(&b, 0);  /* dig_bf_interface = 0 */

#ifndef ENABLE_AERIAL
  put8(&b, 0);  /* ldpcBaseGraph */
  put32(&b, 0); /* tbSizeLbrmBytes */
  put8(&b, 0);  /* numSpatialStreamIndices = 0 */
#endif

  EXPECT_ACCEPT(unpack_ul_tti_request, b, nfapi_nr_ul_tti_request_t, (void)0);
  return 0;
}

/* Test 6: Valid maximum preambles in RACH: num_preamble = 64 (preamble_list[64] capacity) */
static int test_rach_valid_max_preambles(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put8(&b, 1);  /* number_of_pdus = 1 */

  /* RACH indication body */
  put16(&b, 0);  /* phy_cell_id */
  put8(&b, 0);   /* symbol_index */
  put8(&b, 0);   /* slot_index */
  put8(&b, 0);   /* freq_index */
  put8(&b, 0);   /* avg_rssi */
  put8(&b, 0);   /* avg_snr */
  put8(&b, 64);  /* num_preamble = 64 (VALID MAX CAPACITY) */

  for (int i = 0; i < 64; i++) {
    put8(&b, 0);  /* preamble_index */
    put16(&b, 0); /* timing_advance */
    put32(&b, 0); /* preamble_pwr */
  }

  EXPECT_ACCEPT(unpack_nr_rach_indication, b, nfapi_nr_rach_indication_t, free(msg->pdu_list));
  return 0;
}

/* Test 7: Valid maximum TA offsets: num_ta = NFAPI_NR_MAX_NUM_TA_NSEC */
static int test_srs_toa_valid_max_ta(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put16(&b, 0); /* rnti */
  put8(&b, NFAPI_NR_MAX_NUM_TA_NSEC); /* exact maximum capacity */

  for (int i = 0; i < NFAPI_NR_MAX_NUM_TA_NSEC; i++) {
    put16(&b, 0); /* ta_offset_nsec */
  }

  EXPECT_ACCEPT(unpack_nr_srs_toa_vendor_ext_indication, b, nfapi_nr_srs_toa_vendor_ext_indication_t, (void)0);
  return 0;
}

/* Test 8: Truncated UL_TTI request buffer ends prematurely */
static int test_truncated_ul_tti(void)
{
  body_t b = {0};
  put16(&b, 0); /* SFN */
  put16(&b, 0); /* Slot */
  put8(&b, 2);  /* n_pdus = 2, but buffer truncated immediately */

  EXPECT_REJECT(unpack_ul_tti_request, b, nfapi_nr_ul_tti_request_t, (void)0);
  return 0;
}

/* Test 9: Truncated RACH indication buffer ends prematurely mid-preamble */
static int test_truncated_rach(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put8(&b, 1);  /* number_of_pdus = 1 */
  put16(&b, 0); /* phy_cell_id */
  put8(&b, 0);  /* symbol_index */
  put8(&b, 0);  /* slot_index */
  put8(&b, 0);  /* freq_index */
  put8(&b, 0);  /* avg_rssi */
  put8(&b, 0);  /* avg_snr */
  put8(&b, 10); /* num_preamble = 10, but truncated after 1 preamble */
  put8(&b, 0);
  put16(&b, 0);
  put32(&b, 0);

  EXPECT_REJECT(unpack_nr_rach_indication, b, nfapi_nr_rach_indication_t, free(msg->pdu_list));
  return 0;
}

/* Test 10: Truncated SRS TOA buffer ends prematurely */
static int test_truncated_srs_toa(void)
{
  body_t b = {0};
  put16(&b, 0); /* sfn */
  put16(&b, 0); /* slot */
  put16(&b, 0); /* rnti */
  put8(&b, 10); /* num_ta = 10, but buffer truncated */

  EXPECT_REJECT(unpack_nr_srs_toa_vendor_ext_indication, b, nfapi_nr_srs_toa_vendor_ext_indication_t, (void)0);
  return 0;
}

int main(void)
{
  fapi_test_init();

  if (test_srs_num_symbols_oversized())
    return 1;
  if (test_pusch_cb_len_oversized())
    return 1;
  if (test_ul_tti_n_group_max_capacity())
    return 1;
  if (test_srs_valid_max_symbols())
    return 1;
  if (test_pusch_valid_max_cb())
    return 1;
  if (test_rach_valid_max_preambles())
    return 1;
  if (test_srs_toa_valid_max_ta())
    return 1;
  if (test_truncated_ul_tti())
    return 1;
  if (test_truncated_rach())
    return 1;
  if (test_truncated_srs_toa())
    return 1;

  printf("All P7 boundary and regression tests passed\n");
  return 0;
}
