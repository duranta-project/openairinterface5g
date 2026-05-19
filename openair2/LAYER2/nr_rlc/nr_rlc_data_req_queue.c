/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "nr_rlc_data_req_queue.h"
#include <pthread.h>
#include <stdlib.h>
#include "assertions.h"
#include "common/utils/LOG/log.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "openair2/LAYER2/nr_up/nr_up.h"

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

typedef struct {
  protocol_ctxt_t ctxt_pP;
  srb_flag_t srb_flagP;
  rb_id_t rb_idP;
  int sdu_id;
  sdu_size_t sdu_sizeP;
  uint8_t *sdu_pP;
  ue_id_t pdcp_ue_id;
  bool report_polling;
} nr_rlc_data_req_queue_item;

#define NR_RLC_DATA_REQ_QUEUE_SIZE 10000

typedef struct {
  nr_rlc_data_req_queue_item q[NR_RLC_DATA_REQ_QUEUE_SIZE];
  volatile int start;
  volatile int length;
  pthread_mutex_t m;
  pthread_cond_t c;
} nr_rlc_data_req_queue_t;

static nr_rlc_data_req_queue_t g_nr_rlc_data_req_queue;

static void *nr_rlc_data_req_queue_thread(void *_)
{
  int i;

  UNUSED(_);
  pthread_setname_np(pthread_self(), "NR RLC data_req");
  while (1) {
    if (pthread_mutex_lock(&g_nr_rlc_data_req_queue.m) != 0) {
      abort();
    }
    while (g_nr_rlc_data_req_queue.length == 0) {
      if (pthread_cond_wait(&g_nr_rlc_data_req_queue.c, &g_nr_rlc_data_req_queue.m) != 0) {
        abort();
      }
    }
    i = g_nr_rlc_data_req_queue.start;
    if (pthread_mutex_unlock(&g_nr_rlc_data_req_queue.m) != 0) {
      abort();
    }

    nr_rlc_data_req_queue_item *item = &g_nr_rlc_data_req_queue.q[i];
    /* PDU present: push. NULL PDU: DBS refresh only. */
    int desired_buffer_size = nr_rlc_data_req(&item->ctxt_pP, item->srb_flagP, item->rb_idP, item->sdu_id, item->sdu_sizeP, item->sdu_pP);
    LOG_D(RLC,
          "UE %ld %s %ld: desired_buffer_size=%d\n",
          item->ctxt_pP.rntiMaybeUEid,
          item->srb_flagP ? "SRB" : "DRB",
          item->rb_idP,
          desired_buffer_size);
    if (item->report_polling && !item->srb_flagP && desired_buffer_size >= 0) {
      nr_up_if_t *iface = get_nr_up_if();
      DevAssert(iface && iface->up_ddds_indication);
      LOG_D(RLC, "Send DDDS indication to pdcp_ue_id %ld\n", item->pdcp_ue_id);
      iface->up_ddds_indication(item->pdcp_ue_id, item->rb_idP, desired_buffer_size);
    }

    if (pthread_mutex_lock(&g_nr_rlc_data_req_queue.m) != 0) {
      abort();
    }

    g_nr_rlc_data_req_queue.length--;
    g_nr_rlc_data_req_queue.start = (g_nr_rlc_data_req_queue.start + 1) % NR_RLC_DATA_REQ_QUEUE_SIZE;

    if (pthread_cond_signal(&g_nr_rlc_data_req_queue.c) != 0) {
      abort();
    }
    if (pthread_mutex_unlock(&g_nr_rlc_data_req_queue.m) != 0) {
      abort();
    }
  }
}

void nr_rlc_data_req_queue_init(void)
{
  pthread_t t;

  pthread_mutex_init(&g_nr_rlc_data_req_queue.m, NULL);
  pthread_cond_init(&g_nr_rlc_data_req_queue.c, NULL);

  if (pthread_create(&t, NULL, nr_rlc_data_req_queue_thread, NULL) != 0) {
    LOG_E(RLC, "%s: fatal\n", __func__);
    exit(1);
  }
}

void nr_rlc_enqueue_data_req(const protocol_ctxt_t *ctxt_pP,
                             srb_flag_t srb_flagP,
                             rb_id_t rb_idP,
                             int sdu_id,
                             sdu_size_t sdu_sizeP,
                             uint8_t *sdu_pP,
                             ue_id_t pdcp_ue_id,
                             bool report_polling)
{
  int i;
  int logged = 0;

  if (pthread_mutex_lock(&g_nr_rlc_data_req_queue.m) != 0) {
    abort();
  }
  while (g_nr_rlc_data_req_queue.length == NR_RLC_DATA_REQ_QUEUE_SIZE) {
    if (!logged) {
      logged = 1;
      LOG_W(RLC, "%s: rlc_data_req queue is full\n", __func__);
    }
    if (pthread_cond_wait(&g_nr_rlc_data_req_queue.c, &g_nr_rlc_data_req_queue.m) != 0) {
      abort();
    }
  }

  i = (g_nr_rlc_data_req_queue.start + g_nr_rlc_data_req_queue.length) % NR_RLC_DATA_REQ_QUEUE_SIZE;
  g_nr_rlc_data_req_queue.length++;

  g_nr_rlc_data_req_queue.q[i].ctxt_pP = *ctxt_pP;
  g_nr_rlc_data_req_queue.q[i].srb_flagP = srb_flagP;
  g_nr_rlc_data_req_queue.q[i].rb_idP = rb_idP;
  g_nr_rlc_data_req_queue.q[i].sdu_id = sdu_id;
  g_nr_rlc_data_req_queue.q[i].sdu_sizeP = sdu_sizeP;
  g_nr_rlc_data_req_queue.q[i].sdu_pP = sdu_pP;
  g_nr_rlc_data_req_queue.q[i].pdcp_ue_id = pdcp_ue_id;
  g_nr_rlc_data_req_queue.q[i].report_polling = report_polling;

  if (pthread_cond_signal(&g_nr_rlc_data_req_queue.c) != 0) {
    abort();
  }
  if (pthread_mutex_unlock(&g_nr_rlc_data_req_queue.m) != 0) {
    abort();
  }
}
