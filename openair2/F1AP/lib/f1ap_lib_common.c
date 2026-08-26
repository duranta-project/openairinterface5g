/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "f1ap_lib_common.h"
#include "f1ap_messages_types.h"

#include "OCTET_STRING.h"
#include "common/utils/utils.h"
#include "common/utils/assertions.h"
#include "common/utils/oai_asn1.h"

bool eq_f1ap_plmn(const plmn_id_t *a, const plmn_id_t *b)
{
  _EQ_CHECK_INT(a->mcc, b->mcc);
  _EQ_CHECK_INT(a->mnc, b->mnc);
  _EQ_CHECK_INT(a->mnc_digit_length, b->mnc_digit_length);
  return true;
}

bool eq_f1ap_freq_info(const f1ap_nr_frequency_info_t *a, const f1ap_nr_frequency_info_t *b)
{
  _EQ_CHECK_INT(a->arfcn, b->arfcn);
  _EQ_CHECK_INT(a->band, b->band);
  return true;
}

bool eq_f1ap_tx_bandwidth(const f1ap_transmission_bandwidth_t *a, const f1ap_transmission_bandwidth_t *b)
{
  _EQ_CHECK_INT(a->nrb, b->nrb);
  _EQ_CHECK_INT(a->scs, b->scs);
  return true;
}

bool eq_f1ap_cell_info(const f1ap_served_cell_info_t *a, const f1ap_served_cell_info_t *b)
{
  _EQ_CHECK_LONG(a->nr_cellid, b->nr_cellid);
  _EQ_CHECK_INT(a->nr_pci, b->nr_pci);
  if ((!a->tac) ^ (!b->tac))
    return false;
  if (a->tac)
    _EQ_CHECK_INT(*a->tac, *b->tac);
  _EQ_CHECK_INT(a->num_plmn, b->num_plmn);
  if (a->num_plmn > F1AP_MAX_NB_PLMNS)
    return false;
  for (int i = 0; i < a->num_plmn; ++i) {
    if (!eq_f1ap_plmn(&a->served_plmn_list[i].plmn, &b->served_plmn_list[i].plmn))
      return false;
    _EQ_CHECK_INT(a->served_plmn_list[i].num_nssai, b->served_plmn_list[i].num_nssai);
    if (a->served_plmn_list[i].num_nssai > MAX_NUM_SLICES)
      return false;
    for (int j = 0; j < a->served_plmn_list[i].num_nssai; ++j) {
      _EQ_CHECK_INT(a->served_plmn_list[i].nssai[j].sst, b->served_plmn_list[i].nssai[j].sst);
      _EQ_CHECK_INT(a->served_plmn_list[i].nssai[j].sd, b->served_plmn_list[i].nssai[j].sd);
    }
  }
  _EQ_CHECK_INT(a->mode, b->mode);
  if (a->mode == F1AP_MODE_TDD) {
    /* TDD */
    if (!eq_f1ap_tx_bandwidth(&a->tdd.tbw, &b->tdd.tbw))
      return false;
    if (!eq_f1ap_freq_info(&a->tdd.freqinfo, &b->tdd.freqinfo))
      return false;
  } else if (a->mode == F1AP_MODE_FDD) {
    /* FDD */
    if (!eq_f1ap_tx_bandwidth(&a->fdd.dl_tbw, &b->fdd.dl_tbw))
      return false;
    if (!eq_f1ap_freq_info(&a->fdd.dl_freqinfo, &b->fdd.dl_freqinfo))
      return false;
    if (!eq_f1ap_tx_bandwidth(&a->fdd.ul_tbw, &b->fdd.ul_tbw))
      return false;
    if (!eq_f1ap_freq_info(&a->fdd.ul_freqinfo, &b->fdd.ul_freqinfo))
      return false;
  }
  _EQ_CHECK_INT(a->measurement_timing_config_len, b->measurement_timing_config_len);
  _EQ_CHECK_INT(*a->measurement_timing_config, *b->measurement_timing_config);
  if (!eq_f1ap_plmn(&a->plmn, &b->plmn))
    return false;
  return true;
}

