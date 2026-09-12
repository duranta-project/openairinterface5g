/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * Sidelink PSCCH + PSSCH transmit -> receive loopback unit test.
 *
 * Two UEs (UE_TX, UE_RX) are created with the real production init path
 * (configure_NR_UE + configure_SL_UE) and the real production TX/RX PHY
 * functions are exercised end to end.
 *
 *   TX  : nr_generate_sci1()  (PSCCH SCI-1A)
 *         nr_ue_slsch_procedures()  (SCI-2 + SLSCH on PSSCH)
 *         -> frequency-domain buffer txdataF
 *
 *   PSSCH/SLSCH RX (perfect frequency-domain loopback, no channel):
 *         txdataF is copied verbatim into rxdataF (no OFDM (de)modulation),
 *         then nr_rx_pssch() + nr_slsch_procedures() are called and the
 *         decoded transport block is compared with the transmitted one.
 *
 *   PSCCH RX (time-domain loopback, because pdcch_processing() runs its own
 *         OFDM FEP from ue->common_vars.rxdata):
 *         txdataF is OFDM-modulated to time domain, copied into the RX UE
 *         rxdata, and the real RX_PSCCH branch (pdcch_processing()) is run.
 *         The decoded SCI-1A is captured through a minimal if_inst.
 *
 * Both stages are expected to PASS. All results are printed with the
 * "[SCISIM]" prefix.
 */

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "common/config/config_userapi.h"
#include "common/utils/load_module_shlib.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "common/ran_context.h"
#include "common/utils/nr/nr_common.h"
#include "PHY/types.h"
#include "PHY/impl_defs_nr.h"
#include "PHY/defs_nr_common.h"
#include "PHY/defs_nr_UE.h"
#include "PHY/defs_gNB.h"
#include "PHY/INIT/phy_init.h"
#include "PHY/INIT/nr_phy_init.h"
#include "PHY/MODULATION/nr_modulation.h"
#include "PHY/MODULATION/modulation_UE.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#include "PHY/NR_UE_TRANSPORT/nr_transport_proto_ue.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_dci_defs.h"
#include "PHY/CODING/nrPolar_tools/nr_polar_defs.h"
#include "PHY/CODING/nrLDPC_coding/nrLDPC_coding_interface.h"
#include "openair2/COMMON/e1ap_messages_types.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "openair2/NR_UE_PHY_INTERFACE/NR_IF_Module.h"
#include "openair1/SCHED_NR_UE/defs.h"
#include "openair1/SIMULATION/TOOLS/sim.h"
#include "NR_SL-SSB-TimeAllocation-r16.h"
#include "executables/nr-uesoftmodem.h"
#include "common/utils/threadPool/thread-pool.h"

// nr_slsch_procedures() is a public function of the SL scheduler lib but has no
// public header declaration; declare it here to call it directly.
int nr_slsch_procedures(PHY_VARS_NR_UE *ue,
                        int frame_rx,
                        int slot_rx,
                        int SLSCH_id,
                        const UE_nr_rxtx_proc_t *proc,
                        nr_phy_data_t *phy_data,
                        int8_t *ack_nack_rcvd,
                        int num_acks,
                        const sl_nr_psfch_pdu_t *rx_psfch);

//////////////////// link stubs (as in psbchsim.c) ////////////////////
void e1_bearer_context_setup(const e1ap_bearer_setup_req_t *req) { abort(); }
void e1_bearer_context_modif(const e1ap_bearer_mod_req_t *req) { abort(); }
void e1_bearer_release_cmd(const e1ap_bearer_release_cmd_t *cmd) { abort(); }
int8_t nr_rrc_RA_succeeded(const module_id_t mod_id, const uint8_t gNB_index) { return 1; }
NR_IF_Module_t *NR_IF_Module_init(int Mod_id) { return NULL; }
double cpuf;
void get_num_re_dmrs(nfapi_nr_ue_pusch_pdu_t *pusch_pdu, uint8_t *nb_dmrs_re_per_rb, uint16_t *number_dmrs_symbols) {}
uint64_t downlink_frequency[MAX_NUM_CCs][4];
int64_t uplink_frequency_offset[MAX_NUM_CCs][4];
THREAD_STRUCT thread_struct;
instance_t DUuniqInstance = 0;
instance_t CUuniqInstance = 0;
openair0_config_t openair0_cfg_g[MAX_CARDS];
RAN_CONTEXT_t RC;
char *uecap_file;
configmodule_interface_t *uniqCfg = NULL;
void nr_rrc_ue_generate_RRCSetupRequest(module_id_t module_id, const uint8_t gNB_index) {}
int8_t nr_mac_rrc_data_req_ue(const module_id_t Mod_idP, const int CC_id, const uint8_t gNB_id,
                              const frame_t frameP, const rb_id_t Srb_id, uint8_t *buffer_pP) { return 0; }
