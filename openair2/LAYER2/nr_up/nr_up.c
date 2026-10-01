/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up.h"
#include "nr_up_data_delivery_status.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>

#include "assertions.h"
#include "log.h"
#include "common/openairinterface5g_limits.h"
#include "common/utils/ds/hashtable.h"
#include "common/utils/utils.h"

/* Request polling if DBS has not been refreshed for too long. */
#define NR_UP_REPORT_POLLING_TIMER_MS 20u

#define NR_UP_UE_KEY(ue_id) ((hash_key_t)(ue_id))

typedef struct {
  pthread_mutex_t lock;
  hash_table_t ues;
} nr_up_budget_manager_t;

static nr_up_if_t nr_up_if;
static nr_up_budget_manager_t g_nr_up_manager;

/** @brief Return the nr-up interface function-pointer table */
struct nr_up_if_s *get_nr_up_if(void)
{
  return &nr_up_if;
}

static _Atomic uint64_t nr_up_time_ms;

/** Time-manager callback, once per real or simulated millisecond */
void nr_up_ms_tick(void)
{
  atomic_fetch_add_explicit(&nr_up_time_ms, 1, memory_order_relaxed);
}

/** Elapsed managed milliseconds */
uint64_t nr_up_current_time_ms(void)
{
  return atomic_load_explicit(&nr_up_time_ms, memory_order_relaxed);
}

/** @brief Create the per-UE budget hashtable */
void nr_up_manager_init(void)
{
  if (g_nr_up_manager.ues.nodes != NULL) {
    return;
  }

  if (pthread_mutex_init(&g_nr_up_manager.lock, NULL) != 0) {
    abort();
  }

  g_nr_up_manager.ues = hashtable_create(MAX_MOBILES_PER_GNB, NULL);
}

static void nr_up_manager_lock(void)
{
  DevAssert(g_nr_up_manager.ues.nodes != NULL);
  if (pthread_mutex_lock(&g_nr_up_manager.lock) != 0) {
    abort();
  }
}

static void nr_up_manager_unlock(void)
{
  DevAssert(g_nr_up_manager.ues.nodes != NULL);
  if (pthread_mutex_unlock(&g_nr_up_manager.lock) != 0) {
    abort();
  }
}

/** @brief Look up a UE budget entry */
static nr_up_ue_state_t *nr_up_manager_lookup_ue(ue_id_t ue_id)
{
  void *data = NULL;
  if (hashtable_get(&g_nr_up_manager.ues, NR_UP_UE_KEY(ue_id), &data) == HASH_TABLE_OK) {
    return data;
  }
  return NULL;
}

/** @brief Look up a DRB budget slot under a UE (drb_id is 1..MAX_DRBS_PER_UE) */
static nr_up_desired_buffer_t *nr_up_manager_lookup_drb(ue_id_t ue_id, rb_id_t drb_id)
{
  DevAssert(drb_id >= 1 && drb_id <= MAX_DRBS_PER_UE);
  nr_up_ue_state_t *ue = nr_up_manager_lookup_ue(ue_id);
  if (ue == NULL) {
    return NULL;
  }
  return &ue->drb[drb_id - 1];
}

/** @brief Create a per-UE budget entry (if not already present) */
void nr_up_add_ue(ue_id_t ue_id)
{
  nr_up_manager_init();
  nr_up_manager_lock();
  if (nr_up_manager_lookup_ue(ue_id) == NULL) {
    nr_up_ue_state_t *ue = calloc_or_fail(1, sizeof(*ue));
    for (int i = 0; i < MAX_DRBS_PER_UE; ++i) {
      ue->drb[i].dbs.last_dbs_sync = NR_UP_DBS_NEVER_SYNCED;
    }
    hashtable_rc_t rc = hashtable_insert(&g_nr_up_manager.ues, NR_UP_UE_KEY(ue_id), ue, free);
    DevAssert(rc == HASH_TABLE_OK);
  }
  nr_up_manager_unlock();
}

