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
