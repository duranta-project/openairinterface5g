/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "rrc_defs.h"
#include "rrc_proto.h"

// Maximum channel bandwidth (MHz) per band and SCS (15/30/60 kHz) for a single CC, 0 = SCS not defined
// for the band. Transcribed from TS 38.101-1 v15.7.0 Table 5.3.5-1.
typedef struct {
  int band;
  int dl[3];
  int ul[3];
} fr1_max_bw_t;
 
static const fr1_max_bw_t fr1_max_bw[] = {
    {1, {20, 20, 20}, {20, 20, 20}},      {2, {20, 20, 20}, {20, 20, 20}},      {3, {30, 30, 30}, {30, 30, 30}},
    {5, {20, 20, 0}, {20, 20, 0}},        {7, {20, 20, 20}, {20, 20, 20}},      {8, {20, 20, 0}, {20, 20, 0}},
    {12, {15, 15, 0}, {15, 15, 0}},       {20, {20, 20, 0}, {20, 20, 0}},       {25, {20, 20, 20}, {20, 20, 20}},
    {28, {20, 20, 0}, {20, 20, 0}},       {34, {15, 15, 15}, {15, 15, 15}},     {38, {20, 20, 20}, {20, 20, 20}},
    {39, {40, 40, 40}, {40, 40, 40}},     {40, {50, 80, 80}, {50, 80, 80}},     {41, {50, 100, 100}, {50, 100, 100}},
    {50, {50, 80, 80}, {50, 60, 60}},     {51, {5, 0, 0}, {5, 0, 0}},           {66, {40, 40, 40}, {40, 40, 40}},
    {70, {25, 25, 25}, {15, 15, 15}},     {71, {20, 20, 0}, {20, 20, 0}},       {74, {20, 20, 20}, {20, 20, 20}},
    {75, {20, 20, 20}, {20, 20, 20}},     {76, {5, 0, 0}, {5, 0, 0}},           {77, {50, 100, 100}, {50, 100, 100}},
    {78, {50, 100, 100}, {50, 100, 100}}, {79, {50, 100, 100}, {50, 100, 100}}, {80, {30, 30, 30}, {30, 30, 30}},
    {81, {20, 20, 0}, {20, 20, 0}},       {82, {20, 20, 0}, {20, 20, 0}},       {83, {20, 20, 0}, {20, 20, 0}},
    {84, {20, 20, 20}, {20, 20, 20}},     {86, {40, 40, 40}, {40, 40, 40}},
};

/* 0 if the band / SCS combination is not in the table (band missing, or SCS not defined for the band) */
static int max_channel_bw_mhz(int band, bool is_fr2, int scs, bool dl)
{
  if (is_fr2)
    return (scs == 2 || scs == 3) ? 200 : 0; /* mandatory set: 50, 100, 200 MHz */
  if (scs < 0 || scs > 2)
    return 0;
  for (size_t i = 0; i < sizeof(fr1_max_bw) / sizeof(fr1_max_bw[0]); i++)
    if (fr1_max_bw[i].band == band)
      return dl ? fr1_max_bw[i].dl[scs] : fr1_max_bw[i].ul[scs];
  return 0;
}

// The band is a candidate if it is in the filter
static bool band_passes_filter(const NR_FreqBandList_t *filter, int band)
{
  if (!filter)
    return true; /* NOTE 1: enquiry without frequencyBandListFilter is not supported; treat as no restriction */
  for (int i = 0; i < filter->list.count; i++) {
    const NR_FreqBandInformation_t *fb = filter->list.array[i];
    if (fb->present == NR_FreqBandInformation_PR_bandInformationNR && fb->choice.bandInformationNR->bandNR == band)
      return true; /* E-UTRA bands in the filter are irrelevant for rat-Type nr */
  }
  return false;
}

/*
 * No capability file: build a minimal container with the right structure. Values (band, SCS, BW)
 * are filled in by the common update path afterwards. The band combination / feature sets are only
 * generated if the band is in the frequencyBandListFilter (5.6.1.4: candidate band combinations)
 *   band list -> 1 band combination (single NR band, class A) -> featureSetCombination 0
 *             -> DL set 1 / UL set 1 -> one DL and one UL per-CC feature set
 */
