/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "nr_up/nr_up_rlc_queue.h"
#include <pthread.h>
#include <stdlib.h>
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"

/* NR PDCP and RLC both use "big locks". In some cases a thread may do
 * lock(rlc) followed by lock(pdcp) (typically when running 'rx_sdu').
 * Another thread may first do lock(pdcp) and then lock(rlc) (typically
 * the GTP module calls 'nr_pdcp_data_req' that, in a previous implementation
 * was indirectly calling 'rlc_data_req' which does lock(rlc)).
 * To avoid the resulting deadlock it is enough to ensure that a call
 * to lock(pdcp) will never be followed by a call to lock(rlc). So,
 * here we chose to have a separate thread that deals with rlc_data_req,
 * out of the PDCP lock. Other solutions may be possible.
 * So instead of calling 'rlc_data_req' directly we have a queue and a
 * separate thread emptying it.
 */

static nr_up_budget_refresh_fn_t g_nr_up_budget_refresh_cb;

void nr_up_rlc_queue_set_budget_refresh_cb(nr_up_budget_refresh_fn_t cb)
{
  g_nr_up_budget_refresh_cb = cb;
}

typedef struct {
  protocol_ctxt_t ctxt_pP;
  srb_flag_t srb_flagP;
  rb_id_t rb_idP;
  int sdu_id;
  sdu_size_t sdu_sizeP;
  uint8_t *sdu_pP;
  ue_id_t cu_ue_id;
} nr_up_rlc_data_req_queue_item;

#define NR_UP_RLC_DATA_REQ_QUEUE_SIZE 10000

typedef struct {
  nr_up_rlc_data_req_queue_item q[NR_UP_RLC_DATA_REQ_QUEUE_SIZE];
  volatile int start;
  volatile int length;
  pthread_mutex_t m;
  pthread_cond_t c;
} nr_up_rlc_data_req_queue;

static nr_up_rlc_data_req_queue g_nr_up_rlc_queue;

static void *nr_up_rlc_data_req_thread(void *_)
{
  int i;

  UNUSED(_);
  pthread_setname_np(pthread_self(), "NR-UP RLC queue");
  while (1) {
    if (pthread_mutex_lock(&g_nr_up_rlc_queue.m) != 0) {
      abort();
    }
    while (g_nr_up_rlc_queue.length == 0) {
      if (pthread_cond_wait(&g_nr_up_rlc_queue.c, &g_nr_up_rlc_queue.m) != 0) {
        abort();
      }
    }
    i = g_nr_up_rlc_queue.start;
    if (pthread_mutex_unlock(&g_nr_up_rlc_queue.m) != 0) {
      abort();
    }

    int tx_space = nr_rlc_data_req(&g_nr_up_rlc_queue.q[i].ctxt_pP,
                                   g_nr_up_rlc_queue.q[i].srb_flagP,
                                   g_nr_up_rlc_queue.q[i].rb_idP,
                                   g_nr_up_rlc_queue.q[i].sdu_id,
                                   g_nr_up_rlc_queue.q[i].sdu_sizeP,
                                   g_nr_up_rlc_queue.q[i].sdu_pP);

    if (!g_nr_up_rlc_queue.q[i].srb_flagP && g_nr_up_rlc_queue.q[i].cu_ue_id != NR_UP_CU_UE_ID_NONE && g_nr_up_budget_refresh_cb != NULL) {
      g_nr_up_budget_refresh_cb(g_nr_up_rlc_queue.q[i].cu_ue_id, g_nr_up_rlc_queue.q[i].rb_idP, tx_space);
    }

    if (pthread_mutex_lock(&g_nr_up_rlc_queue.m) != 0) {
      abort();
    }

    g_nr_up_rlc_queue.length--;
    g_nr_up_rlc_queue.start = (g_nr_up_rlc_queue.start + 1) % NR_UP_RLC_DATA_REQ_QUEUE_SIZE;

    if (pthread_cond_signal(&g_nr_up_rlc_queue.c) != 0) {
      abort();
    }
    if (pthread_mutex_unlock(&g_nr_up_rlc_queue.m) != 0) {
      abort();
    }
  }
}

void nr_up_rlc_queue_init(void)
{
  pthread_t t;

  pthread_mutex_init(&g_nr_up_rlc_queue.m, NULL);
  pthread_cond_init(&g_nr_up_rlc_queue.c, NULL);

  if (pthread_create(&t, NULL, nr_up_rlc_data_req_thread, NULL) != 0) {
    LOG_E(NR_UP, "%s: fatal\n", __func__);
    exit(1);
  }
}

void nr_up_enqueue_rlc_data_req(const protocol_ctxt_t *ctxt_pP,
                                srb_flag_t srb_flagP,
                                rb_id_t rb_idP,
                                int sdu_id,
                                sdu_size_t sdu_sizeP,
                                uint8_t *sdu_pP,
                                ue_id_t cu_ue_id)
{
  int i;
  int logged = 0;

  if (!srb_flagP && cu_ue_id != NR_UP_CU_UE_ID_NONE) {
    DevAssert(cu_ue_id != 0);
  }

  if (pthread_mutex_lock(&g_nr_up_rlc_queue.m) != 0) {
    abort();
  }
  while (g_nr_up_rlc_queue.length == NR_UP_RLC_DATA_REQ_QUEUE_SIZE) {
    if (!logged) {
      logged = 1;
      LOG_W(NR_UP, "%s: rlc_data_req queue is full\n", __func__);
    }
    if (pthread_cond_wait(&g_nr_up_rlc_queue.c, &g_nr_up_rlc_queue.m) != 0) {
      abort();
    }
  }

  i = (g_nr_up_rlc_queue.start + g_nr_up_rlc_queue.length) % NR_UP_RLC_DATA_REQ_QUEUE_SIZE;
  g_nr_up_rlc_queue.length++;

  g_nr_up_rlc_queue.q[i].ctxt_pP = *ctxt_pP;
  g_nr_up_rlc_queue.q[i].srb_flagP = srb_flagP;
  g_nr_up_rlc_queue.q[i].rb_idP = rb_idP;
  g_nr_up_rlc_queue.q[i].sdu_id = sdu_id;
  g_nr_up_rlc_queue.q[i].sdu_sizeP = sdu_sizeP;
  g_nr_up_rlc_queue.q[i].sdu_pP = sdu_pP;
  g_nr_up_rlc_queue.q[i].cu_ue_id = cu_ue_id;

  if (pthread_cond_signal(&g_nr_up_rlc_queue.c) != 0) {
    abort();
  }
  if (pthread_mutex_unlock(&g_nr_up_rlc_queue.m) != 0) {
    abort();
  }
}
