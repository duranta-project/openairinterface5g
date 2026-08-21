/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_up/nr_up_backend_if.h"

#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "assertions.h"
#include "common/platform_constants.h"
#include "common/utils/ds/hashtable.h"
#include "common/utils/utils.h"

/* Per-DRB budget key: ue_id in the upper bits, drb_id in the low 8 bits. */
#define NR_UP_DRB_BUDGET_KEY(ue_id, drb_id) ((((hash_key_t)(ue_id)) << 8) | ((hash_key_t)(drb_id)&0xffu))

typedef struct {
  pthread_mutex_t lock;
  hash_table_t *drbs;
} nr_up_drb_budget_manager_t;

static nr_up_drb_budget_manager_t g_nr_up_manager;

size_t nr_up_timespec_diff_ms(const struct timespec *later, const struct timespec *earlier)
{
  return (later->tv_sec - earlier->tv_sec) * 1000 + (later->tv_nsec - earlier->tv_nsec) / 1000000;
}

void nr_up_manager_init(void)
{
  if (g_nr_up_manager.drbs != NULL) {
    return;
  }

  if (pthread_mutex_init(&g_nr_up_manager.lock, NULL) != 0) {
    abort();
  }

  g_nr_up_manager.drbs = hashtable_create(1024, NULL, free);
  AssertFatal(g_nr_up_manager.drbs != NULL, "nr_up_manager_init: hashtable_create failed\n");
}

void nr_up_manager_lock(void)
{
  DevAssert(g_nr_up_manager.drbs != NULL);
  if (pthread_mutex_lock(&g_nr_up_manager.lock) != 0) {
    abort();
  }
}

void nr_up_manager_unlock(void)
{
  DevAssert(g_nr_up_manager.drbs != NULL);
  if (pthread_mutex_unlock(&g_nr_up_manager.lock) != 0) {
    abort();
  }
}

nr_up_drb_budget_t *nr_up_manager_lookup_drb(ue_id_t ue_id, rb_id_t drb_id)
{
  hash_key_t key;
  void *data = NULL;

  DevAssert(drb_id >= 1 && drb_id <= MAX_DRBS_PER_UE);

  key = NR_UP_DRB_BUDGET_KEY(ue_id, drb_id);
  if (hashtable_get(g_nr_up_manager.drbs, key, &data) == HASH_TABLE_OK) {
    return data;
  }

  return NULL;
}

static nr_up_drb_budget_t *nr_up_manager_insert_drb(ue_id_t ue_id, rb_id_t drb_id)
{
  DevAssert(drb_id >= 1 && drb_id <= MAX_DRBS_PER_UE);
  hash_key_t key = NR_UP_DRB_BUDGET_KEY(ue_id, drb_id);
  nr_up_drb_budget_t *drb_budget = calloc_or_fail(1, sizeof(nr_up_drb_budget_t));
  hashtable_rc_t rc = hashtable_insert(g_nr_up_manager.drbs, key, drb_budget);
  DevAssert(rc == HASH_TABLE_OK);
  return drb_budget;
}

void nr_up_manager_release_drb(ue_id_t ue_id, rb_id_t drb_id)
{
  hash_key_t key;

  DevAssert(drb_id >= 1 && drb_id <= MAX_DRBS_PER_UE);
  if (g_nr_up_manager.drbs == NULL) {
    return;
  }

  key = NR_UP_DRB_BUDGET_KEY(ue_id, drb_id);
  nr_up_manager_lock();
  hashtable_remove(g_nr_up_manager.drbs, key);
  nr_up_manager_unlock();
}

void nr_up_drb_budget_consume(ue_id_t ue_id, rb_id_t drb_id, size_t bytes)
{
  if (bytes == 0) {
    return;
  }

  nr_up_manager_lock();
  nr_up_drb_budget_t *drb_budget = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (drb_budget != NULL) {
    if (bytes >= drb_budget->available_tx_bytes) {
      drb_budget->available_tx_bytes = 0;
    } else {
      drb_budget->available_tx_bytes -= bytes;
    }
  }
  nr_up_manager_unlock();
}

void nr_up_drb_budget_sync(ue_id_t ue_id, rb_id_t drb_id, uint32_t available_tx_bytes)
{
  nr_up_drb_budget_t *drb_budget;
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);

  nr_up_manager_lock();
  drb_budget = nr_up_manager_lookup_drb(ue_id, drb_id);
  if (drb_budget == NULL) {
    drb_budget = nr_up_manager_insert_drb(ue_id, drb_id);
  }
  drb_budget->available_tx_bytes = available_tx_bytes;
  drb_budget->last_rlc_refresh = now;
  nr_up_manager_unlock();
}
