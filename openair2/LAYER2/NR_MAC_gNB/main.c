/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "NR_DRB-ToAddMod.h"
#include "NR_DRB-ToAddModList.h"
#include "NR_MAC_COMMON/nr_mac.h"
#include "NR_MAC_COMMON/nr_mac_common.h"
#include "NR_MAC_gNB/mac_proto.h"
#include "NR_MAC_gNB/mac_rrc_ul.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"
#include "NR_PHY_INTERFACE/NR_IF_Module.h"
#include "NR_RLC-BearerConfig.h"
#include "NR_RadioBearerConfig.h"
#include "NR_ServingCellConfig.h"
#include "NR_ServingCellConfigCommon.h"
#include "NR_TAG.h"
#include "assertions.h"
#include "common/ngran_types.h"
#include "common/ran_context.h"
#include "executables/softmodem-common.h"
#include "linear_alloc.h"
#include "nr_pdcp/nr_pdcp_entity.h"
#include "nr_pdcp/nr_pdcp_oai_api.h"
#include "nr_rlc/nr_rlc_oai_api.h"
#include "openair2/F1AP/f1ap_ids.h"
#include "openair2/F1AP/lib/f1ap_interface_management.h"
#include "seq_arr.h"
#include "system.h"
#include "time_meas.h"
#include "utils.h"

#define MACSTATSSTRLEN 36256

void *nrmac_stats_thread(void *arg) {

  gNB_MAC_INST *gNB = (gNB_MAC_INST *)arg;

  char output[MACSTATSSTRLEN] = {0};
  const char *end = output + MACSTATSSTRLEN;
  FILE *file = fopen("nrMAC_stats.log","w");
  if (!file) {
    LOG_W(NR_MAC, "Cannot open nrMAC_stats.log: %d, %s\n", errno, strerror(errno));
    return NULL;
  }

  while (oai_exit == 0) {
    char *p = output;
    NR_SCHED_LOCK(&gNB->sched_lock);
    p += dump_mac_stats(gNB, p, end - p, false);
    p += snprintf(p, end - p, "\n");
    p += print_meas_log(&gNB->gNB_scheduler, "gNB_scheduler", NULL, NULL, p, end - p);
    p += print_meas_log(&gNB->rx_ulsch_sdu, "rx_ulsch_sdu", NULL, NULL, p, end - p);
    p += print_meas_log(&gNB->schedule_dlsch, "dlsch scheduler", NULL, NULL, p, end - p);
    p += print_meas_log(&gNB->schedule_ulsch, "ulsch scheduler", NULL, NULL, p, end - p);
    p += print_meas_log(&gNB->schedule_ra, "RA scheduler", NULL, NULL, p, end - p);
    p += print_meas_log(&gNB->rlc_data_req, "rlc_data_req", NULL, NULL, p, end - p);
    p += print_meas_log(&gNB->nr_srs_ri_computation_timer, "UL-RI computation time", NULL, NULL, p, end - p);
    p += print_meas_log(&gNB->nr_srs_tpmi_computation_timer, "UL-TPMI computation time", NULL, NULL, p, end - p);
    NR_SCHED_UNLOCK(&gNB->sched_lock);
    size_t len = p - output;
    if (fwrite(output, len, 1, file) != 1 || fflush(file) != 0) {
      LOG_E(NR_MAC, "error while writing nrMAC_stats.log: %d, %s\n", errno, strerror(errno));
      break;
    }
    sleep(1);
    if (ftruncate(fileno(file), 0) != 0 || fseek(file, 0, SEEK_SET) != 0) {
      LOG_E(NR_MAC, "error while writing nrMAC_stats.log: %d, %s\n", errno, strerror(errno));
      break;
    }
  }
  fclose(file);
  return NULL;
}

void clear_mac_stats(gNB_MAC_INST *gNB) {
  UE_iterator(gNB->UE_info.connected_ue_list, UE) {
    memset(&UE->mac_stats,0,sizeof(UE->mac_stats));
  }
}

static char *st_append(char *start, const char *end, const char *format, ...)
{
  size_t space = end - start;
  va_list args;
  va_start(args, format);
  size_t written = vsnprintf(start, space, format, args);
  va_end(args);
  if (written <= space)
    return start + written;
  else
    return (char *)end;
}

