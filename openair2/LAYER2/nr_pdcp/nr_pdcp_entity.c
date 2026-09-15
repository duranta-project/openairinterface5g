/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pdcp_entity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nr_pdcp_security_nea2.h"
#include "nr_pdcp_security_nea1.h"
#include "nr_pdcp_integrity_nia2.h"
#include "nr_pdcp_integrity_nia1.h"
#include "nr_pdcp_sdu.h"

#include "LOG/log.h"

/**
 * @brief returns the maximum PDCP PDU size
 *        which corresponds to data PDU for DRBs with 18 bits PDCP SN
 *        and integrity enabled
*/
int nr_max_pdcp_pdu_size(sdu_size_t sdu_size)
{
  return (sdu_size + LONG_PDCP_HEADER_SIZE + PDCP_INTEGRITY_SIZE);
}

static void nr_pdcp_entity_recv_pdu(nr_pdcp_entity_t *entity,
                                    char *_buffer, int size)
{
  unsigned char    *buffer = (unsigned char *)_buffer;
  nr_pdcp_sdu_t    *sdu;
  int              rcvd_sn;
  uint32_t         rcvd_hfn;
  uint32_t         rcvd_count;
  int              header_size;
  int              integrity_size;
  int              sdap_header_size = 0;
  int              rx_deliv_sn;
  uint32_t         rx_deliv_hfn;

  if (entity->entity_suspended) {
    LOG_W(PDCP,
          "PDCP entity (%s) %d is suspended. Quit RX procedure.\n",
          entity->type > NR_PDCP_DRB_UM ? "SRB" : "DRB",
          entity->rb_id);
    return;
  }

  if (size < 1) {
    LOG_E(PDCP, "bad PDU received (size = %d)\n", size);
    return;
  }

  if (entity->type != NR_PDCP_SRB && !(buffer[0] & 0x80)) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    /* TODO: This is something of a hack. The most significant bit
       in buffer[0] should be 1 if the packet is a data packet. We are
       processing malformed data packets if the most significant bit
       is 0. Rather than exit(1), this hack allows us to continue for now.
       We need to investigate why this hack is neccessary. */
    buffer[0] |= 128;
  }
  entity->stats.rxpdu_pkts++;
  entity->stats.rxpdu_bytes += size;

  if (entity->has_sdap_rx) sdap_header_size = 1; // SDAP Header is one byte

  if (entity->sn_size == SHORT_SN_SIZE) {
    rcvd_sn = ((buffer[0] & 0xf) <<  8) |
                buffer[1];
    header_size = SHORT_PDCP_HEADER_SIZE;
  } else {
    rcvd_sn = ((buffer[0] & 0x3) << 16) |
               (buffer[1]        <<  8) |
                buffer[2];
    header_size = LONG_PDCP_HEADER_SIZE;
  }
  entity->stats.rxpdu_sn = rcvd_sn;

  /* SRBs always have MAC-I, even if integrity is not active */
  if (entity->has_integrity || entity->type == NR_PDCP_SRB) {
    integrity_size = PDCP_INTEGRITY_SIZE;
  } else {
    integrity_size = 0;
  }

  if (size < header_size + sdap_header_size + integrity_size + 1) {
    LOG_E(PDCP, "bad PDU received (size = %d)\n", size);

    entity->stats.rxpdu_dd_pkts++;
    entity->stats.rxpdu_dd_bytes += size;

    return;
  }

  rx_deliv_sn  = entity->rx_deliv & entity->sn_max;
  rx_deliv_hfn = entity->rx_deliv >> entity->sn_size;

  if (rcvd_sn < rx_deliv_sn - entity->window_size) {
    rcvd_hfn = rx_deliv_hfn + 1;
  } else if (rcvd_sn >= rx_deliv_sn + entity->window_size) {
    rcvd_hfn = rx_deliv_hfn - 1;
  } else {
    rcvd_hfn = rx_deliv_hfn;
  }

  rcvd_count = (rcvd_hfn << entity->sn_size) | rcvd_sn;

  nr_pdcp_integrity_data_t msg_integrity = { 0 };

  /* the MAC-I/header/rcvd_count is needed by some RRC procedures, store it */
  if (entity->has_integrity || entity->type == NR_PDCP_SRB) {
    msg_integrity.count = rcvd_count;
    memcpy(msg_integrity.mac, &buffer[size-4], 4);
    msg_integrity.header_size = header_size;
    memcpy(msg_integrity.header, buffer, header_size);
  }

  if (entity->has_ciphering)
    entity->decipher(entity->security_context,
                     buffer + header_size + sdap_header_size,
                     size - (header_size + sdap_header_size),
                     entity->rb_id, rcvd_count, entity->is_gnb ? 0 : 1);

  if (entity->has_integrity) {
    unsigned char integrity[PDCP_INTEGRITY_SIZE] = {0};
    entity->integrity(entity->integrity_context, integrity,
                      buffer, size - integrity_size,
                      entity->rb_id, rcvd_count, entity->is_gnb ? 0 : 1);
    if (memcmp(integrity, buffer + size - integrity_size, PDCP_INTEGRITY_SIZE) != 0) {
      LOG_E(PDCP, "discard NR PDU, integrity failed\n");
      entity->stats.rxpdu_dd_pkts++;
      entity->stats.rxpdu_dd_bytes += size;

      return;
    }
  }

  if (rcvd_count < entity->rx_deliv
      || nr_pdcp_sdu_in_list(entity->rx_list, rcvd_count)) {
    /* 1280 ms = 128 frames @ 10 ms, to match MAC cadence */
    if (entity->pdu_discarded != 0 && entity->t_current > entity->t_log_pdu_discard + 1280) {
      LOG_W(PDCP, "%d NR PDU discarded (rcvd_count < rx_deliv or duplicate)\n", entity->pdu_discarded);
      entity->pdu_discarded = 0;
      entity->t_log_pdu_discard = entity->t_current;
    }
    entity->pdu_discarded++;
    entity->stats.rxpdu_dd_pkts++;
    entity->stats.rxpdu_dd_bytes += size;

    return;
  }

  sdu = nr_pdcp_new_sdu(rcvd_count,
                        (char *)buffer + header_size,
                        size - header_size - integrity_size,
                        &msg_integrity);
  entity->rx_list = nr_pdcp_sdu_list_add(entity->rx_list, sdu);
  entity->rx_size += size-header_size;

  if (rcvd_count >= entity->rx_next) {
    entity->rx_next = rcvd_count + 1;
  }

  /* TODO(?): out of order delivery */

  if (rcvd_count == entity->rx_deliv) {
    /* deliver all SDUs starting from rx_deliv up to discontinuity or end of list */
    uint32_t count = entity->rx_deliv;
    while (entity->rx_list != NULL && count == entity->rx_list->count) {
      nr_pdcp_sdu_t *cur = entity->rx_list;
      entity->deliver_sdu(entity->deliver_sdu_data, entity,
                          cur->buffer, cur->size,
                          &cur->msg_integrity);
      entity->rx_list = cur->next;
      entity->rx_size -= cur->size;
      entity->stats.txsdu_pkts++;
      entity->stats.txsdu_bytes += cur->size;

      nr_pdcp_free_sdu(cur);
      count++;
    }
    entity->rx_deliv = count;
    LOG_D(PDCP,
          "%s: entity (%s) %d - rx_deliv = %d, rcvd_sn = %d \n",
          __func__,
          entity->type == NR_PDCP_DRB_AM ? "DRB" : "SRB",
          entity->rb_id,
          entity->rx_deliv,
          rcvd_sn);
  }

  if (entity->t_reordering_start != 0 && entity->rx_deliv >= entity->rx_reord) {
    /* stop and reset t-Reordering */
    entity->t_reordering_start = 0;
  }

  if (entity->t_reordering_start == 0 && entity->rx_deliv < entity->rx_next) {
    entity->rx_reord = entity->rx_next;
    entity->t_reordering_start = entity->t_current;
  }
}

