/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "OctetString.h"
#include "fgs_nas_utils.h"
#include "common/utils/utils.h" // text_info_t, TO_ENUM, TO_TEXT

#ifndef FGS_MOBILE_IDENTITY_H_
#define FGS_MOBILE_IDENTITY_H_

// 4x 1bit according to 9.11.3.4 5GS mobile identity (type 5G-S-TMSI)
#define DIGIT1 0xF

typedef struct {
  uint8_t spare: 5;
  uint8_t typeofidentity: 3;
} NoIdentity5GSMobileIdentity_t;

typedef struct {
  uint8_t spare1: 1;
  uint8_t supiformat: 3;
  uint8_t spare2: 1;
  uint8_t typeofidentity: 3;
  uint8_t mccdigit2: 4;
  uint8_t mccdigit1: 4;
  uint8_t mncdigit3: 4;
  uint8_t mccdigit3: 4;
  uint8_t mncdigit2: 4;
  uint8_t mncdigit1: 4;
  uint8_t routingindicatordigit2: 4;
  uint8_t routingindicatordigit1: 4;
  uint8_t routingindicatordigit4: 4;
  uint8_t routingindicatordigit3: 4;
  uint8_t spare3: 1;
  uint8_t spare4: 1;
  uint8_t spare5: 1;
  uint8_t spare6: 1;
  uint8_t protectionschemeId: 4;
  uint8_t homenetworkpki;
  char schemeoutput[128];
} Suci5GSMobileIdentity_t;

typedef struct {
  uint8_t spare: 4;
  uint8_t oddeven: 1;
  uint8_t typeofidentity: 3;
  uint8_t mccdigit2: 4;
  uint8_t mccdigit1: 4;
  uint8_t mncdigit3: 4;
  uint8_t mccdigit3: 4;
  uint8_t mncdigit2: 4;
  uint8_t mncdigit1: 4;
  uint8_t amfregionid;
  uint16_t amfsetid: 10;
  uint16_t amfpointer: 6;
  uint32_t tmsi;
} Guti5GSMobileIdentity_t;

typedef struct {
  uint8_t digit1: 4;
  uint8_t oddeven: 1;
  uint8_t typeofidentity: 3;
  uint8_t digitp1: 4;
  uint8_t digitp: 4;
} Imei5GSMobileIdentity_t;

typedef struct {
  uint8_t digittac01: 4;
  uint8_t oddeven: 1;
  uint8_t typeofidentity: 3;
  uint8_t digittac02: 4;
  uint8_t digittac03: 4;
  uint8_t digittac04: 4;
  uint8_t digittac05: 4;
  uint8_t digittac06: 4;
  uint8_t digittac07: 4;
  uint8_t digittac08: 4;
  uint8_t digit09: 4;
  uint8_t digit10: 4;
  uint8_t digit11: 4;
  uint8_t digit12: 4;
  uint8_t digit13: 4;
  uint8_t digit14: 4;
  uint8_t digitsv1: 4;
  uint8_t digitsv2: 4;
  uint8_t spare: 4;
} Imeisv5GSMobileIdentity_t;

typedef struct {
  uint8_t digit1: 4;
  uint8_t spare: 1;
  uint8_t typeofidentity: 3;
  uint16_t amfsetid: 10;
  uint16_t amfpointer: 6;
  uint32_t tmsi;
} Stmsi5GSMobileIdentity_t;

typedef struct {
  uint8_t spare: 5;
  uint8_t typeofidentity: 3;
  uint8_t macaddr;
} Macaddr5GSMobileIdentity_t;

#define FOREACH_IDENTITY_TYPE(TYPE_DEF)       \
  TYPE_DEF(FGS_MOBILE_IDENTITY_NOIDENTITY, 0) \
  TYPE_DEF(FGS_MOBILE_IDENTITY_SUCI, 1)       \
  TYPE_DEF(FGS_MOBILE_IDENTITY_5G_GUTI, 2)    \
  TYPE_DEF(FGS_MOBILE_IDENTITY_IMEI, 3)       \
  TYPE_DEF(FGS_MOBILE_IDENTITY_5GS_TMSI, 4)   \
  TYPE_DEF(FGS_MOBILE_IDENTITY_IMEISV, 5)     \
  TYPE_DEF(FGS_MOBILE_IDENTITY_MAC_ADDR, 6)   \
  TYPE_DEF(FGS_MOBILE_IDENTITY_EUI64, 7)

typedef enum { FOREACH_IDENTITY_TYPE(TO_ENUM) } fgs_identity_type_t;

static const text_info_t fgs_identity_type_text[] = {FOREACH_IDENTITY_TYPE(TO_TEXT)};

// clang-format off
typedef union FGSMobileIdentity_tag {
  NoIdentity5GSMobileIdentity_t   noidentity;
  Suci5GSMobileIdentity_t         suci;
  Guti5GSMobileIdentity_t         guti;
  Imei5GSMobileIdentity_t         imei;
  Stmsi5GSMobileIdentity_t        stmsi;
  Imeisv5GSMobileIdentity_t       imeisv;
  Macaddr5GSMobileIdentity_t      macaddress;
} FGSMobileIdentity;
// clang-format on

/// IEI of the 5G-GUTI in the messages that carry it as an optional IE (TS 24.501: Registration Accept 8.2.7,
/// Configuration Update Command 8.2.19)
#define IEI_5G_GUTI 0x77

int encode_5gs_mobile_identity(const FGSMobileIdentity *fgsmobileidentity, uint8_t iei, uint8_t *buffer, uint32_t len);

/** @brief Equality check of two 5G-GUTI mobile identities (all fields) */
bool eq_fgs_guti(const Guti5GSMobileIdentity_t *a, const Guti5GSMobileIdentity_t *b);

int decode_5gs_mobile_identity(FGSMobileIdentity *fgsmobileidentity, uint8_t iei, const uint8_t *buffer, uint32_t len);

int encode_stmsi_5gs_mobile_identity(uint8_t *buffer, const Stmsi5GSMobileIdentity_t *stmsi, uint32_t len);
int decode_stmsi_5gs_mobile_identity(Stmsi5GSMobileIdentity_t *stmsi, const uint8_t *buffer, uint32_t len);

#endif /* FGS MOBILE IDENTITY_H_ */
