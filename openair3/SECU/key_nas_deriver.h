/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef KEY_NAS_DERIVER_H
#define KEY_NAS_DERIVER_H

#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>

#define SECURITY_KEY_LEN 32

typedef enum {
  NAS_ENC_ALG = 0x01,
  NAS_INT_ALG = 0x02,
  RRC_ENC_ALG = 0x03,
  RRC_INT_ALG = 0x04,
  UP_ENC_ALG = 0x05,
  UP_INT_ALG = 0x06
} algorithm_type_dist_t;

void derive_kgnb(const uint8_t kamf[32], const uint32_t count, uint8_t *kgnb);

void derive_keNB(const uint8_t kasme[32], const uint32_t nas_count, uint8_t *keNB);

void derive_keNB_star(const uint8_t *kenb_32,
                      const uint16_t pci,
                      const uint32_t earfcn_dl,
                      const bool is_rel8_only,
                      uint8_t *kenb_star);

void derive_key_nas(algorithm_type_dist_t nas_alg_type, uint8_t nas_enc_alg_id, const uint8_t kasme[32], uint8_t knas[32]);

void derive_skgNB(const uint8_t *keNB, const uint16_t sk_counter, uint8_t *skgNB);

void nr_derive_key(algorithm_type_dist_t alg_type, uint8_t alg_id, const uint8_t key[32], uint8_t out[16]);

void nr_derive_key_ng_ran_star(uint16_t pci, uint64_t nr_arfcn_dl, const uint8_t key[32], uint8_t *key_ng_ran_star);

void nr_derive_nh(const uint8_t k_amf[SECURITY_KEY_LEN], const uint8_t *sync_input, uint8_t *nh);

uint16_t nr_derive_short_mac_i(uint8_t integrity_alg,
                               const uint8_t kgnb[SECURITY_KEY_LEN],
                               uint16_t source_pci,
                               uint64_t target_cell_id,
                               uint16_t source_crnti);

void log_hex_buffer(const char *label, const uint8_t *buf, const int len);

#endif
