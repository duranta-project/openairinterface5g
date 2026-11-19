/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include <stdio.h>
#include <stdlib.h>
#include "common/utils/LOG/log.h"
#include "common/utils/oai_asn1.h"
#include "LAYER2/NR_MAC_COMMON/ue_capability_handler.h"

typedef struct {
  long dl_bw;
  long ul_bw;
  int scs;
} candidate_t;

static NR_UE_NR_Capability_t make_capability(const candidate_t *first, const candidate_t *second)
{
  NR_UE_NR_Capability_t cap = {0};
  asn1cCalloc(cap.rf_Parameters.supportedBandCombinationList, bands);
  asn1cCalloc(cap.featureSetCombinations, combinations);
  asn1cCalloc(cap.featureSets, sets);
  asn1cCalloc(sets->featureSetsDownlink, dl_sets);
  asn1cCalloc(sets->featureSetsUplink, ul_sets);
  asn1cCalloc(sets->featureSetsDownlinkPerCC, dl_ccs);
  asn1cCalloc(sets->featureSetsUplinkPerCC, ul_ccs);

  const candidate_t *candidates[] = {first, second, NULL};
  for (int i = 0; candidates[i]; i++) {
    const candidate_t *cc = candidates[i];
    asn1cSequenceAdd(bands->list, NR_BandCombination_t, bc);
    bc->featureSetCombination = i;
    asn1cSequenceAdd(bc->bandList.list, NR_BandParameters_t, band);
    band->present = NR_BandParameters_PR_nr;
    asn1cCalloc(band->choice.nr, nr_band);
    nr_band->bandNR = 78;
    asn1cCallocOne(nr_band->ca_BandwidthClassDL_NR, NR_CA_BandwidthClassNR_a);
    asn1cCallocOne(nr_band->ca_BandwidthClassUL_NR, NR_CA_BandwidthClassNR_a);

    asn1cSequenceAdd(combinations->list, NR_FeatureSetCombination_t, combination);
    asn1cSequenceAdd(combination->list, NR_FeatureSetsPerBand_t, per_band);
    asn1cSequenceAdd(per_band->list, NR_FeatureSet_t, feature);
    feature->present = NR_FeatureSet_PR_nr;
    asn1cCalloc(feature->choice.nr, nr);
    nr->downlinkSetNR = i + 1;
    nr->uplinkSetNR = i + 1;

    asn1cSequenceAdd(dl_sets->list, NR_FeatureSetDownlink_t, dl_set);
    asn1cSequenceAdd(dl_set->featureSetListPerDownlinkCC.list, long, dl_id);
    *dl_id = i + 1;
    asn1cSequenceAdd(dl_ccs->list, NR_FeatureSetDownlinkPerCC_t, dl);
    dl->supportedSubcarrierSpacingDL = cc->scs;
    dl->supportedBandwidthDL.present = NR_SupportedBandwidth_PR_fr1;
    dl->supportedBandwidthDL.choice.fr1 = cc->dl_bw;

    asn1cSequenceAdd(ul_sets->list, NR_FeatureSetUplink_t, ul_set);
    asn1cSequenceAdd(ul_set->featureSetListPerUplinkCC.list, long, ul_id);
    *ul_id = i + 1;
    asn1cSequenceAdd(ul_ccs->list, NR_FeatureSetUplinkPerCC_t, ul);
    ul->supportedSubcarrierSpacingUL = cc->scs;
    ul->supportedBandwidthUL.present = NR_SupportedBandwidth_PR_fr1;
    ul->supportedBandwidthUL.choice.fr1 = cc->ul_bw;
  }
  return cap;
}

static int check(const char *name, int cell_bw, const candidate_t *first, const candidate_t *second, int expected_id)
{
  NR_UE_NR_Capability_t cap = make_capability(first, second);
  NR_feature_set_ids_t ids = get_feature_set_ids(&cap, 78, NR_SubcarrierSpacing_kHz30, cell_bw, NR_SA);
  ASN_STRUCT_RESET(asn_DEF_NR_UE_NR_Capability, &cap);
  if (ids.dlset_id == expected_id && ids.ulset_id == expected_id
      && ids.dl_feature_set_percc_id == expected_id && ids.ul_feature_set_percc_id == expected_id)
    return 0;
  fprintf(stderr, "%s: expected ID %d, got DL/UL sets %d/%d, per-CC %d/%d\n",
          name, expected_id, ids.dlset_id, ids.ulset_id, ids.dl_feature_set_percc_id, ids.ul_feature_set_percc_id);
  return 1;
}

int main(void)
{
  const long narrow = NR_SupportedBandwidth__fr1_mhz40;
  const long wide = NR_SupportedBandwidth__fr1_mhz100;
  const int scs = NR_SubcarrierSpacing_kHz30;
  const candidate_t both_wide = {wide, wide, scs};
  const candidate_t dl_narrow = {narrow, wide, scs};
  const candidate_t ul_narrow = {wide, narrow, scs};
  const candidate_t both_narrow = {narrow, narrow, scs};
  const candidate_t wrong_scs = {narrow, narrow, NR_SubcarrierSpacing_kHz15};

  logInit();
  int failed = 0;
  failed += check("exact bandwidth", 100, &both_wide, NULL, 1);
  failed += check("wider bandwidth", 40, &both_wide, NULL, 1);
  failed += check("skip narrow DL", 100, &dl_narrow, &both_wide, 2);
  failed += check("skip narrow UL", 100, &ul_narrow, &both_wide, 2);
  failed += check("fallback for narrow DL", 100, &dl_narrow, NULL, 1);
  failed += check("fallback for narrow UL", 100, &ul_narrow, NULL, 1);
  failed += check("fallback for narrow DL and UL", 100, &both_narrow, NULL, 1);
  failed += check("keep first fallback", 100, &dl_narrow, &ul_narrow, 1);
  failed += check("keep first bandwidth match", 40, &dl_narrow, &both_wide, 1);
  failed += check("skip wrong SCS", 40, &wrong_scs, &both_wide, 2);
  failed += check("reject wrong SCS", 100, &wrong_scs, NULL, 0);
  logTerm();
  return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