static int nr_pdcp_entity_process_sdu(nr_pdcp_entity_t *entity,
                                      char *buffer,
                                      int size,
                                      int sdu_id,
                                      char *pdu_buffer,
                                      int pdu_max_size)
{
  uint32_t count;
  int      sn;
  int      header_size;
  int      integrity_size;
  int      sdap_header_size = 0;
  char    *buf = pdu_buffer;
  DevAssert(nr_max_pdcp_pdu_size(size) <= pdu_max_size);
  int      dc_bit;

  if (entity->entity_suspended) {
    LOG_W(PDCP,
          "PDCP entity (%s) %d is suspended. Quit SDU processing.\n",
          entity->type > NR_PDCP_DRB_UM ? "SRB" : "DRB",
          entity->rb_id);
    return -1;
  }

  entity->stats.rxsdu_pkts++;
  entity->stats.rxsdu_bytes += size;


  count = entity->tx_next;
  sn = entity->tx_next & entity->sn_max;

  if (entity->has_sdap_tx) sdap_header_size = 1; // SDAP header is one byte

  /* D/C bit is only to be set for DRBs */
  if (entity->type == NR_PDCP_DRB_AM || entity->type == NR_PDCP_DRB_UM) {
    dc_bit = 0x80;
  } else {
    dc_bit = 0;
  }

  if (entity->sn_size == SHORT_SN_SIZE) {
    buf[0] = dc_bit | ((sn >> 8) & 0xf);
    buf[1] = sn & 0xff;
    header_size = SHORT_PDCP_HEADER_SIZE;
  } else {
    buf[0] = dc_bit | ((sn >> 16) & 0x3);
    buf[1] = (sn >> 8) & 0xff;
    buf[2] = sn & 0xff;
    header_size = LONG_PDCP_HEADER_SIZE;
  }

  /* SRBs always have MAC-I, even if integrity is not active */
  if (entity->has_integrity || entity->type == NR_PDCP_SRB) {
    integrity_size = PDCP_INTEGRITY_SIZE;
  } else {
    integrity_size = 0;
  }

  memcpy(buf + header_size, buffer, size);

  if (entity->has_integrity) {
    uint8_t integrity[PDCP_INTEGRITY_SIZE] = {0};
    entity->integrity(entity->integrity_context,
                      integrity,
                      (unsigned char *)buf, header_size + size,
                      entity->rb_id, count, entity->is_gnb ? 1 : 0);

    memcpy((unsigned char *)buf + header_size + size, integrity, PDCP_INTEGRITY_SIZE);
  } else if (integrity_size == PDCP_INTEGRITY_SIZE) {
    // set MAC-I to 0 for SRBs with integrity not active
    memset(buf + header_size + size, 0, PDCP_INTEGRITY_SIZE);
  }

  if (entity->has_ciphering) {
    entity->cipher(entity->security_context,
                   (unsigned char *)buf + header_size + sdap_header_size,
                   size + integrity_size - sdap_header_size,
                   entity->rb_id, count, entity->is_gnb ? 1 : 0);
  }

  entity->tx_next++;

  entity->stats.txpdu_pkts++;
  entity->stats.txpdu_bytes += header_size + size + integrity_size;
  entity->stats.txpdu_sn = sn;

  return header_size + size + integrity_size;
}