size_t dump_mac_stats(gNB_MAC_INST *gNB, char *output, size_t strlen, bool reset_rsrp)
{
  const char *begin = output;
  const char *end = output + strlen;

  /* this function is called from gNB_dlsch_ulsch_scheduler(), so assumes the
   * scheduler to be locked*/
  NR_SCHED_ENSURE_LOCKED(&gNB->sched_lock);

  UE_iterator(gNB->UE_info.connected_ue_list, UE) {
    NR_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    NR_mac_stats_t *stats = &UE->mac_stats;
    const int avg_rsrp = stats->num_rsrp_meas > 0 ? stats->cumul_rsrp / stats->num_rsrp_meas : 0;
    const int avg_sinrx10 = stats->num_sinr_meas > 0 ? stats->cumul_sinrx10 / stats->num_sinr_meas : 0;

    output = st_append(output, end, "UE RNTI %04x CU-UE-ID ", UE->rnti);
    if (du_exists_f1_ue_data(UE->rnti)) {
      f1_ue_data_t ued = du_get_f1_ue_data(UE->rnti);
      output = st_append(output, end, "%d", ued.secondary_ue);
    } else {
      output = st_append(output, end, "(none)");
    }

    bool in_sync = !sched_ctrl->ul_failure;
    output = st_append(output,
                       end,
                       " %s PH %d dB PCMAX %d dBm",
                       in_sync ? "in-sync" : "out-of-sync",
                       sched_ctrl->ph,
                       sched_ctrl->pcmax);

    if (stats->num_rsrp_meas)
      output = st_append(output, end, ", average RSRP %d (%d meas)", avg_rsrp, stats->num_rsrp_meas);

    if (stats->num_sinr_meas) {
      output = st_append(output,
                         end,
                         ", average SINR %d.%d (%d meas)",
                         avg_sinrx10 / 10,
                         avg_sinrx10 % 10,
                         stats->num_sinr_meas);
    }

    output = st_append(output, end, "\n");

    if(sched_ctrl->CSI_report.cri_ri_li_pmi_cqi_report.print_report)
      output = st_append(output,
                         end,
                         "UE %04x: CQI %d, RI %d, PMI (%d,%d)\n",
                         UE->rnti,
                         sched_ctrl->CSI_report.cri_ri_li_pmi_cqi_report.wb_cqi_1tb,
                         sched_ctrl->CSI_report.cri_ri_li_pmi_cqi_report.ri+1,
                         sched_ctrl->CSI_report.cri_ri_li_pmi_cqi_report.pmi_x1,
                         sched_ctrl->CSI_report.cri_ri_li_pmi_cqi_report.pmi_x2);

    if (stats->srs_stats[0] != '\0') {
      output = st_append(output, end, "UE %04x: %s\n", UE->rnti, stats->srs_stats);
    }

    output = st_append(output,
                       end,
                       "UE %04x: dlsch_rounds ", UE->rnti);
    output = st_append(output, end, "%"PRIu64, stats->dl.rounds[0]);
    for (int i = 1; i < gNB->dl_bler.harq_round_max; i++)
      output = st_append(output, end, "/%"PRIu64, stats->dl.rounds[i]);

    float pucch_snr = nr_mac_get_snr(&sched_ctrl->pucch_pc);
    float pucch_snr_diff = (pucch_snr * 10.0f - sched_ctrl->pucch_pc.target_snrx10) / 10.0f;
    output = st_append(output,
                       end,
                       ", dlsch_errors %" PRIu64
                       ", pucch0_DTX %d (SNR %.1f%+.1f dB), BLER %.5f MCS (%d) %d CCE fail %d, goodput %.2f Mbps\n",
                       stats->dl.errors,
                       stats->pucch0_DTX,
                       pucch_snr,
                       pucch_snr_diff,
                       sched_ctrl->dl_bler_stats.bler,
                       UE->current_DL_BWP.mcsTableIdx,
                       sched_ctrl->dl_bler_stats.mcs,
                       sched_ctrl->dl_cce_fail,
                       UE->dl_thr_ue_display / 1e6);
    if (reset_rsrp) {
      stats->num_rsrp_meas = 0;
      stats->cumul_rsrp = 0;
      stats->num_sinr_meas = 0;
      stats->cumul_sinrx10 = 0;
    }
    output = st_append(output,
                       end,
                       "UE %04x: ulsch_rounds ", UE->rnti);
    output = st_append(output, end, "%"PRIu64, stats->ul.rounds[0]);
    for (int i = 1; i < gNB->ul_bler.harq_round_max; i++)
      output = st_append(output, end, "/%"PRIu64, stats->ul.rounds[i]);

    float snr = nr_mac_get_snr(&sched_ctrl->pusch_pc);
    float diff_target = (snr * 10.0f - sched_ctrl->pusch_pc.target_snrx10) / 10.0f;
    output = st_append(
        output,
        end,
        ", ulsch_errors %" PRIu64
        ", ulsch_DTX %d, BLER %.5f MCS (%d) %d (Qm %d deltaMCS %d dB) NPRB %d SNR %.1f (%+.1f) dB CCE fail %d, goodput %.2f Mbps\n",
        stats->ul.errors,
        stats->ulsch_DTX,
        sched_ctrl->ul_bler_stats.bler,
        UE->current_UL_BWP.mcs_table,
        sched_ctrl->ul_bler_stats.mcs,
        nr_get_Qm_ul(sched_ctrl->ul_bler_stats.mcs, UE->current_UL_BWP.mcs_table),
        UE->mac_stats.deltaMCS,
        UE->mac_stats.NPRB,
        snr,
        diff_target,
        sched_ctrl->ul_cce_fail,
        UE->ul_thr_ue_display / 1e6);

    for (int i = 0; i < seq_arr_size(&sched_ctrl->lc_config); i++) {
      const nr_lc_config_t *c = seq_arr_at(&sched_ctrl->lc_config, i);
      output = st_append(output,
                         end,
                         "UE %04x: LCID %d: TX %14"PRIu64" RX %14"PRIu64" bytes\n",
                         UE->rnti,
                         c->lcid,
                         stats->dl.lc_bytes[c->lcid],
                         stats->ul.lc_bytes[c->lcid]);
    }
  }
  DevAssert(output <= end);
  return output - begin;
}

