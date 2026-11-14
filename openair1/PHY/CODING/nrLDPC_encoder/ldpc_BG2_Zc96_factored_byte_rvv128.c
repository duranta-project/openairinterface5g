/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 16 bytes: needs VLEN >= 128
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 16)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 16)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 16)
// generated: BG2 Zc=96, rvv128-bit, factored
// 173 terms vs 1461 expanded (8.45x); 6 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc96_byte_rvv128(uint8_t *c,uint8_t *d) {
  uint8_t kd[768] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<6; i2++) {
     const uint8_t *cb=c+16*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+24)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+26)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+77)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+209)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+231)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+192)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+453)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+462)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+596)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+611)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+641)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+781)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+784)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+815)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1031)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1051)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1178)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1182)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1222)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1365)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1434)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1553)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1563)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1624)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1730)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1803)),LGF_RVL((const uint8_t *)(cb+1817))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+16*i2),p0);
     LGF_RVS((uint8_t *)(d+960+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+59)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+200)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1174)),LGF_RVL((const uint8_t *)(cb+1396))))));   //row 10
     LGF_RVS((uint8_t *)(d+1728+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+90)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1158)),LGF_RVL((const uint8_t *)(cb+1390)))));   //row 18
     LGF_RVS((uint8_t *)(d+2112+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+212)),LGF_RVL((const uint8_t *)(cb+436))));   //row 22
     LGF_RVS((uint8_t *)(d+2208+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+10)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+662)),LGF_RVL((const uint8_t *)(cb+1055)))));   //row 23
     LGF_RVS((uint8_t *)(d+2304+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+278)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+441)),LGF_RVL((const uint8_t *)(cb+1792)))));   //row 24
     LGF_RVS((uint8_t *)(d+2400+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+45)),LGF_RVL((const uint8_t *)(cb+981))));   //row 25
     LGF_RVS((uint8_t *)(d+2592+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+7)),LGF_RVL((const uint8_t *)(cb+1202))));   //row 27
     LGF_RVS((uint8_t *)(d+2688+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+262)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+399)),LGF_RVL((const uint8_t *)(cb+1032)))));   //row 28
     LGF_RVS((uint8_t *)(d+2784+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+14)),LGF_RVL((const uint8_t *)(cb+785))));   //row 29
     LGF_RVS((uint8_t *)(d+2880+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+408)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1018)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1396)),LGF_RVL((const uint8_t *)(cb+1784))))));   //row 30
     LGF_RVS((uint8_t *)(d+3456+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+25)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+439)),LGF_RVL((const uint8_t *)(cb+1418)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+16*(i2-1);
       uint8_t *dm=d+16*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+78)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+193)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+454)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+642)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1223)),LGF_RVL((const uint8_t *)(cm+1804))))))));
       LGF_RVS((uint8_t *)(dm+96),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+96)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+27)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+612)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+816)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1052)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1183)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1435)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1625)),LGF_RVL((const uint8_t *)(cm+1731))))))))));
       LGF_RVS((uint8_t *)(dm+192),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+192)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+25)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+210)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+597)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+782)),LGF_RVL((const uint8_t *)(cm+1554))))))));
       LGF_RVS((uint8_t *)(dm+288),p3);
     }
  }

  memcpy(kd,d,96); memcpy(kd+96,d,96);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+80, *km=kd+80; uint8_t *dm=d+80;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+78)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+193)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+454)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+642)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1223)),LGF_RVL((const uint8_t *)(cm+1804))))))));
    LGF_RVS((uint8_t *)(dm+96),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+96)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+27)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+612)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+816)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1052)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1183)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1435)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1625)),LGF_RVL((const uint8_t *)(cm+1731))))))))));
    LGF_RVS((uint8_t *)(dm+192),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+192)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+25)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+210)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+597)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+782)),LGF_RVL((const uint8_t *)(cm+1554))))))));
    LGF_RVS((uint8_t *)(dm+288),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*192,d+k*96,96); memcpy(kd+k*192+96,d+k*96,96); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<6; i2++) {
     const uint8_t *cb=c+16*i2, *kb=kd+16*i2;
     LGF_RVS((uint8_t *)(d+384+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+221)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+72)),LGF_RVL((const uint8_t *)(cb+266)))));   //row 4 [P1<<29 + 2 sys]
     LGF_RVS((uint8_t *)(d+480+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+240)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+10)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+236)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+985)),LGF_RVL((const uint8_t *)(cb+1424)))))));   //row 5 [P1<<48 + 4 sys]
     LGF_RVS((uint8_t *)(d+576+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+280)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+33)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1052)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1348)),LGF_RVL((const uint8_t *)(cb+1777)))))));   //row 6 [P1<<88 + 4 sys]
     LGF_RVS((uint8_t *)(d+672+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+198)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+623)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+272)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1050)),LGF_RVL((const uint8_t *)(cb+1360)))))));   //row 7 [P1<<6+P3<<47 + 3 sys]
     LGF_RVS((uint8_t *)(d+768+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+440)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+22)),LGF_RVL((const uint8_t *)(cb+262)))));   //row 8 [P2<<56 + 2 sys]
     LGF_RVS((uint8_t *)(d+864+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+89)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+274)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+220)),LGF_RVL((const uint8_t *)(cb+1572))))));   //row 9 [P0<<89+P1<<82 + 2 sys]
     LGF_RVS((uint8_t *)(d+1056+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+634)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+32)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1436)),LGF_RVL((const uint8_t *)(cb+1806))))));   //row 11 [P3<<58 + 3 sys]
     LGF_RVS((uint8_t *)(d+1152+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+203)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+231)),LGF_RVL((const uint8_t *)(cb+669)))));   //row 12 [P1<<11 + 2 sys]
     LGF_RVS((uint8_t *)(d+1248+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+646)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+49)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+221)),LGF_RVL((const uint8_t *)(cb+1571))))));   //row 13 [P3<<70 + 3 sys]
     LGF_RVS((uint8_t *)(d+1344+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+213)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+643)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+211)),LGF_RVL((const uint8_t *)(cb+1174))))));   //row 14 [P1<<21+P3<<67 + 2 sys]
     LGF_RVS((uint8_t *)(d+1440+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+63)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+273)),LGF_RVL((const uint8_t *)(cb+68)))));   //row 15 [P0<<63+P1<<81 + 1 sys]
     LGF_RVS((uint8_t *)(d+1536+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+231)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+448)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+279)),LGF_RVL((const uint8_t *)(cb+1809))))));   //row 16 [P1<<39+P2<<64 + 2 sys]
     LGF_RVS((uint8_t *)(d+1632+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+201)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+390)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+254)),LGF_RVL((const uint8_t *)(cb+983))))));   //row 17 [P1<<9+P2<<6 + 2 sys]
     LGF_RVS((uint8_t *)(d+1824+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+60)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+58)),LGF_RVL((const uint8_t *)(cb+234)))));   //row 19 [P0<<60 + 2 sys]
     LGF_RVS((uint8_t *)(d+1920+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+249)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+268)),LGF_RVL((const uint8_t *)(cb+829)))));   //row 20 [P1<<57 + 2 sys]
     LGF_RVS((uint8_t *)(d+2016+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+643)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+61)),LGF_RVL((const uint8_t *)(cb+1615)))));   //row 21 [P3<<67 + 2 sys]
     LGF_RVS((uint8_t *)(d+2496+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+439)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+661)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+451)),LGF_RVL((const uint8_t *)(cb+1385))))));   //row 26 [P2<<55+P3<<85 + 2 sys]
     LGF_RVS((uint8_t *)(d+2976+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+650)),LGF_RVL((const uint8_t *)(cb+195))));   //row 31 [P3<<74 + 1 sys]
     LGF_RVS((uint8_t *)(d+3072+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+401)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+84)),LGF_RVL((const uint8_t *)(cb+968)))));   //row 32 [P2<<17 + 2 sys]
     LGF_RVS((uint8_t *)(d+3168+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+28)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+453)),LGF_RVL((const uint8_t *)(cb+1427)))));   //row 33 [P0<<28 + 2 sys]
     LGF_RVS((uint8_t *)(d+3264+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+465)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+588)),LGF_RVL((const uint8_t *)(cb+77)))));   //row 34 [P2<<81+P3<<12 + 1 sys]
     LGF_RVS((uint8_t *)(d+3360+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+210)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+269)),LGF_RVL((const uint8_t *)(cb+1048)))));   //row 35 [P1<<18 + 2 sys]
     LGF_RVS((uint8_t *)(d+3552+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+37)),LGF_RVL((const uint8_t *)(kb+607))));   //row 37 [P0<<37+P3<<31 + 0 sys]
     LGF_RVS((uint8_t *)(d+3648+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+286)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+276)),LGF_RVL((const uint8_t *)(cb+1015)))));   //row 38 [P1<<94 + 2 sys]
     LGF_RVS((uint8_t *)(d+3744+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+441)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+93)),LGF_RVL((const uint8_t *)(cb+1380)))));   //row 39 [P2<<57 + 2 sys]
     LGF_RVS((uint8_t *)(d+3840+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+11)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+643)),LGF_RVL((const uint8_t *)(cb+391)))));   //row 40 [P0<<11+P3<<67 + 1 sys]
     LGF_RVS((uint8_t *)(d+3936+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+252)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+243)),LGF_RVL((const uint8_t *)(cb+967)))));   //row 41 [P1<<60 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