static NR_UE_NR_Capability_t *create_minimal_capability(int band, bool with_band_combination)
{
  NR_UE_NR_Capability_t *cap = CALLOC(1, sizeof(*cap));
  AssertFatal(cap, "out of memory\n");
  asn1cSequenceAdd(cap->rf_Parameters.supportedBandListNR.list, NR_BandNR_t, bandnr);
  bandnr->bandNR = band;
  if (!with_band_combination)
    return cap;

  asn1cCalloc(cap->rf_Parameters.supportedBandCombinationList, bcl);
  asn1cSequenceAdd(bcl->list, NR_BandCombination_t, bc);
  asn1cSequenceAdd(bc->bandList.list, NR_BandParameters_t, bp);
  bp->present = NR_BandParameters_PR_nr;
  asn1cCalloc(bp->choice.nr, bp_nr);
  bp_nr->bandNR = band;
  asn1cCalloc(bp_nr->ca_BandwidthClassDL_NR, dl_class);
  *dl_class = NR_CA_BandwidthClassNR_a;
  asn1cCalloc(bp_nr->ca_BandwidthClassUL_NR, ul_class);
  *ul_class = NR_CA_BandwidthClassNR_a;
  bc->featureSetCombination = 0;

  asn1cCalloc(cap->featureSetCombinations, fscs);
  asn1cSequenceAdd(fscs->list, NR_FeatureSetCombination_t, fsc);
  asn1cSequenceAdd(fsc->list, NR_FeatureSetsPerBand_t, fspb);
  asn1cSequenceAdd(fspb->list, NR_FeatureSet_t, fset);
  fset->present = NR_FeatureSet_PR_nr;
  asn1cCalloc(fset->choice.nr, fset_nr);
  fset_nr->downlinkSetNR = 1;
  fset_nr->uplinkSetNR = 1;

  asn1cCalloc(cap->featureSets, fs);
  asn1cCalloc(fs->featureSetsDownlink, dls);
  asn1cSequenceAdd(dls->list, NR_FeatureSetDownlink_t, dl);
  asn1cSequenceAdd(dl->featureSetListPerDownlinkCC.list, NR_FeatureSetDownlinkPerCC_Id_t, dl_cc_id);
  *dl_cc_id = 1;
  asn1cCalloc(fs->featureSetsDownlinkPerCC, dlccs);
  asn1cSequenceAdd(dlccs->list, NR_FeatureSetDownlinkPerCC_t, dl_cc);
  (void)dl_cc; /* SCS/BW set by update_feature_sets() */
  asn1cCalloc(fs->featureSetsUplink, uls);
  asn1cSequenceAdd(uls->list, NR_FeatureSetUplink_t, ul);
  asn1cSequenceAdd(ul->featureSetListPerUplinkCC.list, NR_FeatureSetUplinkPerCC_Id_t, ul_cc_id);
  *ul_cc_id = 1;
  asn1cCalloc(fs->featureSetsUplinkPerCC, ulccs);
  asn1cSequenceAdd(ulccs->list, NR_FeatureSetUplinkPerCC_t, ul_cc);
  (void)ul_cc;
  return cap;
}

/* Index = enum value of NR-SupportedBandwidth (fr1 / fr2) */
static const int fr1_bw_mhz[] = {5, 10, 15, 20, 25, 30, 40, 50, 60, 80, 100};
static const int fr2_bw_mhz[] = {50, 100, 200, 400};

static void set_supported_bandwidth(NR_SupportedBandwidth_t *sb, bool is_fr2, int bw_mhz)
{
  const int *t = is_fr2 ? fr2_bw_mhz : fr1_bw_mhz;
  const int n = is_fr2 ? (int)(sizeof(fr2_bw_mhz) / sizeof(int)) : (int)(sizeof(fr1_bw_mhz) / sizeof(int));
  int idx = 0;
  while (idx < n && t[idx] < bw_mhz)
    idx++;
  AssertFatal(idx < n, "%d MHz is larger than any SupportedBandwidth value\n", bw_mhz);
  sb->present = is_fr2 ? NR_SupportedBandwidth_PR_fr2 : NR_SupportedBandwidth_PR_fr1;
  if (is_fr2)
    sb->choice.fr2 = idx;
  else
    sb->choice.fr1 = idx;
}

/*
 * Set SCS and (maximum) bandwidth in the per-CC feature sets reachable from the band combinations
 * (band combination -> featureSetCombination -> FeatureSetsPerBand[0] -> FeatureSet(nr)
 * -> featureSetsDownlink/Uplink -> featureSets{Downlink,Uplink}PerCC).
 * Everything else in those entries (MIMO layers, modulation, channelBW-90mhz, ...) is not modified
 */
