/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "ran_func_dapp_subs.h"
#include "common/utils/assertions.h"
#include "common/utils/alg/find.h"

#include <assert.h>
#include <pthread.h>

/** @brief Mutex protecting all per-SM dapp_subs_data_t operations. */
static pthread_mutex_t dapp_mutex = PTHREAD_MUTEX_INITIALIZER;

/** @brief Mutex protecting the global Format 1 and Format 2 registries. */
static pthread_mutex_t g_map_mtx = PTHREAD_MUTEX_INITIALIZER;

/** @brief Global list of RIC request IDs subscribed to Format 1 (E3 data reports). */
static seq_arr_t g_all_frmt1_subs;

/** @brief Global list of RIC request IDs subscribed to Format 2 (subscription map). */
static seq_arr_t g_all_frmt2_subs;

/** @brief Guard for one-time initialization of the global lists. */
static bool g_map_inited = false;

/* ── Helpers ── */

/**
 * @brief Predicate: compare two uint32_t values for equality.
 *
 * Used with find_if() to locate a RIC request ID in a seq_arr_t.
 */
static bool eq_uint32(const void *value, const void *it)
{
  return *(const uint32_t *)value == *(const uint32_t *)it;
}

/**
 * @brief Check whether a seq_arr_t contains a given uint32_t value.
 *
 * @param s  The sequence to search.
 * @param v  The value to look for.
 * @return   true if found, false otherwise.
 */
static bool seq_contains(const seq_arr_t *s, uint32_t v)
{
  for (void *it = seq_arr_front((seq_arr_t *)s); it != seq_arr_end((seq_arr_t *)s); it = seq_arr_next(s, it))
    if (*(uint32_t *)it == v)
      return true;
  return false;
}

/**
 * @brief Append a uint32_t to a seq_arr_t if not already present.
 *
 * @param s  The sequence to modify.
 * @param v  The value to insert.
 */
static void seq_push_unique(seq_arr_t *s, uint32_t v)
{
  if (!seq_contains(s, v))
    seq_arr_push_back(s, &v, sizeof(v));
}

/**
 * @brief Remove the first occurrence of a uint32_t from a seq_arr_t.
 *
 * @param s  The sequence to modify.
 * @param v  The value to remove.
 * @return   true if an element was removed, false if not found.
 */
static bool seq_remove_first(seq_arr_t *s, uint32_t v)
{
  for (void *it = seq_arr_front(s); it != seq_arr_end(s); it = seq_arr_next(s, it)) {
    if (*(uint32_t *)it == v) {
      seq_arr_erase(s, it);
      return true;
    }
  }
  return false;
}

/**
 * @brief Copy all uint32_t elements from a seq_arr_t into a heap-allocated array.
 *
 * Must be called while g_map_mtx is held. Sets *out to NULL and returns 0
 * if the sequence is empty or allocation fails.
 *
 * @param      s    The sequence to snapshot.
 * @param[out] out  Set to the allocated array of IDs.
 * @return          Number of elements copied.
 */
static size_t seq_snapshot_locked(seq_arr_t *s, uint32_t **out)
{
  size_t n = 0;
  for (void *it = seq_arr_front(s); it != seq_arr_end(s); it = seq_arr_next(s, it))
    ++n;

  if (n == 0) {
    *out = NULL;
    return 0;
  }

  uint32_t *ids = malloc(n * sizeof(*ids));
  if (ids == NULL) {
    *out = NULL;
    return 0;
  }

  size_t i = 0;
  for (void *it = seq_arr_front(s); it != seq_arr_end(s); it = seq_arr_next(s, it))
    ids[i++] = *(uint32_t *)it;

  *out = ids;
  return n;
}

/**
 * @brief Lazily initialize both global subscription lists.
 *
 * Called under g_map_mtx before any global list operation.
 */
static void subs_init_once(void)
{
  if (!g_map_inited) {
    seq_arr_init(&g_all_frmt1_subs, sizeof(uint32_t));
    seq_arr_init(&g_all_frmt2_subs, sizeof(uint32_t));
    g_map_inited = true;
  }
}

/* ── Per-SM init / remove ── */

void init_dapp_subs_data(dapp_subs_data_t *dapp_subs_data)
{
  pthread_mutex_lock(&dapp_mutex);
  seq_arr_init(&dapp_subs_data->frmt_1_subs, sizeof(uint32_t));
  seq_arr_init(&dapp_subs_data->frmt_2_subs, sizeof(uint32_t));
  pthread_mutex_unlock(&dapp_mutex);
}

