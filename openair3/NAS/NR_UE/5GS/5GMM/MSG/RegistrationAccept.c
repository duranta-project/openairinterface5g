/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief 5GS registration accept procedures
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "conversions.h"
#include "RegistrationAccept.h"
#include "fgs_nas_utils.h"
#include "common/utils/eq_check.h"
#include "common/utils/utils.h"
#include "ds/byte_array.h"
#include "fgmm_lib.h"

#define IEI_5G_GUTI 0x77
#define FGS_REGISTRATION_RESULT_LEN 2
#define REGISTRATION_ACCEPT_MIN_LEN FGS_REGISTRATION_RESULT_LEN // (5GS registration result) 2 octets

/** @brief Decode 5GS registration result (9.11.3.6 3GPP TS 24.501) */
static int decode_fgs_registration_result(registration_accept_msg *out, const byte_array_t buffer)
{
  if (buffer.len < FGS_REGISTRATION_RESULT_LEN) {
    PRINT_ERROR("5GS registration result decoding failure: invalid buffer length\n");
    return -1;
  }
  out->result = buffer.buf[1] & 0x7; // skip length of contents octet
  out->sms_allowed = buffer.buf[1] & 0x8;
  return 2; // length 2 octets (no IEI)
}

static int encode_fgs_registration_result(byte_array_t buffer, const registration_accept_msg *out)
{
  if (buffer.len < FGS_REGISTRATION_RESULT_LEN) {
    PRINT_ERROR("5GS registration result encoding failure: invalid buffer length\n");
    return -1;
  }
  uint8_t *buf = buffer.buf;
  buf[0] = 1;
  buf[1] = 0x00 | (out->sms_allowed & 0x8) | (out->result & 0x7);
  return 2; // encoded length 2 octets (no IEI)
}

/**
 * @brief Allowed NSSAI from Registration Accept according to 3GPP TS 24.501 Table 8.2.7.1.1
 *
 * @param content value part of the S-NSSAI IE, bounded by the caller
 * @return number of octets decoded, or -1 on failure
 */
static int decode_nssai_ie(nr_nas_msg_snssai_t *nssai, uint8_t *num_slices, const byte_array_t content)
{
  const uint8_t *buf = content.buf;
  const uint8_t *end = buf + content.len;
  int nssai_cnt = 0;
  while (buf < end) {
    const int item_len = *buf++; // Length of S-NSSAI IE item

    // The cases below read item_len octets, so the list must hold them
    if (item_len > end - buf) {
      PRINT_ERROR("S-NSSAI item claims %d octets, %td left\n", item_len, end - buf);
      return -1;
    }
    if (nssai_cnt == NAS_MAX_NUMBER_SLICES) {
      PRINT_ERROR("More than %d S-NSSAIs in list\n", NAS_MAX_NUMBER_SLICES);
      return -1;
    }

    nr_nas_msg_snssai_t *item = nssai + nssai_cnt;
    switch (item_len) {
      case 1:
        item->sst = *buf++;
        nssai_cnt++;
        break;

      case 2:
        item->sst = *buf++;
        item->hplmn_sst = malloc_or_fail(sizeof(*item->hplmn_sst));
        *item->hplmn_sst = *buf++;
        nssai_cnt++;
        break;

      case 4:
        item->sst = *buf++;
        item->sd = malloc_or_fail(sizeof(*item->sd));
        *item->sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        nssai_cnt++;
        break;

      case 5:
        item->sst = *buf++;
        item->sd = malloc_or_fail(sizeof(*item->sd));
        *item->sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        item->hplmn_sst = malloc_or_fail(sizeof(*item->hplmn_sst));
        *item->hplmn_sst = *buf++;
        nssai_cnt++;
        break;

      case 8:
        item->sst = *buf++;
        item->sd = malloc_or_fail(sizeof(*item->sd));
        *item->sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        item->hplmn_sst = malloc_or_fail(sizeof(*item->hplmn_sst));
        *item->hplmn_sst = *buf++;
        item->hplmn_sd = malloc_or_fail(sizeof(*item->hplmn_sd));
        *item->hplmn_sd = 0xffffff & ntoh_int24_buf(buf);
        buf += 3;
        nssai_cnt++;
        break;

      default:
        PRINT_ERROR("Unknown length %d in Allowed S-NSSAI list item\n", item_len);
        return -1;
    }
  }
  *num_slices = nssai_cnt;
  return content.len;
}

