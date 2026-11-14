/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 32 bytes: needs VLEN >= 256
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 32)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 32)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 32)
// generated: BG2 Zc=352, rvv256-bit, factored
// 173 terms vs 1479 expanded (8.55x); 11 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc352_byte_rvv256(uint8_t *c,uint8_t *d) {
  uint8_t kd[2816] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<11; i2++) {
     const uint8_t *cb=c+32*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+16)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+97)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+155)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+756)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+846)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+871)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1421)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1581)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2114)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2176)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2218)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2878)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2897)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2989)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3646)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3520)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4240)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4263)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4278)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4964)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5016)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5648)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5773)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5802)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+6440)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+6458)),LGF_RVL((const uint8_t *)(cb+6468))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+32*i2),p0);
     LGF_RVS((uint8_t *)(d+3520+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+111)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+729)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4398)),LGF_RVL((const uint8_t *)(cb+4951))))));   //row 10
     LGF_RVS((uint8_t *)(d+6336+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+173)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4255)),LGF_RVL((const uint8_t *)(cb+4950)))));   //row 18
     LGF_RVS((uint8_t *)(d+7744+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+722)),LGF_RVL((const uint8_t *)(cb+1571))));   //row 22
     LGF_RVS((uint8_t *)(d+8096+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+110)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2244)),LGF_RVL((const uint8_t *)(cb+3670)))));   //row 23
     LGF_RVS((uint8_t *)(d+8448+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+817)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1516)),LGF_RVL((const uint8_t *)(cb+6397)))));   //row 24
     LGF_RVS((uint8_t *)(d+8800+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+72)),LGF_RVL((const uint8_t *)(cb+3656))));   //row 25
     LGF_RVS((uint8_t *)(d+9504+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+42)),LGF_RVL((const uint8_t *)(cb+4328))));   //row 27
     LGF_RVS((uint8_t *)(d+9856+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+768)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1432)),LGF_RVL((const uint8_t *)(cb+3669)))));   //row 28
     LGF_RVS((uint8_t *)(d+10208+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+139)),LGF_RVL((const uint8_t *)(cb+2977))));   //row 29
     LGF_RVS((uint8_t *)(d+10560+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1492)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3693)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5021)),LGF_RVL((const uint8_t *)(cb+6365))))));   //row 30
     LGF_RVS((uint8_t *)(d+12672+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+73)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1562)),LGF_RVL((const uint8_t *)(cb+5057)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+32*(i2-1);
       uint8_t *dm=d+32*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+156)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+847)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1422)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2115)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4264)),LGF_RVL((const uint8_t *)(cm+6459))))))));
       LGF_RVS((uint8_t *)(dm+352),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+352)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+17)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2177)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2879)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3521)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4279)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4965)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+5803)),LGF_RVL((const uint8_t *)(cm+6469))))))))));
       LGF_RVS((uint8_t *)(dm+704),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+704)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+98)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+872)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2219)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2898)),LGF_RVL((const uint8_t *)(cm+5774))))))));
       LGF_RVS((uint8_t *)(dm+1056),p3);
     }
  }

  memcpy(kd,d,352); memcpy(kd+352,d,352);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+320, *km=kd+320; uint8_t *dm=d+320;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+156)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+847)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1422)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2115)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4264)),LGF_RVL((const uint8_t *)(cm+6459))))))));
    LGF_RVS((uint8_t *)(dm+352),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+352)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+17)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2177)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2879)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3521)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4279)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4965)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+5803)),LGF_RVL((const uint8_t *)(cm+6469))))))))));
    LGF_RVS((uint8_t *)(dm+704),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+704)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+98)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+872)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2219)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2898)),LGF_RVL((const uint8_t *)(cm+5774))))))));
    LGF_RVS((uint8_t *)(dm+1056),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*704,d+k*352,352); memcpy(kd+k*704+352,d+k*352,352); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<11; i2++) {
     const uint8_t *cb=c+32*i2, *kb=kd+32*i2;
     LGF_RVS((uint8_t *)(d+1408+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+787)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+86)),LGF_RVL((const uint8_t *)(cb+771)))));   //row 4 [P1<<83 + 2 sys]
     LGF_RVS((uint8_t *)(d+1760+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+764)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+79)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+788)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3555)),LGF_RVL((const uint8_t *)(cb+5031)))))));   //row 5 [P1<<60 + 4 sys]
     LGF_RVS((uint8_t *)(d+2112+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+733)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+47)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3674)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4938)),LGF_RVL((const uint8_t *)(cb+6491)))))));   //row 6 [P1<<29 + 4 sys]
     LGF_RVS((uint8_t *)(d+2464+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+751)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2167)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+752)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3645)),LGF_RVL((const uint8_t *)(cb+4952)))))));   //row 7 [P1<<47+P3<<55 + 3 sys]
     LGF_RVS((uint8_t *)(d+2816+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1569)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+53)),LGF_RVL((const uint8_t *)(cb+735)))));   //row 8 [P2<<161 + 2 sys]
     LGF_RVS((uint8_t *)(d+3168+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+99)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+768)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+808)),LGF_RVL((const uint8_t *)(cb+5774))))));   //row 9 [P0<<99+P1<<64 + 2 sys]
     LGF_RVS((uint8_t *)(d+3872+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2253)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+91)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5103)),LGF_RVL((const uint8_t *)(cb+6360))))));   //row 11 [P3<<141 + 3 sys]
     LGF_RVS((uint8_t *)(d+4224+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+708)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+826)),LGF_RVL((const uint8_t *)(cb+2123)))));   //row 12 [P1<<4 + 2 sys]
     LGF_RVS((uint8_t *)(d+4576+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2239)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+29)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+795)),LGF_RVL((const uint8_t *)(cb+5659))))));   //row 13 [P3<<127 + 3 sys]
     LGF_RVS((uint8_t *)(d+4928+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+712)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2278)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+715)),LGF_RVL((const uint8_t *)(cb+4369))))));   //row 14 [P1<<8+P3<<166 + 2 sys]
     LGF_RVS((uint8_t *)(d+5280+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+103)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+744)),LGF_RVL((const uint8_t *)(cb+137)))));   //row 15 [P0<<103+P1<<40 + 1 sys]
     LGF_RVS((uint8_t *)(d+5632+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+721)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1573)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+782)),LGF_RVL((const uint8_t *)(cb+6494))))));   //row 16 [P1<<17+P2<<165 + 2 sys]
     LGF_RVS((uint8_t *)(d+5984+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+766)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1571)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+838)),LGF_RVL((const uint8_t *)(cb+3543))))));   //row 17 [P1<<62+P2<<163 + 2 sys]
     LGF_RVS((uint8_t *)(d+6688+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+145)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+13)),LGF_RVL((const uint8_t *)(cb+839)))));   //row 19 [P0<<145 + 2 sys]
     LGF_RVS((uint8_t *)(d+7040+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+877)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+832)),LGF_RVL((const uint8_t *)(cb+2868)))));   //row 20 [P1<<173 + 2 sys]
     LGF_RVS((uint8_t *)(d+7392+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2152)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+156)),LGF_RVL((const uint8_t *)(cb+5798)))));   //row 21 [P3<<40 + 2 sys]
     LGF_RVS((uint8_t *)(d+9152+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1461)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2257)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1444)),LGF_RVL((const uint8_t *)(cb+4966))))));   //row 26 [P2<<53+P3<<145 + 2 sys]
     LGF_RVS((uint8_t *)(d+10912+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2260)),LGF_RVL((const uint8_t *)(cb+821))));   //row 31 [P3<<148 + 1 sys]
     LGF_RVS((uint8_t *)(d+11264+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1550)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+116)),LGF_RVL((const uint8_t *)(cb+3593)))));   //row 32 [P2<<142 + 2 sys]
     LGF_RVS((uint8_t *)(d+11616+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+29)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1513)),LGF_RVL((const uint8_t *)(cb+5065)))));   //row 33 [P0<<29 + 2 sys]
     LGF_RVS((uint8_t *)(d+11968+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1449)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2274)),LGF_RVL((const uint8_t *)(cb+11)))));   //row 34 [P2<<41+P3<<162 + 1 sys]
     LGF_RVS((uint8_t *)(d+12320+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+876)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+830)),LGF_RVL((const uint8_t *)(cb+3672)))));   //row 35 [P1<<172 + 2 sys]
     LGF_RVS((uint8_t *)(d+13024+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+167)),LGF_RVL((const uint8_t *)(kb+2150))));   //row 37 [P0<<167+P3<<38 + 0 sys]
     LGF_RVS((uint8_t *)(d+13376+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+723)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+816)),LGF_RVL((const uint8_t *)(cb+3527)))));   //row 38 [P1<<19 + 2 sys]
     LGF_RVS((uint8_t *)(d+13728+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1513)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+109)),LGF_RVL((const uint8_t *)(cb+4934)))));   //row 39 [P2<<105 + 2 sys]
     LGF_RVS((uint8_t *)(d+14080+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+156)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2194)),LGF_RVL((const uint8_t *)(cb+1568)))));   //row 40 [P0<<156+P3<<82 + 1 sys]
     LGF_RVS((uint8_t *)(d+14432+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+712)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+836)),LGF_RVL((const uint8_t *)(cb+3526)))));   //row 41 [P1<<8 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
