/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Unit test for UCI on PUSCH codeword generation in nr_ue_ulsch_procedures().
 *
 * This test instantiates a PHY_VARS_NR_UE, a NR_UL_UE_HARQ_t and the other
 * structures required to call nr_ue_ulsch_procedures() directly (i.e. without
 * the channel simulation and gNB decoding done by nr_ulsim). It reads a MATLAB
 * generated binary vector that contains:
 *   - the ULSCH payload bits and the HARQ-ACK / CSI-part1 / CSI-part2 UCI bits,
 *   - the reference (scrambled) codeword.
 * The codeword produced by the UE is then compared bit-by-bit against the
 * reference codeword read from the file.
 *
 * Usage: test_nr_ulsch_ue_uci_on_pusch <matlab_vector_file>
 */

#include "PHY/INIT/nr_parms.h"
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/config/config_userapi.h"
#include "common/ran_context.h"
#include "common/utils/LOG/log.h"
#include "common/utils/bits.h"
#include "common/utils/load_module_shlib.h"
#include "common/utils/threadPool/thread-pool.h"
#include "executables/nr-uesoftmodem.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "openair1/SIMULATION/TOOLS/sim.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "PHY/INIT/nr_phy_init.h"
#include "PHY/defs_gNB.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/defs_nr_common.h"
#include "PHY/impl_defs_nr.h"
#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "SCHED_NR/phy_frame_config_nr.h"
#include "openair1/SIMULATION/NR_PHY/nr_unitary_defs.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"

/* ------------------------------------------------------------------------- */
/* Globals and stubs required by the libraries linked into this test.        */
/* These mirror the ones defined in nr_ulschsim.c.                           */
/* ------------------------------------------------------------------------- */
THREAD_STRUCT thread_struct;
PHY_VARS_gNB *gNB;
PHY_VARS_NR_UE *UE;
RAN_CONTEXT_t RC;
nrLDPC_coding_interface_t nrLDPC_coding_interface = {0};

static softmodem_params_t softmodem_params;
softmodem_params_t *get_softmodem_params(void)
{
  return &softmodem_params;
}
void init_downlink_harq_status(NR_DL_UE_HARQ_t *dl_harq)
{
}

double cpuf;

PHY_VARS_NR_UE *PHY_vars_UE_g[1][1] = {{NULL}};
uint16_t n_rnti = 0x1234;

nrUE_params_t nrUE_params;
nrUE_params_t *get_nrUE_params(void)
{
  return &nrUE_params;
}

configmodule_interface_t *uniqCfg = NULL;

/* ------------------------------------------------------------------------- */
/* MATLAB vector parsing helpers (copied from ulsim.c).                      */
/* ------------------------------------------------------------------------- */
static void copy_bytes_to_packed_bits(const uint8_t *in, const uint32_t num_bits, const bool is_ulsch, uint8_t *out)
{
  if (is_ulsch) { // MATLAB computes CRC for input of MSB first
    for (uint_fast32_t b = 0; b < num_bits; b++) {
      out[b / 8] |= ((in[b] & 1) << (7 - (b % 8)));
    }
  } else {
    for (uint_fast32_t b = 0; b < num_bits; b++) {
      out[b / 8] |= (in[b] << (b % 8));
    }
  }
}

