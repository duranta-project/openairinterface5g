/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "xnap_ids.h"

#include <pthread.h>
#include <stdlib.h>
#include "ds/hashtable.h"
#include "common/utils/assertions.h"
#include "common/utils/utils.h"
#include "common/utils/LOG/log.h"

/* One table per side: entries keyed on the locally allocated XnAP UE ID,
 * plus an index rrc_ue_id -> local XnAP UE ID */
typedef struct {
  hash_table_t *by_xnap_id;
  hash_table_t *by_rrc_ue_id;
  pthread_mutex_t mutex;
  uint32_t next_id;
  // source table: entries keyed on s_ng_node_ue_xnap_id, else on t_ng_node_ue_xnap_id
  bool source_side;
} xn_ue_table_t;

static xn_ue_table_t source = {.mutex = PTHREAD_MUTEX_INITIALIZER, .next_id = 1, .source_side = true};
static xn_ue_table_t target = {.mutex = PTHREAD_MUTEX_INITIALIZER, .next_id = 1, .source_side = false};

static void table_init(xn_ue_table_t *t)
{
  pthread_mutex_lock(&t->mutex);
  DevAssert(t->by_xnap_id == NULL);
  t->by_xnap_id = hashtable_create(1319, NULL, free);
  t->by_rrc_ue_id = hashtable_create(1319, NULL, free);
  DevAssert(t->by_xnap_id != NULL && t->by_rrc_ue_id != NULL);
  pthread_mutex_unlock(&t->mutex);
}

/* All helpers below expect the table mutex to be held */
static uint32_t local_id(const xn_ue_table_t *t, const xn_ue_data_t *e)
{
  return t->source_side ? e->s_ng_node_ue_xnap_id : e->t_ng_node_ue_xnap_id;
}

static xn_ue_data_t *table_get(xn_ue_table_t *t, uint32_t xnap_id)
{
  void *data = NULL;
  return hashtable_get(t->by_xnap_id, xnap_id, &data) == HASH_TABLE_OK ? data : NULL;
}

static xn_ue_data_t *table_get_by_rrc_ue_id(xn_ue_table_t *t, uint32_t rrc_ue_id)
{
  void *id = NULL;
  if (hashtable_get(t->by_rrc_ue_id, rrc_ue_id, &id) != HASH_TABLE_OK)
    return NULL;
  return table_get(t, *(uint32_t *)id);
}

static bool table_remove(xn_ue_table_t *t, uint32_t xnap_id)
{
  xn_ue_data_t *e = table_get(t, xnap_id);
  if (e == NULL)
    return false;
  if (e->rrc_ue_id != 0)
    hashtable_remove(t->by_rrc_ue_id, e->rrc_ue_id);
  hashtable_remove(t->by_xnap_id, xnap_id);
  return true;
}

static uint32_t table_add(xn_ue_table_t *t, const xn_ue_data_t *data)
{
  uint32_t xnap_id = t->next_id++;
  xn_ue_data_t *e = malloc_or_fail(sizeof(*e));
  *e = *data;
  if (t->source_side)
    e->s_ng_node_ue_xnap_id = xnap_id;
  else
    e->t_ng_node_ue_xnap_id = xnap_id;
  hashtable_rc_t rc = hashtable_insert(t->by_xnap_id, xnap_id, e);
  AssertFatal(rc == HASH_TABLE_OK, "XnAP UE ID %u already in use\n", xnap_id);
  return xnap_id;
}

/* index the entry xnap_id by rrc_ue_id; an entry left over from an earlier handover
 * of this UE (e.g. the UE was released during the handover) is dropped */
static void table_index(xn_ue_table_t *t, uint32_t rrc_ue_id, uint32_t xnap_id)
{
  xn_ue_data_t *stale = table_get_by_rrc_ue_id(t, rrc_ue_id);
  if (stale != NULL && local_id(t, stale) == xnap_id)
    return;
  if (stale != NULL) {
    LOG_W(XNAP, "UE %u: dropping stale Xn handover (XnAP UE ID %u)\n", rrc_ue_id, local_id(t, stale));
    table_remove(t, local_id(t, stale));
  }
  uint32_t *id = malloc_or_fail(sizeof(*id));
  *id = xnap_id;
  hashtable_rc_t rc = hashtable_insert(t->by_rrc_ue_id, rrc_ue_id, id);
  AssertFatal(rc == HASH_TABLE_OK, "UE %u: cannot index Xn handover\n", rrc_ue_id);
}

static bool table_copy(const xn_ue_data_t *e, xn_ue_data_t *out)
{
  if (e != NULL)
    *out = *e;
  return e != NULL;
}

/* Remove up to max entries towards assoc_id (e.g. the Xn peer is lost),
 * copying them to out; returns the number of entries removed */
static int table_remove_by_assoc(xn_ue_table_t *t, sctp_assoc_t assoc_id, xn_ue_data_t *out, int max)
{
  int n = 0;
  hash_table_iterator_s it = hashtable_get_iterator(t->by_xnap_id);
  xn_ue_data_t *e = NULL;
  while (n < max && hashtable_iterator_getnext(&it, (void **)&e)) {
    if (e->assoc_id == assoc_id)
      out[n++] = *e;
  }
  for (int i = 0; i < n; i++)
    table_remove(t, local_id(t, &out[i]));
  return n;
}

static bool table_remove_by_rrc_ue_id(xn_ue_table_t *t, uint32_t rrc_ue_id)
{
  xn_ue_data_t *e = table_get_by_rrc_ue_id(t, rrc_ue_id);
  return e != NULL && table_remove(t, local_id(t, e));
}

void xn_init_ue_data(void)
{
  table_init(&source);
  table_init(&target);
}

/* ------------------------------------------------------------------ */
/* Source side                                                        */
/* ------------------------------------------------------------------ */