void remove_dapp_subs_data(dapp_subs_data_t *dapp_subs_data, uint32_t ric_req_id)
{
  pthread_mutex_lock(&dapp_mutex);

  elm_arr_t elm_f1 = find_if(&dapp_subs_data->frmt_1_subs, &ric_req_id, eq_uint32);
  const bool had_f1 = (elm_f1.it != NULL);

  elm_arr_t elm_f2 = find_if(&dapp_subs_data->frmt_2_subs, &ric_req_id, eq_uint32);
  const bool had_f2 = (elm_f2.it != NULL);

  if (had_f1)
    seq_arr_erase(&dapp_subs_data->frmt_1_subs, elm_f1.it);
  if (had_f2)
    seq_arr_erase(&dapp_subs_data->frmt_2_subs, elm_f2.it);

  pthread_mutex_unlock(&dapp_mutex);

  if (had_f1)
    ric_subs_frmt1_remove(ric_req_id);
  if (had_f2)
    ric_subs_frmt2_remove(ric_req_id);
}

/* ── Per-SM insert ── */

void insert_frmt_1_ric_id(dapp_subs_data_t *d, uint32_t ric_req_id)
{
  pthread_mutex_lock(&dapp_mutex);
  /* Dedup like the global registry (ric_subs_frmt1_add via seq_push_unique):
   * a duplicate ric_req_id here would desync the per-SM list from the global
   * (remove erases one entry each), stranding a still-active subscriber. */
  if (find_if(&d->frmt_1_subs, &ric_req_id, eq_uint32).it == NULL)
    seq_arr_push_back(&d->frmt_1_subs, &ric_req_id, sizeof(ric_req_id));
  pthread_mutex_unlock(&dapp_mutex);
}

void insert_frmt_2_ric_id(dapp_subs_data_t *d, uint32_t ric_req_id)
{
  pthread_mutex_lock(&dapp_mutex);
  /* Dedup to match the global registry (see insert_frmt_1_ric_id). */
  if (find_if(&d->frmt_2_subs, &ric_req_id, eq_uint32).it == NULL)
    seq_arr_push_back(&d->frmt_2_subs, &ric_req_id, sizeof(ric_req_id));
  pthread_mutex_unlock(&dapp_mutex);
}

/* ── Global Format 1 ── */

void ric_subs_frmt1_add(uint32_t ric_req_id)
{
  pthread_mutex_lock(&g_map_mtx);
  subs_init_once();
  seq_push_unique(&g_all_frmt1_subs, ric_req_id);
  pthread_mutex_unlock(&g_map_mtx);
}

size_t ric_subs_frmt1_snapshot(uint32_t **out)
{
  assert(out != NULL);
  *out = NULL;

  pthread_mutex_lock(&g_map_mtx);
  subs_init_once();
  size_t n = seq_snapshot_locked(&g_all_frmt1_subs, out);
  pthread_mutex_unlock(&g_map_mtx);

  return n;
}

void ric_subs_frmt1_remove(uint32_t ric_req_id)
{
  pthread_mutex_lock(&g_map_mtx);
  subs_init_once();
  (void)seq_remove_first(&g_all_frmt1_subs, ric_req_id);
  pthread_mutex_unlock(&g_map_mtx);
}

/* ── Global Format 2 ── */

void ric_subs_frmt2_add(uint32_t ric_req_id)
{
  pthread_mutex_lock(&g_map_mtx);
  subs_init_once();
  seq_push_unique(&g_all_frmt2_subs, ric_req_id);
  pthread_mutex_unlock(&g_map_mtx);
}

size_t ric_subs_frmt2_snapshot(uint32_t **out)
{
  assert(out != NULL);
  *out = NULL;

  pthread_mutex_lock(&g_map_mtx);
  subs_init_once();
  size_t n = seq_snapshot_locked(&g_all_frmt2_subs, out);
  pthread_mutex_unlock(&g_map_mtx);

  return n;
}

void ric_subs_frmt2_remove(uint32_t ric_req_id)
{
  pthread_mutex_lock(&g_map_mtx);
  subs_init_once();
  (void)seq_remove_first(&g_all_frmt2_subs, ric_req_id);
  pthread_mutex_unlock(&g_map_mtx);
}