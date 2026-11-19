/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _REMOTE_SIM_H_
#define _REMOTE_SIM_H_

#include <stdbool.h>

typedef enum {
  AUTHENTICATE_SUCCESS,
  AUTHENTICATE_FAILURE,
  AUTHENTICATE_RESYNC
} remote_sim_authenticate_result_type_t;

/* 31.102 7.1.2.1 for fields
 * 33.102 6.3.7 for length of fields
 */
typedef struct {
  int res_length;                /* is 8 bytes for MILENAGE (35.206 Table 5) */
  unsigned char res[16];         /* but 33.102 6.3.7 says it's 4 to 16 bytes */
  unsigned char ck[16];
  unsigned char ik[16];
} remote_sim_authenticate_success_t;

/* 31.102 7.1.2.1 for fields
 * 33.102 6.3.7 for length of fields (AUTS is SQNMS xor AK (48b) | MAC-S (64b))
 */
typedef struct {
  unsigned char auts[14];
} remote_sim_authenticate_resync_t;

typedef struct {
  remote_sim_authenticate_result_type_t type;
  union {
    remote_sim_authenticate_success_t success;
    remote_sim_authenticate_resync_t resync;
  };
} remote_sim_authenticate_result_t;

void remote_sim_init(char *server_port);
bool remote_sim_enabled(void);
remote_sim_authenticate_result_t remote_sim_authenticate(unsigned char *rand, unsigned char *autn);

#endif /* _REMOTE_SIM_H_ */