uint32_t xn_add_source_ue(uint32_t rrc_ue_id, sctp_assoc_t target_assoc_id)
{
  pthread_mutex_lock(&source.mutex);
  xn_ue_data_t data = {.rrc_ue_id = rrc_ue_id, .assoc_id = target_assoc_id};
  uint32_t s_xnap_id = table_add(&source, &data);
  table_index(&source, rrc_ue_id, s_xnap_id);
  pthread_mutex_unlock(&source.mutex);
  return s_xnap_id;
}

bool xn_set_source_ue_target_id(uint32_t s_xnap_id, uint32_t t_xnap_id)
{
  pthread_mutex_lock(&source.mutex);
  xn_ue_data_t *e = table_get(&source, s_xnap_id);
  if (e != NULL)
    e->t_ng_node_ue_xnap_id = t_xnap_id;
  pthread_mutex_unlock(&source.mutex);
  return e != NULL;
}

bool xn_get_source_ue(uint32_t rrc_ue_id, xn_ue_data_t *out)
{
  pthread_mutex_lock(&source.mutex);
  bool found = table_copy(table_get_by_rrc_ue_id(&source, rrc_ue_id), out);
  pthread_mutex_unlock(&source.mutex);
  return found;
}

bool xn_get_source_ue_by_xnap_id(uint32_t s_xnap_id, xn_ue_data_t *out)
{
  pthread_mutex_lock(&source.mutex);
  bool found = table_copy(table_get(&source, s_xnap_id), out);
  pthread_mutex_unlock(&source.mutex);
  return found;
}

bool xn_remove_source_ue(uint32_t s_xnap_id)
{
  pthread_mutex_lock(&source.mutex);
  bool removed = table_remove(&source, s_xnap_id);
  pthread_mutex_unlock(&source.mutex);
  return removed;
}

int xn_remove_source_ues_by_assoc(sctp_assoc_t assoc_id, xn_ue_data_t *out, int max)
{
  pthread_mutex_lock(&source.mutex);
  int n = table_remove_by_assoc(&source, assoc_id, out, max);
  pthread_mutex_unlock(&source.mutex);
  return n;
}

/* ------------------------------------------------------------------ */
/* Target side                                                        */
/* ------------------------------------------------------------------ */

/* Target entry of the handover s_xnap_id from the peer assoc_id, or NULL; target.mutex held */
static xn_ue_data_t *target_get_by_source_id(sctp_assoc_t assoc_id, uint32_t s_xnap_id)
{
  hash_table_iterator_s it = hashtable_get_iterator(target.by_xnap_id);
  xn_ue_data_t *e = NULL;
  while (hashtable_iterator_getnext(&it, (void **)&e))
    if (e->assoc_id == assoc_id && e->s_ng_node_ue_xnap_id == s_xnap_id)
      return e;
  return NULL;
}

uint32_t xn_add_target_ue(uint32_t s_xnap_id, sctp_assoc_t source_assoc_id)
{
  pthread_mutex_lock(&target.mutex);
  uint32_t t_xnap_id = 0;
  /* a repeated HandoverRequest of the same handover gets no second entry */
  if (target_get_by_source_id(source_assoc_id, s_xnap_id) == NULL) {
    xn_ue_data_t data = {.s_ng_node_ue_xnap_id = s_xnap_id, .assoc_id = source_assoc_id};
    t_xnap_id = table_add(&target, &data);
  }
  pthread_mutex_unlock(&target.mutex);
  return t_xnap_id;
}

bool xn_bind_target_ue(uint32_t t_xnap_id, uint32_t rrc_ue_id)
{
  pthread_mutex_lock(&target.mutex);
  xn_ue_data_t *e = table_get(&target, t_xnap_id);
  if (e != NULL) {
    table_index(&target, rrc_ue_id, t_xnap_id);
    e->rrc_ue_id = rrc_ue_id;
  }
  pthread_mutex_unlock(&target.mutex);
  return e != NULL;
}

bool xn_get_target_ue(uint32_t rrc_ue_id, xn_ue_data_t *out)
{
  pthread_mutex_lock(&target.mutex);
  bool found = table_copy(table_get_by_rrc_ue_id(&target, rrc_ue_id), out);
  pthread_mutex_unlock(&target.mutex);
  return found;
}

bool xn_get_target_ue_by_xnap_id(uint32_t t_xnap_id, xn_ue_data_t *out)
{
  pthread_mutex_lock(&target.mutex);
  bool found = table_copy(table_get(&target, t_xnap_id), out);
  pthread_mutex_unlock(&target.mutex);
  return found;
}

bool xn_remove_target_ue(uint32_t t_xnap_id)
{
  pthread_mutex_lock(&target.mutex);
  bool removed = table_remove(&target, t_xnap_id);
  pthread_mutex_unlock(&target.mutex);
  return removed;
}

int xn_remove_target_ues_by_assoc(sctp_assoc_t assoc_id, xn_ue_data_t *out, int max)
{
  pthread_mutex_lock(&target.mutex);
  int n = table_remove_by_assoc(&target, assoc_id, out, max);
  pthread_mutex_unlock(&target.mutex);
  return n;
}

/* ------------------------------------------------------------------ */
/* Both sides                                                         */
/* ------------------------------------------------------------------ */

bool xn_remove_ue(uint32_t rrc_ue_id)
{
  pthread_mutex_lock(&source.mutex);
  bool removed = table_remove_by_rrc_ue_id(&source, rrc_ue_id);
  pthread_mutex_unlock(&source.mutex);
  pthread_mutex_lock(&target.mutex);
  removed |= table_remove_by_rrc_ue_id(&target, rrc_ue_id);
  pthread_mutex_unlock(&target.mutex);
  return removed;
}
