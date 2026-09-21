/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_prach_lut.h"

#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"

#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define NR_PRACH_LUT_MAX_PREAMBLES 64
#define NR_PRACH_LUT_ALIGNMENT 64

typedef struct {
  uint64_t generation;
  unsigned num_keys;
  unsigned num_preambles;
  nr_prach_waveform_key_t keys[NR_PRACH_LUT_MAX_KEYS];
  c16_t *samples[NR_PRACH_LUT_MAX_KEYS][NR_PRACH_LUT_MAX_PREAMBLES];
  int32_t power[NR_PRACH_LUT_MAX_KEYS][NR_PRACH_LUT_MAX_PREAMBLES];
} nr_prach_lut_table_t;

struct nr_prach_lut_s {
  pthread_t worker;
  pthread_mutex_t config_mutex;
  pthread_cond_t config_cond;
  nr_prach_lut_config_t configured;
  nr_prach_lut_config_t pending;
  uint64_t pending_generation;
  bool configured_valid;
  bool pending_valid;
  atomic_bool stop;
  atomic_uint_fast64_t generation;
  _Atomic(nr_prach_lut_table_t *) published;
  atomic_uint_fast64_t readers;
};

bool nr_prach_waveform_key_equal(const nr_prach_waveform_key_t *a, const nr_prach_waveform_key_t *b)
{
  return a != NULL && b != NULL && a->sequence_length == b->sequence_length && a->root_sequence_index == b->root_sequence_index
         && a->num_root_sequences == b->num_root_sequences && a->restricted_set == b->restricted_set && a->ncs == b->ncs
         && a->dftlen == b->dftlen && a->frequency_start == b->frequency_start && a->amplitude == b->amplitude;
}

static bool config_is_valid(const nr_prach_lut_config_t *config)
{
  if (config == NULL || config->num_keys > NR_PRACH_LUT_MAX_KEYS || config->num_preambles > NR_PRACH_LUT_MAX_PREAMBLES)
    return false;

  for (unsigned i = 0; i < config->num_keys; ++i) {
    const nr_prach_waveform_key_t *key = &config->keys[i];
    const int zc_length = key->sequence_length == 0 ? 839 : 139;
    if ((key->sequence_length != 0 && key->sequence_length != 1) || key->root_sequence_index < 0
        || key->root_sequence_index >= zc_length - 1 || key->num_root_sequences <= 0
        || key->num_root_sequences > NR_PRACH_LUT_MAX_PREAMBLES || key->restricted_set < 0 || key->restricted_set > 1
        || (key->restricted_set == 1 && key->ncs == 0) || key->ncs < 0 || key->ncs >= zc_length || key->dftlen <= 0
        || key->frequency_start < 0 || key->frequency_start >= key->dftlen || key->amplitude < INT16_MIN
        || key->amplitude > INT16_MAX)
      return false;
  }

  return true;
}

static void copy_or_disable_config(nr_prach_lut_config_t *destination, const nr_prach_lut_config_t *source)
{
  if (config_is_valid(source))
    *destination = *source;
  else
    memset(destination, 0, sizeof(*destination));
}

static bool config_equal(const nr_prach_lut_config_t *a, const nr_prach_lut_config_t *b)
{
  if (a->num_keys != b->num_keys || a->num_preambles != b->num_preambles)
    return false;

  for (unsigned i = 0; i < a->num_keys; ++i)
    if (!nr_prach_waveform_key_equal(&a->keys[i], &b->keys[i]))
      return false;

  return true;
}

static bool generation_is_obsolete(const nr_prach_lut_t *lut, uint64_t generation)
{
  return atomic_load_explicit(&lut->stop, memory_order_seq_cst)
         || atomic_load_explicit(&lut->generation, memory_order_seq_cst) != generation;
}

static int table_key_index(const nr_prach_lut_table_t *table, const nr_prach_waveform_key_t *key)
{
  for (unsigned i = 0; i < table->num_keys; ++i)
    if (nr_prach_waveform_key_equal(&table->keys[i], key))
      return i;
  return -1;
}

static void table_free(nr_prach_lut_table_t *table)
{
  if (table == NULL)
    return;

  for (unsigned key = 0; key < table->num_keys; ++key)
    for (unsigned preamble = 0; preamble < table->num_preambles; ++preamble)
      free(table->samples[key][preamble]);

  free(table);
}

