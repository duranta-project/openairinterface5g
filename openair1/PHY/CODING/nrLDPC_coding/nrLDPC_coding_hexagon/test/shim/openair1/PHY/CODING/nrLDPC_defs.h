/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
/* Stand-in for openair1/PHY/CODING/nrLDPC_defs.h, carrying only the encoder
 * parameters that LDPCencoder() reads (gen_code 0). The real header pulls in the
 * PHY defs and the thread pool, which do not build for the standalone runtime. */
#ifndef LDPC_SIMTEST_NRLDPC_DEFS_H
#define LDPC_SIMTEST_NRLDPC_DEFS_H
#include <stdint.h>
typedef struct {
  unsigned char gen_code;
  uint32_t K;
  uint32_t Kb;
  uint32_t Zc;
  uint32_t F;
  uint8_t BG;
} encoder_implemparams_t;
#endif
