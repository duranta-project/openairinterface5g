/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 32 bytes: needs VLEN >= 256
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 32)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 32)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 32)
// generated: BG2 Zc=288, rvv256-bit, factored
// 173 terms vs 1481 expanded (8.56x); 9 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc288_byte_rvv256(uint8_t *c,uint8_t *d) {
  uint8_t kd[2304] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<9; i2++) {
     const uint8_t *cb=c+32*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+2)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+18)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+94)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+601)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+681)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+695)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1204)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1272)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1762)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1819)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1821)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2325)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2407)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2413)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2883)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2945)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3528)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3539)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3570)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4080)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4129)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4676)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4718)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4735)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5233)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5262)),LGF_RVL((const uint8_t *)(cb+5310))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+32*i2),p0);
     LGF_RVS((uint8_t *)(d+2880+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+46)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+617)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3557)),LGF_RVL((const uint8_t *)(cb+4128))))));   //row 10
     LGF_RVS((uint8_t *)(d+5184+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+17)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3562)),LGF_RVL((const uint8_t *)(cb+4174)))));   //row 18
     LGF_RVS((uint8_t *)(d+6336+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+630)),LGF_RVL((const uint8_t *)(cb+1284))));   //row 22
     LGF_RVS((uint8_t *)(d+6624+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+68)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1843)),LGF_RVL((const uint8_t *)(cb+2936)))));   //row 23
     LGF_RVS((uint8_t *)(d+6912+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+606)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1194)),LGF_RVL((const uint8_t *)(cb+5285)))));   //row 24
     LGF_RVS((uint8_t *)(d+7200+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+128)),LGF_RVL((const uint8_t *)(cb+2943))));   //row 25
     LGF_RVS((uint8_t *)(d+7776+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+13)),LGF_RVL((const uint8_t *)(cb+3466))));   //row 27
     LGF_RVS((uint8_t *)(d+8064+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+682)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1229)),LGF_RVL((const uint8_t *)(cb+2923)))));   //row 28
     LGF_RVS((uint8_t *)(d+8352+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+133)),LGF_RVL((const uint8_t *)(cb+2329))));   //row 29
     LGF_RVS((uint8_t *)(d+8640+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1239)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2936)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4136)),LGF_RVL((const uint8_t *)(cb+5254))))));   //row 30
     LGF_RVS((uint8_t *)(d+10368+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+121)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1281)),LGF_RVL((const uint8_t *)(cb+4058)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+32*(i2-1);
       uint8_t *dm=d+32*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+602)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1205)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1763)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3571)),LGF_RVL((const uint8_t *)(cm+5311))))))));
       LGF_RVS((uint8_t *)(dm+288),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+288)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+19)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1822)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2408)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2946)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3540)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4130)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4677)),LGF_RVL((const uint8_t *)(cm+5234))))))))));
       LGF_RVS((uint8_t *)(dm+576),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+576)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+95)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+682)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1820)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2414)),LGF_RVL((const uint8_t *)(cm+4719))))))));
       LGF_RVS((uint8_t *)(dm+864),p3);
     }
  }

  memcpy(kd,d,288); memcpy(kd+288,d,288);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+256, *km=kd+256; uint8_t *dm=d+256;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+602)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1205)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1763)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3571)),LGF_RVL((const uint8_t *)(cm+5311))))))));
    LGF_RVS((uint8_t *)(dm+288),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+288)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+19)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1822)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2408)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2946)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3540)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4130)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4677)),LGF_RVL((const uint8_t *)(cm+5234))))))))));
    LGF_RVS((uint8_t *)(dm+576),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+576)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+95)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+682)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1820)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2414)),LGF_RVL((const uint8_t *)(cm+4719))))))));
    LGF_RVS((uint8_t *)(dm+864),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*576,d+k*288,288); memcpy(kd+k*576+288,d+k*288,288); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<9; i2++) {
     const uint8_t *cb=c+32*i2, *kb=kd+32*i2;
     LGF_RVS((uint8_t *)(d+1152+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+627)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+42)),LGF_RVL((const uint8_t *)(cb+600)))));   //row 4 [P1<<51 + 2 sys]
     LGF_RVS((uint8_t *)(d+1440+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+647)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+40)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+716)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2964)),LGF_RVL((const uint8_t *)(cb+4169)))))));   //row 5 [P1<<71 + 4 sys]
     LGF_RVS((uint8_t *)(d+1728+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+715)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+109)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2967)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4139)),LGF_RVL((const uint8_t *)(cb+5317)))))));   //row 6 [P1<<139 + 4 sys]
     LGF_RVS((uint8_t *)(d+2016+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+684)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1793)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+673)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3015)),LGF_RVL((const uint8_t *)(cb+4067)))))));   //row 7 [P1<<108+P3<<65 + 3 sys]
     LGF_RVS((uint8_t *)(d+2304+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1240)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+70)),LGF_RVL((const uint8_t *)(cb+645)))));   //row 8 [P2<<88 + 2 sys]
     LGF_RVS((uint8_t *)(d+2592+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+24)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+625)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+673)),LGF_RVL((const uint8_t *)(cb+4648))))));   //row 9 [P0<<24+P1<<49 + 2 sys]
     LGF_RVS((uint8_t *)(d+3168+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1792)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+28)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4062)),LGF_RVL((const uint8_t *)(cb+5300))))));   //row 11 [P3<<64 + 3 sys]
     LGF_RVS((uint8_t *)(d+3456+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+707)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+609)),LGF_RVL((const uint8_t *)(cb+1850)))));   //row 12 [P1<<131 + 2 sys]
     LGF_RVS((uint8_t *)(d+3744+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1775)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+76)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+613)),LGF_RVL((const uint8_t *)(cb+4670))))));   //row 13 [P3<<47 + 3 sys]
     LGF_RVS((uint8_t *)(d+4032+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+706)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1825)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+719)),LGF_RVL((const uint8_t *)(cb+3507))))));   //row 14 [P1<<130+P3<<97 + 2 sys]
     LGF_RVS((uint8_t *)(d+4320+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+96)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+704)),LGF_RVL((const uint8_t *)(cb+139)))));   //row 15 [P0<<96+P1<<128 + 1 sys]
     LGF_RVS((uint8_t *)(d+4608+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+604)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1160)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+624)),LGF_RVL((const uint8_t *)(cb+5193))))));   //row 16 [P1<<28+P2<<8 + 2 sys]
     LGF_RVS((uint8_t *)(d+4896+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+641)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1194)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+696)),LGF_RVL((const uint8_t *)(cb+2923))))));   //row 17 [P1<<65+P2<<42 + 2 sys]
     LGF_RVS((uint8_t *)(d+5472+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+41)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+79)),LGF_RVL((const uint8_t *)(cb+604)))));   //row 19 [P0<<41 + 2 sys]
     LGF_RVS((uint8_t *)(d+5760+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+654)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+578)),LGF_RVL((const uint8_t *)(cb+2407)))));   //row 20 [P1<<78 + 2 sys]
     LGF_RVS((uint8_t *)(d+6048+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1809)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+91)),LGF_RVL((const uint8_t *)(cb+4683)))));   //row 21 [P3<<81 + 2 sys]
     LGF_RVS((uint8_t *)(d+7488+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1252)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1861)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1294)),LGF_RVL((const uint8_t *)(cb+4060))))));   //row 26 [P2<<100+P3<<133 + 2 sys]
     LGF_RVS((uint8_t *)(d+8928+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1867)),LGF_RVL((const uint8_t *)(cb+656))));   //row 31 [P3<<139 + 1 sys]
     LGF_RVS((uint8_t *)(d+9216+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1223)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+32)),LGF_RVL((const uint8_t *)(cb+2969)))));   //row 32 [P2<<71 + 2 sys]
     LGF_RVS((uint8_t *)(d+9504+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1287)),LGF_RVL((const uint8_t *)(cb+4038)))));   //row 33 [P0<<2 + 2 sys]
     LGF_RVS((uint8_t *)(d+9792+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1177)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1842)),LGF_RVL((const uint8_t *)(cb+37)))));   //row 34 [P2<<25+P3<<114 + 1 sys]
     LGF_RVS((uint8_t *)(d+10080+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+669)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+636)),LGF_RVL((const uint8_t *)(cb+3017)))));   //row 35 [P1<<93 + 2 sys]
     LGF_RVS((uint8_t *)(d+10656+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+97)),LGF_RVL((const uint8_t *)(kb+1784))));   //row 37 [P0<<97+P3<<56 + 0 sys]
     LGF_RVS((uint8_t *)(d+10944+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+577)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+577)),LGF_RVL((const uint8_t *)(cb+2950)))));   //row 38 [P1<<1 + 2 sys]
     LGF_RVS((uint8_t *)(d+11232+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1294)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+119)),LGF_RVL((const uint8_t *)(cb+4064)))));   //row 39 [P2<<142 + 2 sys]
     LGF_RVS((uint8_t *)(d+11520+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+73)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1830)),LGF_RVL((const uint8_t *)(cb+1158)))));   //row 40 [P0<<73+P3<<102 + 1 sys]
     LGF_RVS((uint8_t *)(d+11808+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+595)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+624)),LGF_RVL((const uint8_t *)(cb+2927)))));   //row 41 [P1<<19 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
