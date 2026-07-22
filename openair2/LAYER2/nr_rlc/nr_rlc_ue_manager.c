/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_rlc_ue_manager.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "LOG/log.h"

typedef struct {
  pthread_mutex_t lock;
  nr_rlc_ue_t     **ue_list;
  int             ue_count;
  nr_rlc_op_mode_t mode;
} nr_rlc_ue_manager_internal_t;

nr_rlc_ue_manager_t *new_nr_rlc_ue_manager(nr_rlc_op_mode_t mode)
{
  nr_rlc_ue_manager_internal_t *ret;

  ret = calloc(1, sizeof(nr_rlc_ue_manager_internal_t));
  if (ret == NULL) {
    LOG_E(RLC, "%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  /* Initialise the RLC manager mutex with PTHREAD_PRIO_INHERIT. */
  pthread_mutexattr_t mattr;
  pthread_mutexattr_init(&mattr);
  pthread_mutexattr_setprotocol(&mattr, PTHREAD_PRIO_INHERIT);
  if (pthread_mutex_init(&ret->lock, &mattr)) abort();
  pthread_mutexattr_destroy(&mattr);
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

void nr_rlc_manager_lock(nr_rlc_ue_manager_t *_m)
{
  nr_rlc_ue_manager_internal_t *m = _m;
  if (pthread_mutex_lock(&m->lock)) {
    LOG_E(RLC, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

void nr_rlc_manager_unlock(nr_rlc_ue_manager_t *_m)
{
  nr_rlc_ue_manager_internal_t *m = _m;
  if (pthread_mutex_unlock(&m->lock)) {
    LOG_E(RLC, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

/* must be called with lock acquired */
nr_rlc_ue_t *nr_rlc_manager_get_ue(nr_rlc_ue_manager_t *_m, int ue_id)
{
  /* TODO: optimze */
  nr_rlc_ue_manager_internal_t *m = _m;
  int i;

  for (i = 0; i < m->ue_count; i++)
    if (m->ue_list[i]->ue_id == ue_id)
      return m->ue_list[i];

  LOG_D(RLC, "New UE with ID %d\n", ue_id);

  m->ue_count++;
  m->ue_list = realloc(m->ue_list, sizeof(nr_rlc_ue_t *) * m->ue_count);
  if (m->ue_list == NULL) {
    LOG_E(RLC, "%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
  m->ue_list[m->ue_count-1] = calloc(1, sizeof(nr_rlc_ue_t));
  if (m->ue_list[m->ue_count-1] == NULL) {
    LOG_E(RLC, "%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  m->ue_list[m->ue_count-1]->ue_id = ue_id;

  return m->ue_list[m->ue_count-1];
}

/* must be called with lock acquired */
void nr_rlc_manager_remove_ue(nr_rlc_ue_manager_t *_m, int ue_id)
{
  nr_rlc_ue_manager_internal_t *m = _m;
  nr_rlc_ue_t *ue;
  int i;
  int j;

  for (i = 0; i < m->ue_count; i++)
    if (m->ue_list[i]->ue_id == ue_id)
      break;

  if (i == m->ue_count) {
    LOG_W(RLC, "Warning: ue %d not found\n", ue_id);
    return;
  }

  ue = m->ue_list[i];

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

  free(ue);

  m->ue_count--;
  if (m->ue_count == 0) {
    free(m->ue_list);
    m->ue_list = NULL;
    return;
  }

  memmove(&m->ue_list[i], &m->ue_list[i+1],
          (m->ue_count - i) * sizeof(nr_rlc_ue_t *));
  m->ue_list = realloc(m->ue_list, m->ue_count * sizeof(nr_rlc_ue_t *));
  if (m->ue_list == NULL) {
    LOG_E(RLC, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
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
