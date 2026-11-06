/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*
 * Google Benchmark suite for L3 NR RRC ASN.1 UPER message encoding and
 * decoding (3GPP TS 38.331): PCCH Paging roundtrip, RRCReconfiguration
 * encoding, and SIB2 UPER encode/decode roundtrip.
 */

#include "benchmark/benchmark.h"
#include <cstring>

extern "C" {
#include "openair2/RRC/NR/MESSAGES/asn1_msg.h"
#include "common/ran_context.h"
#include "common/utils/LOG/log.h"
#include "common/utils/nr/nr_common.h"
#include "NR_DRB-ToAddMod.h"
#include "NR_DRB-ToAddModList.h"
#include "NR_SRB-ToAddModList.h"
#include "NR_SIB2.h"
#include "NR_PCCH-Message.h"
#include "NR_PagingRecord.h"
#include "asn_application.h"
#include "ds/byte_array.h"
RAN_CONTEXT_t RC;
}

namespace {

static NR_SRB_ToAddModList_t *make_srb_config_list(void)
{
  NR_SRB_ToAddModList_t *list = (NR_SRB_ToAddModList_t *)calloc_or_fail(1, sizeof(*list));
  for (int i = 1; i <= 2; i++) {
    NR_SRB_ToAddMod_t *srb = (NR_SRB_ToAddMod_t *)calloc_or_fail(1, sizeof(*srb));
    ASN_SEQUENCE_ADD(&list->list, srb);
    srb->srb_Identity = i;
    srb->reestablishPDCP = (long *)calloc_or_fail(1, sizeof(*srb->reestablishPDCP));
    *srb->reestablishPDCP = 0;
  }
  return list;
}

static NR_DRB_ToAddModList_t *make_drb_config_list(int num_drbs)
{
  NR_DRB_ToAddModList_t *list = (NR_DRB_ToAddModList_t *)calloc_or_fail(1, sizeof(*list));
  for (int i = 1; i <= num_drbs; i++) {
    NR_DRB_ToAddMod_t *drb = (NR_DRB_ToAddMod_t *)calloc_or_fail(1, sizeof(*drb));
    ASN_SEQUENCE_ADD(&list->list, drb);
    drb->drb_Identity = i;
    drb->reestablishPDCP = (long *)calloc_or_fail(1, sizeof(*drb->reestablishPDCP));
    *drb->reestablishPDCP = 0;
  }
  return list;
}

static void BM_nr_rrc_paging_roundtrip(benchmark::State &state)
{
  const int num_records = static_cast<int>(state.range(0));
  nr_paging_params_t params[NR_PCCH_MAX_PAGING_RECORDS] = {};
  nr_paging_params_t decoded[NR_PCCH_MAX_PAGING_RECORDS] = {};
  for (int i = 0; i < num_records; i++) {
    params[i].ue_identity_type = NR_PagingUE_Identity_PR_ng_5G_S_TMSI;
    params[i].ue_identity.fiveg_s_tmsi = nr_construct_5g_s_tmsi(1, 0, 0xc00006f0U + i);
    params[i].access_type = (i % 2 == 1);
  }

  for (auto _ : state) {
    byte_array_t msg = do_NR_Paging(num_records, params);
    int count = 0;
    int rc = nr_pcch_decode(msg, decoded, &count);
    benchmark::DoNotOptimize(rc);
    benchmark::DoNotOptimize(count);
    free_byte_array(msg);
  }
  state.SetItemsProcessed(state.iterations() * num_records);
}

static void BM_nr_rrc_reconfiguration_encode(benchmark::State &state)
{
  const int num_drbs = static_cast<int>(state.range(0));
  nr_rrc_reconfig_param_t params = {};
  params.srb_config_list = make_srb_config_list();
  params.drb_config_list = make_drb_config_list(num_drbs);
  int drb_rel[] = {3};
  params.drb_rel = drb_rel;
  params.n_drb_rel = 1;
  params.num_nas_msg = 1;
  params.masterKeyUpdate = false;
  params.nextHopChainingCount = 1;

  uint8_t nas_payload[16] = "NAS_PDU_BENCH";
  params.dedicated_NAS_msg_list[0].buf = nas_payload;
  params.dedicated_NAS_msg_list[0].len = sizeof(nas_payload);

  for (auto _ : state) {
    byte_array_t msg = do_RRCReconfiguration(&params);
    benchmark::DoNotOptimize(msg.len);
    benchmark::DoNotOptimize(msg.buf);
    free_byte_array(msg);
  }

  ASN_STRUCT_FREE(asn_DEF_NR_DRB_ToAddModList, params.drb_config_list);
  ASN_STRUCT_FREE(asn_DEF_NR_SRB_ToAddModList, params.srb_config_list);
}

static void BM_nr_rrc_sib2_roundtrip(benchmark::State &state)
{
  NR_SIB2_t *sib2 = (NR_SIB2_t *)calloc_or_fail(1, sizeof(*sib2));
  sib2->cellReselectionInfoCommon.q_Hyst = NR_SIB2__cellReselectionInfoCommon__q_Hyst_dB0;
  sib2->cellReselectionServingFreqInfo.cellReselectionPriority = 3;
  sib2->cellReselectionServingFreqInfo.threshServingLowP = 10;
  sib2->intraFreqCellReselectionInfo.q_RxLevMin = -56;
  sib2->intraFreqCellReselectionInfo.s_IntraSearchP = 22;
  sib2->intraFreqCellReselectionInfo.t_ReselectionNR = 1;
  sib2->intraFreqCellReselectionInfo.deriveSSB_IndexFromCell = true;

  for (auto _ : state) {
    byte_array_t ba = do_SIB2_NR(sib2);
    NR_SIB2_t *decoded = nullptr;
    asn_dec_rval_t dec = uper_decode(nullptr, &asn_DEF_NR_SIB2, (void **)&decoded, ba.buf, ba.len, 0, 0);
    benchmark::DoNotOptimize(dec.code);
    ASN_STRUCT_FREE(asn_DEF_NR_SIB2, decoded);
    free_byte_array(ba);
  }

  ASN_STRUCT_FREE(asn_DEF_NR_SIB2, sib2);
}

BENCHMARK(BM_nr_rrc_paging_roundtrip)->Arg(1)->Arg(4)->Arg(8);
BENCHMARK(BM_nr_rrc_reconfiguration_encode)->Arg(1)->Arg(2)->Arg(4);
BENCHMARK(BM_nr_rrc_sib2_roundtrip);

} // namespace

int main(int argc, char **argv)
{
  logInit();
  g_log->log_component[NR_RRC].level = -1;
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
