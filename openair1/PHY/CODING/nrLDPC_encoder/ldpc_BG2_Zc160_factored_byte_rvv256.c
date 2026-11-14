/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 32 bytes: needs VLEN >= 256
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 32)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 32)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 32)
// generated: BG2 Zc=160, rvv256-bit, factored
// 173 terms vs 1477 expanded (8.54x); 5 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc160_byte_rvv256(uint8_t *c,uint8_t *d) {
  uint8_t kd[1280] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<5; i2++) {
     const uint8_t *cb=c+32*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+19)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+136)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+159)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+357)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+413)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+479)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+654)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+799)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1058)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1083)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1119)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1288)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1381)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1439)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1745)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1759)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1931)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2007)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2079)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2296)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2399)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2612)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2667)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2719)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2925)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2934)),LGF_RVL((const uint8_t *)(cb+3039))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+32*i2),p0);
     LGF_RVS((uint8_t *)(d+1600+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+337)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2076)),LGF_RVL((const uint8_t *)(cb+2260))))));   //row 10
     LGF_RVS((uint8_t *)(d+2880+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1938)),LGF_RVL((const uint8_t *)(cb+2326)))));   //row 18
     LGF_RVS((uint8_t *)(d+3520+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+644))));   //row 22
     LGF_RVS((uint8_t *)(d+3680+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1035)),LGF_RVL((const uint8_t *)(cb+1758)))));   //row 23
     LGF_RVS((uint8_t *)(d+3840+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+709)),LGF_RVL((const uint8_t *)(cb+2967)))));   //row 24
     LGF_RVS((uint8_t *)(d+4000+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+1665))));   //row 25
     LGF_RVS((uint8_t *)(d+4320+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+1952))));   //row 27
     LGF_RVS((uint8_t *)(d+4480+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+766)),LGF_RVL((const uint8_t *)(cb+1710)))));   //row 28
     LGF_RVS((uint8_t *)(d+4640+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+1434))));   //row 29
     LGF_RVS((uint8_t *)(d+4800+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1635)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2291)),LGF_RVL((const uint8_t *)(cb+3014))))));   //row 30
     LGF_RVS((uint8_t *)(d+5760+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+703)),LGF_RVL((const uint8_t *)(cb+2322)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+32*(i2-1);
       uint8_t *dm=d+32*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+320)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+640)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+960)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1920)),LGF_RVL((const uint8_t *)(cm+2880))))))));
       LGF_RVS((uint8_t *)(dm+160),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+160)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+137)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1084)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1280)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1600)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2008)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2240)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2560)),LGF_RVL((const uint8_t *)(cm+2935))))))))));
       LGF_RVS((uint8_t *)(dm+320),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+320)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+20)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+414)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1059)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1289)),LGF_RVL((const uint8_t *)(cm+2668))))))));
       LGF_RVS((uint8_t *)(dm+480),p3);
     }
  }

  memcpy(kd,d,160); memcpy(kd+160,d,160);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+128, *km=kd+128; uint8_t *dm=d+128;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+320)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+640)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+960)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1920)),LGF_RVL((const uint8_t *)(cm+2880))))))));
    LGF_RVS((uint8_t *)(dm+160),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+160)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+137)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1084)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1280)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1600)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2008)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2240)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2560)),LGF_RVL((const uint8_t *)(cm+2935))))))))));
    LGF_RVS((uint8_t *)(dm+320),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+320)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+20)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+414)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1059)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1289)),LGF_RVL((const uint8_t *)(cm+2668))))))));
    LGF_RVS((uint8_t *)(dm+480),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*320,d+k*160,160); memcpy(kd+k*320+160,d+k*160,160); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<5; i2++) {
     const uint8_t *cb=c+32*i2, *kb=kd+32*i2;
     LGF_RVS((uint8_t *)(d+640+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+477)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+456)))));   //row 4 [P1<<157 + 2 sys]
     LGF_RVS((uint8_t *)(d+800+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+384)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+451)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1742)),LGF_RVL((const uint8_t *)(cb+2381)))))));   //row 5 [P1<<64 + 4 sys]
     LGF_RVS((uint8_t *)(d+960+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+468)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1724)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2339)),LGF_RVL((const uint8_t *)(cb+2925)))))));   //row 6 [P1<<148 + 4 sys]
     LGF_RVS((uint8_t *)(d+1120+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+416)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1038)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1645)),LGF_RVL((const uint8_t *)(cb+2388)))))));   //row 7 [P1<<96+P3<<78 + 3 sys]
     LGF_RVS((uint8_t *)(d+1280+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+727)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+385)))));   //row 8 [P2<<87 + 2 sys]
     LGF_RVS((uint8_t *)(d+1440+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+51)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+405)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+2657))))));   //row 9 [P0<<51+P1<<85 + 2 sys]
     LGF_RVS((uint8_t *)(d+1760+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+962)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2247)),LGF_RVL((const uint8_t *)(cb+2884))))));   //row 11 [P3<<2 + 3 sys]
     LGF_RVS((uint8_t *)(d+1920+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+368)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+1073)))));   //row 12 [P1<<48 + 2 sys]
     LGF_RVS((uint8_t *)(d+2080+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+986)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+432)),LGF_RVL((const uint8_t *)(cb+2662))))));   //row 13 [P3<<26 + 3 sys]
     LGF_RVS((uint8_t *)(d+2240+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+377)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+987)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+2058))))));   //row 14 [P1<<57+P3<<27 + 2 sys]
     LGF_RVS((uint8_t *)(d+2400+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+73)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+419)),LGF_RVL((const uint8_t *)(cb+0)))));   //row 15 [P0<<73+P1<<99 + 1 sys]
     LGF_RVS((uint8_t *)(d+2560+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+431)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+783)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+2959))))));   //row 16 [P1<<111+P2<<143 + 2 sys]
     LGF_RVS((uint8_t *)(d+2720+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+429)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+658)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+1624))))));   //row 17 [P1<<109+P2<<18 + 2 sys]
     LGF_RVS((uint8_t *)(d+3040+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+154)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+478)))));   //row 19 [P0<<154 + 2 sys]
     LGF_RVS((uint8_t *)(d+3200+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+424)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+1428)))));   //row 20 [P1<<104 + 2 sys]
     LGF_RVS((uint8_t *)(d+3360+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+993)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+2577)))));   //row 21 [P3<<33 + 2 sys]
     LGF_RVS((uint8_t *)(d+4160+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+653)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+967)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+2340))))));   //row 26 [P2<<13+P3<<7 + 2 sys]
     LGF_RVS((uint8_t *)(d+4960+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+980)),LGF_RVL((const uint8_t *)(cb+320))));   //row 31 [P3<<20 + 1 sys]
     LGF_RVS((uint8_t *)(d+5120+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+762)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+1620)))));   //row 32 [P2<<122 + 2 sys]
     LGF_RVS((uint8_t *)(d+5280+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+13)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+2328)))));   //row 33 [P0<<13 + 2 sys]
     LGF_RVS((uint8_t *)(d+5440+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+659)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1038)),LGF_RVL((const uint8_t *)(cb+0)))));   //row 34 [P2<<19+P3<<78 + 1 sys]
     LGF_RVS((uint8_t *)(d+5600+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+326)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+1757)))));   //row 35 [P1<<6 + 2 sys]
     LGF_RVS((uint8_t *)(d+5920+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+0)),LGF_RVL((const uint8_t *)(kb+1104))));   //row 37 [P0<<0+P3<<144 + 0 sys]
     LGF_RVS((uint8_t *)(d+6080+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+339)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+1693)))));   //row 38 [P1<<19 + 2 sys]
     LGF_RVS((uint8_t *)(d+6240+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+778)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+2264)))));   //row 39 [P2<<138 + 2 sys]
     LGF_RVS((uint8_t *)(d+6400+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+36)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1103)),LGF_RVL((const uint8_t *)(cb+640)))));   //row 40 [P0<<36+P3<<143 + 1 sys]
     LGF_RVS((uint8_t *)(d+6560+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+375)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+1602)))));   //row 41 [P1<<55 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