static void prepare_ue_pusch_pdu_from_matlab_vector(const bool uci_on_pusch,
                                                    FILE *vect_file,
                                                    nfapi_nr_ue_pusch_pdu_t *pusch_config_pdu,
                                                    uint8_t *cw_buf)
{
  if (!uci_on_pusch)
    return;

  if (vect_file == NULL)
    return;

  struct vect_vars {
    uint32_t A;
    uint32_t oack;
    uint32_t ocsi1;
    uint32_t ocsi2;
    uint32_t cwlen;
    uint32_t cwlen_scr;
  } __attribute__((packed));

  struct vect_vars var = {0};
  if (1 != fread(&var, sizeof(var), 1, vect_file)) {
    printf("Error reading from matlab vector file\n");
    exit(-1);
  }

  const uint16_t buff_len = var.A + var.oack + var.ocsi1 + var.ocsi2;
  uint8_t vec_bits[buff_len];
  memset(vec_bits, 0, sizeof(vect_file));

  uint8_t *p_vec_bits = vec_bits;
  if (var.A != fread(p_vec_bits, sizeof(uint8_t), var.A, vect_file)) {
    printf("Error reading ULSCH bits from file\n");
    exit(-1);
  }
  p_vec_bits += var.A;
  if (var.oack != fread(p_vec_bits, sizeof(uint8_t), var.oack, vect_file)) {
    printf("Error reading ACK bits from file\n");
    exit(-1);
  }
  p_vec_bits += var.oack;
  if (var.ocsi1 != fread(p_vec_bits, sizeof(uint8_t), var.ocsi1, vect_file)) {
    printf("Error reading CSI1 bits from file\n");
    exit(-1);
  }
  p_vec_bits += var.ocsi1;
  if (var.ocsi2 != fread(p_vec_bits, sizeof(uint8_t), var.ocsi2, vect_file)) {
    printf("Error reading CSI2 bits from file\n");
    exit(-1);
  }

  if (var.cwlen != fread(cw_buf, sizeof(uint8_t), var.cwlen, vect_file)) {
    printf("Error reading cw bits from file\n");
    exit(-1);
  }

  memset(cw_buf, 0, var.cwlen_scr);
  if (var.cwlen_scr != fread(cw_buf, sizeof(uint8_t), var.cwlen_scr, vect_file)) {
    printf("Error reading cw bits from file\n");
    exit(-1);
  }

  uint16_t tb_buf_size = (var.A + 7) / 8;
  pusch_config_pdu->pusch_data.tb_size = tb_buf_size;
  pusch_config_pdu->tx_request_body.pdu_length = tb_buf_size;
  uint8_t *pb = pusch_config_pdu->tx_request_body.fapiTxPdu;
  memset(pb, 0, tb_buf_size);
  p_vec_bits = vec_bits;
  copy_bytes_to_packed_bits(p_vec_bits, var.A, true, pb);

  pusch_config_pdu->pusch_uci.harq_ack_bit_length = var.oack;
  pb = (uint8_t *)&pusch_config_pdu->pusch_uci.harq_payload;
  memset(pb, 0, sizeof(pusch_config_pdu->pusch_uci.harq_payload));
  p_vec_bits += var.A;
  copy_bytes_to_packed_bits(p_vec_bits, var.oack, false, pb);

  pusch_config_pdu->pusch_uci.csi_payload.p1_bits = var.ocsi1;
  pb = (uint8_t *)&pusch_config_pdu->pusch_uci.csi_payload.part1_payload;
  memset(pb, 0, sizeof(pusch_config_pdu->pusch_uci.csi_payload.part1_payload));
  p_vec_bits += var.oack;
  copy_bytes_to_packed_bits(p_vec_bits, var.ocsi1, false, pb);

  pusch_config_pdu->pusch_uci.csi_payload.p2_bits = var.ocsi2;
  pb = (uint8_t *)&pusch_config_pdu->pusch_uci.csi_payload.part2_payload;
  memset(pb, 0, sizeof(pusch_config_pdu->pusch_uci.csi_payload.part2_payload));
  p_vec_bits += var.ocsi1;
  copy_bytes_to_packed_bits(p_vec_bits, var.ocsi2, false, pb);
}

