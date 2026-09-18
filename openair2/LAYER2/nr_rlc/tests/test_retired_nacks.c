/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "../nr_rlc_entity.h"
#include "../nr_rlc_entity_am.h"
#include "../nr_rlc_pdu.h"
#include "common/utils/assertions.h"
#include <string.h>

static void deliver_sdu(void *data, nr_rlc_entity_t *entity, char *buf, int size)
{
  UNUSED(data);
  UNUSED(entity);
  UNUSED(buf);
  UNUSED(size);
  AssertFatal(0, "Unexpected SDU reception\n");
}

static void acknowledge_sdu(void *data, nr_rlc_entity_t *entity, int sdu_id)
{
  UNUSED(entity);
  UNUSED(sdu_id);
  (*(int *)data)++;
}

static void max_retx_reached(void *data, nr_rlc_entity_t *entity)
{
  UNUSED(data);
  UNUSED(entity);
  AssertFatal(0, "Unexpected maximum retransmissions\n");
}

static void check_retired_nack(int sn_bits, int start_sn, int nack_range)
{
  int acknowledged = 0;
  nr_rlc_entity_t *entity = new_nr_rlc_entity_am(100000, 100000, deliver_sdu, NULL,
                                               acknowledge_sdu, &acknowledged, max_retx_reached, NULL,
                                               45, 35, 0, -1, -1, 8, sn_bits);
  nr_rlc_entity_am_t *am = (nr_rlc_entity_am_t *)entity;
  int sn_mask = (1 << sn_bits) - 1;
  am->tx_next_ack = start_sn;
  am->tx_next = start_sn;
  char sdu[20] = {0};
  char pdu[100];
  for (int i = 0; i < 3; i++) {
    entity->recv_sdu(entity, sdu, sizeof(sdu), i);
    AssertFatal(entity->generate_pdu(entity, pdu, sizeof(pdu)) > 0, "Expected an AMD PDU\n");
  }

  /* TS 38.322, 6.2.2.5: STATUS with a retired NACK, optionally ranging into outstanding SNs. */
  nr_rlc_pdu_encoder_t encoder;
  memset(pdu, 0, sizeof(pdu));
  nr_rlc_pdu_encoder_init(&encoder, pdu, sizeof(pdu));
  nr_rlc_pdu_encoder_put_bits(&encoder, 0, 4); /* D/C and CPT */
  nr_rlc_pdu_encoder_put_bits(&encoder, (start_sn + 3) & sn_mask, sn_bits);
  nr_rlc_pdu_encoder_put_bits(&encoder, 1, 1); /* E1 */
  nr_rlc_pdu_encoder_put_bits(&encoder, 0, sn_bits == 12 ? 7 : 1);
  nr_rlc_pdu_encoder_put_bits(&encoder, (start_sn - 1) & sn_mask, sn_bits);
  nr_rlc_pdu_encoder_put_bits(&encoder, 0, 1); /* E1 */
  nr_rlc_pdu_encoder_put_bits(&encoder, 0, 1); /* E2 */
  nr_rlc_pdu_encoder_put_bits(&encoder, nack_range > 1, 1); /* E3 */
  nr_rlc_pdu_encoder_put_bits(&encoder, 0, sn_bits == 12 ? 1 : 3);
  if (nack_range > 1)
    nr_rlc_pdu_encoder_put_bits(&encoder, nack_range, 8);
  entity->recv_pdu(entity, pdu, encoder.byte + (encoder.bit != 0));

  int retransmissions = 0;
  for (nr_rlc_sdu_segment_t *segment = am->retransmit_list; segment != NULL; segment = segment->next)
    retransmissions++;
  AssertFatal(acknowledged == (nack_range == 1 ? 3 : 1), "Incorrect acknowledgement count: %d\n", acknowledged);
  AssertFatal(retransmissions == (nack_range == 1 ? 0 : 2), "Incorrect retransmission count: %d\n", retransmissions);
  entity->delete_entity(entity);
}

int main(void)
{
  for (int sn_bits = 12; sn_bits <= 18; sn_bits += 6) {
    check_retired_nack(sn_bits, 10, 1);
    check_retired_nack(sn_bits, 10, 3);
    check_retired_nack(sn_bits, 0, 1);
    check_retired_nack(sn_bits, (1 << sn_bits) - 1, 3);
  }
  return 0;
}