/** @brief Remove a UE and all DRB budget slots */
void nr_up_release_ue(ue_id_t ue_id)
{
  if (g_nr_up_manager.ues.nodes == NULL) {
    return;
  }
  nr_up_manager_lock();
  hashtable_remove(&g_nr_up_manager.ues, NR_UP_UE_KEY(ue_id), free);
  nr_up_manager_unlock();
}

/** @brief Reset one DRB budget slot after bearer release */
void nr_up_release_drb(ue_id_t ue_id, rb_id_t drb_id)
{
  if (g_nr_up_manager.ues.nodes == NULL) {
    return;
  }
  nr_up_manager_lock();
  nr_up_desired_buffer_t *db = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (db != NULL) {
    *db = (nr_up_desired_buffer_t){.dbs.last_dbs_sync = NR_UP_DBS_NEVER_SYNCED};
  }
  nr_up_manager_unlock();
}

/** @brief Copy cached DBS under the manager lock. */
nr_up_dbs_t nr_up_manager_get_dbs(ue_id_t ue_id, rb_id_t drb_id)
{
  nr_up_manager_lock();
  nr_up_desired_buffer_t *drb_budget = nr_up_manager_lookup_drb(ue_id, drb_id);
  AssertFatal(drb_budget != NULL, "%s(): ue %lx drb %ld not added\n", __func__, ue_id, drb_id);
  nr_up_dbs_t dbs = drb_budget->dbs;
  nr_up_manager_unlock();
  return dbs;
}

/** @brief Copy cached DDS report state under the manager lock. */
nr_up_dds_t nr_up_get_dds(ue_id_t ue_id, rb_id_t drb_id)
{
  nr_up_dds_t dds = {0};
  nr_up_manager_lock();
  nr_up_desired_buffer_t *db = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (db == NULL) {
    nr_up_manager_unlock();
    LOG_W(NR_UP, "%s(): ue %lx drb %ld missing, skip\n", __func__, ue_id, drb_id);
    return dds;
  }
  dds = db->dds_report;
  nr_up_manager_unlock();
  return dds;
}

/** @brief Store DDS report state under the manager lock. */
void nr_up_set_dds(ue_id_t ue_id, rb_id_t drb_id, nr_up_dds_t dds)
{
  nr_up_manager_lock();
  nr_up_desired_buffer_t *db = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (db == NULL) {
    nr_up_manager_unlock();
    LOG_W(NR_UP, "%s(): ue %lx drb %ld missing, skip\n", __func__, ue_id, drb_id);
    return;
  }
  db->dds_report = dds;
  nr_up_manager_unlock();
}

/** @brief Send DDS toward the peer when due. TS 38.425 clause 5.4.2.1.
 * Also runs when len > 0 (UL G-PDU, DDS optional). */
void nr_up_data_delivery_status(ue_id_t ue_id, rb_id_t drb_id, uint32_t space, bool is_poll, uint8_t *buf, size_t len)
{
  nr_up_dds_t dds = nr_up_get_dds(ue_id, drb_id);
  const bool report = nr_up_data_delivery_status_precheck(&dds, space, is_poll);
  if (report)
    nr_up_set_dds(ue_id, drb_id, dds);
  if (!report && len == 0)
    return;

  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface != NULL && iface->data_delivery_status != NULL);
  const nr_up_data_delivery_status_req_t req = {
      .ue_id = ue_id,
      .drb_id = drb_id,
      .desired_buffer_size = space,
      .report_dds = report,
      .pdu = {.buf = buf, .len = len},
  };
  iface->data_delivery_status(&req);
}

/** @brief RLC to PDCP DRB delivery through the active nr-up backend (F1-U or mono/UE). */
void nr_up_dds_user_data(protocol_ctxt_t *ctxt, ue_id_t rlc_ue_id, rb_id_t drb_id, uint8_t *buf, size_t len, int tx_space)
{
  nr_up_if_t *iface = get_nr_up_if();
  DevAssert(iface != NULL && iface->dds_user_data != NULL);
  iface->dds_user_data(ctxt, rlc_ue_id, drb_id, buf, len, tx_space);
}

