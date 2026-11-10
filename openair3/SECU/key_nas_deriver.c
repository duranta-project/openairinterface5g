/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "aes_128.h"
#include "aes_128_cbc_cmac.h"

#include "common/utils/assertions.h"
#include "common/utils/LOG/log.h"
#include "kdf.h"
#include "key_nas_deriver.h"
#include "nas_stream_eea1.h"
#include "nas_stream_eea2.h"
#include "nas_stream_eia1.h"
#include "nas_stream_eia2.h"
#include "secu_defs.h"
#include "snow3g.h"
#include <arpa/inet.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#define FC_KENB (0x11)
#define FC_NH (0x6F) // A.10 3GPP TS 33.501
#define FC_KENB_STAR (0x13)
/* 33401 #A.7 Algorithm for key derivation function.
 * This FC should be used for:
 * - NAS Encryption algorithm
 * - NAS Integrity algorithm
 * - RRC Encryption algorithm
 * - RRC Integrity algorithm
 * - User Plane Encryption algorithm
 */
#define FC_ALG_KEY_DER (0x15)
#define FC_KASME_TO_CK (0x16)

#define NR_FC_ALG_KEY_DER (0x69)
#define NR_FC_ALG_KEY_NG_RAN_STAR_DER (0x70)

void log_hex_buffer(const char *label, const uint8_t *buf, const int len)
{
  char hex_str[len * 2 + 1];
  for (int i = 0; i < len; i++) {
    snprintf(&hex_str[i * 2], 3, "%02x", buf[i]);
  }
  hex_str[len * 2] = '\0';
  LOG_D(UTIL, "%s: %s\n", label, hex_str);
}

static const char* alg_type_to_str(algorithm_type_dist_t alg_type)
{
  switch (alg_type) {
    case NAS_ENC_ALG: return "NAS_ENC_ALG";
    case NAS_INT_ALG: return "NAS_INT_ALG";
    case RRC_ENC_ALG: return "RRC_ENC_ALG";
    case RRC_INT_ALG: return "RRC_INT_ALG";
    case UP_ENC_ALG:  return "UP_ENC_ALG";
    case UP_INT_ALG:  return "UP_INT_ALG";
    default:          return "UNKNOWN_ALG_TYPE";
  }
}

static void derive_key_common(algorithm_type_dist_t alg_type, uint8_t alg_id, const uint8_t kasme[32], uint8_t FC, uint8_t out[32])
{
  uint8_t s[7];

  /* FC */
  s[0] = FC;

  /* P0 = algorithm type distinguisher */
  s[1] = (uint8_t)(alg_type & 0xFF);

  /* L0 = length(P0) = 1 */
  s[2] = 0x00;
  s[3] = 0x01;

  /* P1 */
  s[4] = alg_id;

  /* L1 = length(P1) = 1 */
  s[5] = 0x00;
  s[6] = 0x01;

  byte_array_t data = {.len = 7, .buf = s};
  kdf(kasme, data, 32, out);
  log_hex_buffer("Input key (kgnb)", kasme, 32);
  log_hex_buffer("Derivation string (s)", s, 7);
  log_hex_buffer(alg_type_to_str(alg_type), out, 32);
}

/*!
 * @brief Derive the kNASenc from kasme and perform truncate on the generated key to
 * reduce his size to 128 bits. Definition of the derivation function can
 * be found in 3GPP TS.33401 #A.7
 * @param[in] nas_alg_type NAS algorithm distinguisher
 * @param[in] nas_enc_alg_id NAS encryption/integrity algorithm identifier.
 * Possible values are:
 *      - 0 for EIA0 algorithm (Null Integrity Protection algorithm)
 *      - 1 for 128-EIA1 SNOW 3G
 *      - 2 for 128-EIA2 AES
 * @param[in] kasme Key for MME as provided by AUC
 * @param[out] knas Pointer to reference where output of KDF will be stored.
 */
void derive_key_nas(algorithm_type_dist_t alg_type, uint8_t alg_id, const uint8_t kasme[32], uint8_t knas[32])
{
  derive_key_common(alg_type, alg_id, kasme, FC_ALG_KEY_DER, knas);
}

void nr_derive_key(algorithm_type_dist_t alg_type, uint8_t alg_id, const uint8_t key[32], uint8_t out[16])
{
  uint8_t tmp[32] = {0};
  derive_key_common(alg_type, alg_id, key, NR_FC_ALG_KEY_DER, tmp);

  // Only the last 16 bytes are needed in 5G
  memcpy(out, &tmp[16], 16);
}

/*!
 * @brief Compute the shortMAC-I of RRCReestablishmentRequest (TS 38.331 5.3.7.4): the 16 least
 * significant bits of the MAC-I calculated over the encoded VarShortMAC-Input, with the KRRCint
 * and integrity algorithm of the source PCell, and all bits of COUNT, BEARER and DIRECTION set to 1.
 * @param[in] integrity_alg integrity algorithm of the source PCell (NIA0/1/2)
 * @param[in] kgnb KgNB in use in the source PCell
 * @param[in] source_pci physical cell identity of the source PCell
 * @param[in] target_cell_id cell identity (36 bits) of the cell the UE re-establishes on
 * @param[in] source_crnti C-RNTI of the UE in the source PCell
 */
