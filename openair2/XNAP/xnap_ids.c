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

/* Source-side table: s_ng_node_ue_xnap_id → xn_ue_data_t */
static hash_table_t *xn_ue_mapping;
static pthread_mutex_t xn_ue_mutex = PTHREAD_MUTEX_INITIALIZER;
static uint32_t xn_next_ue_id = 1;

void xn_init_ue_data(void)
{
  pthread_mutex_lock(&xn_ue_mutex);
  DevAssert(xn_ue_mapping == NULL);
  xn_ue_mapping = hashtable_create(1319, NULL, free);
  DevAssert(xn_ue_mapping != NULL);
  pthread_mutex_unlock(&xn_ue_mutex);
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
