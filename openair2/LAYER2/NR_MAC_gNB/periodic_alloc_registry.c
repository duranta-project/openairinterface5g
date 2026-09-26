/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Registry of UE-specific periodic frequency allocations. See header.
 */

#include "periodic_alloc_registry.h"

#include <stdbool.h>
#include <stdlib.h>

#include "NR_MAC_COMMON/nr_mac.h"
#include "common/utils/LOG/log.h"
#include "assertions.h"

/* Dynamic array of entries. Protected by the caller's MAC sched_lock. */
static periodic_alloc_t *g_entries = NULL;
static size_t g_count = 0;
static size_t g_cap = 0;

static void add_entry(const periodic_alloc_t *e)
{
  if (g_count == g_cap) {
    size_t ncap = g_cap ? g_cap * 2 : 32;
    periodic_alloc_t *n = realloc(g_entries, ncap * sizeof(*n));
    AssertFatal(n != NULL, "periodic_alloc registry realloc (%zu entries) failed\n", ncap);
    g_entries = n;
    g_cap = ncap;
  }
  g_entries[g_count++] = *e;
}

void periodic_alloc_unregister_ue(uid_t uid)
{
  size_t i = 0;
  while (i < g_count) {
    if (g_entries[i].uid == uid)
      g_entries[i] = g_entries[--g_count]; // swap-remove (order irrelevant)
    else
      i++;
  }
}

/* --- per-type extraction from the UE's current active BWP config ---------- */

static void register_pucch(const NR_UE_info_t *UE, const NR_UE_UL_BWP_t *ul)
{
  const NR_PUCCH_Config_t *pucch = ul->pucch_Config;
  if (!pucch || !pucch->resourceToAddModList)
    return;

  for (int i = 0; i < pucch->resourceToAddModList->list.count; i++) {
    const NR_PUCCH_Resource_t *res = pucch->resourceToAddModList->list.array[i];
    if (!res)
      continue;

    int nrof_prb = 1; // formats 0/1/4 occupy a single PRB
    switch (res->format.present) {
      case NR_PUCCH_Resource__format_PR_format2:
        nrof_prb = res->format.choice.format2->nrofPRBs;
        break;
      case NR_PUCCH_Resource__format_PR_format3:
        nrof_prb = res->format.choice.format3->nrofPRBs;
        break;
      default:
        break;
    }

    periodic_alloc_t e = {
        .uid = UE->uid,
        .rnti = UE->rnti,
        .type = PA_PUCCH,
        .dir = PA_DIR_UL,
        .resource_id = (int)res->pucch_ResourceId,
        .rb_start = (uint16_t)(ul->BWPStart + res->startingPRB),
        .rb_count = (uint16_t)nrof_prb,
    };
    add_entry(&e);

    if (res->secondHopPRB) {
      e.rb_start = (uint16_t)(ul->BWPStart + *res->secondHopPRB);
      add_entry(&e);
    }
  }
}

static void register_srs(const NR_UE_info_t *UE, const NR_UE_UL_BWP_t *ul)
{
  const NR_SRS_Config_t *srs = ul->srs_Config;
  if (!srs || !srs->srs_ResourceSetToAddModList)
    return;

  /* Only periodic SRS is deterministic / known in advance (and the only kind
   * OAI schedules, see nr_schedule_srs). Aperiodic SRS is DCI-triggered. */
  int res_id = -1;
  bool has_periodic = false;
  for (int i = 0; i < srs->srs_ResourceSetToAddModList->list.count; i++) {
    const NR_SRS_ResourceSet_t *set = srs->srs_ResourceSetToAddModList->list.array[i];
    if (!set || set->resourceType.present != NR_SRS_ResourceSet__resourceType_PR_periodic)
      continue;
    has_periodic = true;
    if (set->srs_ResourceIdList && set->srs_ResourceIdList->list.count > 0)
      res_id = (int)*set->srs_ResourceIdList->list.array[0];
    break;
  }
  if (!has_periodic)
    return;

  /* OAI reserves the whole UL BWP for a scheduled SRS (see nr_fill_nfapi_srs).
   * TODO: for precise mitigation, derive the actual SRS bandwidth (m_SRS). */
  periodic_alloc_t e = {
      .uid = UE->uid,
      .rnti = UE->rnti,
      .type = PA_SRS,
      .dir = PA_DIR_UL,
      .resource_id = res_id,
      .rb_start = (uint16_t)ul->BWPStart,
      .rb_count = (uint16_t)ul->BWPSize,
  };
  add_entry(&e);
}

static void register_csirs(const NR_UE_info_t *UE, const NR_UE_DL_BWP_t *dl)
{
  const NR_CSI_MeasConfig_t *csi = UE->sc_info.csi_MeasConfig;
  if (!csi || !csi->nzp_CSI_RS_ResourceToAddModList)
    return;

  for (int i = 0; i < csi->nzp_CSI_RS_ResourceToAddModList->list.count; i++) {
    const NR_NZP_CSI_RS_Resource_t *nzp = csi->nzp_CSI_RS_ResourceToAddModList->list.array[i];
    if (!nzp)
      continue;
    /* Clip the configured freqBand to the BWP, as in nr_csirs_scheduling. */
    int start = nzp->resourceMapping.freqBand.startingRB;
    if (start < dl->BWPStart)
      start = dl->BWPStart;
    int nrofrbs = nzp->resourceMapping.freqBand.nrofRBs;
    if (nrofrbs > dl->BWPStart + dl->BWPSize - start)
      nrofrbs = dl->BWPStart + dl->BWPSize - start;
    if (nrofrbs <= 0)
      continue;

    periodic_alloc_t e = {
        .uid = UE->uid,
        .rnti = UE->rnti,
        .type = PA_CSIRS,
        .dir = PA_DIR_DL,
        .resource_id = (int)nzp->nzp_CSI_RS_ResourceId,
        .rb_start = (uint16_t)start,
        .rb_count = (uint16_t)nrofrbs,
    };
    add_entry(&e);
  }
}

void periodic_alloc_refresh_ue(NR_UE_info_t *UE)
{
  if (!UE)
    return;

  periodic_alloc_unregister_ue(UE->uid);

  register_pucch(UE, &UE->current_UL_BWP);
  register_srs(UE, &UE->current_UL_BWP);
  register_csirs(UE, &UE->current_DL_BWP);
}

size_t periodic_alloc_count(void)
{
  return g_count;
}

const periodic_alloc_t *periodic_alloc_at(size_t index)
{
  return index < g_count ? &g_entries[index] : NULL;
}