/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
  if (argc < 2) {
    printf("Usage: %s <matlab_vector_file>\n", argv[0]);
    return EXIT_FAILURE;
  }

  const char *vector_filename = argv[1];

  stop = false;
  __attribute__((unused)) struct sigaction oldaction;
  sigaction(SIGINT, &sigint_action, &oldaction);

  if ((uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY)) == 0) {
    printf("[TEST_UCI_ON_PUSCH] Error, configuration module init failed\n");
    return EXIT_FAILURE;
  }
  logInit();
  set_glog(OAILOG_WARNING);
  randominit();
  cpuf = get_cpu_freq_GHz();

  /* Parameters matching nr_ulsim -m27 -u1 -R51 -r51 (UCI on PUSCH MATLAB test) */
  const uint16_t N_RB_UL = 51;
  const uint16_t N_RB_DL = 51;
  const int mu = 1;
  const uint16_t Nid_cell = 0;
  const uint8_t Imcs = 27;
  const uint8_t mcs_table = 0;
  const uint16_t nb_rb = 51;
  const uint16_t nb_symb_sch = 12;
  const int start_symbol = 0;
  const int start_rb = 0;
  const uint8_t Nl = 1; // number of layers
  const uint8_t num_dmrs_cdm_grps_no_data = 1;
  const uint8_t harq_pid = 0;
  const int frame = 1;
  const int slot = 8;

  const uint8_t mapping_type = typeB;
  const pusch_dmrs_type_t dmrs_config_type = pusch_dmrs_type1;
  const pusch_dmrs_AdditionalPosition_t add_pos = pusch_dmrs_pos0;
  const pusch_maxLength_t length_dmrs = pusch_len1;

  /* ----------------------- Configure UE ---------------------------------- */
  UE = calloc(1, sizeof(*UE));
  UE->frame_parms.N_RB_DL = N_RB_DL;
  UE->frame_parms.N_RB_UL = N_RB_UL;
  UE->frame_parms.Ncp = NR_NORMAL;
  UE->max_ldpc_iterations = 5;
  crcTableInit();
  UE->frame_parms.nb_antennas_tx = 1;
  UE->frame_parms.nb_antennas_rx = 1;
  UE->frame_parms.ofdm_offset_divisor = UINT_MAX;
  int ret_loader = load_nrLDPC_coding_interface(NULL, &UE->nrLDPC_coding_interface, 32);
  AssertFatal(ret_loader == 0, "error loading LDPC library\n");
  nrUE_cell_params_t cell_params = {.N_RB_DL = N_RB_DL, .band = 78, .numerology = mu, .used_by_ue = 1, .rf_frequency = 3600000000};
  nr_init_frame_parms_ue_sa(&UE->frame_parms, &cell_params);

  if (init_nr_ue_signal(UE, 1) != 0) {
    printf("Error at UE NR initialisation.\n");
    return EXIT_FAILURE;
  }
  nr_init_ul_harq_processes(UE->ul_harq_processes, NR_MAX_HARQ_PROCESSES, UE->frame_parms.N_RB_UL, UE->frame_parms.nb_antennas_tx);
  initFloatingCoresTpool(1, &nrUE_params.Tpool, false, "UE-tpool");

  /* ----------------------- Compute transport block parameters ------------ */
  const uint8_t mod_order = nr_get_Qm_ul(Imcs, mcs_table);
  const uint16_t code_rate = nr_get_code_rate_ul(Imcs, mcs_table);
  const uint16_t l_prime_mask =
      get_l_prime(nb_symb_sch, mapping_type, add_pos, length_dmrs, start_symbol, NR_MIB__dmrs_TypeA_Position_pos2);
  const int number_dmrs_symbols = count_bits64_with_mask(l_prime_mask, start_symbol, nb_symb_sch);
  const uint8_t nb_re_dmrs = ((dmrs_config_type == pusch_dmrs_type1) ? 6 : 4) * num_dmrs_cdm_grps_no_data;
  const unsigned int TBS = nr_compute_tbs(mod_order, code_rate, nb_rb, nb_symb_sch, nb_re_dmrs * number_dmrs_symbols, 0, 0, Nl);
  const unsigned int available_bits = nr_get_G(nb_rb, nb_symb_sch, nb_re_dmrs, number_dmrs_symbols, 0, mod_order, Nl);

  printf("[TEST_UCI_ON_PUSCH] mod_order %u code_rate %u TBS %u available_bits (G) %u\n", mod_order, code_rate, TBS, available_bits);

  NR_UL_UE_HARQ_t *harq_process_ul_ue = &UE->ul_harq_processes[harq_pid];
  harq_process_ul_ue->round = 0;

  /* ----------------------- Build the PUSCH PDU --------------------------- */
  nr_phy_data_tx_t phy_data = {0};
  NR_UE_ULSCH_t *ulsch_ue = &phy_data.ulsch;
  nfapi_nr_ue_pusch_pdu_t *pusch_config_pdu = &ulsch_ue->pusch_pdu;

  pusch_config_pdu->rnti = n_rnti;
  pusch_config_pdu->pdu_bit_map = PUSCH_PDU_BITMAP_PUSCH_DATA;
  pusch_config_pdu->qam_mod_order = mod_order;
  pusch_config_pdu->rb_size = nb_rb;
  pusch_config_pdu->rb_start = start_rb;
  pusch_config_pdu->nr_of_symbols = nb_symb_sch;
  pusch_config_pdu->start_symbol_index = start_symbol;
  pusch_config_pdu->ul_dmrs_symb_pos = l_prime_mask;
  pusch_config_pdu->dmrs_config_type = dmrs_config_type;
  pusch_config_pdu->mcs_index = Imcs;
  pusch_config_pdu->mcs_table = mcs_table;
  pusch_config_pdu->num_dmrs_cdm_grps_no_data = num_dmrs_cdm_grps_no_data;
  pusch_config_pdu->nrOfLayers = Nl;
  pusch_config_pdu->dmrs_ports = (1 << Nl) - 1;
  pusch_config_pdu->target_code_rate = code_rate;
  pusch_config_pdu->tbslbrm = 0;
  pusch_config_pdu->ldpcBaseGraph = get_BG(TBS, code_rate);
  pusch_config_pdu->pusch_data.tb_size = TBS / 8;
  pusch_config_pdu->pusch_data.new_data_indicator = true;
  pusch_config_pdu->pusch_data.rv_index = 0;
  pusch_config_pdu->pusch_data.harq_process_id = harq_pid;
  pusch_config_pdu->transform_precoding = transformPrecoder_disabled;
  pusch_config_pdu->data_scrambling_id = Nid_cell;

  /* UCI on PUSCH configuration. The bit lengths and payloads below are written by prepare_ue_pusch_pdu_from_matlab_vector() with
   * the values from the MATLAB vector. */
  const nfapi_nr_ue_pusch_uci_t pusch_uci = {.alpha_scaling = 3,
                                             .beta_offset_csi1 = 13,
                                             .beta_offset_csi2 = 13,
                                             .beta_offset_harq_ack = 11,
                                             .harq_ack_bit_length = 3,
                                             .harq_payload = 3};
  pusch_config_pdu->pusch_uci = pusch_uci;

  /* The ULSCH payload bits are written (MSB packed) directly into payload_AB */
  pusch_config_pdu->tx_request_body.fapiTxPdu = harq_process_ul_ue->payload_AB;

  /* ----------------------- Load the MATLAB vector ------------------------ */
  uint8_t *cw_buf = calloc(1, available_bits);
  FILE *vect_file = fopen(vector_filename, "rb");
  if (vect_file == NULL) {
    printf("[TEST_UCI_ON_PUSCH] Error opening MATLAB vector file %s\n", vector_filename);
    return EXIT_FAILURE;
  }
  prepare_ue_pusch_pdu_from_matlab_vector(true, vect_file, pusch_config_pdu, cw_buf);
  fclose(vect_file);

  ulsch_ue->status = NR_ACTIVE;

  UE->phy_sim_test_buf = calloc(1, (available_bits + 7) / 8);

  /* ----------------------- Run UE ULSCH/UCI on PUSCH procedure ----------- */
  const int samplesF_per_slot = UE->frame_parms.symbols_per_slot * UE->frame_parms.ofdm_symbol_size;
  c16_t *txdataF_buf = calloc(UE->frame_parms.nb_antennas_tx * samplesF_per_slot, sizeof(c16_t));
  c16_t *txdataF[UE->frame_parms.nb_antennas_tx];
  for (int i = 0; i < UE->frame_parms.nb_antennas_tx; i++)
    txdataF[i] = &txdataF_buf[i * samplesF_per_slot];
  bool was_symbol_used[NR_SYMBOLS_PER_SLOT] = {0};

  nr_ue_ulsch_procedures(UE, frame, slot, &phy_data, (c16_t **)&txdataF, was_symbol_used);

  /* ----------------------- Compare codewords ----------------------------- */
  uint32_t errors_scrambling = 0;
  for (unsigned int i = 0; i < available_bits; i++) {
    const uint8_t current_bit = (UE->phy_sim_test_buf[i / 8] >> (i & 7)) & 1;
    const uint8_t test_vector_bit = cw_buf[i] & 1;
    if (current_bit != test_vector_bit)
      errors_scrambling++;
  }

  int ret;
  if (errors_scrambling == 0) {
    printf("*************\n");
    printf("UCI on PUSCH test OK against MATLAB generated codeword\n");
    printf("*************\n");
    ret = EXIT_SUCCESS;
  } else {
    printf(
        "\x1B[31m"
        "UCI on PUSCH codeword mismatch: %u/%u bits differ\n"
        "\x1B[0m",
        errors_scrambling,
        available_bits);
    ret = EXIT_FAILURE;
  }

  /* ----------------------- Cleanup --------------------------------------- */
  free(UE->phy_sim_test_buf);
  free(cw_buf);
  free(txdataF_buf);
  free_nr_ue_ul_harq(UE->ul_harq_processes, NR_MAX_HARQ_PROCESSES, UE->frame_parms.N_RB_UL, UE->frame_parms.nb_antennas_tx);
  term_nr_ue_signal(UE);
  free(UE);
  abortTpool(&nrUE_params.Tpool);
  loader_reset();
  logTerm();

  return ret;
}
