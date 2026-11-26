/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include "nrppa_pos_assistance.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "NRPPA_Assistance-Information.h"
#include "aper_decoder.h"
#include "aper_encoder.h"
#include "constraints.h"
#include "common/utils/oai_asn1.h"
#include "common/utils/utils.h"

static bool period_to_enum(uint32_t frames, long *value)
{
  switch (frames) {
    case 8: *value = NRPPA_BroadcastPeriodicity_ms80; return true;
    case 16: *value = NRPPA_BroadcastPeriodicity_ms160; return true;
    case 32: *value = NRPPA_BroadcastPeriodicity_ms320; return true;
    case 64: *value = NRPPA_BroadcastPeriodicity_ms640; return true;
    case 128: *value = NRPPA_BroadcastPeriodicity_ms1280; return true;
    case 256: *value = NRPPA_BroadcastPeriodicity_ms2560; return true;
    case 512: *value = NRPPA_BroadcastPeriodicity_ms5120; return true;
    default: return false;
  }
}

bool nrppa_encode_pos_assistance(const uint8_t *segment, size_t segment_len, uint32_t period_frames,
                                 uint8_t **encoded, size_t *encoded_len)
{
  if (!encoded || !encoded_len)
    return false;
  *encoded = NULL;
  *encoded_len = 0;
  long period;
  if (!segment || !segment_len || segment_len > INT_MAX || !period_to_enum(period_frames, &period))
    return false;
  NRPPA_Assistance_Information_t info = {0};
  SystemInformation__Member *system_info = calloc_or_fail(1, sizeof(*system_info));
  PosSIBs__Member *sib = calloc_or_fail(1, sizeof(*sib));
  PosSIB_Segments__Member *part = calloc_or_fail(1, sizeof(*part));
  system_info->broadcastPeriodicity = period;
  sib->posSIB_Type = NRPPA_PosSIB_Type_posSibType6_1;
  asn1cSeqAdd(&info.systemInformation.list, system_info);
  asn1cSeqAdd(&system_info->posSIBs.list, sib);
  asn1cSeqAdd(&sib->posSIB_Segments.list, part);
  bool ok = OCTET_STRING_fromBuf(&part->assistanceDataSIBelement, (const char *)segment, segment_len) == 0;
  char error[256];
  size_t error_len = sizeof(error);
  ok = ok && asn_check_constraints(&asn_DEF_NRPPA_Assistance_Information, &info, error, &error_len) == 0;
  void *buf = NULL;
  ssize_t size = ok ? aper_encode_to_new_buffer(&asn_DEF_NRPPA_Assistance_Information, NULL, &info, &buf) : -1;
  ASN_STRUCT_FREE_CONTENTS_ONLY(asn_DEF_NRPPA_Assistance_Information, &info);
  if (size <= 0) {
    free(buf);
    return false;
  }
  *encoded = buf;
  *encoded_len = size;
  return true;
}

bool nrppa_decode_pos_assistance(const uint8_t *encoded, size_t encoded_len, uint8_t **segment,
                                 size_t *segment_len, uint32_t *period_frames)
{
  if (!segment || !segment_len || !period_frames)
    return false;
  *segment = NULL;
  *segment_len = 0;
  *period_frames = 0;
  if (!encoded || !encoded_len)
    return false;
  NRPPA_Assistance_Information_t *info = NULL;
  asn_dec_rval_t result = aper_decode_complete(NULL, &asn_DEF_NRPPA_Assistance_Information,
                                                (void **)&info, encoded, encoded_len);
  bool ok = result.code == RC_OK && result.consumed == encoded_len && info && !info->iE_Extensions &&
            info->systemInformation.list.count == 1;
  if (ok) {
    SystemInformation__Member *system_info = info->systemInformation.list.array[0];
    ok = system_info && !system_info->iE_Extensions && system_info->broadcastPeriodicity >= NRPPA_BroadcastPeriodicity_ms80 &&
         system_info->broadcastPeriodicity <= NRPPA_BroadcastPeriodicity_ms5120 && system_info->posSIBs.list.count == 1;
    if (ok) {
      PosSIBs__Member *sib = system_info->posSIBs.list.array[0];
      ok = sib && sib->posSIB_Type == NRPPA_PosSIB_Type_posSibType6_1 && !sib->iE_Extensions &&
           !sib->assistanceInformationMetaData && !sib->broadcastPriority && sib->posSIB_Segments.list.count == 1;
      if (ok) {
        PosSIB_Segments__Member *part = sib->posSIB_Segments.list.array[0];
        ok = part && !part->iE_Extensions && part->assistanceDataSIBelement.buf && part->assistanceDataSIBelement.size;
        if (ok) {
          *segment_len = part->assistanceDataSIBelement.size;
          *segment = malloc(*segment_len);
          ok = *segment != NULL;
          if (ok) {
            memcpy(*segment, part->assistanceDataSIBelement.buf, *segment_len);
            *period_frames = 8u << system_info->broadcastPeriodicity;
          }
        }
      }
    }
  }
  ASN_STRUCT_FREE(asn_DEF_NRPPA_Assistance_Information, info);
  if (!ok) {
    free(*segment);
    *segment = NULL;
    *segment_len = 0;
  }
  return ok;
}