static c16_t *allocate_row(int dftlen)
{
  if (dftlen <= 0 || (size_t)dftlen > SIZE_MAX / sizeof(c16_t))
    return NULL;

  c16_t *row = NULL;
  const size_t bytes = (size_t)dftlen * sizeof(*row);
  if (posix_memalign((void **)&row, NR_PRACH_LUT_ALIGNMENT, bytes) != 0)
    return NULL;

  return row;
}

static nr_prach_lut_table_t *build_table(nr_prach_lut_t *lut, const nr_prach_lut_config_t *config, uint64_t generation)
{
  if (generation_is_obsolete(lut, generation))
    return NULL;

  nr_prach_lut_table_t *table = calloc(1, sizeof(*table));
  if (table == NULL)
    return NULL;

  table->generation = generation;
  table->num_preambles = config->num_preambles;
  for (unsigned source_key = 0; source_key < config->num_keys; ++source_key) {
    if (table_key_index(table, &config->keys[source_key]) < 0)
      table->keys[table->num_keys++] = config->keys[source_key];
  }

  for (unsigned key_index = 0; key_index < table->num_keys; ++key_index) {
    if (generation_is_obsolete(lut, generation)) {
      table_free(table);
      return NULL;
    }

    const nr_prach_waveform_key_t *key = &table->keys[key_index];
    c16_t roots[64][839] __attribute__((aligned(NR_PRACH_LUT_ALIGNMENT)));
    memset(roots, 0, sizeof(roots));
    compute_nr_prach_seq((uint8_t)key->sequence_length, (uint8_t)key->num_root_sequences, (uint8_t)key->root_sequence_index, roots);

    for (unsigned preamble = 0; preamble < table->num_preambles; ++preamble) {
      if (generation_is_obsolete(lut, generation)) {
        table_free(table);
        return NULL;
      }

      c16_t *row = allocate_row(key->dftlen);
      if (row == NULL) {
        table_free(table);
        return NULL;
      }

      const int32_t power = nr_prach_generate_waveform(key, preamble, roots, row);
      if (power < 0) {
        free(row);
        table_free(table);
        return NULL;
      }
      table->power[key_index][preamble] = power;
      table->samples[key_index][preamble] = row;

      if (generation_is_obsolete(lut, generation)) {
        table_free(table);
        return NULL;
      }
    }
  }

  return table;
}

static void wait_for_readers(nr_prach_lut_t *lut)
{
  while (atomic_load_explicit(&lut->readers, memory_order_seq_cst) != 0)
    usleep(1000);
}

static void *nr_prach_lut_worker(void *opaque)
{
  nr_prach_lut_t *lut = opaque;

  while (true) {
    nr_prach_lut_config_t config;
    uint64_t generation;

    pthread_mutex_lock(&lut->config_mutex);
    while (!lut->pending_valid && !atomic_load_explicit(&lut->stop, memory_order_seq_cst))
      pthread_cond_wait(&lut->config_cond, &lut->config_mutex);

    if (atomic_load_explicit(&lut->stop, memory_order_seq_cst)) {
      pthread_mutex_unlock(&lut->config_mutex);
      break;
    }

    config = lut->pending;
    generation = lut->pending_generation;
    lut->pending_valid = false;
    pthread_mutex_unlock(&lut->config_mutex);

    nr_prach_lut_table_t *candidate = build_table(lut, &config, generation);
    if (candidate == NULL)
      continue;

    nr_prach_lut_table_t *retired = NULL;
    pthread_mutex_lock(&lut->config_mutex);
    if (!atomic_load_explicit(&lut->stop, memory_order_seq_cst)
        && atomic_load_explicit(&lut->generation, memory_order_seq_cst) == candidate->generation) {
      retired = atomic_exchange_explicit(&lut->published, candidate, memory_order_seq_cst);
      candidate = NULL;
    }
    pthread_mutex_unlock(&lut->config_mutex);

    table_free(candidate);
    if (retired != NULL) {
      wait_for_readers(lut);
      table_free(retired);
    }
  }

  return NULL;
}

