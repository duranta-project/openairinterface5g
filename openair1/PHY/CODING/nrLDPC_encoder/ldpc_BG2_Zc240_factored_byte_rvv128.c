/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 16 bytes: needs VLEN >= 128
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 16)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 16)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 16)
// generated: BG2 Zc=240, rvv128-bit, factored
// 173 terms vs 1477 expanded (8.54x); 15 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc240_byte_rvv128(uint8_t *c,uint8_t *d) {
  uint8_t kd[1920] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<15; i2++) {
     const uint8_t *cb=c+16*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+74)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+142)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+145)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+511)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+611)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+719)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1031)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1131)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1443)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1461)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1614)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1973)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2015)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2103)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2427)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2510)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2903)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2976)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3039)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3383)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3559)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3849)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3883)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3995)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4395)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4432)),LGF_RVL((const uint8_t *)(cb+4487))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+16*i2),p0);
     LGF_RVS((uint8_t *)(d+2400+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+38)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+697)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3088)),LGF_RVL((const uint8_t *)(cb+3592))))));   //row 10
     LGF_RVS((uint8_t *)(d+4320+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+128)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3091)),LGF_RVL((const uint8_t *)(cb+3570)))));   //row 18
     LGF_RVS((uint8_t *)(d+5280+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+675)),LGF_RVL((const uint8_t *)(cb+1004))));   //row 22
     LGF_RVS((uint8_t *)(d+5520+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1534)),LGF_RVL((const uint8_t *)(cb+2511)))));   //row 23
     LGF_RVS((uint8_t *)(d+5760+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+561)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+979)),LGF_RVL((const uint8_t *)(cb+4450)))));   //row 24
     LGF_RVS((uint8_t *)(d+6000+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+66)),LGF_RVL((const uint8_t *)(cb+2495))));   //row 25
     LGF_RVS((uint8_t *)(d+6480+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+64)),LGF_RVL((const uint8_t *)(cb+3061))));   //row 27
     LGF_RVS((uint8_t *)(d+6720+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+487)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1104)),LGF_RVL((const uint8_t *)(cb+2416)))));   //row 28
     LGF_RVS((uint8_t *)(d+6960+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+25)),LGF_RVL((const uint8_t *)(cb+1977))));   //row 29
     LGF_RVS((uint8_t *)(d+7200+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+997)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2539)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3581)),LGF_RVL((const uint8_t *)(cb+4337))))));   //row 30
     LGF_RVS((uint8_t *)(d+8640+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+178)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1072)),LGF_RVL((const uint8_t *)(cb+3466)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+16*(i2-1);
       uint8_t *dm=d+16*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+145)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+611)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1031)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1461)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2903)),LGF_RVL((const uint8_t *)(cm+4432))))))));
       LGF_RVS((uint8_t *)(dm+240),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+240)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+142)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1614)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2103)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2427)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2976)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3383)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3849)),LGF_RVL((const uint8_t *)(cm+4487))))))))));
       LGF_RVS((uint8_t *)(dm+480),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+480)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+74)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+511)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1443)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1973)),LGF_RVL((const uint8_t *)(cm+3995))))))));
       LGF_RVS((uint8_t *)(dm+720),p3);
     }
  }

  memcpy(kd,d,240); memcpy(kd+240,d,240);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+224, *km=kd+224; uint8_t *dm=d+224;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+145)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+611)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1031)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1461)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2903)),LGF_RVL((const uint8_t *)(cm+4432))))))));
    LGF_RVS((uint8_t *)(dm+240),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+240)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+142)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1614)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2103)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2427)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2976)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3383)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3849)),LGF_RVL((const uint8_t *)(cm+4487))))))))));
    LGF_RVS((uint8_t *)(dm+480),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+480)),LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+74)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+511)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1443)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1973)),LGF_RVL((const uint8_t *)(cm+3995))))))));
    LGF_RVS((uint8_t *)(dm+720),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*480,d+k*240,240); memcpy(kd+k*480+240,d+k*240,240); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<15; i2++) {
     const uint8_t *cb=c+16*i2, *kb=kd+16*i2;
     LGF_RVS((uint8_t *)(d+960+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+660)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+29)),LGF_RVL((const uint8_t *)(cb+620)))));   //row 4 [P1<<180 + 2 sys]
     LGF_RVS((uint8_t *)(d+1200+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+687)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+121)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+521)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2569)),LGF_RVL((const uint8_t *)(cb+3448)))))));   //row 5 [P1<<207 + 4 sys]
     LGF_RVS((uint8_t *)(d+1440+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+536)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+137)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2472)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3532)),LGF_RVL((const uint8_t *)(cb+4444)))))));   //row 6 [P1<<56 + 4 sys]
     LGF_RVS((uint8_t *)(d+1680+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+652)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1594)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+566)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2586)),LGF_RVL((const uint8_t *)(cb+3447)))))));   //row 7 [P1<<172+P3<<154 + 3 sys]
     LGF_RVS((uint8_t *)(d+1920+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1185)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+176)),LGF_RVL((const uint8_t *)(cb+649)))));   //row 8 [P2<<225 + 2 sys]
     LGF_RVS((uint8_t *)(d+2160+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+48)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+548)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+647)),LGF_RVL((const uint8_t *)(cb+4078))))));   //row 9 [P0<<48+P1<<68 + 2 sys]
     LGF_RVS((uint8_t *)(d+2640+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1491)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+178)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3574)),LGF_RVL((const uint8_t *)(cb+4488))))));   //row 11 [P3<<51 + 3 sys]
     LGF_RVS((uint8_t *)(d+2880+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+552)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+604)),LGF_RVL((const uint8_t *)(cb+1562)))));   //row 12 [P1<<72 + 2 sys]
     LGF_RVS((uint8_t *)(d+3120+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1659)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+48)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+537)),LGF_RVL((const uint8_t *)(cb+4007))))));   //row 13 [P3<<219 + 3 sys]
     LGF_RVS((uint8_t *)(d+3360+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+684)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1602)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+562)),LGF_RVL((const uint8_t *)(cb+3112))))));   //row 14 [P1<<204+P3<<162 + 2 sys]
     LGF_RVS((uint8_t *)(d+3600+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+217)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+637)),LGF_RVL((const uint8_t *)(cb+38)))));   //row 15 [P0<<217+P1<<157 + 1 sys]
     LGF_RVS((uint8_t *)(d+3840+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+655)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1162)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+650)),LGF_RVL((const uint8_t *)(cb+4343))))));   //row 16 [P1<<175+P2<<202 + 2 sys]
     LGF_RVS((uint8_t *)(d+4080+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+675)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1178)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+676)),LGF_RVL((const uint8_t *)(cb+2573))))));   //row 17 [P1<<195+P2<<218 + 2 sys]
     LGF_RVS((uint8_t *)(d+4560+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+88)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+39)),LGF_RVL((const uint8_t *)(cb+564)))));   //row 19 [P0<<88 + 2 sys]
     LGF_RVS((uint8_t *)(d+4800+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+486)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+597)),LGF_RVL((const uint8_t *)(cb+2147)))));   //row 20 [P1<<6 + 2 sys]
     LGF_RVS((uint8_t *)(d+5040+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1451)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+238)),LGF_RVL((const uint8_t *)(cb+3853)))));   //row 21 [P3<<11 + 2 sys]
     LGF_RVS((uint8_t *)(d+6240+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1150)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1526)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1106)),LGF_RVL((const uint8_t *)(cb+3426))))));   //row 26 [P2<<190+P3<<86 + 2 sys]
     LGF_RVS((uint8_t *)(d+7440+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1486)),LGF_RVL((const uint8_t *)(cb+681))));   //row 31 [P3<<46 + 1 sys]
     LGF_RVS((uint8_t *)(d+7680+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1076)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+179)),LGF_RVL((const uint8_t *)(cb+2414)))));   //row 32 [P2<<116 + 2 sys]
     LGF_RVS((uint8_t *)(d+7920+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+106)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1006)),LGF_RVL((const uint8_t *)(cb+3362)))));   //row 33 [P0<<106 + 2 sys]
     LGF_RVS((uint8_t *)(d+8160+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1095)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1581)),LGF_RVL((const uint8_t *)(cb+184)))));   //row 34 [P2<<135+P3<<141 + 1 sys]
     LGF_RVS((uint8_t *)(d+8400+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+655)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+565)),LGF_RVL((const uint8_t *)(cb+2625)))));   //row 35 [P1<<175 + 2 sys]
     LGF_RVS((uint8_t *)(d+8880+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+154)),LGF_RVL((const uint8_t *)(kb+1554))));   //row 37 [P0<<154+P3<<114 + 0 sys]
     LGF_RVS((uint8_t *)(d+9120+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+585)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+522)),LGF_RVL((const uint8_t *)(cb+2441)))));   //row 38 [P1<<105 + 2 sys]
     LGF_RVS((uint8_t *)(d+9360+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1149)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+167)),LGF_RVL((const uint8_t *)(cb+3405)))));   //row 39 [P2<<189 + 2 sys]
     LGF_RVS((uint8_t *)(d+9600+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+67)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1620)),LGF_RVL((const uint8_t *)(cb+1038)))));   //row 40 [P0<<67+P3<<180 + 1 sys]
     LGF_RVS((uint8_t *)(d+9840+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+710)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+533)),LGF_RVL((const uint8_t *)(cb+2615)))));   //row 41 [P1<<230 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
