/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "ran_func_slice.h"
#include "../../flexric/test/rnd/fill_rnd_data_slice.h"
#include "oai_ns_slice_ie.h"
#include "NR_MAC_gNB/nr_mac_gNB.h"
#include "NR_MAC_gNB/gNB_scheduler_types.h"
#include "common/utils/LOG/log.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int mod_id = 0;

static bool is_default_slice(uint32_t sd)
{
  return sd == NS_SLICE_DEFAULT_SD;
}

static void append_ns_policy_from_scheduler(ns_slice_policy_list_t *list,
                                            slice_scheduler_t *sched,
                                            ns_slice_dir_e direction)
{
  if (sched == NULL)
    return;

  const int n = slice_sch_get_num_slices(sched);
  if (n <= 0)
    return;

  for (int i = 0; i < n; ++i) {
    const slice_nssai_t *nssai = slice_sch_get_slice_nssai(sched, i);
    if (nssai == NULL)
      continue;

    float ded = 0.f;
    float min = 0.f;
    float max = 0.f;
    if (slice_sch_get_slice_config(sched, nssai->sst, nssai->sd, NULL, NULL, &ded, &min, &max) != 0)
      continue;

    const uint32_t new_len = list->len_entries + 1;
    ns_slice_policy_entry_t *grown =
        realloc(list->entries, new_len * sizeof(ns_slice_policy_entry_t));
    assert(grown != NULL && "Memory exhausted");
    list->entries = grown;

    ns_slice_policy_entry_t *e = &list->entries[list->len_entries];
    e->sst = nssai->sst;
    e->sd = nssai->sd;
    e->direction = direction;
    e->dedicated_pct = ded * 100.f;
    e->min_pct = min * 100.f;
    e->max_pct = max * 100.f;
    list->len_entries = new_len;
  }
}

static void read_ns_policy_from_mac(ns_slice_policy_list_t *out, gNB_MAC_INST *mac)
{
  assert(out != NULL);
  assert(mac != NULL);

  out->len_entries = 0;
  out->entries = NULL;

  NR_SCHED_LOCK(&mac->sched_lock);

  if (mac->slice_scheduler_dl != NULL && mac->scheduler_type_dl == SCHE_NS)
    append_ns_policy_from_scheduler(out, mac->slice_scheduler_dl, NS_SLICE_DIR_DL);

  if (mac->slice_scheduler_ul != NULL && mac->scheduler_type_ul == SCHE_NS)
    append_ns_policy_from_scheduler(out, mac->slice_scheduler_ul, NS_SLICE_DIR_UL);

  NR_SCHED_UNLOCK(&mac->sched_lock);
}

static bool validate_ns_entry(const ns_slice_policy_entry_t *e, char *err, size_t err_len)
{
  const char *dir = e->direction == NS_SLICE_DIR_UL ? "ul" : e->direction == NS_SLICE_DIR_DL ? "dl" : "?";

  if (e->direction != NS_SLICE_DIR_DL && e->direction != NS_SLICE_DIR_UL) {
    snprintf(err, err_len, "SST=%u SD=0x%06x: invalid direction", e->sst, e->sd);
    return false;
  }
  if (is_default_slice(e->sd)) {
    snprintf(err, err_len, "SST=%u SD=0x%06x %s: cannot control default slice (sd=0xffffff)", e->sst, e->sd, dir);
    return false;
  }
  if (e->dedicated_pct < 0.f || e->dedicated_pct > 100.f || e->min_pct < 0.f || e->min_pct > 100.f || e->max_pct < 0.f
      || e->max_pct > 100.f) {
    snprintf(err, err_len, "SST=%u SD=0x%06x %s: ratios must be in [0, 100]%%", e->sst, e->sd, dir);
    return false;
  }
  if (e->dedicated_pct > e->min_pct) {
    snprintf(err,
             err_len,
             "SST=%u SD=0x%06x %s: dedicated %.1f%% > min %.1f%% (need dedicated <= min <= max)",
             e->sst,
             e->sd,
             dir,
             e->dedicated_pct,
             e->min_pct);
    return false;
  }
  if (e->min_pct > e->max_pct) {
    snprintf(err,
             err_len,
             "SST=%u SD=0x%06x %s: min %.1f%% > max %.1f%% (need dedicated <= min <= max)",
             e->sst,
             e->sd,
             dir,
             e->min_pct,
             e->max_pct);
    return false;
  }
  return true;
}