static bool nr_pdcp_entity_check_integrity(nr_pdcp_entity_t *entity,
                                           const uint8_t *buffer,
                                           int buffer_size,
                                           const nr_pdcp_integrity_data_t *msg_integrity)
{
  if (!entity->has_integrity)
    return false;

  int header_size = msg_integrity->header_size;

  uint8_t b[buffer_size + header_size];

  for (int i = 0; i < header_size; i++)
    b[i] = msg_integrity->header[i];

  memcpy(b + header_size, buffer, buffer_size);

  unsigned char mac[4];
  entity->integrity(entity->integrity_context, mac,
                    b, buffer_size + header_size,
                    entity->rb_id, msg_integrity->count, entity->is_gnb ? 0 : 1);

  return memcmp(mac, msg_integrity->mac, 4) == 0;
}

/* may be called several times, take care to clean previous settings */
static void nr_pdcp_entity_set_security(nr_pdcp_entity_t *entity, const nr_pdcp_entity_security_keys_and_algos_t *parameters)
{
  if (parameters->integrity_algorithm != -1) {
    entity->security_keys_and_algos.integrity_algorithm = parameters->integrity_algorithm;
    memcpy(entity->security_keys_and_algos.integrity_key, parameters->integrity_key, NR_K_KEY_SIZE);
  }
  if (parameters->ciphering_algorithm != -1) {
    entity->security_keys_and_algos.ciphering_algorithm = parameters->ciphering_algorithm;
    memcpy(entity->security_keys_and_algos.ciphering_key, parameters->ciphering_key, NR_K_KEY_SIZE);
  }

  if (parameters->integrity_algorithm == 0) {
    entity->has_integrity = 0;
    if (entity->free_integrity != NULL)
      entity->free_integrity(entity->integrity_context);
    entity->free_integrity = NULL;
  }

  if (parameters->integrity_algorithm != 0 && parameters->integrity_algorithm != -1) {
    entity->has_integrity = 1;
    if (entity->free_integrity != NULL)
      entity->free_integrity(entity->integrity_context);
    if (parameters->integrity_algorithm == 2) {
      entity->integrity_context = nr_pdcp_integrity_nia2_init(entity->security_keys_and_algos.integrity_key);
      entity->integrity = nr_pdcp_integrity_nia2_integrity;
      entity->free_integrity = nr_pdcp_integrity_nia2_free_integrity;
    } else if (parameters->integrity_algorithm == 1) {
      entity->integrity_context = nr_pdcp_integrity_nia1_init(entity->security_keys_and_algos.integrity_key);
      entity->integrity = nr_pdcp_integrity_nia1_integrity;
      entity->free_integrity = nr_pdcp_integrity_nia1_free_integrity;
    } else {
      LOG_E(PDCP, "FATAL: only nia1 and nia2 supported for the moment\n");
      exit(1);
    }
  }

  if (parameters->ciphering_algorithm == 0) {
    entity->has_ciphering = 0;
    if (entity->free_security != NULL)
      entity->free_security(entity->security_context);
    entity->free_security = NULL;
  }

  if (parameters->ciphering_algorithm != 0 && parameters->ciphering_algorithm != -1) {
    entity->has_ciphering = 1;
    if (entity->free_security != NULL)
      entity->free_security(entity->security_context);
    if (parameters->ciphering_algorithm == 2) {
      entity->security_context = nr_pdcp_security_nea2_init(entity->security_keys_and_algos.ciphering_key);
      entity->cipher = nr_pdcp_security_nea2_cipher;
      entity->decipher = nr_pdcp_security_nea2_cipher;
      entity->free_security = nr_pdcp_security_nea2_free_security;
    } else if (parameters->ciphering_algorithm == 1) {
      entity->security_context = nr_pdcp_security_nea1_init(entity->security_keys_and_algos.ciphering_key);
      entity->cipher = nr_pdcp_security_nea1_cipher;
      entity->decipher = nr_pdcp_security_nea1_cipher;
      entity->free_security = nr_pdcp_security_nea1_free_security;
    } else {
      LOG_E(PDCP, "FATAL: only nea1 and nea2 supported for the moment\n");
      exit(1);
    }
  }
}

