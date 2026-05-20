/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_rlc_entity_tm.h"

#include <stdlib.h>
#include <string.h>

#include "nr_rlc_pdu.h"
#include "common/utils/time_stat.h"
#include "LOG/log.h"
#include "common/platform_types.h"

/*************************************************************************/
/* PDU RX functions                                                      */
/*************************************************************************/

void nr_rlc_entity_tm_recv_pdu(nr_rlc_entity_t *_entity,
                               char *buffer, int size)
{
  nr_rlc_entity_tm_t *entity = (nr_rlc_entity_tm_t *)_entity;

  entity->common.stats.rxpdu_pkts++;
  entity->common.stats.rxpdu_bytes += size;

  entity->common.deliver_sdu(entity->common.deliver_sdu_data,
                             (nr_rlc_entity_t *)entity,
                             buffer, size);

  entity->common.stats.txsdu_pkts++;
  entity->common.stats.txsdu_bytes += size;
}

/*************************************************************************/
/* TX functions                                                          */
/*************************************************************************/

static int generate_tx_pdu(nr_rlc_entity_tm_t *entity, char *buffer, int size)
{
  nr_rlc_sdu_segment_t *sdu;
  int ret;

  if (entity->tx_list == NULL)
    return 0;

  sdu = entity->tx_list;

  /* not enough room? do nothing */
  if (sdu->size > size)
    return 0;

  entity->tx_list = entity->tx_list->next;
  if (entity->tx_list == NULL)
    entity->tx_end = NULL;

  ret = sdu->size;

  memcpy(buffer, sdu->sdu->data, sdu->size);

  entity->tx_size -= sdu->size;
  entity->common.stats.txpdu_pkts++;
  entity->common.stats.txpdu_bytes += size;

  /* No need to 'zero' time-of-arrival; 
  Segmented packets do need to be duplicated in time-sensitive use cases */
  if (entity->common.avg_time_is_on) {
    uint64_t time_now = time_average_now();
    uint64_t waited_time = time_now - sdu->sdu->time_of_arrival;
    time_average_add(entity->common.txsdu_avg_time_to_tx, time_now, waited_time);
  }

  /* update buffer status */
  entity->common.bstatus.tx_size -= sdu->size;

  nr_rlc_free_sdu_segment(sdu);

  return ret;
}

nr_rlc_entity_buffer_status_t nr_rlc_entity_tm_buffer_status(nr_rlc_entity_t *_entity, int maxsize)
{
  UNUSED(maxsize);
  nr_rlc_entity_tm_t *entity = (nr_rlc_entity_tm_t *)_entity;
  nr_rlc_entity_buffer_status_t ret;

  ret.status_size = 0;
  ret.tx_size = entity->common.bstatus.tx_size;
  ret.retx_size = 0;

  return ret;
}

int nr_rlc_entity_tm_generate_pdu(nr_rlc_entity_t *_entity,
                                  char *buffer, int size)
{
  nr_rlc_entity_tm_t *entity = (nr_rlc_entity_tm_t *)_entity;

  return generate_tx_pdu(entity, buffer, size);
}

/*************************************************************************/
/* SDU RX functions                                                      */
/*************************************************************************/

void nr_rlc_entity_tm_recv_sdu(nr_rlc_entity_t *_entity,
                               char *buffer, int size,
                               int sdu_id)
{
  nr_rlc_entity_tm_t *entity = (nr_rlc_entity_tm_t *)_entity;
  nr_rlc_sdu_segment_t *sdu;

  entity->common.stats.rxsdu_pkts++;
  entity->common.stats.rxsdu_bytes += size;

  if (size > NR_SDU_MAX) {
    LOG_E(RLC, "%s:%d:%s: fatal: SDU size too big (%d bytes)\n",
          __FILE__, __LINE__, __FUNCTION__, size);
    exit(1);
  }

  /* log SDUs rejected, at most once per second */
  if (entity->sdu_rejected != 0
      && entity->t_current > entity->t_log_buffer_full + 1000) {
    LOG_E(RLC, "%d SDU rejected, SDU buffer full\n", entity->sdu_rejected);
    entity->sdu_rejected = 0;
    entity->t_log_buffer_full = entity->t_current;
  }

  if (entity->common.do_drop && entity->tx_size + size > entity->tx_maxsize) {
    entity->sdu_rejected++;

    entity->common.stats.rxsdu_dd_pkts++;
    entity->common.stats.rxsdu_dd_bytes += size;

    return;
  }

  entity->tx_size += size;

  sdu = nr_rlc_new_sdu(buffer, size, sdu_id);

  nr_rlc_sdu_segment_list_append(&entity->tx_list, &entity->tx_end, sdu);

  /* update buffer status */
  entity->common.bstatus.tx_size += sdu->size;

  if (entity->common.avg_time_is_on)
    sdu->sdu->time_of_arrival = time_average_now();
}

/*************************************************************************/
/* time/timers                                                           */
/*************************************************************************/

void nr_rlc_entity_tm_set_time(nr_rlc_entity_t *_entity, uint64_t now)
{
  nr_rlc_entity_tm_t *entity = (nr_rlc_entity_tm_t *)_entity;

  entity->t_current = now;
}

/*************************************************************************/
/* discard/re-establishment/delete                                       */
/*************************************************************************/

void nr_rlc_entity_tm_discard_sdu(nr_rlc_entity_t *_entity, int sdu_id)
{
  UNUSED(_entity);
  UNUSED(sdu_id);
}

static void clear_entity(nr_rlc_entity_tm_t *entity)
{
  entity->t_current = 0;

  entity->t_log_buffer_full = 0;
  entity->sdu_rejected      = 0;

  nr_rlc_free_sdu_segment_list(entity->tx_list);

  entity->tx_list         = NULL;
  entity->tx_end          = NULL;
  entity->tx_size         = 0;

  entity->common.bstatus.tx_size = 0;
}

void nr_rlc_entity_tm_reestablishment(nr_rlc_entity_t *_entity)
{
  nr_rlc_entity_tm_t *entity = (nr_rlc_entity_tm_t *)_entity;
  clear_entity(entity);
}

void nr_rlc_entity_tm_delete(nr_rlc_entity_t *_entity)
{
  nr_rlc_entity_tm_t *entity = (nr_rlc_entity_tm_t *)_entity;
  clear_entity(entity);
  time_average_free(entity->common.txsdu_avg_time_to_tx);
  free(entity);
}

int nr_rlc_entity_tm_available_tx_space(nr_rlc_entity_t *_entity)
{
  nr_rlc_entity_tm_t *entity = (nr_rlc_entity_tm_t *)_entity;
  return entity->tx_maxsize - entity->tx_size;
}

int nr_rlc_entity_tm_tx_list_occupancy(nr_rlc_entity_t *_entity)
{
  nr_rlc_entity_tm_t *entity = (nr_rlc_entity_tm_t *)_entity;
  return entity->tx_size;
}