nr_prach_lut_t *nr_prach_lut_create(void)
{
  nr_prach_lut_t *lut = calloc(1, sizeof(*lut));
  if (lut == NULL)
    return NULL;

  if (pthread_mutex_init(&lut->config_mutex, NULL) != 0) {
    free(lut);
    return NULL;
  }

  if (pthread_cond_init(&lut->config_cond, NULL) != 0) {
    pthread_mutex_destroy(&lut->config_mutex);
    free(lut);
    return NULL;
  }

  atomic_init(&lut->stop, false);
  atomic_init(&lut->generation, 0);
  atomic_init(&lut->published, NULL);
  atomic_init(&lut->readers, 0);
  if (!atomic_is_lock_free(&lut->published) || !atomic_is_lock_free(&lut->readers) || !atomic_is_lock_free(&lut->generation)) {
    pthread_cond_destroy(&lut->config_cond);
    pthread_mutex_destroy(&lut->config_mutex);
    free(lut);
    return NULL;
  }

  pthread_attr_t attr;
  int error = pthread_attr_init(&attr);
  if (error == 0) {
    error = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (error == 0)
      error = pthread_attr_setschedpolicy(&attr, SCHED_OTHER);
    if (error == 0)
      error = pthread_create(&lut->worker, &attr, nr_prach_lut_worker, lut);
    (void)pthread_attr_destroy(&attr);
  }

  if (error != 0) {
    pthread_cond_destroy(&lut->config_cond);
    pthread_mutex_destroy(&lut->config_mutex);
    free(lut);
    return NULL;
  }

  return lut;
}

void nr_prach_lut_configure(nr_prach_lut_t *lut, const nr_prach_lut_config_t *config)
{
  if (lut == NULL)
    return;

  nr_prach_lut_config_t copied;
  copy_or_disable_config(&copied, config);

  pthread_mutex_lock(&lut->config_mutex);
  if (lut->configured_valid && config_equal(&lut->configured, &copied)) {
    pthread_mutex_unlock(&lut->config_mutex);
    return;
  }

  lut->configured = copied;
  lut->configured_valid = true;
  lut->pending = copied;
  lut->pending_generation = atomic_fetch_add_explicit(&lut->generation, 1, memory_order_seq_cst) + 1;
  lut->pending_valid = true;
  pthread_cond_signal(&lut->config_cond);
  pthread_mutex_unlock(&lut->config_mutex);
}

bool nr_prach_lut_acquire(nr_prach_lut_t *lut,
                          const nr_prach_waveform_key_t *key,
                          uint8_t preamble_index,
                          nr_prach_lut_view_t *view)
{
  if (lut == NULL || key == NULL || view == NULL)
    return false;

  atomic_fetch_add_explicit(&lut->readers, 1, memory_order_seq_cst);
  nr_prach_lut_table_t *table = atomic_load_explicit(&lut->published, memory_order_seq_cst);
  const uint64_t generation = atomic_load_explicit(&lut->generation, memory_order_seq_cst);

  if (table != NULL && table->generation == generation && preamble_index < table->num_preambles) {
    const int key_index = table_key_index(table, key);
    if (key_index >= 0) {
      view->samples = table->samples[key_index][preamble_index];
      view->power = table->power[key_index][preamble_index];
      return true;
    }
  }

  atomic_fetch_sub_explicit(&lut->readers, 1, memory_order_seq_cst);
  return false;
}

void nr_prach_lut_release(nr_prach_lut_t *lut)
{
  if (lut != NULL)
    atomic_fetch_sub_explicit(&lut->readers, 1, memory_order_seq_cst);
}

void nr_prach_lut_destroy(nr_prach_lut_t *lut)
{
  if (lut == NULL)
    return;

  pthread_mutex_lock(&lut->config_mutex);
  atomic_store_explicit(&lut->stop, true, memory_order_seq_cst);
  atomic_fetch_add_explicit(&lut->generation, 1, memory_order_seq_cst);
  lut->pending_valid = false;
  pthread_cond_broadcast(&lut->config_cond);
  pthread_mutex_unlock(&lut->config_mutex);

  pthread_join(lut->worker, NULL);
  nr_prach_lut_table_t *retired = atomic_exchange_explicit(&lut->published, NULL, memory_order_seq_cst);
  wait_for_readers(lut);
  table_free(retired);

  pthread_cond_destroy(&lut->config_cond);
  pthread_mutex_destroy(&lut->config_mutex);
  free(lut);
}