static double ratio_sum_scheduler(const slice_scheduler_t *sched, bool min_not_dedicated)
{
  double sum = 0.0;
  const int n = slice_sch_get_num_slices(sched);
  for (int i = 0; i < n; ++i) {
    const slice_nssai_t *nssai = slice_sch_get_slice_nssai(sched, i);
    if (nssai == NULL)
      continue;
    float ded = 0.f;
    float min = 0.f;
    if (slice_sch_get_slice_config(sched, nssai->sst, nssai->sd, NULL, NULL, &ded, &min, NULL) == 0)
      sum += min_not_dedicated ? min : ded;
  }
  return sum;
}

static double dedicated_sum_scheduler(const slice_scheduler_t *sched)
{
  return ratio_sum_scheduler(sched, false);
}

static sm_ag_if_ans_t make_slice_ctrl_out(slice_ctrl_out_e rc, const char *diag)
{
  sm_ag_if_ans_t ans = {.type = CTRL_OUTCOME_SM_AG_IF_ANS_V0};
  ans.ctrl_out.type = SLICE_AGENT_IF_CTRL_ANS_V0;
  ans.ctrl_out.slice.ans = rc;
  if (diag != NULL && diag[0] != '\0') {
    ans.ctrl_out.slice.len_diag = strlen(diag);
    ans.ctrl_out.slice.diagnostic = malloc(ans.ctrl_out.slice.len_diag + 1);
    assert(ans.ctrl_out.slice.diagnostic != NULL && "Memory exhausted");
    memcpy(ans.ctrl_out.slice.diagnostic, diag, ans.ctrl_out.slice.len_diag + 1);
  }
  return ans;
}

bool read_slice_sm(void *data)
{
  assert(data != NULL);

  slice_ind_data_t *slice = (slice_ind_data_t *)data;
  oai_slice_ind_msg_layout_t *ind_msg = oai_slice_ind_msg(&slice->msg);
  fill_slice_ind_data(slice);
  ind_msg->tstamp = time_now_us();

  gNB_MAC_INST *mac = RC.nrmac[mod_id];
  read_ns_policy_from_mac(&ind_msg->ns_policy, mac);

  return true;
}

void read_slice_setup_sm(void *data)
{
  assert(data != NULL);
  assert(0 != 0 && "Not supported");
}

