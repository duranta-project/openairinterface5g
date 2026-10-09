/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "xnap_ids.h"

#include <pthread.h>
#include <stdlib.h>
#include "ds/hashtable.h"
#include "common/utils/assertions.h"

static bool add_hashtable_data(hash_table_t *ht, uint64_t ue_id, const xn_ue_data_t *data)
{
  xn_ue_data_t *idata = malloc(sizeof(*idata));
  AssertFatal(idata, "cannot allocate memory\n");
  *idata = *data;
  hashtable_rc_t ret = hashtable_insert(ht, ue_id, idata);
  return ret == HASH_TABLE_OK;
}

static bool target_add_hashtable_data(hash_table_t *ht, uint64_t ue_id, const xn_target_ue_data_t *data)
{
  xn_target_ue_data_t *idata = malloc(sizeof(*idata));
  AssertFatal(idata, "cannot allocate memory\n");
  *idata = *data;
  hashtable_rc_t ret = hashtable_insert(ht, ue_id, idata);
  return ret == HASH_TABLE_OK;
}

static xn_ue_data_t *get_hashtable_data(hash_table_t *ht, uint64_t xn_ue_id)
{
  void *data = NULL;
  hashtable_rc_t ret = hashtable_get(ht, xn_ue_id, &data);
  AssertFatal(ret == HASH_TABLE_OK && data != NULL, "element for xn_ue_id %ld not found\n", xn_ue_id);
  return data;
}

/* Source-side table: s_ng_node_ue_xnap_id → xn_ue_data_t */
static hash_table_t *xn_ue_mapping;
static pthread_mutex_t xn_ue_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t xn_next_ue_id = 1;

/* Target-side table: t_ng_node_ue_xnap_id → xnap_target_ue_data_t */
static hash_table_t *xn_target_ue_mapping;
static pthread_mutex_t xn_target_ue_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t xn_next_target_ue_id = 1;

void xn_init_ue_data(void)
{
  pthread_mutex_lock(&xn_ue_mutex);
  DevAssert(xn_ue_mapping == NULL);
  xn_ue_mapping = hashtable_create(1319, NULL, free);
  DevAssert(xn_ue_mapping != NULL);
  pthread_mutex_unlock(&xn_ue_mutex);

  pthread_mutex_lock(&xn_target_ue_mutex);
  DevAssert(xn_target_ue_mapping == NULL);
  xn_target_ue_mapping = hashtable_create(1319, NULL, free);
  DevAssert(xn_target_ue_mapping != NULL);
  pthread_mutex_unlock(&xn_target_ue_mutex);
}

uint32_t xn_alloc_ue_id(void)
{
  pthread_mutex_lock(&xn_ue_mutex);
  uint32_t id = xn_next_ue_id++;
  pthread_mutex_unlock(&xn_ue_mutex);
  return id;
}

bool xn_add_ue_data(uint32_t xn_ue_id, const xn_ue_data_t *data)
{
  pthread_mutex_lock(&xn_ue_mutex);
  DevAssert(xn_ue_mapping != NULL);
  uint32_t key = xn_ue_id;
  if (hashtable_is_key_exists(xn_ue_mapping, key) == HASH_TABLE_OK) {
    pthread_mutex_unlock(&xn_ue_mutex);
    return false;
  }
  bool ret = add_hashtable_data(xn_ue_mapping, key, data);
  pthread_mutex_unlock(&xn_ue_mutex);
  return ret;
}

bool xn_exists_ue_data(uint32_t xn_ue_id)
{
  pthread_mutex_lock(&xn_ue_mutex);
  DevAssert(xn_ue_mapping != NULL);
  hashtable_rc_t rc = hashtable_is_key_exists(xn_ue_mapping, xn_ue_id);
  pthread_mutex_unlock(&xn_ue_mutex);
  return rc == HASH_TABLE_OK;
}

xn_ue_data_t xn_get_ue_data(uint32_t xn_ue_id)
{
  pthread_mutex_lock(&xn_ue_mutex);
  DevAssert(xn_ue_mapping != NULL);
  xn_ue_data_t ued = *get_hashtable_data(xn_ue_mapping, xn_ue_id);
  pthread_mutex_unlock(&xn_ue_mutex);
  return ued;
}

bool xn_remove_ue_data(uint32_t xn_ue_id)
{
  pthread_mutex_lock(&xn_ue_mutex);
  DevAssert(xn_ue_mapping != NULL);
  hashtable_rc_t rc = hashtable_remove(xn_ue_mapping, xn_ue_id);
  pthread_mutex_unlock(&xn_ue_mutex);
  return rc == HASH_TABLE_OK;
}

/* ------------------------------------------------------------------ */
/* Target-side table                                                    */
/* ------------------------------------------------------------------ */

uint32_t xn_alloc_target_ue_id(void)
{
  pthread_mutex_lock(&xn_target_ue_mutex);
  uint32_t id = xn_next_target_ue_id++;
  pthread_mutex_unlock(&xn_target_ue_mutex);
  return id;
}

bool xn_add_target_ue_data(uint32_t t_xn_ue_id, const xn_target_ue_data_t *data)
{
  pthread_mutex_lock(&xn_target_ue_mutex);
  DevAssert(xn_target_ue_mapping != NULL);
  uint32_t key = t_xn_ue_id;
  if (hashtable_is_key_exists(xn_target_ue_mapping, key) == HASH_TABLE_OK) {
    pthread_mutex_unlock(&xn_target_ue_mutex);
    return false;
  }
  bool ret = target_add_hashtable_data(xn_target_ue_mapping, key, data);
  pthread_mutex_unlock(&xn_target_ue_mutex);
  return ret;
}

bool xn_exists_target_ue_data(uint32_t t_xnap_ue_id)
{
  pthread_mutex_lock(&xn_target_ue_mutex);
  DevAssert(xn_target_ue_mapping != NULL);
  hashtable_rc_t rc = hashtable_is_key_exists(xn_target_ue_mapping, t_xnap_ue_id);
  pthread_mutex_unlock(&xn_target_ue_mutex);
  return rc == HASH_TABLE_OK;
}

xn_target_ue_data_t xn_get_target_ue_data(uint32_t t_xnap_ue_id)
{
  pthread_mutex_lock(&xn_target_ue_mutex);
  DevAssert(xn_target_ue_mapping != NULL);
  void *data = NULL;
  hashtable_rc_t rc = hashtable_get(xn_target_ue_mapping, t_xnap_ue_id, &data);
  AssertFatal(rc == HASH_TABLE_OK && data != NULL,
              "t_xnap_ue_id %u not found in target UE mapping\n", t_xnap_ue_id);
  xn_target_ue_data_t result = *(xn_target_ue_data_t *)data;
  pthread_mutex_unlock(&xn_target_ue_mutex);
  return result;
}

bool xn_remove_target_ue_data(uint32_t t_xnap_ue_id)
{
  pthread_mutex_lock(&xn_target_ue_mutex);
  DevAssert(xn_target_ue_mapping != NULL);
  hashtable_rc_t rc = hashtable_remove(xn_target_ue_mapping, t_xnap_ue_id);
  pthread_mutex_unlock(&xn_target_ue_mutex);
  return rc == HASH_TABLE_OK;
}