static void mac_rrc_init(gNB_MAC_INST *mac, ngran_node_t node_type)
{
  switch (node_type) {
    case ngran_gNB_CU:
      AssertFatal(1 == 0, "nothing to do for CU\n");
      break;
    case ngran_gNB_DU:
      mac_rrc_ul_f1ap_init(&mac->mac_rrc);
      break;
    case ngran_gNB:
      mac_rrc_ul_direct_init(&mac->mac_rrc);
      break;
    default:
      AssertFatal(0 == 1, "Unknown node type %d\n", node_type);
      break;
  }
}

void mac_top_init_gNB(ngran_node_t node_type,
                      NR_ServingCellConfigCommon_t *scc,
                      const nr_mac_config_t *config,
                      const nr_rlc_configuration_t *default_rlc_config)
{
  AssertFatal(RC.nb_nr_macrlc_inst == 1, "what is the point of calling %s() if you don't need exactly one MAC?\n", __func__);

  if (RC.nb_nr_macrlc_inst > 0) {

    RC.nrmac = (gNB_MAC_INST **) malloc16(RC.nb_nr_macrlc_inst *sizeof(gNB_MAC_INST *));
    
    AssertFatal(RC.nrmac != NULL,"can't ALLOCATE %zu Bytes for %d gNB_MAC_INST with size %zu \n",
                RC.nb_nr_macrlc_inst * sizeof(gNB_MAC_INST *),
                RC.nb_nr_macrlc_inst, sizeof(gNB_MAC_INST));

    for (module_id_t i = 0; i < RC.nb_nr_macrlc_inst; i++) {

      RC.nrmac[i] = (gNB_MAC_INST *) malloc16(sizeof(gNB_MAC_INST));
      
      AssertFatal(RC.nrmac != NULL,"can't ALLOCATE %zu Bytes for %d gNB_MAC_INST with size %zu \n",
                  RC.nb_nr_macrlc_inst * sizeof(gNB_MAC_INST *),
                  RC.nb_nr_macrlc_inst, sizeof(gNB_MAC_INST));
      
      LOG_D(MAC,"[MAIN] ALLOCATE %zu Bytes for %d gNB_MAC_INST @ %p\n",sizeof(gNB_MAC_INST), RC.nb_nr_macrlc_inst, RC.mac);
      
      bzero(RC.nrmac[i], sizeof(gNB_MAC_INST));
      nr_mac_pcch_queue_init(&RC.nrmac[i]->common_channels[0]);

      RC.nrmac[i]->Mod_id = i;

      RC.nrmac[i]->tag = (NR_TAG_t*)malloc(sizeof(NR_TAG_t));
      memset((void*)RC.nrmac[i]->tag,0,sizeof(NR_TAG_t));
      for(int n = 0; n < MAX_NUM_OF_SSB; n++)
        RC.nrmac[i]->sib1_pdsch[n].time_domain_allocation = -1;
      RC.nrmac[i]->common_channels[0].ServingCellConfigCommon = scc;
      RC.nrmac[i]->radio_config = *config;
      RC.nrmac[i]->rlc_config = *default_rlc_config;

      RC.nrmac[i]->first_MIB = true;
      RC.nrmac[i]->num_scheduled_prach_rx = 0;
      RC.nrmac[i]->common_channels[0].mib = get_new_MIB_NR(scc);

      RC.nrmac[i]->cset0_bwp_start = 0;
      RC.nrmac[i]->cset0_bwp_size = 0;

      RC.nrmac[i]->ul_next = (fsn_t) {.mu = *scc->ssbSubcarrierSpacing};
      RC.nrmac[i]->print_ue_stats = true;

      pthread_mutex_init(&RC.nrmac[i]->sched_lock, NULL);

      uid_linear_allocator_init(&RC.nrmac[i]->UE_info.uid_allocator);

      RC.nrmac[i]->ul_ri_tpmi_select = nr_ul_ri_tpmi_select_default;
      RC.nrmac[i]->ul_tda_select = nr_ul_tda_select_default;
      RC.nrmac[i]->ul_beam_select = nr_ul_beam_select_default;
      RC.nrmac[i]->ul_mcs_select = nr_ul_mcs_select_default;
      RC.nrmac[i]->ul_rb_alloc = nr_ul_proportional_fair;

      RC.nrmac[i]->dl_lcid_alloc = nr_dl_lcid_alloc_default;

      if (get_softmodem_params()->phy_test) {
        RC.nrmac[i]->pre_processor_dl = nr_preprocessor_phytest;
        RC.nrmac[i]->pre_processor_ul = nr_ul_preprocessor_phytest;
      } else {
        RC.nrmac[i]->pre_processor_dl = nr_dlsch_preprocessor;
        RC.nrmac[i]->pre_processor_ul = nr_ulsch_preprocessor;
        RC.nrmac[i]->dl_ri_pmi_select = nr_dl_ri_pmi_select_default;
        RC.nrmac[i]->dl_mcs_select = nr_dl_mcs_select_default;
        RC.nrmac[i]->dl_beam_select = nr_dl_beam_select_default;
        RC.nrmac[i]->dl_tda_select = nr_dl_tda_select_default;
        RC.nrmac[i]->dl_rb_alloc = nr_dl_proportional_fair;
      }
      if (!IS_SOFTMODEM_NOSTATS)
        threadCreate(&RC.nrmac[i]->stats_thread,
                     nrmac_stats_thread,
                     (void *)RC.nrmac[i],
                     "MAC_STATS",
                     -1,
                     sched_get_priority_min(SCHED_OAI) + 1);
      mac_rrc_init(RC.nrmac[i], node_type);
    }//END for (i = 0; i < RC.nb_nr_macrlc_inst; i++)

    nr_rlc_op_mode_t mode = NODE_IS_MONOLITHIC(node_type) ? NR_RLC_OP_MODE_MONO_GNB : NR_RLC_OP_MODE_SPLIT_GNB;
    int success = nr_rlc_module_init(mode);
    AssertFatal(success == 0,"Could not initialize RLC layer\n");
  } else {
    RC.nrmac = NULL;
  }

  // Initialize Linked-List for Active UEs
  for (module_id_t i = 0; i < RC.nb_nr_macrlc_inst; i++) {
    gNB_MAC_INST *nrmac = RC.nrmac[i];
    nrmac->if_inst = NR_IF_Module_init(i);
    memset(&nrmac->UE_info, 0, sizeof(nrmac->UE_info));
  }

  du_init_f1_ue_data();

  srand48(0);
}

