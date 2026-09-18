/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gnb_config_xn.h"
#include <string.h>
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "common/utils/utils.h"
#include "gnb_paramdef.h"
#include "openair3/SCTP/sctp_default_values.h"

xnap_net_config_t read_ip_config_xn(uint32_t gnb_idx)
{
  xnap_net_config_t nc = {0};
  paramdef_t XnCandidateParams[] = XN_CANDIDATE_PARAMS_DESC;
  paramlist_def_t XnCandidateList = {GNB_CONFIG_STRING_CANDIDATE_GNB_IPV4_ADDRESS_FOR_XNC, NULL, 0};
  paramdef_t SCTPParams[] = GNBSCTPPARAMS_DESC;
  char aprefix[MAX_OPTNAME_SIZE * 2 + 8];
  sprintf(aprefix, "%s.[%i].%s", GNB_CONFIG_STRING_GNB_LIST, gnb_idx, GNB_CONFIG_STRING_XN_PARAMETERS);
  config_getlist(config_get_if(), &XnCandidateList, XnCandidateParams, sizeofArray(XnCandidateParams), aprefix);
  AssertFatal(XnCandidateList.numelt <= XNAP_MAX_NB_CANDIDATES,
              "Xn candidates limit exceeded (%d > %d)\n", XnCandidateList.numelt, XNAP_MAX_NB_CANDIDATES);

  LOG_I(XNAP, "Number of candidate gNBs configured: %d\n", XnCandidateList.numelt);
  for (int l = 0; l < XnCandidateList.numelt; l++) {
    nc.nb_of_candidate_gNBs++;
    nc.candidate_gnb_address_for_xnc[l] =
        strdup(*gpd(XnCandidateList.paramarray[l], sizeofArray(XnCandidateParams), GNB_CONFIG_STRING_CANDIDATE_GNB_ADDRESS_FOR_XNC)->strptr);
    LOG_I(XNAP, "Candidate gNB %d address: %s \n", l + 1, nc.candidate_gnb_address_for_xnc[l]);
  }

  paramdef_t XnParams[] = XNPARAMS_DESC;
  const int nb_xn_params = sizeofArray(XnParams);
  config_get(config_get_if(), XnParams, nb_xn_params, aprefix);
  char **gnb_xn_ip_strptr = gpd(XnParams, nb_xn_params, GNB_CONFIG_STRING_GNB_IPV4_ADDRESS_FOR_XNC)->strptr;
  AssertFatal(gnb_xn_ip_strptr != NULL, "gNB IP not added in the CU/gNB configuration file\n");
  nc.gnb_xn_interface_ip_address = strdup(*gnb_xn_ip_strptr);

  nc.sctp_streams.sctp_out_streams = SCTP_OUT_STREAMS;
  nc.sctp_streams.sctp_in_streams = SCTP_IN_STREAMS;
  sprintf(aprefix, "%s.[%i].%s", GNB_CONFIG_STRING_GNB_LIST, 0, GNB_CONFIG_STRING_SCTP_CONFIG);
  config_get(config_get_if(), SCTPParams, sizeofArray(SCTPParams), aprefix);
  nc.sctp_streams.sctp_out_streams = *(SCTPParams[GNB_SCTP_OUTSTREAMS_IDX].uptr);
  nc.sctp_streams.sctp_in_streams = *(SCTPParams[GNB_SCTP_INSTREAMS_IDX].uptr);

  return nc;
}

int is_xnap_enabled(void)
{
  char aprefix[MAX_OPTNAME_SIZE * 2 + 8];
  snprintf(aprefix, sizeof(aprefix), "%s.[%i].%s", GNB_CONFIG_STRING_GNB_LIST, 0, GNB_CONFIG_STRING_XN_PARAMETERS);
  paramdef_t XnParams[] = XNPARAMS_DESC;
  const int nb_xn_params = sizeofArray(XnParams);
  config_get(config_get_if(), XnParams, nb_xn_params, aprefix);
  char **gnb_xn_ip_strptr = gpd(XnParams, nb_xn_params, GNB_CONFIG_STRING_GNB_IPV4_ADDRESS_FOR_XNC)->strptr;

  int xn_enabled = (gnb_xn_ip_strptr != NULL) && (*gnb_xn_ip_strptr != NULL);
  LOG_I(XNAP, "Xn interface %s (gNB Xn-C address %s)\n",
        xn_enabled ? "enabled" : "disabled", xn_enabled ? *gnb_xn_ip_strptr : "not configured");

  return xn_enabled;
}

xnap_setup_req_t read_ng_setup_info(const ngap_register_gnb_cnf_t *cnf, uint32_t gnb_idx)
{
  LOG_I(XNAP, "[gNB %u] Reading info required for Xn setup from NGAP_REGISTER_GNB_CNF\n", gnb_idx);
  xnap_setup_req_t setup_info = {0};

  setup_info.gNB_id = cnf->gNB_id;
  if (cnf->num_plmn > 0)
    setup_info.plmn = cnf->plmn[0].plmn;

  /* NGAP carries single TAC, so only one TAI support, it will become a list once NGAP support multiple TACs */
  setup_info.num_tai = 1;
  setup_info.tai_support = calloc_or_fail(setup_info.num_tai, sizeof(*setup_info.tai_support));

  for (int i = 0; i < setup_info.num_tai; i++) {
    xnap_tai_support_t *tai = &setup_info.tai_support[i];
    tai->tac = cnf->tac;
    tai->num_plmn = cnf->num_plmn;
    tai->plmn_support = calloc_or_fail(tai->num_plmn, sizeof(*tai->plmn_support));

    for (int j = 0; j < tai->num_plmn; j++) {
      const ngap_plmn_t *src_plmn = &cnf->plmn[j];
      xnap_plmn_support_t *plmn_support = &tai->plmn_support[j];

      plmn_support->plmn = src_plmn->plmn;
      plmn_support->num_nssai = src_plmn->num_nssai;

      if (plmn_support->num_nssai > 0) {
      plmn_support->nssai = calloc_or_fail(plmn_support->num_nssai, sizeof(*plmn_support->nssai));
      memcpy(plmn_support->nssai, src_plmn->s_nssai, plmn_support->num_nssai * sizeof(*plmn_support->nssai));
      }
    }
  }

  setup_info.num_amf_regions = cnf->num_amf_regions;
  if (setup_info.num_amf_regions > 0) {
    setup_info.amf_region_info = calloc_or_fail(cnf->num_amf_regions, sizeof(*setup_info.amf_region_info));
    memcpy(setup_info.amf_region_info, cnf->amf_region_info, cnf->num_amf_regions * sizeof(*setup_info.amf_region_info));
  }

  return setup_info;
}