uint16_t nr_derive_short_mac_i(uint8_t integrity_alg,
                               const uint8_t kgnb[SECURITY_KEY_LEN],
                               uint16_t source_pci,
                               uint64_t target_cell_id,
                               uint16_t source_crnti)
{
  /* NIA0 produces an all-zero MAC-I (TS 33.501 D.1) */
  if (integrity_alg == EIA0_ALG_ID)
    return 0;

  uint8_t krrcint[16];
  nr_derive_key(RRC_INT_ALG, integrity_alg, kgnb, krrcint);

  /* VarShortMAC-Input: sourcePhysCellId (10 bits), targetCellIdentity (36 bits) and
   * source-c-RNTI (16 bits), encoded MSB first and padded to 64 bits */
  const uint64_t input =
      ((uint64_t)(source_pci & 0x3ff) << 54) | ((target_cell_id & 0xfffffffffULL) << 18) | ((uint64_t)source_crnti << 2);
  uint8_t var_input[8];
  for (int i = 0; i < 8; i++)
    var_input[i] = (input >> (56 - 8 * i)) & 0xff;

  stream_security_context_t *ctx = stream_integrity_init(integrity_alg, krrcint);
  nas_stream_cipher_t cipher = {
      .context = ctx,
      .count = 0xffffffff,
      .bearer = 0x1f,
      .direction = SECU_DIRECTION_DOWNLINK,
      .message = var_input,
      .blength = 64,
  };
  uint8_t mac[4];
  stream_compute_integrity(integrity_alg, &cipher, mac);
  stream_integrity_free(integrity_alg, ctx);
  return ((uint16_t)mac[2] << 8) | mac[3];
}

/*!
 * @brief Derive the keys from key and perform truncate on the generated key to
 * reduce his size to 128 bits. Definition of the derivation function can
 * be found in 3GPP TS.33401 #A.7
 * @param[in] alg_type Algorithm distinguisher
 * @param[in] alg_id Algorithm identifier.
 * Possible values are:
 *      - 0 for EIA0 algorithm (Null Integrity Protection algorithm)
 *      - 1 for 128-EIA1 SNOW 3G
 *      - 2 for 128-EIA2 AES
 * @param[in] key The top key used to derive other subkeys
 * @param[out] out Pointer to reference where output of KDF will be stored.
 * NOTE: knas is dynamically allocated by the KDF function
 */

/** @brief KgNB derivation function (Annex A.9 3GPP TS 33.501)
 *
 * Derives KgNB from K_AMF and the uplink NAS COUNT.
 * This function is applied when cryptographically protected 5G radio bearers are
 * established and when a key change on-the-fly is performed.
 *
 * KDF input S structure (per Annex A.9):
 * - FC = 0x6E
 * - P0 = Uplink NAS COUNT (count parameter)
 * - L0 = length of uplink NAS COUNT (0x00 0x04)
 * - P1 = Access type distinguisher (0x01 for 3GPP access)
 * - L1 = length of Access type distinguisher (0x00 0x01)
 *
 * @param[in] kamf 256-bit K_AMF key (input key KEY to KDF)
 * @param[in] count Uplink NAS COUNT value (P0 in KDF input S)
 * @param[out] kgnb Derived 256-bit KgNB key */
void derive_kgnb(const uint8_t kamf[32], const uint32_t count, uint8_t *kgnb)
{
  /* Compute the KDF input parameter
   * S = FC(0x6E) || UL NAS Count || 0x00 0x04 || 0x01 || 0x00 0x01
   */
  uint8_t input[32] = {0};

  input[0] = 0x6E;
  // P0
  input[1] = count >> 24;
  input[2] = (uint8_t)(count >> 16);
  input[3] = (uint8_t)(count >> 8);
  input[4] = (uint8_t)count;
  // L0
  input[5] = 0;
  input[6] = 4;
  // P1
  input[7] = 0x01;
  // L1
  input[8] = 0;
  input[9] = 1;

  byte_array_t data = {.buf = input, .len = 10};
  kdf(kamf, data, 32, kgnb);

  log_hex_buffer("KAMF", kamf, 32);
  log_hex_buffer("KDF input for kgnb", input, 10);
  log_hex_buffer("Derived kgnb", kgnb, 32);
}

void derive_keNB(const uint8_t kasme[32], const uint32_t nas_count, uint8_t *keNB)
{
  uint8_t s[7] = {0};

  // FC
  s[0] = FC_KENB;
  // P0 = Uplink NAS count
  s[1] = (nas_count & 0xff000000) >> 24;
  s[2] = (nas_count & 0x00ff0000) >> 16;
  s[3] = (nas_count & 0x0000ff00) >> 8;
  s[4] = (nas_count & 0x000000ff);

  // Length of NAS count
  s[5] = 0x00;
  s[6] = 0x04;

  byte_array_t data = {.buf = s, .len = 7};
  kdf(kasme, data, 32, keNB);
}

