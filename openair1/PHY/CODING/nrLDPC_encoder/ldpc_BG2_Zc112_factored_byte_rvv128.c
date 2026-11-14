/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 16 bytes: needs VLEN >= 128
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 16)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 16)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 16)
// generated: BG2 Zc=112, rvv128-bit, factored
// 173 terms vs 1459 expanded (8.43x); 7 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc112_byte_rvv128(uint8_t *c,uint8_t *d) {
  uint8_t kd[896] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<7; i2++) {
     const uint8_t *cb=c+16*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+40)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+53)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+72)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+243)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+297)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+334)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+454)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+471)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+716)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+718)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+741)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+899)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+932)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+975)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1132)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1164)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1347)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1356)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1439)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1656)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1678)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1821)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1836)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1883)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2024)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2037)),LGF_RVL((const uint8_t *)(cb+2047))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+16*i2),p0);
     LGF_RVS((uint8_t *)(d+1120+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+62)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+262)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1352)),LGF_RVL((const uint8_t *)(cb+1624))))));   //row 10
     LGF_RVS((uint8_t *)(d+2016+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+68)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1360)),LGF_RVL((const uint8_t *)(cb+1612)))));   //row 18
     LGF_RVS((uint8_t *)(d+2464+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+273)),LGF_RVL((const uint8_t *)(cb+449))));   //row 22
     LGF_RVS((uint8_t *)(d+2576+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+44)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+726)),LGF_RVL((const uint8_t *)(cb+1142)))));   //row 23
     LGF_RVS((uint8_t *)(d+2688+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+265)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+536)),LGF_RVL((const uint8_t *)(cb+2079)))));   //row 24
     LGF_RVS((uint8_t *)(d+2800+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+99)),LGF_RVL((const uint8_t *)(cb+1214))));   //row 25
     LGF_RVS((uint8_t *)(d+3024+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+27)),LGF_RVL((const uint8_t *)(cb+1350))));   //row 27
     LGF_RVS((uint8_t *)(d+3136+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+328)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+548)),LGF_RVL((const uint8_t *)(cb+1201)))));   //row 28
     LGF_RVS((uint8_t *)(d+3248+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+108)),LGF_RVL((const uint8_t *)(cb+957))));   //row 29
     LGF_RVS((uint8_t *)(d+3360+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+554)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1164)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1641)),LGF_RVL((const uint8_t *)(cb+2080))))));   //row 30
     LGF_RVS((uint8_t *)(d+4032+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+511)),LGF_RVL((const uint8_t *)(cb+1651)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+16*(i2-1);
       uint8_t *dm=d+16*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+72)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+334)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+471)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+741)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1439)),LGF_RVL((const uint8_t *)(cm+2024))))))));
       LGF_RVS((uint8_t *)(dm+112),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+112)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+53)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+716)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+899)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1164)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1347)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1656)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1821)),LGF_RVL((const uint8_t *)(cm+2047))))))))));
       LGF_RVS((uint8_t *)(dm+224),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+224)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+40)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+243)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+718)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+975)),LGF_RVL((const uint8_t *)(cm+1883))))))));
       LGF_RVS((uint8_t *)(dm+336),p3);
     }
  }

  memcpy(kd,d,112); memcpy(kd+112,d,112);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+96, *km=kd+96; uint8_t *dm=d+96;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+72)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+334)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+471)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+741)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1439)),LGF_RVL((const uint8_t *)(cm+2024))))))));
    LGF_RVS((uint8_t *)(dm+112),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+112)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+53)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+716)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+899)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1164)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1347)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1656)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1821)),LGF_RVL((const uint8_t *)(cm+2047))))))))));
    LGF_RVS((uint8_t *)(dm+224),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+224)),LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+40)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+243)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+718)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+975)),LGF_RVL((const uint8_t *)(cm+1883))))))));
    LGF_RVS((uint8_t *)(dm+336),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*224,d+k*112,112); memcpy(kd+k*224+112,d+k*112,112); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<7; i2++) {
     const uint8_t *cb=c+16*i2, *kb=kd+16*i2;
     LGF_RVS((uint8_t *)(d+448+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+325)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+88)),LGF_RVL((const uint8_t *)(cb+240)))));   //row 4 [P1<<101 + 2 sys]
     LGF_RVS((uint8_t *)(d+560+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+305)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+73)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+250)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1178)),LGF_RVL((const uint8_t *)(cb+1675)))))));   //row 5 [P1<<81 + 4 sys]
     LGF_RVS((uint8_t *)(d+672+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+321)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+11)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1175)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1599)),LGF_RVL((const uint8_t *)(cb+2126)))))));   //row 6 [P1<<97 + 4 sys]
     LGF_RVS((uint8_t *)(d+784+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+262)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+741)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+327)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1133)),LGF_RVL((const uint8_t *)(cb+1673)))))));   //row 7 [P1<<38+P3<<69 + 3 sys]
     LGF_RVS((uint8_t *)(d+896+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+488)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+35)),LGF_RVL((const uint8_t *)(cb+267)))));   //row 8 [P2<<40 + 2 sys]
     LGF_RVS((uint8_t *)(d+1008+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+72)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+307)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+226)),LGF_RVL((const uint8_t *)(cb+1822))))));   //row 9 [P0<<72+P1<<83 + 2 sys]
     LGF_RVS((uint8_t *)(d+1232+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+771)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+99)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1594)),LGF_RVL((const uint8_t *)(cb+2126))))));   //row 11 [P3<<99 + 3 sys]
     LGF_RVS((uint8_t *)(d+1344+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+333)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+270)),LGF_RVL((const uint8_t *)(cb+777)))));   //row 12 [P1<<109 + 2 sys]
     LGF_RVS((uint8_t *)(d+1456+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+700)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+37)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+225)),LGF_RVL((const uint8_t *)(cb+1823))))));   //row 13 [P3<<28 + 3 sys]
     LGF_RVS((uint8_t *)(d+1568+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+264)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+676)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+260)),LGF_RVL((const uint8_t *)(cb+1439))))));   //row 14 [P1<<40+P3<<4 + 2 sys]
     LGF_RVS((uint8_t *)(d+1680+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+88)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+334)),LGF_RVL((const uint8_t *)(cb+4)))));   //row 15 [P0<<88+P1<<110 + 1 sys]
     LGF_RVS((uint8_t *)(d+1792+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+246)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+545)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+299)),LGF_RVL((const uint8_t *)(cb+2062))))));   //row 16 [P1<<22+P2<<97 + 2 sys]
     LGF_RVS((uint8_t *)(d+1904+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+318)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+450)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+272)),LGF_RVL((const uint8_t *)(cb+1140))))));   //row 17 [P1<<94+P2<<2 + 2 sys]
     LGF_RVS((uint8_t *)(d+2128+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+86)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+35)),LGF_RVL((const uint8_t *)(cb+250)))));   //row 19 [P0<<86 + 2 sys]
     LGF_RVS((uint8_t *)(d+2240+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+253)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+230)),LGF_RVL((const uint8_t *)(cb+916)))));   //row 20 [P1<<29 + 2 sys]
     LGF_RVS((uint8_t *)(d+2352+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+753)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+80)),LGF_RVL((const uint8_t *)(cb+1835)))));   //row 21 [P3<<81 + 2 sys]
     LGF_RVS((uint8_t *)(d+2912+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+557)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+678)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+538)),LGF_RVL((const uint8_t *)(cb+1574))))));   //row 26 [P2<<109+P3<<6 + 2 sys]
     LGF_RVS((uint8_t *)(d+3472+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+742)),LGF_RVL((const uint8_t *)(cb+259))));   //row 31 [P3<<70 + 1 sys]
     LGF_RVS((uint8_t *)(d+3584+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+558)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+108)),LGF_RVL((const uint8_t *)(cb+1141)))));   //row 32 [P2<<110 + 2 sys]
     LGF_RVS((uint8_t *)(d+3696+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+109)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+519)),LGF_RVL((const uint8_t *)(cb+1580)))));   //row 33 [P0<<109 + 2 sys]
     LGF_RVS((uint8_t *)(d+3808+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+537)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+741)),LGF_RVL((const uint8_t *)(cb+29)))));   //row 34 [P2<<89+P3<<69 + 1 sys]
     LGF_RVS((uint8_t *)(d+3920+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+279)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+315)),LGF_RVL((const uint8_t *)(cb+1173)))));   //row 35 [P1<<55 + 2 sys]
     LGF_RVS((uint8_t *)(d+4144+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+40)),LGF_RVL((const uint8_t *)(kb+684))));   //row 37 [P0<<40+P3<<12 + 0 sys]
     LGF_RVS((uint8_t *)(d+4256+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+270)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+261)),LGF_RVL((const uint8_t *)(cb+1217)))));   //row 38 [P1<<46 + 2 sys]
     LGF_RVS((uint8_t *)(d+4368+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+490)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+106)),LGF_RVL((const uint8_t *)(cb+1637)))));   //row 39 [P2<<42 + 2 sys]
     LGF_RVS((uint8_t *)(d+4480+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+35)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+708)),LGF_RVL((const uint8_t *)(cb+546)))));   //row 40 [P0<<35+P3<<36 + 1 sys]
     LGF_RVS((uint8_t *)(d+4592+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+305)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+232)),LGF_RVL((const uint8_t *)(cb+1221)))));   //row 41 [P1<<81 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
