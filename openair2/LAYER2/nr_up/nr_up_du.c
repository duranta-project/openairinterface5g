/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up_du.h"

#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "assertions.h"
#include "common/openairinterface5g_limits.h"
#include "common/platform_constants.h"
#include "common/utils/LOG/log.h"
#include "common/utils/ds/hashtable.h"
#include "common/utils/utils.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"

/* TS 38.425 clause 5.4.2.1 autonomous refresh while DBS is changing */
#define NR_UP_DU_DDDS_TIMER_MS 20u

#define NR_UP_DU_DRB_KEY(ue_id, drb_id) ((((hash_key_t)(ue_id)) << 8) | ((hash_key_t)(drb_id)&0xffu))

typedef struct {
  bool has_last_report;
  uint32_t last_desired_buffer_size;
  struct timespec last_report_time;
} nr_up_du_ddds_state_t;

typedef struct {
  pthread_mutex_t lock;
  hash_table_t *drbs;
  /** Send a DDDS for ue/rb */
  nr_up_du_ddds_tx_fn_t send_ddds;
} nr_up_du_manager_t;

static nr_up_du_manager_t g_nr_up_du;

static uint32_t nr_up_du_rlc_tx_space(ue_id_t ue_id, rb_id_t drb_id)
{
  const int tx_space = nr_rlc_get_available_tx_space(ue_id, drb_id + 3); /* DRB ID to LCID */
  if (tx_space <= 0)
    return 0;
  return tx_space;
}

static size_t nr_up_du_timespec_diff_ms(const struct timespec *later, const struct timespec *earlier)
{
  return (later->tv_sec - earlier->tv_sec) * 1000 + (later->tv_nsec - earlier->tv_nsec) / 1000000;
}

static nr_up_du_ddds_state_t *nr_up_du_lookup(ue_id_t ue_id, rb_id_t drb_id)
{
  void *data = NULL;
  const hash_key_t key = NR_UP_DU_DRB_KEY(ue_id, drb_id);
  if (hashtable_get(g_nr_up_du.drbs, key, &data) == HASH_TABLE_OK)
    return data;
  return NULL;
}

static nr_up_du_ddds_state_t *nr_up_du_insert(ue_id_t ue_id, rb_id_t drb_id)
{
  const hash_key_t key = NR_UP_DU_DRB_KEY(ue_id, drb_id);
  nr_up_du_ddds_state_t *st = calloc_or_fail(1, sizeof(*st));
  hashtable_rc_t rc = hashtable_insert(g_nr_up_du.drbs, key, st);
  AssertFatal(rc == HASH_TABLE_OK, "nr_up_du_insert failed\n");
  return st;
}

static void nr_up_du_update_last_report(nr_up_du_ddds_state_t *st, uint32_t space, const struct timespec *now)
{
  st->has_last_report = true;
  st->last_desired_buffer_size = space;
  st->last_report_time = *now;
}

/** @brief Whether to send DDDS because RLC TX space hit or left zero.
 * CU treats Desired Buffer Size 0 as "stop DL" and a later non-zero as
 * "may send again" (TS 38.425 clause 5.4.2.1). This helper only detects that
 * 0 to non-zero transition (or the first time we see space already 0) */
static bool nr_up_du_zero_dbs_trigger(const nr_up_du_ddds_state_t *st, uint32_t space)
{
  if (!st->has_last_report)
    return space == 0;
  return (space == 0) != (st->last_desired_buffer_size == 0);
}

/** @brief DU timer while DL is still arriving: refresh if DBS changed (skip unchanged) */
static bool nr_up_du_timer_trigger(const nr_up_du_ddds_state_t *st, uint32_t space, const struct timespec *now)
{
  if (!st->has_last_report || space == st->last_desired_buffer_size)
    return false;
  return nr_up_du_timespec_diff_ms(now, &st->last_report_time) >= NR_UP_DU_DDDS_TIMER_MS;
}

void nr_up_init_du(nr_up_du_ddds_tx_fn_t send_ddds)
{
  DevAssert(send_ddds != NULL);
  if (g_nr_up_du.drbs != NULL)
    return;
  if (pthread_mutex_init(&g_nr_up_du.lock, NULL) != 0)
    abort();
  g_nr_up_du.send_ddds = send_ddds;
  g_nr_up_du.drbs = hashtable_create(MAX_MOBILES_PER_GNB * MAX_DRBS_PER_UE, NULL, free);
  AssertFatal(g_nr_up_du.drbs != NULL, "nr_up_init_du: hashtable_create failed\n");
}