static void update_feature_sets(NR_UE_NR_Capability_t *cap, int mu, bool is_fr2, int dl_mhz, int ul_mhz)
{
  NR_FeatureSets_t *fs = cap->featureSets;
  if (!cap->rf_Parameters.supportedBandCombinationList || !cap->featureSetCombinations || !fs)
    return;

  for (int i = 0; i < cap->rf_Parameters.supportedBandCombinationList->list.count; i++) {
    long comb = cap->rf_Parameters.supportedBandCombinationList->list.array[i]->featureSetCombination;
    AssertFatal(comb >= 0 && comb < cap->featureSetCombinations->list.count,
                "UE capabilities: band combination %d refers to missing featureSetCombination %ld\n",
                i,
                comb);
    NR_FeatureSetCombination_t *fsc = cap->featureSetCombinations->list.array[comb];
    if (fsc->list.count < 1)
      continue;
    NR_FeatureSetsPerBand_t *per_band = fsc->list.array[0]; /* single band: first band of the combination */
    for (int j = 0; j < per_band->list.count; j++) {
      NR_FeatureSet_t *set = per_band->list.array[j];
      if (set->present != NR_FeatureSet_PR_nr)
        continue;
      long dl = set->choice.nr->downlinkSetNR; /* 0 = none, otherwise 1-based index */
      if (dl > 0) {
        AssertFatal(fs->featureSetsDownlink && dl <= fs->featureSetsDownlink->list.count,
                    "UE capabilities: missing downlink feature set %ld\n",
                    dl);
        NR_FeatureSetDownlink_t *d = fs->featureSetsDownlink->list.array[dl - 1];
        for (int k = 0; k < d->featureSetListPerDownlinkCC.list.count; k++) {
          long id = *d->featureSetListPerDownlinkCC.list.array[k];
          AssertFatal(fs->featureSetsDownlinkPerCC && id >= 1 && id <= fs->featureSetsDownlinkPerCC->list.count,
                      "UE capabilities: missing DL per-CC feature set %ld\n",
                      id);
          NR_FeatureSetDownlinkPerCC_t *cc = fs->featureSetsDownlinkPerCC->list.array[id - 1];
          cc->supportedSubcarrierSpacingDL = mu;
          set_supported_bandwidth(&cc->supportedBandwidthDL, is_fr2, dl_mhz);
        }
      }
      long ul = set->choice.nr->uplinkSetNR;
      if (ul > 0) {
        AssertFatal(fs->featureSetsUplink && ul <= fs->featureSetsUplink->list.count,
                    "UE capabilities: missing uplink feature set %ld\n",
                    ul);
        NR_FeatureSetUplink_t *u = fs->featureSetsUplink->list.array[ul - 1];
        for (int k = 0; k < u->featureSetListPerUplinkCC.list.count; k++) {
          long id = *u->featureSetListPerUplinkCC.list.array[k];
          AssertFatal(fs->featureSetsUplinkPerCC && id >= 1 && id <= fs->featureSetsUplinkPerCC->list.count,
                      "UE capabilities: missing UL per-CC feature set %ld\n",
                      id);
          NR_FeatureSetUplinkPerCC_t *cc = fs->featureSetsUplinkPerCC->list.array[id - 1];
          cc->supportedSubcarrierSpacingUL = mu;
          set_supported_bandwidth(&cc->supportedBandwidthUL, is_fr2, ul_mhz);
        }
      }
    }
  }
}

/* appliedFreqBandListFilter := received frequencyBandListFilter (absent if none was received) */
static void set_applied_filter(NR_RF_Parameters_t *rf, const NR_FreqBandList_t *filter)
{
  if (rf->appliedFreqBandListFilter) {
    ASN_STRUCT_FREE(asn_DEF_NR_FreqBandList, rf->appliedFreqBandListFilter);
    rf->appliedFreqBandListFilter = NULL;
  }
  if (filter) {
    int ret = asn_copy(&asn_DEF_NR_FreqBandList, (void **)&rf->appliedFreqBandListFilter, filter);
    AssertFatal(ret == 0, "could not copy frequencyBandListFilter\n");
  }
}

/*
 * receivedFilters := received ue-CapabilityEnquiryExt (absent if none was received).
 * The field lives in UE-NR-Capability-v1560, i.e. 4 nonCriticalExtension levels down
 * (v1530 -> v1540 -> v1550 -> v1560)
 */