nrUE_params_t nrUE_params = {0};
nrUE_params_t *get_nrUE_params(void) { return &nrUE_params; }
uint8_t check_if_ue_is_sl_syncsource() { return 0; }
void nr_rrc_mac_config_req_sl_mib(module_id_t module_id, NR_SL_SSB_TimeAllocation_r16_t *ssb_ta,
                                  uint16_t rx_slss_id, uint8_t *sl_mib) {}
void exit_function(const char *file, const char *function, const int line, const char *s, const int assert)
{
  if (assert)
    abort();
  else
    exit(EXIT_SUCCESS);
}
PHY_VARS_NR_UE ***PHY_vars_UE_g = NULL;
////////////////////////////////////////////////////////////////////////

//////////////////// SCI capture through a minimal if_inst /////////////
static int      g_num_sci1 = 0;             // number of SCI-1A (PSCCH) captured
static int      g_num_sci2 = 0;             // number of SCI-2 (PSSCH) captured
static uint8_t  g_sci1_payload[8] = {0};    // last captured SCI-1A payload
static uint16_t g_sci1_len = 0;
static uint8_t  g_sci2_payload[8] = {0};    // last captured SCI-2 payload
static uint16_t g_sci2_len = 0;
static int      g_num_slsch_ind = 0;        // number of SLSCH rx indications

static void test_sl_indication(nr_sidelink_indication_t *ind)
{
  if (ind == NULL)
    return;
  if (ind->sci_ind) {
    sl_nr_sci_indication_t *sci = ind->sci_ind;
    for (int i = 0; i < sci->number_of_SCIs; i++) {
      if (sci->sci_pdu[i].sci_format_type == SL_SCI_FORMAT_1A_ON_PSCCH) {
        g_num_sci1++;
        g_sci1_len = sci->sci_pdu[i].sci_payloadlen;
        memcpy(g_sci1_payload, sci->sci_pdu[i].sci_payloadBits, 8);
      } else if (sci->sci_pdu[i].sci_format_type == SL_SCI_FORMAT_2_ON_PSSCH) {
        g_num_sci2++;
        g_sci2_len = sci->sci_pdu[i].sci_payloadlen;
        memcpy(g_sci2_payload, sci->sci_pdu[i].sci_payloadBits, 8);
      }
    }
  }
  if (ind->rx_ind)
    g_num_slsch_ind++;
}

static nr_ue_if_module_t g_if_inst;
////////////////////////////////////////////////////////////////////////

//////////////////// UE configuration (mirrors psbchsim.c) /////////////
static void configure_NR_UE(PHY_VARS_NR_UE *UE, int mu, int N_RB)
{
  fapi_nr_config_request_t config = {0};
  NR_DL_FRAME_PARMS *fp = &UE->frame_parms;

  config.ssb_config.scs_common = mu;
  config.cell_config.frame_duplex_type = TDD;
  config.carrier_config.dl_grid_size[mu] = N_RB;
  config.carrier_config.ul_grid_size[mu] = N_RB;
  config.carrier_config.dl_frequency = 3300000;
  config.carrier_config.uplink_frequency = 3300000;

  int band = 78;
  nr_init_frame_parms_ue(fp, &config, band);
  fp->ofdm_offset_divisor = 8;

  if (init_nr_ue_signal(UE, 1) != 0) {
    printf("[SCISIM] Error at UE NR initialisation\n");
    exit(-1);
  }
}