static void check_t_reordering(nr_pdcp_entity_t *entity)
{
  uint32_t count;

  /* if t_reordering is set to "infinity" (seen as -1) then do nothing */
  if (entity->t_reordering == -1)
    return;

  if (entity->t_reordering_start == 0
      || entity->t_current <= entity->t_reordering_start + entity->t_reordering)
    return;
  LOG_D(PDCP,
        "%s: entity (%s) %d: t_reordering_start = %ld, t_current = %ld, t_reordering = %d \n",
        __func__,
        entity->type > NR_PDCP_DRB_UM ? "SRB" : "DRB",
        entity->rb_id,
        entity->t_reordering_start,
        entity->t_current,
        entity->t_reordering);

  /* stop timer */
  entity->t_reordering_start = 0;

  /* deliver all SDUs with count < rx_reord */
  while (entity->rx_list != NULL && entity->rx_list->count < entity->rx_reord) {
    nr_pdcp_sdu_t *cur = entity->rx_list;
    entity->deliver_sdu(entity->deliver_sdu_data, entity,
                        cur->buffer, cur->size,
                        &cur->msg_integrity);
    entity->rx_list = cur->next;
    entity->rx_size -= cur->size;
    entity->stats.txsdu_pkts++;
    entity->stats.txsdu_bytes += cur->size;
    nr_pdcp_free_sdu(cur);
  }

  /* deliver all SDUs starting from rx_reord up to discontinuity or end of list */
  count = entity->rx_reord;
  while (entity->rx_list != NULL && count == entity->rx_list->count) {
    nr_pdcp_sdu_t *cur = entity->rx_list;
    entity->deliver_sdu(entity->deliver_sdu_data, entity,
                        cur->buffer, cur->size,
                        &cur->msg_integrity);
    entity->rx_list = cur->next;
    entity->rx_size -= cur->size;
    entity->stats.txsdu_pkts++;
    entity->stats.txsdu_bytes += cur->size;
    nr_pdcp_free_sdu(cur);
    count++;
  }

  entity->rx_deliv = count;

  if (entity->rx_deliv < entity->rx_next) {
    entity->rx_reord = entity->rx_next;
    entity->t_reordering_start = entity->t_current;
  }
}