/** @brief IEs with a two-octet length field (TLV-E) in Registration Accept, TS 24.501 Table 8.2.7.1.1 */
static bool is_tlv_e_iei(uint8_t iei)
{
  switch (iei) {
    case 0x70: // NSSRG information
    case 0x72: // PDU session reactivation result error cause
    case 0x73: // SOR transparent container
    case 0x74: // Ciphering key data
    case 0x75: // CAG information list
    case 0x76: // Operator-defined access category definitions
    case IEI_5G_GUTI: // 0x77
    case 0x78: // EAP message
    case 0x79: // LADN information
    case 0x7A: // Extended emergency number list
    case 0x7B: // Service-level-AA container
    case 0x7C: // Extended CAG information list
    case 0x7D: // NSAG information
      return true;
    default:
      return false;
  }
}

size_t decode_registration_accept(registration_accept_msg *registration_accept, const byte_array_t buffer)
{
  int dec = 0;
  byte_array_t ba = buffer; // local copy of buffer

  if (ba.len < REGISTRATION_ACCEPT_MIN_LEN) {
    PRINT_ERROR("Registration Accept decoding failed: invalid buffer length\n");
    return -1;
  }

  // 5GS registration result (Mandatory)
  if ((dec = decode_fgs_registration_result(registration_accept, ba)) < 0)
    return -1;
  UPDATE_BYTE_ARRAY(ba, dec);

  // 5G-GUTI (Optional)
  if (ba.len > 0 && ba.buf[0] == IEI_5G_GUTI) {
    registration_accept->guti = calloc_or_fail(1, sizeof(*registration_accept->guti));
    if ((dec = decode_5gs_mobile_identity(registration_accept->guti, IEI_5G_GUTI, ba.buf, ba.len)) < 0) {
      PRINT_ERROR("Failed to decode 5GS Mobile Identity in Registration Accept\n");
      return -1;
    }
    UPDATE_BYTE_ARRAY(ba, dec);
  }

  // Decode other Optional IEs
  while (ba.len > 0) {
    const uint8_t iei = ba.buf[0];
    UPDATE_BYTE_ARRAY(ba, 1);

    // Type 1/2 (TS 24.007 11.2.4): IEI in bits 8-5, one octet, no length
    if (iei & 0x80)
      continue;

    /* Type 4 (TLV) and type 6 (TLV-E) have a one- and two-octet length field
       respectively. Bound the IE before decoding it: ba.len is a size_t, so an
       overshooting UPDATE_BYTE_ARRAY() would wrap it, not make it negative. */
    const size_t len_size = is_tlv_e_iei(iei) ? 2 : 1;
    if (ba.len < len_size) {
      PRINT_ERROR("Registration Accept: IEI 0x%02x has a truncated length field\n", iei);
      return -1;
    }
    const size_t content_len = (len_size == 2) ? ((ba.buf[0] << 8) | ba.buf[1]) : ba.buf[0];
    if (content_len > ba.len - len_size) {
      PRINT_ERROR("Registration Accept: IEI 0x%02x claims %zu octets, %zu left\n", iei, content_len, ba.len - len_size);
      return -1;
    }
    const byte_array_t content = {.len = content_len, .buf = ba.buf + len_size};

    switch (iei) {

      case 0x15: // allowed NSSAI
        if (decode_nssai_ie(registration_accept->nas_allowed_nssai, &registration_accept->num_allowed_slices, content) < 0)
          return -1;
        break;

      case 0x31: // configured NSSAI
        if (decode_nssai_ie(registration_accept->config_nssai, &registration_accept->num_configured_slices, content) < 0)
          return -1;
        break;

      default: // not decoded, and skipped whole by the length above
        break;
    }

    UPDATE_BYTE_ARRAY(ba, len_size + content_len);
  }
  if(ba.len != 0) {
    PRINT_ERROR("Failed to decode registration accept: ba.len = %ld\n", ba.len);
    return -1;
  }
  return buffer.len;
}

int encode_registration_accept(const registration_accept_msg *registration_accept, uint8_t *buffer, uint32_t len)
{
  int encoded = 0;

  byte_array_t ba = {.buf = buffer, .len = len};
  encoded += encode_fgs_registration_result(ba, registration_accept);

  if (registration_accept->guti) {
    int mi_enc = encode_5gs_mobile_identity(registration_accept->guti, 0x77, buffer + encoded, len - encoded);
    if (mi_enc < 0)
      return mi_enc;
    encoded += mi_enc;
  }

  return encoded;
}

/** Equality check for NAS Registration Accept */