void mac_top_destroy_gNB(gNB_MAC_INST *mac)
{
  NR_COMMON_channels_t *cc = &mac->common_channels[0];
  nr_mac_pcch_queue_free(cc);
  ASN_STRUCT_FREE(asn_DEF_NR_BCCH_BCH_Message, cc->mib);
  ASN_STRUCT_FREE(asn_DEF_NR_BCCH_DL_SCH_Message, cc->sib1);
  ASN_STRUCT_FREE(asn_DEF_NR_ServingCellConfigCommon, cc->ServingCellConfigCommon);
  NR_UEs_t *UE_info = &mac->UE_info;
  for (int i = 0; i < sizeofArray(UE_info->connected_ue_list); ++i)
    if (UE_info->connected_ue_list[i])
      delete_nr_ue_data(UE_info->connected_ue_list[i], &UE_info->uid_allocator);
  for (int i = 0; i < sizeofArray(UE_info->access_ue_list); ++i)
    if (UE_info->access_ue_list[i])
      delete_nr_ue_data(UE_info->access_ue_list[i], &UE_info->uid_allocator);
  if (mac->f1_config.setup_resp)
    free_f1ap_setup_response(mac->f1_config.setup_resp);
  free(mac->f1_config.setup_resp);
}

void nr_mac_send_f1_setup_req(void)
{
  gNB_MAC_INST *mac = RC.nrmac[0];
  DevAssert(mac);
  mac->mac_rrc.f1_setup_request(mac->f1_config.setup_req);
}