static void nr_pdcp_entity_set_time(nr_pdcp_entity_t *entity, uint64_t now)
{
  entity->t_current = now;

  check_t_reordering(entity);
}

static void deliver_all_sdus(nr_pdcp_entity_t *entity)
{
  // deliver the PDCP SDUs stored in the receiving PDCP entity to upper layers
  while (entity->rx_list != NULL) {
    nr_pdcp_sdu_t *cur = entity->rx_list;
    entity->deliver_sdu(entity->deliver_sdu_data, entity,
                        cur->buffer, cur->size,
                        &cur->msg_integrity);
    entity->rx_list = cur->next;
    entity->rx_size -= cur->size;
    entity->stats.txsdu_pkts++;
    entity->stats.txsdu_bytes += cur->size;
    nr_pdcp_free_sdu(cur);
  }
}

/**
 * @brief PDCP Entity Suspend according to 5.1.4 of 3GPP TS 38.323
 * Transmitting PDCP entity shall:
 * - set TX_NEXT to the initial value;
 * - discard all stored PDCP PDUs (NOTE: PDUs are stored in RLC)
 * Receiving PDCP entity shall:
 * - if t-Reordering is running:
 *   a) stop and reset t-Reordering;
 *   b) deliver all stored PDCP SDUs
 * - set RX_NEXT and RX_DELIV to the initial value.
 */
static void nr_pdcp_entity_suspend(nr_pdcp_entity_t *entity)
{
  /* Transmitting PDCP entity */
  entity->tx_next = 0;
  /* Receiving PDCP entity */
  if (entity->t_reordering_start != 0) {
    entity->t_reordering_start = 0;
    deliver_all_sdus(entity);
  }
  entity->rx_next = 0;
  entity->rx_deliv = 0;
  /* Flag to keep track of PDCP entity status */
  entity->entity_suspended = true;
}

static void free_rx_list(nr_pdcp_entity_t *entity)
{
  nr_pdcp_sdu_t *cur = entity->rx_list;
  while (cur != NULL) {
    nr_pdcp_sdu_t *next = cur->next;
    entity->stats.rxpdu_dd_pkts++;
    entity->stats.rxpdu_dd_bytes += cur->size;
    nr_pdcp_free_sdu(cur);
    cur = next;
  }
  entity->rx_list = NULL;
  entity->rx_size = 0;
}

/**
 * @brief PDCP entity re-establishment according to 5.1.2 of 3GPP TS 38.323
 * @todo  deal with ciphering/integrity algos and keys for transmitting/receiving entity procedures
*/
static void nr_pdcp_entity_reestablish_drb_am(nr_pdcp_entity_t *entity,
                                              const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  /* transmitting entity procedures */
  /* do nothing */

  /* receiving entity procedures */
  /* do nothing */

  /* ciphering and integrity: common for both tx and rx entities */
  entity->set_security(entity, security_parameters);

  /* Flag PDCP entity as re-established */
  entity->entity_suspended = false;
}

