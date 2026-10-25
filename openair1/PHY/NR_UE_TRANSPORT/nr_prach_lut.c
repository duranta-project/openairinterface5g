/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_prach_lut.h"

#include "PHY/NR_TRANSPORT/nr_transport_common_proto.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define NR_PRACH_LUT_PREAMBLES 64

struct nr_prach_lut_s {
  nr_prach_lut_config_t config;
  c16_t *rows[NR_PRACH_LUT_MAX_KEYS];
  int32_t power[NR_PRACH_LUT_MAX_KEYS][NR_PRACH_LUT_PREAMBLES];
};

bool nr_prach_waveform_key_equal(const nr_prach_waveform_key_t *a, const nr_prach_waveform_key_t *b)
{
  return a != NULL && b != NULL && a->sequence_length == b->sequence_length && a->root_sequence_index == b->root_sequence_index
         && a->num_root_sequences == b->num_root_sequences && a->restricted_set == b->restricted_set && a->ncs == b->ncs
         && a->dftlen == b->dftlen && a->frequency_start == b->frequency_start && a->amplitude == b->amplitude;
}

bool nr_prach_lut_config_equal(const nr_prach_lut_config_t *a, const nr_prach_lut_config_t *b)
{
  if (a == NULL || b == NULL || a->num_keys > NR_PRACH_LUT_MAX_KEYS || a->num_keys != b->num_keys
      || a->num_preambles != b->num_preambles)
    return false;

  for (unsigned key = 0; key < a->num_keys; ++key)
    if (!nr_prach_waveform_key_equal(&a->keys[key], &b->keys[key]))
      return false;

  return true;
}

static bool config_is_valid(const nr_prach_lut_config_t *config)
{
  if (config == NULL || config->num_keys > NR_PRACH_LUT_MAX_KEYS || config->num_preambles != NR_PRACH_LUT_PREAMBLES)
    return false;

  for (unsigned key_index = 0; key_index < config->num_keys; ++key_index) {
    const nr_prach_waveform_key_t *key = &config->keys[key_index];
    const int zc_length = key->sequence_length == 0 ? 839 : 139;
    if ((key->sequence_length != 0 && key->sequence_length != 1) || key->root_sequence_index < 0
        || key->root_sequence_index >= zc_length - 1 || key->num_root_sequences <= 0
        || key->num_root_sequences > NR_PRACH_LUT_PREAMBLES || key->restricted_set < 0 || key->restricted_set > 1
        || (key->restricted_set == 1 && key->ncs == 0) || key->ncs < 0 || key->ncs >= zc_length || key->dftlen <= 0
        || key->frequency_start < 0 || key->frequency_start >= key->dftlen || key->amplitude < INT16_MIN
        || key->amplitude > INT16_MAX)
      return false;
  }

  return true;
}

static bool allocation_size(const nr_prach_waveform_key_t *key, size_t *bytes)
{
  if ((size_t)key->dftlen > SIZE_MAX / (NR_PRACH_LUT_PREAMBLES * sizeof(c16_t)))
    return false;

  *bytes = NR_PRACH_LUT_PREAMBLES * (size_t)key->dftlen * sizeof(c16_t);
  return true;
}

static int key_index(const nr_prach_lut_t *lut, const nr_prach_waveform_key_t *key)
{
  for (unsigned index = 0; index < lut->config.num_keys; ++index)
    if (nr_prach_waveform_key_equal(&lut->config.keys[index], key))
      return index;
  return -1;
}

nr_prach_lut_t *nr_prach_lut_build(const nr_prach_lut_config_t *config)
{
  if (!config_is_valid(config))
    return NULL;

  nr_prach_lut_t *lut = calloc(1, sizeof(*lut));
  if (lut == NULL)
    return NULL;

  lut->config = *config;
  for (unsigned key_index = 0; key_index < config->num_keys; ++key_index) {
    const nr_prach_waveform_key_t *key = &config->keys[key_index];
    size_t bytes;
    if (!allocation_size(key, &bytes)) {
      nr_prach_lut_destroy(lut);
      return NULL;
    }

    c16_t(*prach_lut)[key->dftlen] = aligned_alloc(64, bytes);
    if (prach_lut == NULL) {
      nr_prach_lut_destroy(lut);
      return NULL;
    }
    lut->rows[key_index] = (c16_t *)prach_lut;

    c16_t roots[64][839] __attribute__((aligned(64)));
    memset(roots, 0, sizeof(roots));
    compute_nr_prach_seq((uint8_t)key->sequence_length, (uint8_t)key->num_root_sequences, (uint8_t)key->root_sequence_index, roots);
    for (unsigned preamble = 0; preamble < NR_PRACH_LUT_PREAMBLES; ++preamble) {
      const int32_t power = nr_prach_generate_waveform(key, preamble, roots, prach_lut[preamble]);
      if (power < 0) {
        nr_prach_lut_destroy(lut);
        return NULL;
      }
      lut->power[key_index][preamble] = power;
    }
  }

  return lut;
}

void nr_prach_lut_destroy(nr_prach_lut_t *lut)
{
  if (lut == NULL)
    return;

  for (unsigned key = 0; key < lut->config.num_keys; ++key)
    free(lut->rows[key]);
  free(lut);
}

const nr_prach_lut_config_t *nr_prach_lut_config(const nr_prach_lut_t *lut)
{
  return lut == NULL ? NULL : &lut->config;
}

bool nr_prach_lut_copy(const nr_prach_lut_t *lut,
                       const nr_prach_waveform_key_t *key,
                       uint8_t preamble,
                       c16_t *out,
                       size_t capacity,
                       int32_t *power)
{
  if (lut == NULL || key == NULL || out == NULL || power == NULL || preamble >= NR_PRACH_LUT_PREAMBLES)
    return false;

  const int index = key_index(lut, key);
  if (index < 0)
    return false;

  const nr_prach_waveform_key_t *stored_key = &lut->config.keys[index];
  if (capacity < (size_t)stored_key->dftlen || lut->rows[index] == NULL)
    return false;

  const c16_t(*prach_lut)[stored_key->dftlen] = (const c16_t(*)[stored_key->dftlen])lut->rows[index];
  memcpy(out, prach_lut[preamble], (size_t)stored_key->dftlen * sizeof(*out));
  *power = lut->power[index][preamble];
  return true;
}