static void sl_init_frame_parameters(PHY_VARS_NR_UE *UE)
{
  NR_DL_FRAME_PARMS *nr_fp = &UE->frame_parms;
  NR_DL_FRAME_PARMS *sl_fp = &UE->SL_UE_PHY_PARAMS.sl_frame_params;

  memcpy(sl_fp, nr_fp, sizeof(NR_DL_FRAME_PARMS));
  sl_fp->ofdm_offset_divisor = 8;
  sl_fp->att_tx = 1;
  sl_fp->att_rx = 1;
  sl_fp->sl_CarrierFreq = 5880000000;
  sl_fp->N_RB_SL = sl_fp->N_RB_DL;
  sl_fp->ssb_start_subcarrier = UE->SL_UE_PHY_PARAMS.sl_config.sl_bwp_config.sl_ssb_offset_point_a;
  sl_fp->Nid_cell = UE->SL_UE_PHY_PARAMS.sl_config.sl_sync_source.rx_slss_id;
}

static void configure_SL_UE(PHY_VARS_NR_UE *UE, int mu, int N_RB, int ssb_offset, int slss_id)
{
  sl_nr_phy_config_request_t *config = &UE->SL_UE_PHY_PARAMS.sl_config;
  NR_DL_FRAME_PARMS *fp = &UE->SL_UE_PHY_PARAMS.sl_frame_params;

  config->sl_bwp_config.sl_scs = mu;
  config->sl_bwp_config.sl_ssb_offset_point_a = ssb_offset;
  config->sl_carrier_config.sl_bandwidth = N_RB;
  config->sl_carrier_config.sl_grid_size = 106;
  config->sl_sync_source.rx_slss_id = slss_id;
  config->sl_DMRS_ScrambleId = 100; // must match RX coreset.pdcch_dmrs_scrambling_id

  sl_init_frame_parameters(UE);
  sl_ue_phy_init(UE);
  perform_symbol_rotation(fp->symbols_per_slot * fp->slots_per_frame / 10,
                          fp->numerology_index,
                          fp->sl_CarrierFreq,
                          fp->symbol_rotation[link_type_sl]);
  init_timeshift_rotation(fp->ofdm_symbol_size,
                          fp->N_RB_UL * NR_NB_SC_PER_RB,
                          fp->nb_prefix_samples,
                          fp->ofdm_offset_divisor,
                          fp->timeshift_symbol_rotation);
}
////////////////////////////////////////////////////////////////////////

PHY_VARS_NR_UE *UE_TX;
PHY_VARS_NR_UE *UE_RX;

