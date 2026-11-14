/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 16 bytes: needs VLEN >= 128
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 16)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 16)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 16)
// generated: BG2 Zc=144, rvv128-bit, factored
// 173 terms vs 1481 expanded (8.56x); 9 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc144_byte_rvv128(uint8_t *c,uint8_t *d) {
  uint8_t kd[1152] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<9; i2++) {
     const uint8_t *cb=c+16*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+2)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+18)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+94)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+313)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+393)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+407)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+628)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+696)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+898)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+955)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+957)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1173)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1255)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1261)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1443)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1505)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1800)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1811)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1842)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2064)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2113)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2372)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2414)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2431)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2641)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2670)),LGF_RVL((const uint8_t *)(cb+2718))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+16*i2),p0);
     LGF_RVS((uint8_t *)(d+1440+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+46)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+329)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1829)),LGF_RVL((const uint8_t *)(cb+2112))))));   //row 10
     LGF_RVS((uint8_t *)(d+2592+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+17)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1834)),LGF_RVL((const uint8_t *)(cb+2158)))));   //row 18
     LGF_RVS((uint8_t *)(d+3168+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+342)),LGF_RVL((const uint8_t *)(cb+708))));   //row 22
     LGF_RVS((uint8_t *)(d+3312+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+68)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+979)),LGF_RVL((const uint8_t *)(cb+1496)))));   //row 23
     LGF_RVS((uint8_t *)(d+3456+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+318)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+618)),LGF_RVL((const uint8_t *)(cb+2693)))));   //row 24
     LGF_RVS((uint8_t *)(d+3600+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+128)),LGF_RVL((const uint8_t *)(cb+1503))));   //row 25
     LGF_RVS((uint8_t *)(d+3888+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+13)),LGF_RVL((const uint8_t *)(cb+1738))));   //row 27
     LGF_RVS((uint8_t *)(d+4032+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+394)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+653)),LGF_RVL((const uint8_t *)(cb+1483)))));   //row 28
     LGF_RVS((uint8_t *)(d+4176+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+133)),LGF_RVL((const uint8_t *)(cb+1177))));   //row 29
     LGF_RVS((uint8_t *)(d+4320+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+663)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1496)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2120)),LGF_RVL((const uint8_t *)(cb+2662))))));   //row 30
     LGF_RVS((uint8_t *)(d+5184+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+121)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+705)),LGF_RVL((const uint8_t *)(cb+2042)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+16*(i2-1);
       uint8_t *dm=d+16*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+314)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+629)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+899)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1843)),LGF_RVL((const uint8_t *)(cm+2719))))))));
       LGF_RVS((uint8_t *)(dm+144),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+144)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+19)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+958)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1256)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1506)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1812)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2114)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2373)),LGF_RVL((const uint8_t *)(cm+2642))))))))));
       LGF_RVS((uint8_t *)(dm+288),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+288)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+95)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+394)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+956)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1262)),LGF_RVL((const uint8_t *)(cm+2415))))))));
       LGF_RVS((uint8_t *)(dm+432),p3);
     }
  }

  memcpy(kd,d,144); memcpy(kd+144,d,144);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+128, *km=kd+128; uint8_t *dm=d+128;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+314)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+629)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+899)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1843)),LGF_RVL((const uint8_t *)(cm+2719))))))));
    LGF_RVS((uint8_t *)(dm+144),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+144)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+19)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+958)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1256)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1506)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1812)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2114)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2373)),LGF_RVL((const uint8_t *)(cm+2642))))))))));
    LGF_RVS((uint8_t *)(dm+288),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+288)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+95)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+394)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+956)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1262)),LGF_RVL((const uint8_t *)(cm+2415))))))));
    LGF_RVS((uint8_t *)(dm+432),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*288,d+k*144,144); memcpy(kd+k*288+144,d+k*144,144); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<9; i2++) {
     const uint8_t *cb=c+16*i2, *kb=kd+16*i2;
     LGF_RVS((uint8_t *)(d+576+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+339)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+42)),LGF_RVL((const uint8_t *)(cb+312)))));   //row 4 [P1<<51 + 2 sys]
     LGF_RVS((uint8_t *)(d+720+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+359)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+40)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+428)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1524)),LGF_RVL((const uint8_t *)(cb+2153)))))));   //row 5 [P1<<71 + 4 sys]
     LGF_RVS((uint8_t *)(d+864+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+427)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+109)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1527)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2123)),LGF_RVL((const uint8_t *)(cb+2725)))))));   //row 6 [P1<<139 + 4 sys]
     LGF_RVS((uint8_t *)(d+1008+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+396)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+929)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+385)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1575)),LGF_RVL((const uint8_t *)(cb+2051)))))));   //row 7 [P1<<108+P3<<65 + 3 sys]
     LGF_RVS((uint8_t *)(d+1152+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+664)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+70)),LGF_RVL((const uint8_t *)(cb+357)))));   //row 8 [P2<<88 + 2 sys]
     LGF_RVS((uint8_t *)(d+1296+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+24)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+337)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+385)),LGF_RVL((const uint8_t *)(cb+2344))))));   //row 9 [P0<<24+P1<<49 + 2 sys]
     LGF_RVS((uint8_t *)(d+1584+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+928)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+28)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2046)),LGF_RVL((const uint8_t *)(cb+2708))))));   //row 11 [P3<<64 + 3 sys]
     LGF_RVS((uint8_t *)(d+1728+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+419)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+321)),LGF_RVL((const uint8_t *)(cb+986)))));   //row 12 [P1<<131 + 2 sys]
     LGF_RVS((uint8_t *)(d+1872+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+911)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+76)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+325)),LGF_RVL((const uint8_t *)(cb+2366))))));   //row 13 [P3<<47 + 3 sys]
     LGF_RVS((uint8_t *)(d+2016+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+418)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+961)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+431)),LGF_RVL((const uint8_t *)(cb+1779))))));   //row 14 [P1<<130+P3<<97 + 2 sys]
     LGF_RVS((uint8_t *)(d+2160+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+96)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+416)),LGF_RVL((const uint8_t *)(cb+139)))));   //row 15 [P0<<96+P1<<128 + 1 sys]
     LGF_RVS((uint8_t *)(d+2304+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+316)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+584)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+336)),LGF_RVL((const uint8_t *)(cb+2601))))));   //row 16 [P1<<28+P2<<8 + 2 sys]
     LGF_RVS((uint8_t *)(d+2448+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+353)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+618)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+408)),LGF_RVL((const uint8_t *)(cb+1483))))));   //row 17 [P1<<65+P2<<42 + 2 sys]
     LGF_RVS((uint8_t *)(d+2736+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+41)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+79)),LGF_RVL((const uint8_t *)(cb+316)))));   //row 19 [P0<<41 + 2 sys]
     LGF_RVS((uint8_t *)(d+2880+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+366)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+290)),LGF_RVL((const uint8_t *)(cb+1255)))));   //row 20 [P1<<78 + 2 sys]
     LGF_RVS((uint8_t *)(d+3024+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+945)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+91)),LGF_RVL((const uint8_t *)(cb+2379)))));   //row 21 [P3<<81 + 2 sys]
     LGF_RVS((uint8_t *)(d+3744+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+676)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+997)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+718)),LGF_RVL((const uint8_t *)(cb+2044))))));   //row 26 [P2<<100+P3<<133 + 2 sys]
     LGF_RVS((uint8_t *)(d+4464+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1003)),LGF_RVL((const uint8_t *)(cb+368))));   //row 31 [P3<<139 + 1 sys]
     LGF_RVS((uint8_t *)(d+4608+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+647)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+32)),LGF_RVL((const uint8_t *)(cb+1529)))));   //row 32 [P2<<71 + 2 sys]
     LGF_RVS((uint8_t *)(d+4752+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+711)),LGF_RVL((const uint8_t *)(cb+2022)))));   //row 33 [P0<<2 + 2 sys]
     LGF_RVS((uint8_t *)(d+4896+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+601)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+978)),LGF_RVL((const uint8_t *)(cb+37)))));   //row 34 [P2<<25+P3<<114 + 1 sys]
     LGF_RVS((uint8_t *)(d+5040+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+381)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+348)),LGF_RVL((const uint8_t *)(cb+1577)))));   //row 35 [P1<<93 + 2 sys]
     LGF_RVS((uint8_t *)(d+5328+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+97)),LGF_RVL((const uint8_t *)(kb+920))));   //row 37 [P0<<97+P3<<56 + 0 sys]
     LGF_RVS((uint8_t *)(d+5472+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+289)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+289)),LGF_RVL((const uint8_t *)(cb+1510)))));   //row 38 [P1<<1 + 2 sys]
     LGF_RVS((uint8_t *)(d+5616+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+718)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+119)),LGF_RVL((const uint8_t *)(cb+2048)))));   //row 39 [P2<<142 + 2 sys]
     LGF_RVS((uint8_t *)(d+5760+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+73)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+966)),LGF_RVL((const uint8_t *)(cb+582)))));   //row 40 [P0<<73+P3<<102 + 1 sys]
     LGF_RVS((uint8_t *)(d+5904+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+307)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+336)),LGF_RVL((const uint8_t *)(cb+1487)))));   //row 41 [P1<<19 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
