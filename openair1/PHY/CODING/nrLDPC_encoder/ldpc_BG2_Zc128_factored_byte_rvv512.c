/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 64 bytes: needs VLEN >= 512
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 64)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 64)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 64)
// generated: BG2 Zc=128, rvv512-bit, factored
// 173 terms vs 1469 expanded (8.49x); 2 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc128_byte_rvv512(uint8_t *c,uint8_t *d) {
  uint8_t kd[1024] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<2; i2++) {
     const uint8_t *cb=c+64*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+8)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+38)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+80)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+263)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+369)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+372)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+569)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+587)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+793)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+805)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+811)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1053)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1075)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1148)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1383)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1404)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1596)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1616)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1633)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1819)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1845)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2065)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2143)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2159)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2380)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2427)),LGF_RVL((const uint8_t *)(cb+2431))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+64*i2),p0);
     LGF_RVS((uint8_t *)(d+1280+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+11)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+313)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1536)),LGF_RVL((const uint8_t *)(cb+1909))))));   //row 10
     LGF_RVS((uint8_t *)(d+2304+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+92)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1602)),LGF_RVL((const uint8_t *)(cb+1842)))));   //row 18
     LGF_RVS((uint8_t *)(d+2816+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+350)),LGF_RVL((const uint8_t *)(cb+575))));   //row 22
     LGF_RVS((uint8_t *)(d+2944+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+23)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+875)),LGF_RVL((const uint8_t *)(cb+1390)))));   //row 23
     LGF_RVS((uint8_t *)(d+3072+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+302)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+523)),LGF_RVL((const uint8_t *)(cb+2312)))));   //row 24
     LGF_RVS((uint8_t *)(d+3200+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+100)),LGF_RVL((const uint8_t *)(cb+1308))));   //row 25
     LGF_RVS((uint8_t *)(d+3456+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+8)),LGF_RVL((const uint8_t *)(cb+1559))));   //row 27
     LGF_RVS((uint8_t *)(d+3584+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+354)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+613)),LGF_RVL((const uint8_t *)(cb+1287)))));   //row 28
     LGF_RVS((uint8_t *)(d+3712+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+18)),LGF_RVL((const uint8_t *)(cb+1052))));   //row 29
     LGF_RVS((uint8_t *)(d+3840+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+583)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1392)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1801)),LGF_RVL((const uint8_t *)(cb+2388))))));   //row 30
     LGF_RVS((uint8_t *)(d+4608+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+12)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+550)),LGF_RVL((const uint8_t *)(cb+1818)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+64*(i2-1);
       uint8_t *dm=d+64*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+9)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+373)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+588)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+794)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1597)),LGF_RVL((const uint8_t *)(cm+2381))))))));
       LGF_RVS((uint8_t *)(dm+128),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+128)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+39)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+806)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1149)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1405)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1634)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1820)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2144)),LGF_RVL((const uint8_t *)(cm+2428))))))))));
       LGF_RVS((uint8_t *)(dm+256),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+256)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+81)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+370)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+812)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1076)),LGF_RVL((const uint8_t *)(cm+2160))))))));
       LGF_RVS((uint8_t *)(dm+384),p3);
     }
  }

  memcpy(kd,d,128); memcpy(kd+128,d,128);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+64, *km=kd+64; uint8_t *dm=d+64;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+9)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+373)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+588)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+794)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1597)),LGF_RVL((const uint8_t *)(cm+2381))))))));
    LGF_RVS((uint8_t *)(dm+128),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+128)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+39)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+806)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1149)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1405)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1634)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1820)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2144)),LGF_RVL((const uint8_t *)(cm+2428))))))))));
    LGF_RVS((uint8_t *)(dm+256),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+256)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+81)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+370)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+812)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1076)),LGF_RVL((const uint8_t *)(cm+2160))))))));
    LGF_RVS((uint8_t *)(dm+384),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*256,d+k*128,128); memcpy(kd+k*256+128,d+k*128,128); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<2; i2++) {
     const uint8_t *cb=c+64*i2, *kb=kd+64*i2;
     LGF_RVS((uint8_t *)(d+512+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+327)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+51)),LGF_RVL((const uint8_t *)(cb+342)))));   //row 4 [P1<<71 + 2 sys]
     LGF_RVS((uint8_t *)(d+640+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+359)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+103)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+297)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1346)),LGF_RVL((const uint8_t *)(cb+1823)))))));   //row 5 [P1<<103 + 4 sys]
     LGF_RVS((uint8_t *)(d+768+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+286)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+27)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1380)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1837)),LGF_RVL((const uint8_t *)(cb+2332)))))));   //row 6 [P1<<30 + 4 sys]
     LGF_RVS((uint8_t *)(d+896+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+259)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+884)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+257)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1299)),LGF_RVL((const uint8_t *)(cb+1804)))))));   //row 7 [P1<<3+P3<<116 + 3 sys]
     LGF_RVS((uint8_t *)(d+1024+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+614)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+14)),LGF_RVL((const uint8_t *)(cb+350)))));   //row 8 [P2<<102 + 2 sys]
     LGF_RVS((uint8_t *)(d+1152+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+61)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+375)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+331)),LGF_RVL((const uint8_t *)(cb+2125))))));   //row 9 [P0<<61+P1<<119 + 2 sys]
     LGF_RVS((uint8_t *)(d+1408+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+824)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+11)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1900)),LGF_RVL((const uint8_t *)(cb+2386))))));   //row 11 [P3<<56 + 3 sys]
     LGF_RVS((uint8_t *)(d+1536+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+270)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+319)),LGF_RVL((const uint8_t *)(cb+879)))));   //row 12 [P1<<14 + 2 sys]
     LGF_RVS((uint8_t *)(d+1664+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+862)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+83)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+258)),LGF_RVL((const uint8_t *)(cb+2086))))));   //row 13 [P3<<94 + 3 sys]
     LGF_RVS((uint8_t *)(d+1792+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+259)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+872)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+371)),LGF_RVL((const uint8_t *)(cb+1553))))));   //row 14 [P1<<3+P3<<104 + 2 sys]
     LGF_RVS((uint8_t *)(d+1920+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+47)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+341)),LGF_RVL((const uint8_t *)(cb+51)))));   //row 15 [P0<<47+P1<<85 + 1 sys]
     LGF_RVS((uint8_t *)(d+2048+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+264)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+626)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+331)),LGF_RVL((const uint8_t *)(cb+2318))))));   //row 16 [P1<<8+P2<<114 + 2 sys]
     LGF_RVS((uint8_t *)(d+2176+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+370)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+576)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+382)),LGF_RVL((const uint8_t *)(cb+1404))))));   //row 17 [P1<<114+P2<<64 + 2 sys]
     LGF_RVS((uint8_t *)(d+2432+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+57)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+87)),LGF_RVL((const uint8_t *)(cb+276)))));   //row 19 [P0<<57 + 2 sys]
     LGF_RVS((uint8_t *)(d+2560+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+285)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+282)),LGF_RVL((const uint8_t *)(cb+1129)))));   //row 20 [P1<<29 + 2 sys]
     LGF_RVS((uint8_t *)(d+2688+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+850)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+76)),LGF_RVL((const uint8_t *)(cb+2090)))));   //row 21 [P3<<82 + 2 sys]
     LGF_RVS((uint8_t *)(d+3328+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+544)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+890)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+541)),LGF_RVL((const uint8_t *)(cb+1807))))));   //row 26 [P2<<32+P3<<122 + 2 sys]
     LGF_RVS((uint8_t *)(d+3968+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+769)),LGF_RVL((const uint8_t *)(cb+362))));   //row 31 [P3<<1 + 1 sys]
     LGF_RVS((uint8_t *)(d+4096+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+550)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+114)),LGF_RVL((const uint8_t *)(cb+1324)))));   //row 32 [P2<<38 + 2 sys]
     LGF_RVS((uint8_t *)(d+4224+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+107)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+516)),LGF_RVL((const uint8_t *)(cb+1828)))));   //row 33 [P0<<107 + 2 sys]
     LGF_RVS((uint8_t *)(d+4352+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+597)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+804)),LGF_RVL((const uint8_t *)(cb+19)))));   //row 34 [P2<<85+P3<<36 + 1 sys]
     LGF_RVS((uint8_t *)(d+4480+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+319)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+313)),LGF_RVL((const uint8_t *)(cb+1320)))));   //row 35 [P1<<63 + 2 sys]
     LGF_RVS((uint8_t *)(d+4736+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+91)),LGF_RVL((const uint8_t *)(kb+791))));   //row 37 [P0<<91+P3<<23 + 0 sys]
     LGF_RVS((uint8_t *)(d+4864+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+294)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+287)),LGF_RVL((const uint8_t *)(cb+1346)))));   //row 38 [P1<<38 + 2 sys]
     LGF_RVS((uint8_t *)(d+4992+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+546)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+111)),LGF_RVL((const uint8_t *)(cb+1836)))));   //row 39 [P2<<34 + 2 sys]
     LGF_RVS((uint8_t *)(d+5120+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+75)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+888)),LGF_RVL((const uint8_t *)(cb+512)))));   //row 40 [P0<<75+P3<<120 + 1 sys]
     LGF_RVS((uint8_t *)(d+5248+64*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+374)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+257)),LGF_RVL((const uint8_t *)(cb+1381)))));   //row 41 [P1<<118 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
