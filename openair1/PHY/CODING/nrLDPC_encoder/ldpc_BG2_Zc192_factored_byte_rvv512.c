/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 64 bytes: needs VLEN >= 512
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 64)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 64)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 64)
// generated: BG2 Zc=192, rvv512-bit, factored
// 173 terms vs 1469 expanded (8.49x); 3 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc192_byte_rvv512(uint8_t *c,uint8_t *d) {
  uint8_t kd[1536] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<3; i2++) {
     const uint8_t *cb=c+64*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+24)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+26)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+173)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+480)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+497)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+519)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+933)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+942)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1187)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1217)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1268)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1583)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1645)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1648)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1991)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2011)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2334)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2374)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2426)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2805)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2874)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3099)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3185)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3256)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3458)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3627)),LGF_RVL((const uint8_t *)(cb+3641))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+64*i2),p0);
     LGF_RVS((uint8_t *)(d+1920+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+59)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+488)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2326)),LGF_RVL((const uint8_t *)(cb+2740))))));   //row 10
     LGF_RVS((uint8_t *)(d+3456+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+186)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2310)),LGF_RVL((const uint8_t *)(cb+2734)))));   //row 18
     LGF_RVS((uint8_t *)(d+4224+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+404)),LGF_RVL((const uint8_t *)(cb+820))));   //row 22
     LGF_RVS((uint8_t *)(d+4416+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+106)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1238)),LGF_RVL((const uint8_t *)(cb+2015)))));   //row 23
     LGF_RVS((uint8_t *)(d+4608+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+566)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+921)),LGF_RVL((const uint8_t *)(cb+3520)))));   //row 24
     LGF_RVS((uint8_t *)(d+4800+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+45)),LGF_RVL((const uint8_t *)(cb+1941))));   //row 25
     LGF_RVS((uint8_t *)(d+5184+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+103)),LGF_RVL((const uint8_t *)(cb+2354))));   //row 27
     LGF_RVS((uint8_t *)(d+5376+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+454)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+879)),LGF_RVL((const uint8_t *)(cb+2088)))));   //row 28
     LGF_RVS((uint8_t *)(d+5568+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+110)),LGF_RVL((const uint8_t *)(cb+1553))));   //row 29
     LGF_RVS((uint8_t *)(d+5760+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+888)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2074)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2740)),LGF_RVL((const uint8_t *)(cb+3512))))));   //row 30
     LGF_RVS((uint8_t *)(d+6912+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+25)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+919)),LGF_RVL((const uint8_t *)(cb+2858)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+64*(i2-1);
       uint8_t *dm=d+64*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+174)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+481)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+934)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1218)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2375)),LGF_RVL((const uint8_t *)(cm+3628))))))));
       LGF_RVS((uint8_t *)(dm+192),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+192)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+27)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1188)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1584)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2012)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2335)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2875)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3257)),LGF_RVL((const uint8_t *)(cm+3459))))))))));
       LGF_RVS((uint8_t *)(dm+384),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+384)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+25)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+498)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1269)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1646)),LGF_RVL((const uint8_t *)(cm+3186))))))));
       LGF_RVS((uint8_t *)(dm+576),p3);
     }
  }

  memcpy(kd,d,192); memcpy(kd+192,d,192);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+128, *km=kd+128; uint8_t *dm=d+128;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+174)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+481)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+934)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1218)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2375)),LGF_RVL((const uint8_t *)(cm+3628))))))));
    LGF_RVS((uint8_t *)(dm+192),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+192)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+27)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1188)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1584)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2012)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2335)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2875)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3257)),LGF_RVL((const uint8_t *)(cm+3459))))))))));
    LGF_RVS((uint8_t *)(dm+384),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+384)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+25)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+498)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1269)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1646)),LGF_RVL((const uint8_t *)(cm+3186))))))));
    LGF_RVS((uint8_t *)(dm+576),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*384,d+k*192,192); memcpy(kd+k*384+192,d+k*192,192); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<3; i2++) {
     const uint8_t *cb=c+64*i2, *kb=kd+64*i2;
     LGF_RVS((uint8_t *)(d+768+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+413)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+72)),LGF_RVL((const uint8_t *)(cb+458)))));   //row 4 [P1<<29 + 2 sys]
     LGF_RVS((uint8_t *)(d+960+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+432)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+10)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+428)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2041)),LGF_RVL((const uint8_t *)(cb+2768)))))));   //row 5 [P1<<48 + 4 sys]
     LGF_RVS((uint8_t *)(d+1152+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+568)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+129)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2012)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2788)),LGF_RVL((const uint8_t *)(cb+3505)))))));   //row 6 [P1<<184 + 4 sys]
     LGF_RVS((uint8_t *)(d+1344+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+486)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1295)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+464)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2106)),LGF_RVL((const uint8_t *)(cb+2704)))))));   //row 7 [P1<<102+P3<<143 + 3 sys]
     LGF_RVS((uint8_t *)(d+1536+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+920)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+118)),LGF_RVL((const uint8_t *)(cb+454)))));   //row 8 [P2<<152 + 2 sys]
     LGF_RVS((uint8_t *)(d+1728+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+185)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+562)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+412)),LGF_RVL((const uint8_t *)(cb+3204))))));   //row 9 [P0<<185+P1<<178 + 2 sys]
     LGF_RVS((uint8_t *)(d+2112+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1306)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+32)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2780)),LGF_RVL((const uint8_t *)(cb+3630))))));   //row 11 [P3<<154 + 3 sys]
     LGF_RVS((uint8_t *)(d+2304+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+395)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+423)),LGF_RVL((const uint8_t *)(cb+1245)))));   //row 12 [P1<<11 + 2 sys]
     LGF_RVS((uint8_t *)(d+2496+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1318)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+49)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+509)),LGF_RVL((const uint8_t *)(cb+3107))))));   //row 13 [P3<<166 + 3 sys]
     LGF_RVS((uint8_t *)(d+2688+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+405)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1315)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+403)),LGF_RVL((const uint8_t *)(cb+2422))))));   //row 14 [P1<<21+P3<<163 + 2 sys]
     LGF_RVS((uint8_t *)(d+2880+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+63)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+465)),LGF_RVL((const uint8_t *)(cb+68)))));   //row 15 [P0<<63+P1<<81 + 1 sys]
     LGF_RVS((uint8_t *)(d+3072+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+519)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+832)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+471)),LGF_RVL((const uint8_t *)(cb+3633))))));   //row 16 [P1<<135+P2<<64 + 2 sys]
     LGF_RVS((uint8_t *)(d+3264+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+393)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+774)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+542)),LGF_RVL((const uint8_t *)(cb+1943))))));   //row 17 [P1<<9+P2<<6 + 2 sys]
     LGF_RVS((uint8_t *)(d+3648+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+156)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+58)),LGF_RVL((const uint8_t *)(cb+426)))));   //row 19 [P0<<156 + 2 sys]
     LGF_RVS((uint8_t *)(d+3840+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+537)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+460)),LGF_RVL((const uint8_t *)(cb+1597)))));   //row 20 [P1<<153 + 2 sys]
     LGF_RVS((uint8_t *)(d+4032+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1219)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+157)),LGF_RVL((const uint8_t *)(cb+3247)))));   //row 21 [P3<<67 + 2 sys]
     LGF_RVS((uint8_t *)(d+4992+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+823)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1237)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+835)),LGF_RVL((const uint8_t *)(cb+2825))))));   //row 26 [P2<<55+P3<<85 + 2 sys]
     LGF_RVS((uint8_t *)(d+5952+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1322)),LGF_RVL((const uint8_t *)(cb+387))));   //row 31 [P3<<170 + 1 sys]
     LGF_RVS((uint8_t *)(d+6144+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+785)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+84)),LGF_RVL((const uint8_t *)(cb+1928)))));   //row 32 [P2<<17 + 2 sys]
     LGF_RVS((uint8_t *)(d+6336+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+124)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+933)),LGF_RVL((const uint8_t *)(cb+2867)))));   //row 33 [P0<<124 + 2 sys]
     LGF_RVS((uint8_t *)(d+6528+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+945)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1164)),LGF_RVL((const uint8_t *)(cb+173)))));   //row 34 [P2<<177+P3<<12 + 1 sys]
     LGF_RVS((uint8_t *)(d+6720+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+402)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+461)),LGF_RVL((const uint8_t *)(cb+2104)))));   //row 35 [P1<<18 + 2 sys]
     LGF_RVS((uint8_t *)(d+7104+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+37)),LGF_RVL((const uint8_t *)(kb+1183))));   //row 37 [P0<<37+P3<<31 + 0 sys]
     LGF_RVS((uint8_t *)(d+7296+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+574)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+468)),LGF_RVL((const uint8_t *)(cb+2071)))));   //row 38 [P1<<190 + 2 sys]
     LGF_RVS((uint8_t *)(d+7488+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+825)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+93)),LGF_RVL((const uint8_t *)(cb+2820)))));   //row 39 [P2<<57 + 2 sys]
     LGF_RVS((uint8_t *)(d+7680+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+107)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1315)),LGF_RVL((const uint8_t *)(cb+871)))));   //row 40 [P0<<107+P3<<163 + 1 sys]
     LGF_RVS((uint8_t *)(d+7872+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+444)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+531)),LGF_RVL((const uint8_t *)(cb+1927)))));   //row 41 [P1<<60 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