void derive_keNB_star(const uint8_t *kenb_32,
                      const uint16_t pci,
                      const uint32_t earfcn_dl,
                      const bool is_rel8_only,
                      uint8_t *kenb_star)
{
  // see 33.401 section A.5 KeNB* derivation function
  uint8_t s[10] = {0};
  byte_array_t data = {.buf = s};
  // FC = 0x13
  s[0] = FC_KENB_STAR;
  // P0 = PCI (target physical cell id)
  s[1] = (pci & 0x0000ff00) >> 8;
  s[2] = (pci & 0x000000ff);
  // L0 = length of PCI (i.e. 0x00 0x02)
  s[3] = 0x00;
  s[4] = 0x02;
  // P1 = EARFCN-DL (target physical cell downlink frequency)
  if (is_rel8_only) {
    s[5] = (earfcn_dl & 0x0000ff00) >> 8;
    s[6] = (earfcn_dl & 0x000000ff);
    s[7] = 0x00;
    s[8] = 0x02;
    data.len = 9;
  } else {
    s[5] = (earfcn_dl & 0x00ff0000) >> 16;
    s[6] = (earfcn_dl & 0x0000ff00) >> 8;
    s[7] = (earfcn_dl & 0x000000ff);
    s[8] = 0x00;
    s[9] = 0x03;
    data.len = 10;
  }

  kdf(kenb_32, data, 32, kenb_star);

  // L1 length of EARFCN-DL (i.e. L1 = 0x00 0x02 if EARFCN-DL is between 0 and 65535, and L1 = 0x00 0x03 if EARFCN-DL is between
  // 65536 and 262143) NOTE: The length of EARFCN-DL cannot be generally set to 3 bytes for backward compatibility reasons: A Rel-8
  // entity (UE or eNB) would always assume an input parameter length of 2 bytes for the EARFCN-DL. This
  // would lead to different derived keys if another entity assumed an input parameter length of 3 bytes for the
  // EARFCN-DL.
}

void nr_derive_key_ng_ran_star(uint16_t pci, uint64_t nr_arfcn_dl, const uint8_t key[32], uint8_t *key_ng_ran_star)
{
  uint8_t s[10] = {0};

  /* FC */
  s[0] = NR_FC_ALG_KEY_NG_RAN_STAR_DER;

  /* P0 = PCI */
  s[1] = (pci & 0x0000ff00) >> 8;
  s[2] = (pci & 0x000000ff);

  /* L0 = length(P0) = 2 */
  s[3] = 0x00;
  s[4] = 0x02;

  /* P1 = NR ARFCN */
  s[5] = (nr_arfcn_dl & 0x00ff0000) >> 16;
  s[6] = (nr_arfcn_dl & 0x0000ff00) >> 8;
  s[7] = (nr_arfcn_dl & 0x000000ff);

  /* L1 = length(P1) = 3 */
  s[8] = 0x00;
  s[9] = 0x03;

  byte_array_t data = {.buf = s, .len = 10};
  const uint32_t len_key = 32;
  kdf(key, data, len_key, key_ng_ran_star);
  log_hex_buffer("Input key", key, 32);
  log_hex_buffer("KDF input for ng_ran*", s, 10);
  log_hex_buffer("Derived key_ng_ran*", key_ng_ran_star, 32);
}

void derive_skgNB(const uint8_t *keNB, const uint16_t sk_counter, uint8_t *skgNB)
{
  uint8_t s[5] = {0};

  /* FC is 0x1c (see 3gpp 33.401 annex A.15) */
  s[0] = 0x1c;

  /* put sk_counter */
  s[1] = (sk_counter >> 8) & 0xff;
  s[2] = sk_counter & 0xff;

  /* put length of sk_counter (2) */
  s[3] = 0x00;
  s[4] = 0x02;

  byte_array_t data = {.buf = s, .len = 5};
  kdf(keNB, data, 32, skgNB);
}

/** @brief NH derivation function (A.10 3GPP TS 33.501)
 *  @param k_amf: input key
 *  @param sync_input: pointer to the newly derived KgNB for the initial NH derivation,
 *                     and the previous NH for all subsequent derivations
 *  @param nh: pointer to the derivated NH output */
void nr_derive_nh(const uint8_t k_amf[SECURITY_KEY_LEN], const uint8_t *sync_input, uint8_t *nh)
{
  uint8_t s[SECURITY_KEY_LEN + 3] = {0};
  // FC
  s[0] = FC_NH;
  // P0 = SYNC-input
  memcpy(&s[1], sync_input, SECURITY_KEY_LEN);
  // L0 = length of SYNC-input (i.e. 0x00 0x20)
  s[SECURITY_KEY_LEN + 1] = 0x00;
  s[SECURITY_KEY_LEN + 2] = 0x20;

  byte_array_t data = {.buf = s, .len = sizeof(s)};
  kdf(k_amf, data, SECURITY_KEY_LEN, nh);

  log_hex_buffer("KDF input (SYNC string)", s, SECURITY_KEY_LEN + 3);
  log_hex_buffer("Derived NH", nh, SECURITY_KEY_LEN);

}
