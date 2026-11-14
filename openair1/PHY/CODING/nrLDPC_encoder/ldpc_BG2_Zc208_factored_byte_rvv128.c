/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 16 bytes: needs VLEN >= 128
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 16)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 16)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 16)
// generated: BG2 Zc=208, rvv128-bit, factored
// 173 terms vs 1483 expanded (8.57x); 13 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc208_byte_rvv128(uint8_t *c,uint8_t *d) {
  uint8_t kd[1664] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<13; i2++) {
     const uint8_t *cb=c+16*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+17)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+125)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+142)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+434)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+451)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+578)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+879)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1007)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1274)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1294)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1412)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1666)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1681)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1846)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2181)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2190)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2680)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2691)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2698)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2914)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2928)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3341)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3459)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3518)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3756)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3903)),LGF_RVL((const uint8_t *)(cb+3923))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+16*i2),p0);
     LGF_RVS((uint8_t *)(d+2080+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+125)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+476)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2673)),LGF_RVL((const uint8_t *)(cb+2963))))));   //row 10
     LGF_RVS((uint8_t *)(d+3744+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+129)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2699)),LGF_RVL((const uint8_t *)(cb+3052)))));   //row 18
     LGF_RVS((uint8_t *)(d+4576+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+618)),LGF_RVL((const uint8_t *)(cb+958))));   //row 22
     LGF_RVS((uint8_t *)(d+4784+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+52)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1418)),LGF_RVL((const uint8_t *)(cb+2093)))));   //row 23
     LGF_RVS((uint8_t *)(d+4992+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+529)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+993)),LGF_RVL((const uint8_t *)(cb+3832)))));   //row 24
     LGF_RVS((uint8_t *)(d+5200+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+197)),LGF_RVL((const uint8_t *)(cb+2274))));   //row 25
     LGF_RVS((uint8_t *)(d+5616+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+168)),LGF_RVL((const uint8_t *)(cb+2689))));   //row 27
     LGF_RVS((uint8_t *)(d+5824+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+430)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1018)),LGF_RVL((const uint8_t *)(cb+2126)))));   //row 28
     LGF_RVS((uint8_t *)(d+6032+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+50)),LGF_RVL((const uint8_t *)(cb+1691))));   //row 29
     LGF_RVS((uint8_t *)(d+6240+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+902)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2097)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2962)),LGF_RVL((const uint8_t *)(cb+3750))))));   //row 30
     LGF_RVS((uint8_t *)(d+7488+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+197)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+999)),LGF_RVL((const uint8_t *)(cb+3091)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+16*(i2-1);
       uint8_t *dm=d+16*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+143)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+435)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1008)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1413)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2692)),LGF_RVL((const uint8_t *)(cm+3757))))))));
       LGF_RVS((uint8_t *)(dm+208),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+208)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+18)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1275)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1667)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2182)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2681)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2929)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3342)),LGF_RVL((const uint8_t *)(cm+3924))))))))));
       LGF_RVS((uint8_t *)(dm+416),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+416)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+126)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+579)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1295)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1847)),LGF_RVL((const uint8_t *)(cm+3460))))))));
       LGF_RVS((uint8_t *)(dm+624),p3);
     }
  }

  memcpy(kd,d,208); memcpy(kd+208,d,208);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+192, *km=kd+192; uint8_t *dm=d+192;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+143)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+435)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1008)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1413)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2692)),LGF_RVL((const uint8_t *)(cm+3757))))))));
    LGF_RVS((uint8_t *)(dm+208),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+208)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+18)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1275)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1667)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2182)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2681)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2929)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3342)),LGF_RVL((const uint8_t *)(cm+3924))))))))));
    LGF_RVS((uint8_t *)(dm+416),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+416)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+126)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+579)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1295)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1847)),LGF_RVL((const uint8_t *)(cm+3460))))))));
    LGF_RVS((uint8_t *)(dm+624),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*416,d+k*208,208); memcpy(kd+k*416+208,d+k*208,208); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<13; i2++) {
     const uint8_t *cb=c+16*i2, *kb=kd+16*i2;
     LGF_RVS((uint8_t *)(d+832+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+533)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+43)),LGF_RVL((const uint8_t *)(cb+443)))));   //row 4 [P1<<117 + 2 sys]
     LGF_RVS((uint8_t *)(d+1040+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+478)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+136)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+465)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2116)),LGF_RVL((const uint8_t *)(cb+3044)))))));   //row 5 [P1<<62 + 4 sys]
     LGF_RVS((uint8_t *)(d+1248+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+428)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+7)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2114)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3110)),LGF_RVL((const uint8_t *)(cb+3912)))))));   //row 6 [P1<<12 + 4 sys]
     LGF_RVS((uint8_t *)(d+1456+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+523)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1306)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+579)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2158)),LGF_RVL((const uint8_t *)(cb+3055)))))));   //row 7 [P1<<107+P3<<58 + 3 sys]
     LGF_RVS((uint8_t *)(d+1664+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+854)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+101)),LGF_RVL((const uint8_t *)(cb+593)))));   //row 8 [P2<<22 + 2 sys]
     LGF_RVS((uint8_t *)(d+1872+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+205)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+497)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+602)),LGF_RVL((const uint8_t *)(cb+3355))))));   //row 9 [P0<<205+P1<<81 + 2 sys]
     LGF_RVS((uint8_t *)(d+2288+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1256)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+39)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2941)),LGF_RVL((const uint8_t *)(cb+3779))))));   //row 11 [P3<<8 + 3 sys]
     LGF_RVS((uint8_t *)(d+2496+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+465)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+434)),LGF_RVL((const uint8_t *)(cb+1403)))));   //row 12 [P1<<49 + 2 sys]
     LGF_RVS((uint8_t *)(d+2704+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1434)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+32)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+469)),LGF_RVL((const uint8_t *)(cb+3423))))));   //row 13 [P3<<186 + 3 sys]
     LGF_RVS((uint8_t *)(d+2912+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+468)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1357)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+507)),LGF_RVL((const uint8_t *)(cb+2516))))));   //row 14 [P1<<52+P3<<109 + 2 sys]
     LGF_RVS((uint8_t *)(d+3120+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+108)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+518)),LGF_RVL((const uint8_t *)(cb+174)))));   //row 15 [P0<<108+P1<<102 + 1 sys]
     LGF_RVS((uint8_t *)(d+3328+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+470)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1008)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+541)),LGF_RVL((const uint8_t *)(cb+3775))))));   //row 16 [P1<<54+P2<<176 + 2 sys]
     LGF_RVS((uint8_t *)(d+3536+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+558)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+867)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+473)),LGF_RVL((const uint8_t *)(cb+2281))))));   //row 17 [P1<<142+P2<<35 + 2 sys]
     LGF_RVS((uint8_t *)(d+3952+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+52)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+110)),LGF_RVL((const uint8_t *)(cb+540)))));   //row 19 [P0<<52 + 2 sys]
     LGF_RVS((uint8_t *)(d+4160+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+530)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+612)),LGF_RVL((const uint8_t *)(cb+1699)))));   //row 20 [P1<<114 + 2 sys]
     LGF_RVS((uint8_t *)(d+4368+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1271)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+10)),LGF_RVL((const uint8_t *)(cb+3450)))));   //row 21 [P3<<23 + 2 sys]
     LGF_RVS((uint8_t *)(d+5408+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+881)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1409)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+996)),LGF_RVL((const uint8_t *)(cb+3084))))));   //row 26 [P2<<49+P3<<161 + 2 sys]
     LGF_RVS((uint8_t *)(d+6448+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1437)),LGF_RVL((const uint8_t *)(cb+531))));   //row 31 [P3<<189 + 1 sys]
     LGF_RVS((uint8_t *)(d+6656+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+995)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+110)),LGF_RVL((const uint8_t *)(cb+2080)))));   //row 32 [P2<<163 + 2 sys]
     LGF_RVS((uint8_t *)(d+6864+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+179)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+995)),LGF_RVL((const uint8_t *)(cb+3085)))));   //row 33 [P0<<179 + 2 sys]
     LGF_RVS((uint8_t *)(d+7072+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1023)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1441)),LGF_RVL((const uint8_t *)(cb+197)))));   //row 34 [P2<<191+P3<<193 + 1 sys]
     LGF_RVS((uint8_t *)(d+7280+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+597)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+573)),LGF_RVL((const uint8_t *)(cb+2247)))));   //row 35 [P1<<181 + 2 sys]
     LGF_RVS((uint8_t *)(d+7696+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+181)),LGF_RVL((const uint8_t *)(kb+1441))));   //row 37 [P0<<181+P3<<193 + 0 sys]
     LGF_RVS((uint8_t *)(d+7904+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+607)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+573)),LGF_RVL((const uint8_t *)(cb+2253)))));   //row 38 [P1<<191 + 2 sys]
     LGF_RVS((uint8_t *)(d+8112+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1005)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+181)),LGF_RVL((const uint8_t *)(cb+3069)))));   //row 39 [P2<<173 + 2 sys]
     LGF_RVS((uint8_t *)(d+8320+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+163)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1427)),LGF_RVL((const uint8_t *)(cb+1025)))));   //row 40 [P0<<163+P3<<179 + 1 sys]
     LGF_RVS((uint8_t *)(d+8528+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+583)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+607)),LGF_RVL((const uint8_t *)(cb+2277)))));   //row 41 [P1<<167 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