static void set_received_filters(NR_UE_NR_Capability_t *cap, const OCTET_STRING_t *ue_CapEnquiryExt)
{
  if (!ue_CapEnquiryExt) {
    NR_UE_NR_Capability_v1530_t *v1530 = cap->nonCriticalExtension;
    NR_UE_NR_Capability_v1540_t *v1540 = v1530 ? v1530->nonCriticalExtension : NULL;
    NR_UE_NR_Capability_v1550_t *v1550 = v1540 ? v1540->nonCriticalExtension : NULL;
    NR_UE_NR_Capability_v1560_t *v1560 = v1550 ? v1550->nonCriticalExtension : NULL;
    if (v1560 && v1560->receivedFilters) {
      ASN_STRUCT_FREE(asn_DEF_OCTET_STRING, v1560->receivedFilters);
      v1560->receivedFilters = NULL;
    }
    return;
  }
  if (!cap->nonCriticalExtension)
    cap->nonCriticalExtension = CALLOC(1, sizeof(*cap->nonCriticalExtension));
  NR_UE_NR_Capability_v1530_t *v1530 = cap->nonCriticalExtension;
  if (!v1530->nonCriticalExtension)
    v1530->nonCriticalExtension = CALLOC(1, sizeof(*v1530->nonCriticalExtension));
  NR_UE_NR_Capability_v1540_t *v1540 = v1530->nonCriticalExtension;
  if (!v1540->nonCriticalExtension)
    v1540->nonCriticalExtension = CALLOC(1, sizeof(*v1540->nonCriticalExtension));
  NR_UE_NR_Capability_v1550_t *v1550 = v1540->nonCriticalExtension;
  if (!v1550->nonCriticalExtension)
    v1550->nonCriticalExtension = CALLOC(1, sizeof(*v1550->nonCriticalExtension));
  NR_UE_NR_Capability_v1560_t *v1560 = v1550->nonCriticalExtension;
  if (!v1560->receivedFilters)
    v1560->receivedFilters = CALLOC(1, sizeof(*v1560->receivedFilters));
  OCTET_STRING_fromBuf(v1560->receivedFilters, (const char *)ue_CapEnquiryExt->buf, ue_CapEnquiryExt->size);
}

/*
 * Make rrc->UECap.UE_NR_Capability consistent with the UE configuration and the received enquiry filters.
 *  - no container (no capability file): a minimal one is created.
 *  - container loaded from file: edited in place, only the band / SCS / bandwidth dependent fields.
 * filter: frequencyBandListFilter of the nr request (may be NULL), ue_CapEnquiryExt: ue-CapabilityEnquiryExt (may be NULL).
 */
void nr_rrc_ue_update_capability(NR_UE_RRC_INST_t *rrc, const NR_FreqBandList_t *filter, const OCTET_STRING_t *ue_CapEnquiryExt)
{
  const bool allowed = band_passes_filter(filter, rrc->nr_band);
  if (!allowed)
    LOG_W(NR_RRC, "band n%d not in the received frequencyBandListFilter: no band combinations will be generated\n", rrc->nr_band);

  /* a band / SCS combination that is not in the table is not supported */
  const bool is_fr2 = rrc->nr_band >= 257;
  const int dl_mhz = max_channel_bw_mhz(rrc->nr_band, is_fr2, rrc->scs, true);
  const int ul_mhz = max_channel_bw_mhz(rrc->nr_band, is_fr2, rrc->scs, false);
  AssertFatal(dl_mhz > 0 && ul_mhz > 0, "band n%d with SCS %d is not in the channel bandwidth table\n", rrc->nr_band, rrc->scs);

  /*
   * Known limitation: if the band is not in the frequencyBandListFilter, 5.6.1.4 of 331 gives an empty candidate
   * band combination list, so supportedBandCombinationList, featureSetCombinations and featureSets should be
   * omitted. We only do this for a generated capability. A capability loaded from file keeps its band
   * combinations, feature set combinations and feature sets. The scenario should be highly unlikely.
   */

  if (!rrc->UECap.UE_NR_Capability)
    rrc->UECap.UE_NR_Capability = create_minimal_capability(rrc->nr_band, allowed);

  NR_UE_NR_Capability_t *cap = rrc->UECap.UE_NR_Capability;
  NR_RF_Parameters_t *rf = &cap->rf_Parameters;
  /* supportedBandListNR: exactly one entry. Keep the first one (with its per-band parameters), drop the others */
  if (rf->supportedBandListNR.list.count == 0)
    asn1cSeqAdd(&rf->supportedBandListNR.list, CALLOC(1, sizeof(NR_BandNR_t)));
  for (int i = 1; i < rf->supportedBandListNR.list.count; i++)
    ASN_STRUCT_FREE(asn_DEF_NR_BandNR, rf->supportedBandListNR.list.array[i]);
  rf->supportedBandListNR.list.count = 1;
  rf->supportedBandListNR.list.array[0]->bandNR = rrc->nr_band;
  /* band combinations: the (single) band of each combination */
  if (rf->supportedBandCombinationList) {
    for (int i = 0; i < rf->supportedBandCombinationList->list.count; i++) {
      NR_BandCombination_t *bc = rf->supportedBandCombinationList->list.array[i];
      if (bc->bandList.list.count > 0 && bc->bandList.list.array[0]->present == NR_BandParameters_PR_nr)
        bc->bandList.list.array[0]->choice.nr->bandNR = rrc->nr_band;
    }
  }
  update_feature_sets(cap, rrc->scs, is_fr2, dl_mhz, ul_mhz);
  set_applied_filter(rf, filter);
  set_received_filters(cap, ue_CapEnquiryExt);
}