sm_ag_if_ans_t write_ctrl_slice_sm(void const *data)
{
  assert(data != NULL);

  const slice_ctrl_req_data_t *slice_req_ctrl = (const slice_ctrl_req_data_t *)data;
  const slice_ctrl_msg_t *msg = &slice_req_ctrl->msg;

  if (msg->type == SLICE_CTRL_SM_V0_NS_SET_POLICY) {
    gNB_MAC_INST *mac = RC.nrmac[mod_id];
    const ns_slice_policy_list_t *pol = oai_slice_ctrl_ns_policy_const(msg);
    char err[256] = {0};

    for (uint32_t i = 0; i < pol->len_entries; ++i) {
      if (!validate_ns_entry(&pol->entries[i], err, sizeof(err)))
        return make_slice_ctrl_out(SLICE_CTRL_OUT_ERROR, err);
    }

    NR_SCHED_LOCK(&mac->sched_lock);

    for (uint32_t i = 0; i < pol->len_entries; ++i) {
      const ns_slice_policy_entry_t *e = &pol->entries[i];
      slice_scheduler_t *sched = NULL;

      if (e->direction == NS_SLICE_DIR_DL) {
        if (mac->scheduler_type_dl != SCHE_NS || mac->slice_scheduler_dl == NULL) {
          NR_SCHED_UNLOCK(&mac->sched_lock);
          return make_slice_ctrl_out(SLICE_CTRL_OUT_ERROR, "DL NS scheduler not enabled");
        }
        sched = mac->slice_scheduler_dl;
      } else {
        if (mac->scheduler_type_ul != SCHE_NS || mac->slice_scheduler_ul == NULL) {
          NR_SCHED_UNLOCK(&mac->sched_lock);
          return make_slice_ctrl_out(SLICE_CTRL_OUT_ERROR, "UL NS scheduler not enabled");
        }
        sched = mac->slice_scheduler_ul;
      }

      const float ded = e->dedicated_pct / 100.f;
      const float min = e->min_pct / 100.f;
      const float max = e->max_pct / 100.f;
      if (slice_sch_add_slice(sched, e->sst, e->sd, ded, min, max, 0) != 0) {
        NR_SCHED_UNLOCK(&mac->sched_lock);
        snprintf(err,
                 sizeof(err),
                 "SST=%u SD=0x%06x %s: slice_sch_add_slice failed",
                 e->sst,
                 e->sd,
                 e->direction == NS_SLICE_DIR_UL ? "ul" : "dl");
        return make_slice_ctrl_out(SLICE_CTRL_OUT_ERROR, err);
      }
      sched->result_valid = false;
      LOG_I(NR_MAC,
            "NS E2 SET applied: SST=%u SD=0x%06x %s dedicated=%.1f%% min=%.1f%% max=%.1f%%\n",
            e->sst,
            e->sd,
            e->direction == NS_SLICE_DIR_UL ? "ul" : "dl",
            e->dedicated_pct,
            e->min_pct,
            e->max_pct);
    }

    if (mac->slice_scheduler_dl != NULL && mac->scheduler_type_dl == SCHE_NS) {
      if (dedicated_sum_scheduler(mac->slice_scheduler_dl) > 1.0 + 1e-6) {
        NR_SCHED_UNLOCK(&mac->sched_lock);
        return make_slice_ctrl_out(SLICE_CTRL_OUT_ERROR, "sum of DL dedicated ratios exceeds 100%");
      }
      if (ratio_sum_scheduler(mac->slice_scheduler_dl, true) > 1.0 + 1e-6) {
        NR_SCHED_UNLOCK(&mac->sched_lock);
        return make_slice_ctrl_out(SLICE_CTRL_OUT_ERROR, "sum of DL min ratios exceeds 100%");
      }
    }
    if (mac->slice_scheduler_ul != NULL && mac->scheduler_type_ul == SCHE_NS) {
      if (dedicated_sum_scheduler(mac->slice_scheduler_ul) > 1.0 + 1e-6) {
        NR_SCHED_UNLOCK(&mac->sched_lock);
        return make_slice_ctrl_out(SLICE_CTRL_OUT_ERROR, "sum of UL dedicated ratios exceeds 100%");
      }
      if (ratio_sum_scheduler(mac->slice_scheduler_ul, true) > 1.0 + 1e-6) {
        NR_SCHED_UNLOCK(&mac->sched_lock);
        return make_slice_ctrl_out(SLICE_CTRL_OUT_ERROR, "sum of UL min ratios exceeds 100%");
      }
    }

    NR_SCHED_UNLOCK(&mac->sched_lock);
    return make_slice_ctrl_out(SLICE_CTRL_OUT_OK, NULL);
  }

  if (msg->type == SLICE_CTRL_SM_V0_ADD) {
    printf("[E2 Agent]: SLICE CONTROL ADD rx\n");
  } else if (msg->type == SLICE_CTRL_SM_V0_DEL) {
    printf("[E2 Agent]: SLICE CONTROL DEL rx\n");
  } else if (msg->type == SLICE_CTRL_SM_V0_UE_SLICE_ASSOC) {
    printf("[E2 Agent]: SLICE CONTROL ASSOC rx\n");
  } else {
    assert(0 != 0 && "Unknown msg_type!");
  }

  return make_slice_ctrl_out(SLICE_CTRL_OUT_OK, NULL);
}
