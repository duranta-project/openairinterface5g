/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NR_RLC_SDU_H_
#define _NR_RLC_SDU_H_

#include <stdint.h>

typedef struct nr_rlc_sdu_t {
  int sn;
  int upper_layer_id;
  char *data;
  int size;
  int retx_count;

  int ref_count;      /* incremented each time the SDU is segmented */
  int free_count;     /* incremented each time a segment is freed
                       * when it equals ref_count we can free the SDU
                       * completely
                       */

  /* for statistics, will be set to 0 after SDU (or first part of it) has
   * been serialized for MAC for the first time so that only the first
   * transmission is used for statistics
   */
  uint64_t time_of_arrival;  /* unit microsecond */
} nr_rlc_sdu_t;

typedef struct nr_rlc_sdu_segment_t {
  nr_rlc_sdu_t *sdu;
  int size;
  int so;
  int is_first;
  int is_last;
  struct nr_rlc_sdu_segment_t *next;
} nr_rlc_sdu_segment_t;

nr_rlc_sdu_segment_t *nr_rlc_new_sdu(
    char *buffer, int size,
    int upper_layer_id);
/* return 1 if the SDU has been freed too, 0 if not (more segments to free) */
int nr_rlc_free_sdu_segment(nr_rlc_sdu_segment_t *sdu);
void nr_rlc_sdu_segment_list_append(nr_rlc_sdu_segment_t **list,
                                    nr_rlc_sdu_segment_t **end,
                                    nr_rlc_sdu_segment_t *sdu);
void nr_rlc_free_sdu_segment_list(nr_rlc_sdu_segment_t *l);

/* Between begin and end, nr_rlc_free_sdu_segment() on this thread still does the
 * reference counting (so its return value is unchanged) but queues the free() calls,
 * which end then makes.  For callers that free under the RLC manager lock: one RLC AM
 * STATUS PDU can acknowledge thousands of SDUs, two free() each, and at ~700 Mbps DL that
 * held the lock up to 2.4 ms while the MAC scheduler waited for it under sched_lock. */
void nr_rlc_defer_free_begin(void);
void nr_rlc_defer_free_end(void);

#endif /* _NR_RLC_SDU_H_ */
