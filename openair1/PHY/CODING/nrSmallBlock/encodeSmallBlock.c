/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "PHY/CODING/nrSmallBlock/nr_small_block_defs.h"
#include "common/utils/assertions.h"

uint32_t encodeSmallBlock(uint32_t in, int len, int Qm)
{
  uint32_t out = 0;
  if (len == 1) {
    uint8_t bit = in & 1;
    if (Qm == 1)
      out = bit;
    else {
      // Table 5.3.3.1-1 of 38.212
      //   pos 0:  c0      (information bit)
      //   pos 1:  y       -> b(i-1) = bit
      //   pos 2+: x       -> 1
      // Resolved pattern: [bit, bit, 1, 1, ..., 1]
      out = bit ? 3 : 0; // bits 0 and 1
      if (Qm > 2)
        out |= ((1 << Qm) - 1) & ~3; // bits 2..Qm-1 set to 1
    }
  } else if (len == 2) {
    // Table 5.3.3.2-1 of 38.212
    uint8_t c0 = in & 1;
    uint8_t c1 = (in >> 1) & 1;
    uint8_t c2 = c0 ^ c1;
    if (Qm == 1) {
      // codeword [c0 c1 c2]
      out = c0 | (c1 << 1) | (c2 << 2);
    } else {
      // Each group of Qm bits:
      //   pos 0:   data bit (c0, c2, c1 for groups 0, 1, 2)
      //   pos 1:   data bit (c1, c0, c2 for groups 0, 1, 2)
      //   pos 2+:  x -> 1
      // Note: Table 5.3.3.2-1 contains no y placeholders for A=2.
      uint8_t data[3][2] = {{c0, c1}, {c2, c0}, {c1, c2}};
      for (int group = 0; group < 3; group++) {
        int base = group * Qm;
        out |= (data[group][0] << base);
        out |= (data[group][1] << (base + 1));
        for (int p = 2; p < Qm; p++)
          out |= (1 << (base + p));
      }
    }
  } else {
    for (int i = 0; i < len; i++)
      if ((in & (1U << i)) > 0)
        out ^= nrSmallBlockBasis[i];
  }
  return out;
}