bool eq_snssai(const nr_nas_msg_snssai_t *a, const nr_nas_msg_snssai_t *b)
{
  _EQ_CHECK_INT(a->sst, b->sst);

  if ((a->hplmn_sst && !b->hplmn_sst) || (!a->hplmn_sst && b->hplmn_sst))
    return false;
  if (a->hplmn_sst && b->hplmn_sst)
    _EQ_CHECK_INT(*a->hplmn_sst, *b->hplmn_sst);

  if ((a->sd && !b->sd) || (!a->sd && b->sd))
    return false;
  if (a->sd && b->sd)
    _EQ_CHECK_INT(*a->sd, *b->sd);

  if ((a->hplmn_sd && !b->hplmn_sd) || (!a->hplmn_sd && b->hplmn_sd))
    return false;
  if (a->hplmn_sd && b->hplmn_sd)
    _EQ_CHECK_INT(*a->hplmn_sd, *b->hplmn_sd);

  return true;
}


bool eq_fgmm_registration_accept(const registration_accept_msg *a, const registration_accept_msg *b)
{
  if (!a || !b) {
    PRINT_ERROR("Null pointer in registration_accept_msg_eq\n");
    return false;
  }

  // 5GS registration result (mandatory)
  _EQ_CHECK_INT(a->result, b->result);
  _EQ_CHECK_INT(a->sms_allowed, b->sms_allowed);

  // GUTI (optional)
  if (a->guti && b->guti) {
    _EQ_CHECK_INT(a->guti->guti.spare, b->guti->guti.spare);
    _EQ_CHECK_INT(a->guti->guti.oddeven, b->guti->guti.oddeven);
    _EQ_CHECK_INT(a->guti->guti.typeofidentity, b->guti->guti.typeofidentity);
    _EQ_CHECK_INT(a->guti->guti.mccdigit2, b->guti->guti.mccdigit2);
    _EQ_CHECK_INT(a->guti->guti.mccdigit1, b->guti->guti.mccdigit1);
    _EQ_CHECK_INT(a->guti->guti.mncdigit3, b->guti->guti.mncdigit3);
    _EQ_CHECK_INT(a->guti->guti.mccdigit3, b->guti->guti.mccdigit3);
    _EQ_CHECK_INT(a->guti->guti.mncdigit2, b->guti->guti.mncdigit2);
    _EQ_CHECK_INT(a->guti->guti.mncdigit1, b->guti->guti.mncdigit1);
    _EQ_CHECK_INT(a->guti->guti.amfregionid, b->guti->guti.amfregionid);
    _EQ_CHECK_INT(a->guti->guti.amfsetid, b->guti->guti.amfsetid);
    _EQ_CHECK_INT(a->guti->guti.amfpointer, b->guti->guti.amfpointer);
    _EQ_CHECK_INT(a->guti->guti.tmsi, b->guti->guti.tmsi);
  } else if ((a->guti && !b->guti) || (!a->guti && b->guti)) {
    PRINT_ERROR("NAS Equality Check failure: One of the two GUTIs is NULL\n");
    return false;
  }

  _EQ_CHECK_INT(a->num_allowed_slices, b->num_allowed_slices);
  _EQ_CHECK_INT(a->num_configured_slices, b->num_configured_slices);

  // Configured NSSAI (optional)
  for (int i = 0; i < a->num_configured_slices; i++) {
    if (!eq_snssai(&a->config_nssai[i], &b->config_nssai[i])) {
      PRINT_ERROR("NAS Equality Check failure: Configured NSSAI\n");
      return false;
    }
  }
  // Allowed NSSAI (optional)
  for (int i = 0; i < a->num_allowed_slices; i++) {
    if (!eq_snssai(&a->nas_allowed_nssai[i], &b->nas_allowed_nssai[i])) {
      PRINT_ERROR("NAS Equality Check failure: Allowed NSSAI\n");
      return false;
    }
  }

  return true;
}

/** Memory management of NAS Registration Accept */

static void free_nssai(nr_nas_msg_snssai_t *msg)
{
  free(msg->hplmn_sd);
  free(msg->hplmn_sst);
  free(msg->sd);
}

void free_fgmm_registration_accept(registration_accept_msg *msg)
{
  free(msg->guti);
  for (int i = 0; i < NAS_MAX_NUMBER_SLICES; i++) {
    free_nssai(&msg->nas_allowed_nssai[i]);
    free_nssai(&msg->config_nssai[i]);
  }
}