/** @brief Create per-DRB DDDS bookkeeping */
void nr_up_du_add_drb(ue_id_t ue_id, rb_id_t drb_id)
{
  DevAssert(g_nr_up_du.drbs != NULL);
  if (pthread_mutex_lock(&g_nr_up_du.lock) != 0)
    abort();
  AssertFatal(nr_up_du_lookup(ue_id, drb_id) == NULL, "nr_up_du_add_drb: ue %lx drb %ld already present\n", ue_id, drb_id);
  nr_up_du_insert(ue_id, drb_id);
  if (pthread_mutex_unlock(&g_nr_up_du.lock) != 0)
    abort();
}

/** @brief Remap DDDS bookkeeping when F1 reestablishment changes the UE id (with GTP) */
void nr_up_du_update_ue_id(ue_id_t old_ue_id, ue_id_t new_ue_id)
{
  DevAssert(g_nr_up_du.drbs != NULL);
  if (old_ue_id == new_ue_id)
    return;
  if (pthread_mutex_lock(&g_nr_up_du.lock) != 0)
    abort();
  for (rb_id_t drb_id = 1; drb_id <= MAX_DRBS_PER_UE; drb_id++) {
    nr_up_du_ddds_state_t *st = nr_up_du_lookup(old_ue_id, drb_id);
    if (st == NULL)
      continue;
    AssertFatal(nr_up_du_lookup(new_ue_id, drb_id) == NULL,
                "nr_up_du_update_ue_id: ue %lx drb %ld already present\n",
                new_ue_id,
                drb_id);
    nr_up_du_ddds_state_t *nst = nr_up_du_insert(new_ue_id, drb_id);
    *nst = *st;
    hashtable_remove(g_nr_up_du.drbs, NR_UP_DU_DRB_KEY(old_ue_id, drb_id));
  }
  if (pthread_mutex_unlock(&g_nr_up_du.lock) != 0)
    abort();
}

/** @brief Answer F1-U Report Polling: return Desired Buffer Size for a mandatory DDDS */
uint32_t nr_up_du_poll_ddds(ue_id_t ue_id, rb_id_t drb_id)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  const uint32_t space = nr_up_du_rlc_tx_space(ue_id, drb_id);

  if (pthread_mutex_lock(&g_nr_up_du.lock) != 0)
    abort();
  nr_up_du_ddds_state_t *st = nr_up_du_lookup(ue_id, drb_id);
  DevAssert(st != NULL);
  nr_up_du_update_last_report(st, space, &now);
  if (pthread_mutex_unlock(&g_nr_up_du.lock) != 0)
    abort();

  return space;
}

/** @brief Sync RLC TX space into the DU DDDS table, may send DDDS if needed
 * (TS 38.425 §5.4).
 *
 * Send when:
 * - Desired Buffer Size hit/left 0 (§5.4.2.1), or
 * - DBS changed and NR_UP_DU_DDDS_TIMER_MS elapsed (§5.4.2.1 "decides to trigger"). */
void nr_up_du_tx_space_sync(ue_id_t ue_id, rb_id_t drb_id, uint32_t space)
{
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  if (pthread_mutex_lock(&g_nr_up_du.lock) != 0)
    abort();
  nr_up_du_ddds_state_t *st = nr_up_du_lookup(ue_id, drb_id);
  DevAssert(st != NULL);
  const bool send = nr_up_du_zero_dbs_trigger(st, space) || nr_up_du_timer_trigger(st, space, &now);
  if (send)
    nr_up_du_update_last_report(st, space, &now);
  if (pthread_mutex_unlock(&g_nr_up_du.lock) != 0)
    abort();

  if (!send)
    return;
  if (g_nr_up_du.send_ddds(ue_id, drb_id, space) != 0)
    LOG_W(NR_UP, "nr_up_du_tx_space_sync: no F1-U tunnel for ue %lx drb %ld\n", ue_id, drb_id);
}

/** @brief Drop per-DRB DDDS state */
void nr_up_du_release_drb(ue_id_t ue_id, rb_id_t drb_id)
{
  if (g_nr_up_du.drbs == NULL)
    return;
  if (pthread_mutex_lock(&g_nr_up_du.lock) != 0)
    abort();
  hashtable_remove(g_nr_up_du.drbs, NR_UP_DU_DRB_KEY(ue_id, drb_id));
  if (pthread_mutex_unlock(&g_nr_up_du.lock) != 0)
    abort();
}

/** @brief Drop all DDDS state for a UE */
void nr_up_du_release_ue(ue_id_t ue_id)
{
  if (g_nr_up_du.drbs == NULL)
    return;
  if (pthread_mutex_lock(&g_nr_up_du.lock) != 0)
    abort();
  for (rb_id_t drb_id = 1; drb_id <= MAX_DRBS_PER_UE; drb_id++)
    hashtable_remove(g_nr_up_du.drbs, NR_UP_DU_DRB_KEY(ue_id, drb_id));
  if (pthread_mutex_unlock(&g_nr_up_du.lock) != 0)
    abort();
}
