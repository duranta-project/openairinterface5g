/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_refsig.h"
#include "nr_mod_table.h"
c16_t nr_qpsk_mod_table[4];
simde__m128i nr_qpsk_byte_mod_table[256];
int32_t nr_16qam_mod_table[16];
int64_t nr_16qam_byte_mod_table[256];
int64_t nr_64qam_mod_table[4096];
int32_t nr_256qam_mod_table[256];
int32_t nr_1024qam_mod_table[1024];

void nr_generate_modulation_table() {
  float sqrt2 = 0.70711;
  float sqrt10 = 0.31623;
  float sqrt42 = 0.15430;
  float sqrt170 = 0.076696;
  float sqrt682 = 0.038292;
  float val = 32768.0;
  uint32_t i,j;
  short* table;

  // QPSK
  for (i=0; i<4; i++) {
    nr_qpsk_mod_table[i].r = (short)(1 - 2 * (i & 1)) * val * sqrt2 * sqrt2;
    nr_qpsk_mod_table[i].i = (short)(1 - 2 * ((i >> 1) & 1)) * val * sqrt2 * sqrt2;
    //printf("%d j%d\n",nr_qpsk_mod_table[i*2],nr_qpsk_mod_table[i*2+1]);
  }

  //QPSK m128
  table = (short*) nr_qpsk_byte_mod_table;
  for (i=0; i<256; i++) {
    for (j=0; j<4; j++) {
      *table++ = (short)(1 - 2 * ((i >> (j * 2)) & 1)) * val * sqrt2 * sqrt2;
      *table++ = (short)(1 - 2 * ((i >> (j * 2 + 1)) & 1)) * val * sqrt2 * sqrt2;
      //printf("%d j%d\n",nr_qpsk_byte_mod_table[i*8+(j*2)],nr_qpsk_byte_mod_table[i*8+(j*2)+1]);
    }
  }

  //16QAM
  table = (short*) nr_16qam_byte_mod_table;
  for (i=0; i<256; i++) {
    for (j=0; j<2; j++) {
      *table++ = (short)((1 - 2 * ((i >> (j * 4)) & 1)) * (2 - (1 - 2 * ((i >> (j * 4 + 2)) & 1)))) * val * sqrt10 * sqrt2;
      *table++ = (short)((1 - 2 * ((i >> (j * 4 + 1)) & 1)) * (2 - (1 - 2 * ((i >> (j * 4 + 3)) & 1)))) * val * sqrt10 * sqrt2;
      // printf("%d j%d\n",nr_16qam_byte_mod_table[i*4+(j*2)],nr_16qam_byte_mod_table[i*4+(j*2)+1]);
    }
  }

  table = (short*) nr_16qam_mod_table;
  for (i=0; i<16; i++) {
    *table++ = (short)((1 - 2 * (i & 1)) * (2 - (1 - 2 * ((i >> 2) & 1)))) * val * sqrt10 * sqrt2;
    *table++ = (short)((1 - 2 * ((i >> 1) & 1)) * (2 - (1 - 2 * ((i >> 3) & 1)))) * val * sqrt10 * sqrt2;
    //printf("%d j%d\n",table[i*2],table[i*2+1]);
  }

  //64QAM
  table = (short*) nr_64qam_mod_table;
  for (i=0; i<4096; i++) {
    for (j=0; j<2; j++) {
      *table++ = (short)((1 - 2 * ((i >> (j * 6)) & 1))
                         * (4 - (1 - 2 * ((i >> (j * 6 + 2)) & 1)) * (2 - (1 - 2 * ((i >> (j * 6 + 4)) & 1)))))
                 * val * sqrt42 * sqrt2;
      *table++ = (short)((1 - 2 * ((i >> (j * 6 + 1)) & 1))
                         * (4 - (1 - 2 * ((i >> (j * 6 + 3)) & 1)) * (2 - (1 - 2 * ((i >> (j * 6 + 5)) & 1)))))
                 * val * sqrt42 * sqrt2;
      //printf("%d j%d\n",table[i*4+(j*2)],table[i*4+(j*2)+1]);
    }
  }

  //256QAM
  table = (short*) nr_256qam_mod_table;
  for (i=0; i<256; i++) {
    *table++ = (short)((1 - 2 * (i & 1))
                       * (8 - (1 - 2 * ((i >> 2) & 1)) * (4 - (1 - 2 * ((i >> 4) & 1)) * (2 - (1 - 2 * ((i >> 6) & 1))))))
               * val * sqrt170 * sqrt2;
    *table++ = (short)((1 - 2 * ((i >> 1) & 1))
                       * (8 - (1 - 2 * ((i >> 3) & 1)) * (4 - (1 - 2 * ((i >> 5) & 1)) * (2 - (1 - 2 * ((i >> 7) & 1))))))
               * val * sqrt170 * sqrt2;
  }

  // 1024QAM, TS 38.211 5.1.7
  table = (short*)nr_1024qam_mod_table;
  for (i = 0; i < 1024; i++) {
    for (j = 0; j < 2; j++) {
      const int b0 = 1 - 2 * ((i >> j) & 1);
      const int b2 = 1 - 2 * ((i >> (j + 2)) & 1);
      const int b4 = 1 - 2 * ((i >> (j + 4)) & 1);
      const int b6 = 1 - 2 * ((i >> (j + 6)) & 1);
      const int b8 = 1 - 2 * ((i >> (j + 8)) & 1);
      *table++ = (short)(b0 * (16 - b2 * (8 - b4 * (4 - b6 * (2 - b8))))) * val * sqrt682 * sqrt2;
    }
  }
}