static void nr_pdcp_entity_reestablish_drb_um(nr_pdcp_entity_t *entity,
                                              const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  /* transmitting entity procedures */
  entity->tx_next = 0;

  /* receiving entity procedures */
  /* deliver all SDUs if t_reordering is running */
  if (entity->t_reordering_start != 0)
    deliver_all_sdus(entity);
  /* stop t_reordering */
  entity->t_reordering_start = 0;
  /* set rx_next and rx_deliv to the initial value */
  entity->rx_next = 0;
  entity->rx_deliv = 0;

  /* ciphering and integrity: common for both tx and rx entities */
  entity->set_security(entity, security_parameters);

  /* Flag PDCP entity as re-established */
  entity->entity_suspended = false;
}

static void nr_pdcp_entity_reestablish_srb(nr_pdcp_entity_t *entity,
                                           const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  /* transmitting entity procedures */
  entity->tx_next = 0;

  /* receiving entity procedures */
  free_rx_list(entity);
  /* stop t_reordering */
  entity->t_reordering_start = 0;
  /* set rx_next and rx_deliv to the initial value */
  entity->rx_next = 0;
  entity->rx_deliv = 0;

  /* ciphering and integrity: common for both tx and rx entities */
  entity->set_security(entity, security_parameters);

  /* Flag PDCP entity as re-established */
  entity->entity_suspended = false;
}

static void nr_pdcp_entity_release(nr_pdcp_entity_t *entity)
{
  deliver_all_sdus(entity);
}

static void nr_pdcp_entity_delete(nr_pdcp_entity_t *entity)
{
  free_rx_list(entity);
  if (entity->free_security != NULL)
    entity->free_security(entity->security_context);
  if (entity->free_integrity != NULL)
    entity->free_integrity(entity->integrity_context);
  free(entity);
}

static void nr_pdcp_entity_get_stats(nr_pdcp_entity_t *entity,
                                     nr_pdcp_statistics_t *out)
{
  *out = entity->stats;
}

/** @note: this function applies to the gNB only */
static nr_pdcp_count_t nr_pdcp_entity_get_count_dl(nr_pdcp_entity_t *entity)
{
  nr_pdcp_count_t count = {0};
  const int sn_bits = entity->sn_size;
  const uint32_t sn_mask = entity->sn_max;
  count.hfn = entity->rx_deliv >> sn_bits;
  count.sn = entity->rx_deliv & sn_mask;
  return count;
}

/** @note: this function applies to the gNB only */
static nr_pdcp_count_t nr_pdcp_entity_get_count_ul(nr_pdcp_entity_t *entity)
{
  nr_pdcp_count_t count = {0};
  const int sn_bits = entity->sn_size;
  const uint32_t sn_mask = entity->sn_max;
  count.hfn = entity->tx_next >> sn_bits;
  count.sn = entity->tx_next & sn_mask;
  return count;
}

/** @note: this function applies to the gNB only */
static void nr_pdcp_entity_set_count_dl(nr_pdcp_entity_t *entity, nr_pdcp_count_t pdcp_count, int sn_size)
{
  // SN length in bits
  const uint32_t sn_mask = (1 << sn_size) - 1;

  // Compose COUNT values for tx_next (DL) and rx_next (UL)
  uint32_t count = (pdcp_count.hfn << sn_size) | (pdcp_count.sn & sn_mask);

  // Apply the COUNT values
  entity->rx_next = count + 1;
  entity->rx_deliv = count;

  LOG_I(PDCP,
        "DRB %d: Applied PDCP Count\n"
        "  DL COUNT - SN: %d HFN: %d (COUNT=%u)\n"
        "  SN length: %s\n"
        "   PDCP entity state after update:\n"
        "     rx_next  = %u\n"
        "     rx_deliv = %u\n"
        "     sn_size  = %d\n"
        "     sn_max   = %d\n"
        "     window   = %d\n",
        entity->rb_id,
        pdcp_count.sn,
        pdcp_count.hfn,
        count,
        sn_size == LONG_SN_SIZE ? "18-bit" : "12-bit",
        entity->rx_next,
        entity->rx_deliv,
        entity->sn_size,
        entity->sn_max,
        entity->window_size);
}