int main(int argc, char **argv)
{
  if ((uniqCfg = load_configmodule(argc, argv, CONFIG_ENABLECMDLINEONLY)) == 0) {
    exit_fun("[SCISIM] configuration module init failed\n");
  }
  logInit();
  set_glog(OAILOG_INFO); // set_glog(OAILOG_DEBUG) for verbose PHY traces
  // LDPC (SLSCH) decoding dispatches segment decode tasks to this threadpool;
  // without worker threads the tasks never run and decoding always "fails".
  initFloatingCoresTpool(1, &nrUE_params.Tpool, false, "UE-tpool");
  randominit();
  nr_generate_modulation_table();

  const int mu       = 1;
  const int N_RB     = 106;
  const int frame    = 0;
  const int slot     = 10;
  const int harq_pid = 0;

  // ---------- hardcoded, self-consistent Sidelink parameters ----------
  const uint16_t pscch_startrb       = 0;
  const uint16_t pscch_numsym        = 2;
  const uint16_t pscch_numrbs        = 12;
  const uint32_t pscch_dmrs_scr_id   = 100;   // == Nid used for PSSCH scrambling/DMRS
  const uint16_t num_subch           = 1;
  const uint16_t subchannel_size     = 50;
  const uint16_t l_subch             = 1;
  const uint16_t startrb             = 0;
  const uint8_t  pssch_numsym        = 12;
  const uint8_t  pssch_startsym      = 0;
  const uint8_t  mcs                 = 9;
  const uint8_t  mcs_table           = 0;
  const uint8_t  num_layers          = 1;
  const uint8_t  rv_index            = 0;
  const uint8_t  ndi                 = 1;
  const uint8_t  sci2_alpha_times100 = 100;
  const uint8_t  sci2_beta_offset    = 0;
  const uint8_t  pscch_sci_len       = 35;    // SCI-1A payload length (bits)
  const uint8_t  sci2_len            = 35;    // SCI-2 payload length (bits)
  // DMRS mask for pscch_numsym=2, pssch_numsym=12 (sl_dmrs_mask2[0][6]) -> symbols 3 & 10
  const uint16_t dmrs_symbol_position = 1032;

  const uint8_t  mod_order      = nr_get_Qm_ul(mcs, mcs_table);
  const uint16_t target_coderate = nr_get_code_rate_ul(mcs, mcs_table);

  // number of DMRS symbols within the PSSCH (symbols 1 .. pssch_numsym)
  int number_dmrs_symbols = 0;
  for (int l = 1; l < 1 + pssch_numsym; l++)
    number_dmrs_symbols += (dmrs_symbol_position >> l) & 1;

  // Transport-block-size computation (mirrors the MAC, nr_ue_sci_slsch.c)
  const int nREDMRS   = 6 * number_dmrs_symbols;
  const int N_REprime = 12 * pssch_numsym - nREDMRS;                 // no overhead, no CSI
  const int N_REsci1  = 12 * pscch_numrbs * pscch_numsym;
  const int N_REsci2  = get_NREsci2(sci2_alpha_times100, sci2_len, sci2_beta_offset,
                                    pssch_numsym, pscch_numsym, pscch_numrbs,
                                    l_subch, subchannel_size, target_coderate);
  const int N_RE      = N_REprime * l_subch * subchannel_size - N_REsci1 - N_REsci2;
  const uint32_t tb_size = (nr_compute_tbs_sl(mod_order, target_coderate, N_RE, num_layers) + 7) >> 3;
  const uint32_t tbslbrm = nr_compute_tbslbrm(mcs_table, N_RB, num_layers);

  printf("[SCISIM] SL params: mu=%d N_RB=%d frame.slot=%d.%d\n", mu, N_RB, frame, slot);
  printf("[SCISIM] PSCCH: startrb=%u numsym=%u numrbs=%u Nid=%u sci1_len=%u bits\n",
         pscch_startrb, pscch_numsym, pscch_numrbs, pscch_dmrs_scr_id, pscch_sci_len);
  printf("[SCISIM] PSSCH: numsym=%u subch=%u/%u l_subch=%u mcs=%u Qm=%u R=%u dmrs_mask=0x%x(%d syms)\n",
         pssch_numsym, num_subch, subchannel_size, l_subch, mcs, mod_order, target_coderate,
         dmrs_symbol_position, number_dmrs_symbols);
  printf("[SCISIM] PSSCH: N_RE=%d (sci1=%d sci2=%d) => TB size=%u bytes, tbslbrm=%u\n",
         N_RE, N_REsci1, N_REsci2, tb_size, tbslbrm);

  // ------------------------------- set-up UEs --------------------------
  UE_TX = calloc(1, sizeof(PHY_VARS_NR_UE));
  UE_RX = calloc(1, sizeof(PHY_VARS_NR_UE));
  // Must be set before configure_SL_UE(), which allocates the SLSCH HARQ
  // processes (new_gNB_ulsch) and captures max_ldpc_iterations; 0 => the LDPC
  // decoder runs no iterations and never decodes.
  UE_TX->max_ldpc_iterations = 5;
  UE_RX->max_ldpc_iterations = 5;
  configure_NR_UE(UE_TX, mu, N_RB);
  configure_SL_UE(UE_TX, mu, N_RB, 0, 0xFFFF);
  configure_NR_UE(UE_RX, mu, N_RB);
  UE_RX->is_synchronized = 1;
  configure_SL_UE(UE_RX, mu, N_RB, 0, 336);

  NR_DL_FRAME_PARMS *fp_sl = &UE_TX->SL_UE_PHY_PARAMS.sl_frame_params;

  // The TX SL HARQ processes were sized before sl_frame_params.N_RB_UL was set
  // (during configure_NR_UE). Re-init now with the proper N_RB so payload_AB is
  // large enough for the SLSCH transport block.
  nr_init_ul_harq_processes(UE_TX->sl_harq_processes, NR_MAX_HARQ_PROCESSES,
                            fp_sl->N_RB_SL, fp_sl->nb_antennas_tx);

  // Load the LDPC coding interface (libldpc.so) used by SLSCH encode (TX) and
  // decode (RX); without it nr_ulsch_encoding()/nr_slsch_decoding() segfault.
  if (load_nrLDPC_coding_interface(NULL, &UE_TX->nrLDPC_coding_interface, 32) != 0
      || load_nrLDPC_coding_interface(NULL, &UE_RX->nrLDPC_coding_interface, 32) != 0) {
    printf("[SCISIM] Error loading LDPC coding interface\n");
    exit(-1);
  }

  // minimal if_inst so the production RX can deliver SCI/SLSCH indications
  memset(&g_if_inst, 0, sizeof(g_if_inst));
  g_if_inst.sl_indication = test_sl_indication;
  UE_RX->if_inst = &g_if_inst;

  UE_nr_rxtx_proc_t proc = {0};
  proc.frame_tx   = frame;
  proc.nr_slot_tx = slot;
  proc.frame_rx   = frame;
  proc.nr_slot_rx = slot;

  // ------------------------------ build TX PDU -------------------------
  nr_phy_data_tx_t phy_data_tx = {0};
  phy_data_tx.sl_tx_action = SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH;
  sl_nr_tx_config_pscch_pssch_pdu_t *tx = &phy_data_tx.nr_sl_pssch_pscch_pdu;

  // SCI-1A payload (known pattern, masked to pscch_sci_len bits)
  uint64_t sci1_pattern = 0x1234ABCD5ULL & (((uint64_t)1 << pscch_sci_len) - 1);
  memcpy(tx->pscch_sci_payload, &sci1_pattern, sizeof(sci1_pattern));
  tx->pscch_sci_payload_len = pscch_sci_len;

  // SCI-2 payload
  uint64_t sci2_pattern = 0x0A0F0ULL & (((uint64_t)1 << sci2_len) - 1);
  memcpy(tx->sci2_payload, &sci2_pattern, sizeof(sci2_pattern));
  tx->sci2_payload_len = sci2_len;

  tx->startrb              = startrb;
  tx->pscch_numsym         = pscch_numsym;
  tx->pscch_numrbs         = pscch_numrbs;
  tx->pscch_dmrs_scrambling_id = pscch_dmrs_scr_id;
  tx->num_subch            = num_subch;
  tx->subchannel_size      = subchannel_size;
  tx->l_subch              = l_subch;
  tx->pssch_numsym         = pssch_numsym;
  tx->pssch_startsym       = pssch_startsym;
  tx->sci2_beta_offset     = sci2_beta_offset;
  tx->sci2_alpha_times_100 = sci2_alpha_times100;
  tx->tbslbrm              = tbslbrm;
  tx->tb_size              = tb_size;
  tx->target_coderate      = target_coderate;
  tx->harq_pid             = harq_pid;
  tx->mod_order            = mod_order;
  tx->mcs                  = mcs;
  tx->mcs_table            = mcs_table;
  tx->num_layers           = num_layers;
  tx->rv_index             = rv_index;
  tx->ndi                  = ndi;
  tx->dmrs_symbol_position = dmrs_symbol_position;
  tx->num_psfch_pdus       = 0;
  tx->psfch_pdu_list       = NULL;
  tx->slsch_payload        = NULL;
  tx->slsch_payload_length = 0;

  // SLSCH transport block: random bytes in the TX HARQ payload buffer
  NR_UL_UE_HARQ_t *tx_harq = &UE_TX->sl_harq_processes[harq_pid];
  uint8_t *tb_ref = malloc(tb_size);
  for (uint32_t i = 0; i < tb_size; i++) {
    uint8_t b = (uint8_t)rand();
    tx_harq->payload_AB[i] = b;
    tb_ref[i] = b;
  }

  // ------------------------------ TX chain -----------------------------
  const int samplesF_per_slot = fp_sl->symbols_per_slot * fp_sl->ofdm_symbol_size;
  c16_t txdataF_buf[fp_sl->nb_antennas_tx * samplesF_per_slot] __attribute__((aligned(32)));
  memset(txdataF_buf, 0, sizeof(txdataF_buf));
  c16_t *txdataF[fp_sl->nb_antennas_tx];
  for (int i = 0; i < fp_sl->nb_antennas_tx; ++i)
    txdataF[i] = &txdataF_buf[i * samplesF_per_slot];

  phy_data_tx.pscch_Nid = nr_generate_sci1(UE_TX, txdataF[0], fp_sl, AMP, slot, tx) & 0xFFFF;
  nr_ue_slsch_procedures(UE_TX, harq_pid, frame, slot, &phy_data_tx, txdataF);
  printf("[SCISIM] TX done: pscch_Nid=0x%x, SLSCH TB=%u bytes generated\n",
         phy_data_tx.pscch_Nid, tb_size);

  // =====================================================================
  //  STAGE 1: PSSCH / SLSCH  (perfect frequency-domain loopback)
  // =====================================================================
  const int rxFSz = fp_sl->samples_per_slot_wCP;
  c16_t rxdataF[UE_RX->SL_UE_PHY_PARAMS.sl_frame_params.nb_antennas_rx][rxFSz] __attribute__((aligned(32)));
  memset(rxdataF, 0, sizeof(rxdataF));
  memcpy(rxdataF[0], txdataF[0], samplesF_per_slot * sizeof(c16_t)); // no channel

  nr_phy_data_t phy_data_rx = {0};
  phy_data_rx.sl_rx_action = SL_NR_CONFIG_TYPE_RX_PSSCH_SCI;

  sl_nr_rx_config_pssch_sci_pdu_t *rx_sci = &phy_data_rx.nr_sl_pssch_sci_pdu;
  rx_sci->sci2_len            = sci2_len;
  rx_sci->sci2_beta_offset    = sci2_beta_offset;
  rx_sci->sci2_alpha_times_100 = sci2_alpha_times100;
  rx_sci->targetCodeRate      = target_coderate;
  rx_sci->mod_order           = mod_order;
  rx_sci->num_layers          = num_layers;
  rx_sci->dmrs_symbol_position = dmrs_symbol_position;
  rx_sci->Nid                 = phy_data_tx.pscch_Nid;
  rx_sci->startrb             = startrb;
  rx_sci->pscch_numsym        = pscch_numsym;
  rx_sci->pscch_numrbs        = pscch_numrbs;
  rx_sci->num_subch           = num_subch;
  rx_sci->subchannel_size     = subchannel_size;
  rx_sci->l_subch             = l_subch;
  rx_sci->pssch_numsym        = pssch_numsym;
  rx_sci->sense_pssch         = 0;

  sl_nr_rx_config_pssch_pdu_t *rx_slsch = &phy_data_rx.nr_sl_pssch_pdu;
  rx_slsch->tbslbrm        = tbslbrm;
  rx_slsch->tb_size        = tb_size;
  rx_slsch->target_coderate = target_coderate;
  rx_slsch->harq_pid       = harq_pid;
  rx_slsch->mod_order      = mod_order;
  rx_slsch->mcs            = mcs;
  rx_slsch->mcs_table      = mcs_table;
  rx_slsch->num_layers     = num_layers;
  rx_slsch->rv_index       = rv_index;
  rx_slsch->ndi            = ndi;

  // let the SLSCH RX know which SCI PDU describes this PSSCH (mirrors production)
  UE_RX->slsch[0].harq_process->pssch_pdu = rx_sci;

  // nr_rx_pssch writes LLRs into the buffer passed as 'llrs';
  // nr_slsch_procedures later decodes from ue->pssch_vars[0].llr, so use it here.
  int16_t *llrs = UE_RX->pssch_vars[0].llr;

  nr_rx_pssch(UE_RX, &proc, &phy_data_rx, rxFSz, rxdataF, llrs, 0, frame, slot, harq_pid);

  int nbDecode = nr_slsch_procedures(UE_RX, frame, slot, 0, &proc, &phy_data_rx, NULL, 0, NULL);

  uint8_t *b_rx = UE_RX->slsch[0].harq_process->b;
  int tb_match = (memcmp(b_rx, tb_ref, tb_size) == 0);

  // SCI-2 TX vs RX comparison (the SCI-2 indication is delivered regardless of CRC,
  // so compare the payloads directly to see whether SCI-2 actually decoded).
  uint64_t sci2_rx = (*(uint64_t *)g_sci2_payload) & (((uint64_t)1 << sci2_len) - 1);
  uint64_t sci2_tx = sci2_pattern & (((uint64_t)1 << sci2_len) - 1);
  int sci2_match = (g_num_sci2 > 0) && (sci2_rx == sci2_tx);
  printf("[SCISIM] SCI-2: captured=%d, TX payload=0x%llx RX payload=0x%llx, match=%d\n",
         g_num_sci2, (unsigned long long)sci2_tx, (unsigned long long)sci2_rx, sci2_match);

  printf("[SCISIM] PSSCH/SLSCH: nbDecode=%d, SCI2 captured=%d, SLSCH ind=%d, TB match=%d\n",
         nbDecode, g_num_sci2, g_num_slsch_ind, tb_match);
  int pssch_pass = (nbDecode > 0 && tb_match && sci2_match);
  printf("[SCISIM] ===== PSSCH/SLSCH %s =====\n", pssch_pass ? "PASS" : "FAIL");

  // =====================================================================
  //  STAGE 2: PSCCH / SCI-1A  (time-domain loopback -> pdcch_processing)
  // =====================================================================
  // OFDM-modulate the freq-domain TX signal into UE_TX time-domain txData
  int slot_start = get_samples_slot_timestamp(fp_sl, slot);
  c16_t *txp[fp_sl->nb_antennas_tx];
  for (int i = 0; i < fp_sl->nb_antennas_tx; i++)
    txp[i] = UE_TX->common_vars.txData[i] + slot_start;
  bool was_symbol_used[NR_SYMBOLS_PER_SLOT];
  for (int i = 0; i < NR_SYMBOLS_PER_SLOT; i++)
    was_symbol_used[i] = true;
  nr_tx_rotation_and_ofdm_mod(slot, fp_sl, fp_sl->nb_antennas_tx, txdataF, txp,
                              link_type_sl, was_symbol_used, UE_TX->no_phase_pre_comp);

  // time-domain loopback (no channel): copy the whole frame TX -> RX
  for (int aa = 0; aa < fp_sl->nb_antennas_rx; aa++)
    memcpy(UE_RX->common_vars.rxdata[aa], UE_TX->common_vars.txData[aa],
           fp_sl->samples_per_frame * sizeof(c16_t));

  // configure the RX_PSCCH branch exactly as production does
  nr_phy_data_t phy_data_pscch = {0};
  phy_data_pscch.sl_rx_action = SL_NR_CONFIG_TYPE_RX_PSCCH;
  sl_nr_rx_config_pscch_pdu_t *rx_pscch = &phy_data_pscch.nr_sl_pscch_pdu;
  rx_pscch->pscch_startrb          = pscch_startrb;
  rx_pscch->pscch_numsym           = pscch_numsym;
  rx_pscch->pscch_numrbs           = pscch_numrbs;
  rx_pscch->pscch_dmrs_scrambling_id = pscch_dmrs_scr_id;
  rx_pscch->num_subch              = num_subch;
  rx_pscch->subchannel_size        = subchannel_size;
  rx_pscch->l_subch                = l_subch;
  rx_pscch->pssch_numsym           = pssch_numsym;
  rx_pscch->sci_1a_length          = pscch_sci_len;
  rx_pscch->sense_pscch            = 0;

  fapi_nr_dl_config_dci_dl_pdu_rel15_t *rel15 = &phy_data_pscch.phy_pdcch_config.pdcch_config[0];
  rel15->rnti             = 0;
  rel15->BWPSize          = num_subch * subchannel_size;
  rel15->BWPStart         = pscch_startrb;
  rel15->SubcarrierSpacing = fp_sl->subcarrier_spacing;
  rel15->coreset.frequency_domain_resource[0] = pscch_startrb;
  rel15->coreset.frequency_domain_resource[1] = pscch_numrbs;
  rel15->coreset.CoreSetType = NFAPI_NR_CSET_CONFIG_PDCCH_CONFIG;
  rel15->coreset.StartSymbolIndex = 1;
  rel15->coreset.RegBundleSize = 0;
  rel15->coreset.duration = pscch_numsym;
  rel15->coreset.pdcch_dmrs_scrambling_id = pscch_dmrs_scr_id;
  rel15->coreset.scrambling_rnti = 1010;
  rel15->coreset.tci_present_in_dci = 0;
  rel15->number_of_candidates = l_subch;
  rel15->num_dci_options = 1;
  rel15->dci_length_options[0] = pscch_sci_len;
  rel15->L[0] = pscch_numrbs * pscch_numsym;
  rel15->CCE[0] = 0;
  // pdcch_processing() derives the monitored symbols from StartSymbolBitmap;
  // PSCCH is monitored from symbol 1 (MSB-first over the slot).
  rel15->coreset.StartSymbolBitmap = 1 << (fp_sl->symbols_per_slot - 1 - 1);
  phy_data_pscch.phy_pdcch_config.nb_search_space = 1;

  g_num_sci1 = 0;
  pdcch_processing(UE_RX, &proc, &phy_data_pscch, 1);

  int sci1_match = (g_num_sci1 > 0) &&
                   (memcmp(g_sci1_payload, tx->pscch_sci_payload, (pscch_sci_len + 7) / 8) == 0);
  printf("[SCISIM] PSCCH: SCI-1A captured=%d", g_num_sci1);
  if (g_num_sci1 > 0)
    printf(", RX payload=0x%010llx (TX 0x%010llx), match=%d",
           (unsigned long long)(*(uint64_t *)g_sci1_payload & (((uint64_t)1 << pscch_sci_len) - 1)),
           (unsigned long long)sci1_pattern, sci1_match);
  printf("\n");
  int pscch_pass = sci1_match;
  printf("[SCISIM] ===== PSCCH/SCI-1A %s =====\n", pscch_pass ? "PASS" : "FAIL");

  // =====================================================================
  //  STAGE 3: PSFCH ACK/NACK/DTX (perfect frequency-domain loopback)
  // =====================================================================
  sl_nr_tx_rx_config_psfch_pdu_t psfch = {.start_symbol_index = 12,
                                           .hopping_id = 1,
                                           .prb = 0,
                                           .sl_bwp_start = 0,
                                           .initial_cyclic_shift = 3,
                                           .nr_of_symbols = 1,
                                           .bit_len_harq = 1};
  int8_t psfch_decoded[2];
  const uint8_t psfch_mcs[2] = {6, 0}; // ACK then NACK
  for (int i = 0; i < 2; i++) {
    memset(txdataF_buf, 0, sizeof(txdataF_buf));
    memset(rxdataF, 0, sizeof(rxdataF));
    psfch.mcs = psfch_mcs[i];
    nr_generate_psfch0(UE_TX, txdataF, fp_sl, AMP, slot, &psfch);
    memcpy(rxdataF[0], txdataF[0], samplesF_per_slot * sizeof(c16_t));
    psfch_decoded[i] = nr_ue_decode_psfch0(UE_RX, frame, slot, rxdataF, &psfch);
  }
  memset(rxdataF, 0, sizeof(rxdataF));
  int8_t psfch_dtx = nr_ue_decode_psfch0(UE_RX, frame, slot, rxdataF, &psfch);
  int psfch_pass = psfch_decoded[0] == 0 && psfch_decoded[1] == 1 && psfch_dtx < 0;
  printf("[SCISIM] PSFCH: ACK=%d NACK=%d DTX=%d\n", psfch_decoded[0], psfch_decoded[1], psfch_dtx);
  printf("[SCISIM] ===== PSFCH ACK/NACK/DTX %s =====\n", psfch_pass ? "PASS" : "FAIL");

  // ------------------------------- summary -----------------------------
  printf("[SCISIM] SUMMARY: PSSCH/SLSCH=%s  PSCCH/SCI-1A=%s  PSFCH=%s\n",
         pssch_pass ? "PASS" : "FAIL", pscch_pass ? "PASS" : "FAIL", psfch_pass ? "PASS" : "FAIL");

  free(tb_ref);
  return (pssch_pass && pscch_pass && psfch_pass) ? 0 : 1;
}
