/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __openair_GOLD_H__
#define __openair_GOLD_H__

#include <stdint.h>

static inline  uint32_t gold_generic(uint32_t *x1, uint32_t *x2, uint8_t reset)
{
  int32_t n;
  // 3GPP 3x.211
  // Nc = 1600
  // c(n)     = [x1(n+Nc) + x2(n+Nc)]mod2
  // x1(n+31) = [x1(n+3)                     + x1(n)]mod2
  // x2(n+31) = [x2(n+3) + x2(n+2) + x2(n+1) + x2(n)]mod2
  if (reset) {
    // Init value for x1: x1(0) = 1, x1(n) = 0, n=1,2,...,30
    // x1(31) = [x1(3) + x1(0)]mod2 = 1
    *x1 = 1 + (1U << 31);
    // Init value for x2: cinit = sum_{i=0}^30 x2*2^i
    // x2(31) = [x2(3)    + x2(2)    + x2(1)    + x2(0)]mod2
    //        =  (*x2>>3) ^ (*x2>>2) + (*x2>>1) + *x2
    *x2 = *x2 ^ ((*x2 ^ (*x2 >> 1) ^ (*x2 >> 2) ^ (*x2 >> 3)) << 31);
    // x1 and x2 contain bits n = 0,1,...,31
    // Nc = 1600 bits are skipped at the beginning
    // i.e., 1600 / 32 = 50 32bit words
    for (n = 1; n < 50; n++) {
      // Compute x1(0),...,x1(27)
      *x1 = (*x1 >> 1) ^ (*x1 >> 4);
      // Compute x1(28),..,x1(31) and xor
      *x1 = *x1 ^ (*x1 << 31) ^ (*x1 << 28);
      // Compute x2(0),...,x2(27)
      *x2 = (*x2 >> 1) ^ (*x2 >> 2) ^ (*x2 >> 3) ^ (*x2 >> 4);
      // Compute x2(28),..,x2(31) and xor
      *x2 = *x2 ^ (*x2 << 31) ^ (*x2 << 30) ^ (*x2 << 29) ^ (*x2 << 28);
    }
  }
  *x1 = (*x1 >> 1) ^ (*x1 >> 4);
  *x1 = *x1 ^ (*x1 << 31) ^ (*x1 << 28);
  *x2 = (*x2 >> 1) ^ (*x2 >> 2) ^ (*x2 >> 3) ^ (*x2 >> 4);
  *x2 = *x2 ^ (*x2 << 31) ^ (*x2 << 30) ^ (*x2 << 29) ^ (*x2 << 28);
  // c(n) = [x1(n+Nc) + x2(n+Nc)]mod2
  return(*x1 ^ *x2);
}

#endif