/** @note: this function applies to the gNB only */
static void nr_pdcp_entity_set_count_ul(nr_pdcp_entity_t *entity, nr_pdcp_count_t pdcp_count, int sn_size)
{
  // SN length in bits
  const uint32_t sn_mask = (1 << sn_size) - 1;

  // Compose COUNT values for tx_next (DL) and rx_next (UL)
  uint32_t count = (pdcp_count.hfn << sn_size) | (pdcp_count.sn & sn_mask);

  // Apply the COUNT values
  entity->tx_next = count;

  LOG_I(PDCP,
        "DRB %d: Applied PDCP Count\n"
        "  UL COUNT - SN: %d HFN: %d (COUNT=%u)\n"
        "  SN length: %s\n"
        "   PDCP entity state after update:\n"
        "     tx_next  = %u\n"
        "     sn_size  = %d\n"
        "     sn_max   = %d\n"
        "     window   = %d\n",
        entity->rb_id,
        pdcp_count.sn,
        pdcp_count.hfn,
        count,
        sn_size == LONG_SN_SIZE ? "18-bit" : "12-bit",
        entity->tx_next,
        entity->sn_size,
        entity->sn_max,
        entity->window_size);
}

nr_pdcp_entity_t *new_nr_pdcp_entity(
    nr_pdcp_entity_type_t type,
    int is_gnb,
    int rb_id,
    int pdusession_id,
    bool has_sdap_rx,
    bool has_sdap_tx,
    void (*deliver_sdu)(void *deliver_sdu_data,
                        nr_pdcp_entity_t *entity,
                        char *buf,
                        int size,
                        const nr_pdcp_integrity_data_t *msg_integrity),
    void *deliver_sdu_data,
    int sn_size,
    int t_reordering,
    int discard_timer,
    const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  nr_pdcp_entity_t *ret;

  ret = calloc(1, sizeof(nr_pdcp_entity_t));
  if (ret == NULL) {
    LOG_E(PDCP, "%s:%d:%s: out of memory\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  ret->type = type;

  ret->recv_pdu        = nr_pdcp_entity_recv_pdu;
  ret->process_sdu     = nr_pdcp_entity_process_sdu;
  ret->set_security    = nr_pdcp_entity_set_security;
  ret->check_integrity = nr_pdcp_entity_check_integrity;
  ret->set_time        = nr_pdcp_entity_set_time;

  ret->delete_entity = nr_pdcp_entity_delete;
  ret->release_entity = nr_pdcp_entity_release;
  ret->suspend_entity = nr_pdcp_entity_suspend;

  switch (type) {
    case NR_PDCP_DRB_AM:
      ret->reestablish_entity = nr_pdcp_entity_reestablish_drb_am;
      break;
    case NR_PDCP_DRB_UM:
      ret->reestablish_entity = nr_pdcp_entity_reestablish_drb_um;
      break;
    case NR_PDCP_SRB:
      ret->reestablish_entity = nr_pdcp_entity_reestablish_srb;
      break;
  }
  
  ret->get_stats = nr_pdcp_entity_get_stats;
  ret->deliver_sdu = deliver_sdu;
  ret->deliver_sdu_data = deliver_sdu_data;

  ret->rb_id         = rb_id;
  ret->pdusession_id = pdusession_id;
  ret->has_sdap_rx   = has_sdap_rx;
  ret->has_sdap_tx   = has_sdap_tx;
  ret->sn_size       = sn_size;
  ret->t_reordering  = t_reordering;
  ret->discard_timer = discard_timer;

  ret->sn_max        = (1 << sn_size) - 1;
  ret->window_size   = 1 << (sn_size - 1);

  ret->is_gnb = is_gnb;

  nr_pdcp_entity_set_security(ret, security_parameters);

  /* PDCP Count API */
  ret->get_pdcp_count_dl = nr_pdcp_entity_get_count_dl;
  ret->get_pdcp_count_ul = nr_pdcp_entity_get_count_ul;
  ret->set_pdcp_count_dl = nr_pdcp_entity_set_count_dl;
  ret->set_pdcp_count_ul = nr_pdcp_entity_set_count_ul;

  return ret;
}
