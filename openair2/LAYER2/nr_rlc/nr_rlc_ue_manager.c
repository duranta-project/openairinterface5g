/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_rlc_ue_manager.h"

#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include "LOG/log.h"
#include "common/utils/assertions.h"

/* UE IDs are RNTIs on the gNB, small instance numbers on the UE */
#define NR_RLC_UE_ID_RANGE (1 << 16)

typedef struct {
  /* serialises creation, removal and ID update; lock order: this one, then the UE's */
  pthread_mutex_t lock;
  nr_rlc_op_mode_t mode;
  nr_rlc_ue_t *ues;
  /* ue_id -> index in ues + 1, 0: no such UE */
  _Atomic uint16_t id2ue[NR_RLC_UE_ID_RANGE];
} nr_rlc_ue_manager_internal_t;

nr_rlc_ue_manager_t *new_nr_rlc_ue_manager(nr_rlc_op_mode_t mode)
{
  nr_rlc_ue_manager_internal_t *ret;

  ret = calloc(1, sizeof(nr_rlc_ue_manager_internal_t));
  if (ret == NULL) {
    LOG_E(RLC, "%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  if (posix_memalign((void **)&ret->ues, 64, NR_RLC_MAX_UES * sizeof(nr_rlc_ue_t)) != 0) {
    LOG_E(RLC, "%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
  memset(ret->ues, 0, NR_RLC_MAX_UES * sizeof(nr_rlc_ue_t));

  if (pthread_mutex_init(&ret->lock, NULL)) abort();
  for (int i = 0; i < NR_RLC_MAX_UES; i++)
    if (pthread_mutex_init(&ret->ues[i].lock, NULL)) abort();
  ret->mode = mode;

  return ret;
}

bool nr_rlc_manager_rlc_is_split(nr_rlc_ue_manager_t *_m)
{
  nr_rlc_ue_manager_internal_t *m = _m;
  return m->mode == NR_RLC_OP_MODE_SPLIT_GNB;
}

int nr_rlc_manager_get_gnb_flag(nr_rlc_ue_manager_t *_m)
{
  nr_rlc_ue_manager_internal_t *m = _m;
  return m->mode != NR_RLC_OP_MODE_UE;
}

static void lock_mutex(pthread_mutex_t *lock)
{
  if (pthread_mutex_lock(lock)) {
    LOG_E(RLC, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

static void unlock_mutex(pthread_mutex_t *lock)
{
  if (pthread_mutex_unlock(lock)) {
    LOG_E(RLC, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

nr_rlc_ue_t *nr_rlc_manager_find_ue(nr_rlc_ue_manager_t *_m, int ue_id)
{
  nr_rlc_ue_manager_internal_t *m = _m;

  if (ue_id < 0 || ue_id >= NR_RLC_UE_ID_RANGE)
    return NULL;

  while (true) {
    int idx = atomic_load_explicit(&m->id2ue[ue_id], memory_order_acquire);
    if (idx == 0)
      return NULL;
    nr_rlc_ue_t *ue = &m->ues[idx - 1];
    lock_mutex(&ue->lock);
    if (ue->in_use && ue->ue_id == ue_id)
      return ue;
    /* removed or renamed between the table read and the lock: read again */
    unlock_mutex(&ue->lock);
  }
}

void nr_rlc_manager_unlock_ue(nr_rlc_ue_t *ue)
{
  unlock_mutex(&ue->lock);
}

nr_rlc_ue_t *nr_rlc_manager_get_ue(nr_rlc_ue_manager_t *_m, int ue_id)
{
  nr_rlc_ue_manager_internal_t *m = _m;

  nr_rlc_ue_t *ue = nr_rlc_manager_find_ue(m, ue_id);
  if (ue != NULL)
    return ue;

  AssertFatal(ue_id >= 0 && ue_id < NR_RLC_UE_ID_RANGE, "bad UE ID %d\n", ue_id);

  lock_mutex(&m->lock);
  ue = nr_rlc_manager_find_ue(m, ue_id);
  if (ue != NULL) {
    unlock_mutex(&m->lock);
    return ue;
  }

  LOG_D(RLC, "New UE with ID %d\n", ue_id);

  int i;
  for (i = 0; i < NR_RLC_MAX_UES; i++)
    if (!m->ues[i].in_use)
      break;
  AssertFatal(i < NR_RLC_MAX_UES, "no free RLC context for UE %d\n", ue_id);

  ue = &m->ues[i];
  lock_mutex(&ue->lock);
  ue->in_use = true;
  ue->ue_id = ue_id;
  atomic_store_explicit(&m->id2ue[ue_id], i + 1, memory_order_release);
  unlock_mutex(&m->lock);

  return ue;
}

void nr_rlc_manager_remove_ue(nr_rlc_ue_manager_t *_m, int ue_id)
{
  nr_rlc_ue_manager_internal_t *m = _m;
  int j;

  lock_mutex(&m->lock);
  nr_rlc_ue_t *ue = nr_rlc_manager_find_ue(m, ue_id);
  if (ue == NULL) {
    unlock_mutex(&m->lock);
    LOG_W(RLC, "Warning: ue %d not found\n", ue_id);
    return;
  }

  if (ue->srb0 != NULL) {
    /* deliver_sdu_data for srb0 is allocated, needs a free() */
    free(ue->srb0->deliver_sdu_data);
    ue->srb0->delete_entity(ue->srb0);
  }

  for (j = 0; j < 3; j++)
    if (ue->srb[j] != NULL)
      ue->srb[j]->delete_entity(ue->srb[j]);

  for (j = 0; j < MAX_DRBS_PER_UE; j++)
    if (ue->drb[j] != NULL)
      ue->drb[j]->delete_entity(ue->drb[j]);

  ue->srb0 = NULL;
  memset(ue->srb, 0, sizeof(ue->srb));
  memset(ue->drb, 0, sizeof(ue->drb));
  memset(ue->lcid2rb, 0, sizeof(ue->lcid2rb));
  ue->rlf_handler = NULL;
  ue->in_use = false;
  atomic_store_explicit(&m->id2ue[ue_id], 0, memory_order_release);

  unlock_mutex(&ue->lock);
  unlock_mutex(&m->lock);
}

nr_rlc_ue_t *nr_rlc_manager_update_ue_id(nr_rlc_ue_manager_t *_m, int from_id, int to_id)
{
  nr_rlc_ue_manager_internal_t *m = _m;

  AssertFatal(to_id >= 0 && to_id < NR_RLC_UE_ID_RANGE, "bad UE ID %d\n", to_id);

  lock_mutex(&m->lock);
  nr_rlc_ue_t *ue = nr_rlc_manager_find_ue(m, to_id);
  if (ue != NULL) {
    unlock_mutex(&ue->lock);
    unlock_mutex(&m->lock);
    LOG_E(RLC, "UE ID %d already in use\n", to_id);
    return NULL;
  }

  ue = nr_rlc_manager_find_ue(m, from_id);
  if (ue != NULL) {
    int idx = atomic_load_explicit(&m->id2ue[from_id], memory_order_relaxed);
    ue->ue_id = to_id;
    atomic_store_explicit(&m->id2ue[to_id], idx, memory_order_release);
    atomic_store_explicit(&m->id2ue[from_id], 0, memory_order_release);
  }
  unlock_mutex(&m->lock);
  return ue;
}

/* must be called with lock acquired */
void nr_rlc_ue_add_srb_rlc_entity(nr_rlc_ue_t *ue, int srb_id, nr_rlc_entity_t *entity)
{
  if (srb_id < 0 || srb_id > 3) {
    LOG_E(RLC, "%s:%d:%s: fatal, bad srb id\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  /* special case: srb0 */
  if (srb_id == 0) {
    if (ue->srb0 != NULL) {
      LOG_E(RLC, "fatal, srb0 already present\n");
      exit(1);
    }

    ue->srb0 = entity;

    return;
  }

  srb_id--;

  if (ue->srb[srb_id] != NULL) {
    LOG_E(RLC, "%s:%d:%s: fatal, srb already present\n",
          __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  ue->srb[srb_id] = entity;
}

/* must be called with lock acquired */
void nr_rlc_ue_add_drb_rlc_entity(nr_rlc_ue_t *ue, int drb_id, nr_rlc_entity_t *entity)
{
  if (drb_id < 1 || drb_id > MAX_DRBS_PER_UE) {
    LOG_E(RLC, "%s:%d:%s: fatal, bad drb id\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  drb_id--;

  if (ue->drb[drb_id] != NULL) {
    LOG_E(RLC, "%s:%d:%s: fatal, drb already present\n",
          __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  ue->drb[drb_id] = entity;
}