bool eq_f1ap_sys_info(const f1ap_gnb_du_system_info_t *a, const f1ap_gnb_du_system_info_t *b)
{
  if (!a && !b)
    return true;

  /* will fail if not both a/b NULL or set */
  if ((!a) ^ (!b))
    return false;

  /* MIB */
  _EQ_CHECK_INT(a->mib_length, b->mib_length);
  for (int i = 0; i < a->mib_length; i++)
    _EQ_CHECK_INT(a->mib[i], b->mib[i]);
  /* SIB1 */
  _EQ_CHECK_INT(a->sib1_length, b->sib1_length);
  for (int i = 0; i < a->sib1_length; i++)
    _EQ_CHECK_INT(a->sib1[i], b->sib1[i]);
  return true;
}

uint8_t *cp_octet_string(const OCTET_STRING_t *os, int *len)
{
  uint8_t *buf = calloc_or_fail(os->size, sizeof(*buf));
  memcpy(buf, os->buf, os->size);
  *len = os->size;
  return buf;
}

F1AP_SNSSAI_t encode_nssai(const nssai_t *nssai)
{
  F1AP_SNSSAI_t enc = {0};
  INT8_TO_OCTET_STRING(nssai->sst, &enc.sST);
  // see 23.003 sec. 28.4.2: 0xffffff means "no SD"
  if (nssai->sd != 0xffffff) {
    asn1cCalloc(enc.sD, tmp);
    INT24_TO_OCTET_STRING(nssai->sd, tmp);
  }
  return enc;
}

nssai_t decode_nssai(const F1AP_SNSSAI_t *nssai)
{
  // see 23.003 sec. 28.4.2: 0xffffff means "no SD"
  nssai_t dec = { .sd = 0xffffff, };
  OCTET_STRING_TO_INT8(&nssai->sST, dec.sst);
  if (nssai->sD != NULL)
    OCTET_STRING_TO_INT24(nssai->sD, dec.sd);
  return dec;
}

F1AP_Cause_t encode_f1ap_cause(f1ap_Cause_t cause, long cause_value)
{
  F1AP_Cause_t f1_cause = {0};
  switch (cause) {
    case F1AP_CAUSE_RADIO_NETWORK:
      f1_cause.present = F1AP_Cause_PR_radioNetwork;
      f1_cause.choice.radioNetwork = cause_value;
      break;
    case F1AP_CAUSE_TRANSPORT:
      f1_cause.present = F1AP_Cause_PR_transport;
      f1_cause.choice.transport = cause_value;
      break;
    case F1AP_CAUSE_PROTOCOL:
      f1_cause.present = F1AP_Cause_PR_protocol;
      f1_cause.choice.protocol = cause_value;
      break;
    case F1AP_CAUSE_MISC:
      f1_cause.present = F1AP_Cause_PR_misc;
      f1_cause.choice.misc = cause_value;
      break;
    case F1AP_CAUSE_NOTHING:
    default:
      AssertFatal(false, "unknown cause value %d\n", cause);
      break;
  }
  return f1_cause;
}

bool decode_f1ap_cause(F1AP_Cause_t f1_cause, f1ap_Cause_t *cause, long *cause_value)
{
  switch (f1_cause.present) {
    case F1AP_Cause_PR_radioNetwork:
      *cause = F1AP_CAUSE_RADIO_NETWORK;
      *cause_value = f1_cause.choice.radioNetwork;
      break;
    case F1AP_Cause_PR_transport:
      *cause = F1AP_CAUSE_TRANSPORT;
      *cause_value = f1_cause.choice.transport;
      break;
    case F1AP_Cause_PR_protocol:
      *cause = F1AP_CAUSE_PROTOCOL;
      *cause_value = f1_cause.choice.protocol;
      break;
    case F1AP_Cause_PR_misc:
      *cause = F1AP_CAUSE_MISC;
      *cause_value = f1_cause.choice.radioNetwork;
      break;
    case F1AP_Cause_PR_NOTHING:
    default:
      PRINT_ERROR("received illegal F1AP cause %d\n", f1_cause.present);
      return false;
  }
  return true;
}
