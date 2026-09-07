#include <string.h>
#include "PHY/sse_intrin.h"
// generated: BG2 Zc=128, 512-bit, factored
// 173 terms vs 1469 expanded (8.49x); 2 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc128_byte_512(uint8_t *c,uint8_t *d) {
  uint8_t kd[1024] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<2; i2++) {
     const uint8_t *cb=c+64*i2;
     __m512i p0=_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+8)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+38)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+80)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+263)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+369)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+372)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+569)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+587)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+793)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+805)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+811)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1053)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1075)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1148)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1383)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1404)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1596)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1616)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1633)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1819)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1845)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2065)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2143)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2159)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2380)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2427)),_mm512_loadu_si512((const void *)(cb+2431))))))))))))))))))))))))))));
     _mm512_storeu_si512((void *)(d+64*i2),p0);
     _mm512_storeu_si512((void *)(d+1280+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+11)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+313)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1536)),_mm512_loadu_si512((const void *)(cb+1909))))));   //row 10
     _mm512_storeu_si512((void *)(d+2304+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+92)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1602)),_mm512_loadu_si512((const void *)(cb+1842)))));   //row 18
     _mm512_storeu_si512((void *)(d+2816+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+350)),_mm512_loadu_si512((const void *)(cb+575))));   //row 22
     _mm512_storeu_si512((void *)(d+2944+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+23)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+875)),_mm512_loadu_si512((const void *)(cb+1390)))));   //row 23
     _mm512_storeu_si512((void *)(d+3072+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+302)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+523)),_mm512_loadu_si512((const void *)(cb+2312)))));   //row 24
     _mm512_storeu_si512((void *)(d+3200+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+100)),_mm512_loadu_si512((const void *)(cb+1308))));   //row 25
     _mm512_storeu_si512((void *)(d+3456+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+8)),_mm512_loadu_si512((const void *)(cb+1559))));   //row 27
     _mm512_storeu_si512((void *)(d+3584+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+354)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+613)),_mm512_loadu_si512((const void *)(cb+1287)))));   //row 28
     _mm512_storeu_si512((void *)(d+3712+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+18)),_mm512_loadu_si512((const void *)(cb+1052))));   //row 29
     _mm512_storeu_si512((void *)(d+3840+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+583)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1392)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1801)),_mm512_loadu_si512((const void *)(cb+2388))))));   //row 30
     _mm512_storeu_si512((void *)(d+4608+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+12)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+550)),_mm512_loadu_si512((const void *)(cb+1818)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+64*(i2-1);
       uint8_t *dm=d+64*(i2-1);
       __m512i p1=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+9)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+373)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+588)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+794)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1597)),_mm512_loadu_si512((const void *)(cm+2381))))))));
       _mm512_storeu_si512((void *)(dm+128),p1);
       __m512i p2=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+128)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+39)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+806)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1149)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1405)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1634)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1820)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2144)),_mm512_loadu_si512((const void *)(cm+2428))))))))));
       _mm512_storeu_si512((void *)(dm+256),p2);
       __m512i p3=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+256)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+1)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+81)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+370)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+812)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1076)),_mm512_loadu_si512((const void *)(cm+2160))))))));
       _mm512_storeu_si512((void *)(dm+384),p3);
     }
  }

  memcpy(kd,d,128); memcpy(kd+128,d,128);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+64, *km=kd+64; uint8_t *dm=d+64;
    __m512i p1=_mm512_xor_si512(_mm512_loadu_si512((const void *)(km+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+9)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+373)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+588)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+794)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1597)),_mm512_loadu_si512((const void *)(cm+2381))))))));
    _mm512_storeu_si512((void *)(dm+128),p1);
    __m512i p2=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+128)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+39)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+806)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1149)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1405)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1634)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1820)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2144)),_mm512_loadu_si512((const void *)(cm+2428))))))))));
    _mm512_storeu_si512((void *)(dm+256),p2);
    __m512i p3=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+256)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(km+1)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+81)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+370)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+812)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1076)),_mm512_loadu_si512((const void *)(cm+2160))))))));
    _mm512_storeu_si512((void *)(dm+384),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*256,d+k*128,128); memcpy(kd+k*256+128,d+k*128,128); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<2; i2++) {
     const uint8_t *cb=c+64*i2, *kb=kd+64*i2;
     _mm512_storeu_si512((void *)(d+512+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+327)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+51)),_mm512_loadu_si512((const void *)(cb+342)))));   //row 4 [P1<<71 + 2 sys]
     _mm512_storeu_si512((void *)(d+640+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+359)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+103)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+297)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1346)),_mm512_loadu_si512((const void *)(cb+1823)))))));   //row 5 [P1<<103 + 4 sys]
     _mm512_storeu_si512((void *)(d+768+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+286)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+27)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1380)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1837)),_mm512_loadu_si512((const void *)(cb+2332)))))));   //row 6 [P1<<30 + 4 sys]
     _mm512_storeu_si512((void *)(d+896+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+259)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+884)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+257)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1299)),_mm512_loadu_si512((const void *)(cb+1804)))))));   //row 7 [P1<<3+P3<<116 + 3 sys]
     _mm512_storeu_si512((void *)(d+1024+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+614)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+14)),_mm512_loadu_si512((const void *)(cb+350)))));   //row 8 [P2<<102 + 2 sys]
     _mm512_storeu_si512((void *)(d+1152+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+61)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+375)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+331)),_mm512_loadu_si512((const void *)(cb+2125))))));   //row 9 [P0<<61+P1<<119 + 2 sys]
     _mm512_storeu_si512((void *)(d+1408+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+824)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+11)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1900)),_mm512_loadu_si512((const void *)(cb+2386))))));   //row 11 [P3<<56 + 3 sys]
     _mm512_storeu_si512((void *)(d+1536+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+270)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+319)),_mm512_loadu_si512((const void *)(cb+879)))));   //row 12 [P1<<14 + 2 sys]
     _mm512_storeu_si512((void *)(d+1664+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+862)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+83)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+258)),_mm512_loadu_si512((const void *)(cb+2086))))));   //row 13 [P3<<94 + 3 sys]
     _mm512_storeu_si512((void *)(d+1792+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+259)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+872)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+371)),_mm512_loadu_si512((const void *)(cb+1553))))));   //row 14 [P1<<3+P3<<104 + 2 sys]
     _mm512_storeu_si512((void *)(d+1920+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+47)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+341)),_mm512_loadu_si512((const void *)(cb+51)))));   //row 15 [P0<<47+P1<<85 + 1 sys]
     _mm512_storeu_si512((void *)(d+2048+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+264)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+626)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+331)),_mm512_loadu_si512((const void *)(cb+2318))))));   //row 16 [P1<<8+P2<<114 + 2 sys]
     _mm512_storeu_si512((void *)(d+2176+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+370)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+576)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+382)),_mm512_loadu_si512((const void *)(cb+1404))))));   //row 17 [P1<<114+P2<<64 + 2 sys]
     _mm512_storeu_si512((void *)(d+2432+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+57)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+87)),_mm512_loadu_si512((const void *)(cb+276)))));   //row 19 [P0<<57 + 2 sys]
     _mm512_storeu_si512((void *)(d+2560+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+285)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+282)),_mm512_loadu_si512((const void *)(cb+1129)))));   //row 20 [P1<<29 + 2 sys]
     _mm512_storeu_si512((void *)(d+2688+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+850)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+76)),_mm512_loadu_si512((const void *)(cb+2090)))));   //row 21 [P3<<82 + 2 sys]
     _mm512_storeu_si512((void *)(d+3328+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+544)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+890)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+541)),_mm512_loadu_si512((const void *)(cb+1807))))));   //row 26 [P2<<32+P3<<122 + 2 sys]
     _mm512_storeu_si512((void *)(d+3968+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+769)),_mm512_loadu_si512((const void *)(cb+362))));   //row 31 [P3<<1 + 1 sys]
     _mm512_storeu_si512((void *)(d+4096+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+550)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+114)),_mm512_loadu_si512((const void *)(cb+1324)))));   //row 32 [P2<<38 + 2 sys]
     _mm512_storeu_si512((void *)(d+4224+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+107)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+516)),_mm512_loadu_si512((const void *)(cb+1828)))));   //row 33 [P0<<107 + 2 sys]
     _mm512_storeu_si512((void *)(d+4352+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+597)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+804)),_mm512_loadu_si512((const void *)(cb+19)))));   //row 34 [P2<<85+P3<<36 + 1 sys]
     _mm512_storeu_si512((void *)(d+4480+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+319)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+313)),_mm512_loadu_si512((const void *)(cb+1320)))));   //row 35 [P1<<63 + 2 sys]
     _mm512_storeu_si512((void *)(d+4736+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+91)),_mm512_loadu_si512((const void *)(kb+791))));   //row 37 [P0<<91+P3<<23 + 0 sys]
     _mm512_storeu_si512((void *)(d+4864+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+294)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+287)),_mm512_loadu_si512((const void *)(cb+1346)))));   //row 38 [P1<<38 + 2 sys]
     _mm512_storeu_si512((void *)(d+4992+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+546)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+111)),_mm512_loadu_si512((const void *)(cb+1836)))));   //row 39 [P2<<34 + 2 sys]
     _mm512_storeu_si512((void *)(d+5120+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+75)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+888)),_mm512_loadu_si512((const void *)(cb+512)))));   //row 40 [P0<<75+P3<<120 + 1 sys]
     _mm512_storeu_si512((void *)(d+5248+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+374)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+257)),_mm512_loadu_si512((const void *)(cb+1381)))));   //row 41 [P1<<118 + 2 sys]
  }
}