/** @brief Subtract bytes from the available desired buffer size for a DRB. */
void nr_up_consume_dbs(ue_id_t ue_id, rb_id_t drb_id, size_t bytes)
{
  if (bytes == 0) {
    return;
  }
  nr_up_manager_lock();
  nr_up_desired_buffer_t *db = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (db == NULL) {
    nr_up_manager_unlock();
    LOG_W(NR_UP, "%s(): ue %lx drb %ld missing, skip\n", __func__, ue_id, drb_id);
    return;
  }
  size_t left = db->dbs.available_dbs;
  db->dbs.available_dbs = bytes >= left ? 0 : left - bytes;
  nr_up_manager_unlock();
}

/** @brief Set the cached per-DRB TX desired buffer size.
 * Soft-miss if the entry does not exist (e.g. was released after tunnel delete). */
void nr_up_sync_dbs(ue_id_t ue_id, rb_id_t drb_id, uint32_t dbs)
{
  nr_up_manager_lock();
  nr_up_desired_buffer_t *db = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (db == NULL) {
    nr_up_manager_unlock();
    LOG_W(NR_UP, "%s(): ue %lx drb %ld missing, skip\n", __func__, ue_id, drb_id);
    return;
  }
  db->dbs.available_dbs = dbs;
  db->dbs.desired_buffer_size = dbs;
  db->dbs.last_dbs_sync = nr_up_current_time_ms();
  nr_up_manager_unlock();
}

/** @brief Request Report Polling when DBS is stale and the poll interval elapsed */
bool nr_up_report_polling(ue_id_t ue_id, rb_id_t drb_id)
{
  uint64_t now = nr_up_current_time_ms();
  nr_up_manager_lock();
  nr_up_desired_buffer_t *drb_budget = nr_up_manager_lookup_drb(ue_id, drb_id);
  AssertFatal(drb_budget != NULL, "%s(): ue %lx drb %ld not added\n", __func__, ue_id, drb_id);

  /* Takes into account the time since the last poll and the time since the last DBS was received
   * to avoid polling too often (the DU can send DDS autonomously). */
  const bool never_synced = drb_budget->dbs.last_dbs_sync == NR_UP_DBS_NEVER_SYNCED;
  const bool sync_stale = never_synced || (now - drb_budget->dbs.last_dbs_sync) >= NR_UP_REPORT_POLLING_TIMER_MS;
  if (sync_stale && (now - drb_budget->dbs.last_report_poll) >= NR_UP_REPORT_POLLING_TIMER_MS) {
    drb_budget->dbs.last_report_poll = now;
    nr_up_manager_unlock();
    return true; /* Stale budget */
  }

  nr_up_manager_unlock();
  return false; /* No stale budget */
}

/** @brief Remap the UE budget entry when F1 reestablishment changes the UE id */
void nr_up_update_ue_id(ue_id_t old_ue_id, ue_id_t new_ue_id)
{
  if (old_ue_id == new_ue_id) {
    return;
  }
  nr_up_manager_lock();
  nr_up_ue_state_t *ue = nr_up_manager_lookup_ue(old_ue_id);
  if (ue == NULL) {
    nr_up_manager_unlock();
    return;
  }
  AssertFatal(nr_up_manager_lookup_ue(new_ue_id) == NULL, "%s(): ue %lx already present\n", __func__, new_ue_id);
  /* Detach, then reinsert under the new UE id key. */
  hashtable_remove(&g_nr_up_manager.ues, NR_UP_UE_KEY(old_ue_id), NULL);
  hashtable_rc_t rc = hashtable_insert(&g_nr_up_manager.ues, NR_UP_UE_KEY(new_ue_id), ue, free);
  DevAssert(rc == HASH_TABLE_OK);
  nr_up_manager_unlock();
}
