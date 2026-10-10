/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_rlc_sdu.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "LOG/log.h"
#include "common/utils/assertions.h"

nr_rlc_sdu_segment_t *nr_rlc_new_sdu(
    char *buffer, int size,
    int upper_layer_id)
{
  /* allocate sdu header and data together */
  nr_rlc_sdu_t *sdu = malloc(sizeof(nr_rlc_sdu_t) + size);
  nr_rlc_sdu_segment_t *ret = calloc(1, sizeof(nr_rlc_sdu_segment_t));

  DevAssert(sdu != NULL);
  DevAssert(ret != NULL);

  /* only memset the header */
  memset(sdu, 0 , sizeof(*sdu));

  sdu->ref_count      = 1;
  sdu->sn             = -1;                 /* set later */
  sdu->upper_layer_id = upper_layer_id;
  sdu->data           = (char*)(sdu + 1);
  memcpy(sdu->data, buffer, size);
  sdu->size           = size;
  sdu->retx_count     = -1;

  ret->sdu      = sdu;
  ret->size     = size;
  ret->so       = 0;
  ret->is_first = 1;
  ret->is_last  = 1;

  return ret;
}

/* nr_rlc_defer_free_begin()/end(): segments queued for free(), chained through their
 * now unused next field; seg_and_sdu ones also take their SDU, whose last segment they
 * were.  Thread-local, so no locking. */
static __thread bool defer_free;
static __thread nr_rlc_sdu_segment_t *deferred_seg;
static __thread nr_rlc_sdu_segment_t *deferred_seg_and_sdu;

void nr_rlc_defer_free_begin(void)
{
  defer_free = true;
}

void nr_rlc_defer_free_end(void)
{
  defer_free = false;
  while (deferred_seg_and_sdu != NULL) {
    nr_rlc_sdu_segment_t *s = deferred_seg_and_sdu;
    deferred_seg_and_sdu = s->next;
    free(s->sdu);
    free(s);
  }
  while (deferred_seg != NULL) {
    nr_rlc_sdu_segment_t *s = deferred_seg;
    deferred_seg = s->next;
    free(s);
  }
}

int nr_rlc_free_sdu_segment(nr_rlc_sdu_segment_t *sdu)
{
  int ret = 0;

  sdu->sdu->free_count++;
  if (sdu->sdu->free_count == sdu->sdu->ref_count)
    ret = 1;

  if (defer_free) {
    nr_rlc_sdu_segment_t **l = ret ? &deferred_seg_and_sdu : &deferred_seg;
    sdu->next = *l;
    *l = sdu;
    return ret;
  }

  if (ret)
    free(sdu->sdu);
  free(sdu);

  return ret;
}

void nr_rlc_sdu_segment_list_append(nr_rlc_sdu_segment_t **list,
                                    nr_rlc_sdu_segment_t **end,
                                    nr_rlc_sdu_segment_t *sdu)
{
  if (*list == NULL) {
    *list = sdu;
    *end = sdu;
    return;
  }

  (*end)->next = sdu;
  *end = sdu;
}

void nr_rlc_free_sdu_segment_list(nr_rlc_sdu_segment_t *l)
{
  nr_rlc_sdu_segment_t *cur;

  while (l != NULL) {
    cur = l;
    l = l->next;
    nr_rlc_free_sdu_segment(cur);
  }
}
