/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "openair3/NRPPA/nrppa_pos_assistance.h"
#include "NRPPA_Assistance-Information.h"
#include "aper_decoder.h"

int main(void)
{
  const uint8_t original[] = {0x15, 0x43, 0x00, 0xf0, 0x7f};
  for (uint32_t period = 8; period <= 512; period *= 2) {
    uint8_t *encoded = NULL, *decoded = NULL;
    size_t encoded_len = 0, decoded_len = 0;
    uint32_t decoded_period = 0;
    assert(nrppa_encode_pos_assistance(original, sizeof(original), period, &encoded, &encoded_len));
    NRPPA_Assistance_Information_t *independent = NULL;
    asn_dec_rval_t r = aper_decode_complete(NULL, &asn_DEF_NRPPA_Assistance_Information,
                                            (void **)&independent, encoded, encoded_len);
    assert(r.code == RC_OK && r.consumed == encoded_len);
    assert(independent->systemInformation.list.count == 1);
    SystemInformation__Member *system = independent->systemInformation.list.array[0];
    assert(system->posSIBs.list.count == 1);
    PosSIBs__Member *sib = system->posSIBs.list.array[0];
    assert(sib->posSIB_Type == NRPPA_PosSIB_Type_posSibType6_1 && sib->posSIB_Segments.list.count == 1);
    assert(sib->posSIB_Segments.list.array[0]->assistanceDataSIBelement.size == sizeof(original));
    assert(!memcmp(sib->posSIB_Segments.list.array[0]->assistanceDataSIBelement.buf, original, sizeof(original)));
    ASN_STRUCT_FREE(asn_DEF_NRPPA_Assistance_Information, independent);
    assert(nrppa_decode_pos_assistance(encoded, encoded_len, &decoded, &decoded_len, &decoded_period));
    assert(decoded_period == period && decoded_len == sizeof(original));
    assert(!memcmp(decoded, original, sizeof(original)));
    free(decoded);
    assert(!nrppa_decode_pos_assistance(encoded, encoded_len - 1, &decoded, &decoded_len, &decoded_period));
    free(encoded);
  }
  uint8_t *encoded = NULL;
  size_t encoded_len = 0;
  assert(!nrppa_encode_pos_assistance(original, sizeof(original), 15, &encoded, &encoded_len));
  return 0;
}
