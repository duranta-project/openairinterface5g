/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _DEF_H_
#define _DEF_H_

#include <stdbool.h>

typedef struct {
  char *name;
  int fd;
  int mnc_length;      /* 2 or 3 */
  bool pin_needed;
} card_t;

typedef enum {
  AUTHENTICATE_SUCCESS,
  AUTHENTICATE_FAILURE,
  AUTHENTICATE_RESYNC
} authenticate_result_type_t;

/* 31.102 7.1.2.1 for fields
 * 33.102 6.3.7 for length of fields
 */
typedef struct {
  int res_length;                /* is 8 bytes for MILENAGE (35.206 Table 5) */
  unsigned char res[16];         /* but 33.102 6.3.7 says it's 4 to 16 bytes */
  unsigned char ck[16];
  unsigned char ik[16];
  bool kc_valid;
  unsigned char kc[8];
} authenticate_success_t;

/* 31.102 7.1.2.1 for fields
 * 33.102 6.3.7 for length of fields (AUTS is SQNMS xor AK (48b) | MAC-S (64b))
 */
typedef struct {
  unsigned char auts[14];
} authenticate_resync_t;

typedef struct {
  authenticate_result_type_t type;
  union {
    authenticate_success_t success;
    authenticate_resync_t resync;
  };
} authenticate_result_t;

#define MILLISECOND 1       /* the base time unit is ms */
#define SECOND 1000         /* one second is 1000 ms */

#define DEFAULT_TIMEOUT  (3 * SECOND)   /* 3s, arbitrary, change if needed */

/* commands (102.221 10.1.2) */
#define SELECT          0xa4
#define GET_RESPONSE    0xc0
#define READ_RECORD     0xb2
#define READ_BINARY     0xb0
#define STATUS          0xf2
#define VERIFY_PIN      0x20
#define AUTHENTICATE    0x88

#define MF       0x3f, 0x00        /* ISO 7816-4 5.3.1.1 */
#define EF_DIR   0x2f, 0x00        /* ISO 7916-4 8.2.1.1 */
#define EF_UST   0x6f, 0x38        /* 31.102 4.2.8 */
#define EF_EST   0x6f, 0x56        /* 31.102 4.2.47 */
#define EF_IMSI  0x6f, 0x07        /* 31.102 4.2.2 */
#define EF_AD    0x6f, 0xad        /* 31.102 4.2.18 */

/* 102.221 Table 11.1 */
#define SELECT_BY_FILE_ID     0x00
#define SELECT_BY_DF_NAME     0x04

/* 102.221 Table 11.2 */
#define SELECT_ACTIVATION_RETURN_FCP     0x04

/* 102.221 Table 11.11 */
#define READ_RECORD_ABSOLUTE             0x04

/* 102.221 Table 11.8 */
#define STATUS_INITIALIZED               0x01

/* 101.220 Annex E */
#define AID_USIM    0xa0, 0x00, 0x00, 0x00, 0x87, 0x10, 0x02

/* 102.221 Table 10.3 */
#define CLASS_80      0x80

/* dummy stuff */

#define STR(x) ((unsigned char []){ x })

#endif /* _DEF_H_ */
