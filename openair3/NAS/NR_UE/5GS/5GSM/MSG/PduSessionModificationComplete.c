/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief PDU session modification complete procedures
 */

#include "PduSessionModificationComplete.h"
#include <stdint.h>
#include "TLVEncoder.h"

int encode_pdu_session_modification_complete(pdu_session_modification_complete_msg *pdusessionmodificationcomplete,
                                             uint8_t *buffer,
                                             uint32_t len)
{
  if (len < 4) {
    return -1;
  }

  int encoded = 0;

  *(buffer + encoded) = pdusessionmodificationcomplete->protocoldiscriminator;
  encoded++;
  *(buffer + encoded) = pdusessionmodificationcomplete->pdusessionid;
  encoded++;
  *(buffer + encoded) = pdusessionmodificationcomplete->pti;
  encoded++;
  *(buffer + encoded) = pdusessionmodificationcomplete->pdusessionmodificationcompletemsgtype;
  encoded++;

  return encoded;
}
