#include <string.h>
#include "PHY/sse_intrin.h"
// generated: BG2 Zc=192, 512-bit, factored
// 173 terms vs 1469 expanded (8.49x); 3 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc192_byte_512(uint8_t *c,uint8_t *d) {
  uint8_t kd[1536] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<3; i2++) {
     const uint8_t *cb=c+64*i2;
     __m512i p0=_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+24)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+26)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+173)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+480)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+497)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+519)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+933)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+942)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1187)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1217)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1268)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1583)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1645)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1648)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1991)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2011)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2334)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2374)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2426)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2805)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2874)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3099)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3185)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3256)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3458)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3627)),_mm512_loadu_si512((const void *)(cb+3641))))))))))))))))))))))))))));
     _mm512_storeu_si512((void *)(d+64*i2),p0);
     _mm512_storeu_si512((void *)(d+1920+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+59)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+488)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2326)),_mm512_loadu_si512((const void *)(cb+2740))))));   //row 10
     _mm512_storeu_si512((void *)(d+3456+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+186)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2310)),_mm512_loadu_si512((const void *)(cb+2734)))));   //row 18
     _mm512_storeu_si512((void *)(d+4224+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+404)),_mm512_loadu_si512((const void *)(cb+820))));   //row 22
     _mm512_storeu_si512((void *)(d+4416+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+106)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1238)),_mm512_loadu_si512((const void *)(cb+2015)))));   //row 23
     _mm512_storeu_si512((void *)(d+4608+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+566)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+921)),_mm512_loadu_si512((const void *)(cb+3520)))));   //row 24
     _mm512_storeu_si512((void *)(d+4800+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+45)),_mm512_loadu_si512((const void *)(cb+1941))));   //row 25
     _mm512_storeu_si512((void *)(d+5184+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+103)),_mm512_loadu_si512((const void *)(cb+2354))));   //row 27
     _mm512_storeu_si512((void *)(d+5376+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+454)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+879)),_mm512_loadu_si512((const void *)(cb+2088)))));   //row 28
     _mm512_storeu_si512((void *)(d+5568+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+110)),_mm512_loadu_si512((const void *)(cb+1553))));   //row 29
     _mm512_storeu_si512((void *)(d+5760+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+888)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2074)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2740)),_mm512_loadu_si512((const void *)(cb+3512))))));   //row 30
     _mm512_storeu_si512((void *)(d+6912+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+25)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+919)),_mm512_loadu_si512((const void *)(cb+2858)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+64*(i2-1);
       uint8_t *dm=d+64*(i2-1);
       __m512i p1=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+174)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+481)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+934)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1218)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2375)),_mm512_loadu_si512((const void *)(cm+3628))))))));
       _mm512_storeu_si512((void *)(dm+192),p1);
       __m512i p2=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+192)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+27)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1188)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1584)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2012)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2335)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2875)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3257)),_mm512_loadu_si512((const void *)(cm+3459))))))))));
       _mm512_storeu_si512((void *)(dm+384),p2);
       __m512i p3=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+384)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+1)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+25)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+498)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1269)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1646)),_mm512_loadu_si512((const void *)(cm+3186))))))));
       _mm512_storeu_si512((void *)(dm+576),p3);
     }
  }

  memcpy(kd,d,192); memcpy(kd+192,d,192);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+128, *km=kd+128; uint8_t *dm=d+128;
    __m512i p1=_mm512_xor_si512(_mm512_loadu_si512((const void *)(km+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+174)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+481)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+934)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1218)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2375)),_mm512_loadu_si512((const void *)(cm+3628))))))));
    _mm512_storeu_si512((void *)(dm+192),p1);
    __m512i p2=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+192)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+27)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1188)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1584)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2012)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2335)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2875)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3257)),_mm512_loadu_si512((const void *)(cm+3459))))))))));
    _mm512_storeu_si512((void *)(dm+384),p2);
    __m512i p3=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+384)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(km+1)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+25)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+498)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1269)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1646)),_mm512_loadu_si512((const void *)(cm+3186))))))));
    _mm512_storeu_si512((void *)(dm+576),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*384,d+k*192,192); memcpy(kd+k*384+192,d+k*192,192); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<3; i2++) {
     const uint8_t *cb=c+64*i2, *kb=kd+64*i2;
     _mm512_storeu_si512((void *)(d+768+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+413)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+72)),_mm512_loadu_si512((const void *)(cb+458)))));   //row 4 [P1<<29 + 2 sys]
     _mm512_storeu_si512((void *)(d+960+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+432)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+10)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+428)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2041)),_mm512_loadu_si512((const void *)(cb+2768)))))));   //row 5 [P1<<48 + 4 sys]
     _mm512_storeu_si512((void *)(d+1152+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+568)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+129)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2012)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2788)),_mm512_loadu_si512((const void *)(cb+3505)))))));   //row 6 [P1<<184 + 4 sys]
     _mm512_storeu_si512((void *)(d+1344+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+486)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1295)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+464)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2106)),_mm512_loadu_si512((const void *)(cb+2704)))))));   //row 7 [P1<<102+P3<<143 + 3 sys]
     _mm512_storeu_si512((void *)(d+1536+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+920)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+118)),_mm512_loadu_si512((const void *)(cb+454)))));   //row 8 [P2<<152 + 2 sys]
     _mm512_storeu_si512((void *)(d+1728+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+185)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+562)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+412)),_mm512_loadu_si512((const void *)(cb+3204))))));   //row 9 [P0<<185+P1<<178 + 2 sys]
     _mm512_storeu_si512((void *)(d+2112+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1306)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+32)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2780)),_mm512_loadu_si512((const void *)(cb+3630))))));   //row 11 [P3<<154 + 3 sys]
     _mm512_storeu_si512((void *)(d+2304+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+395)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+423)),_mm512_loadu_si512((const void *)(cb+1245)))));   //row 12 [P1<<11 + 2 sys]
     _mm512_storeu_si512((void *)(d+2496+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1318)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+49)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+509)),_mm512_loadu_si512((const void *)(cb+3107))))));   //row 13 [P3<<166 + 3 sys]
     _mm512_storeu_si512((void *)(d+2688+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+405)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1315)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+403)),_mm512_loadu_si512((const void *)(cb+2422))))));   //row 14 [P1<<21+P3<<163 + 2 sys]
     _mm512_storeu_si512((void *)(d+2880+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+63)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+465)),_mm512_loadu_si512((const void *)(cb+68)))));   //row 15 [P0<<63+P1<<81 + 1 sys]
     _mm512_storeu_si512((void *)(d+3072+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+519)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+832)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+471)),_mm512_loadu_si512((const void *)(cb+3633))))));   //row 16 [P1<<135+P2<<64 + 2 sys]
     _mm512_storeu_si512((void *)(d+3264+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+393)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+774)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+542)),_mm512_loadu_si512((const void *)(cb+1943))))));   //row 17 [P1<<9+P2<<6 + 2 sys]
     _mm512_storeu_si512((void *)(d+3648+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+156)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+58)),_mm512_loadu_si512((const void *)(cb+426)))));   //row 19 [P0<<156 + 2 sys]
     _mm512_storeu_si512((void *)(d+3840+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+537)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+460)),_mm512_loadu_si512((const void *)(cb+1597)))));   //row 20 [P1<<153 + 2 sys]
     _mm512_storeu_si512((void *)(d+4032+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1219)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+157)),_mm512_loadu_si512((const void *)(cb+3247)))));   //row 21 [P3<<67 + 2 sys]
     _mm512_storeu_si512((void *)(d+4992+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+823)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1237)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+835)),_mm512_loadu_si512((const void *)(cb+2825))))));   //row 26 [P2<<55+P3<<85 + 2 sys]
     _mm512_storeu_si512((void *)(d+5952+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1322)),_mm512_loadu_si512((const void *)(cb+387))));   //row 31 [P3<<170 + 1 sys]
     _mm512_storeu_si512((void *)(d+6144+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+785)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+84)),_mm512_loadu_si512((const void *)(cb+1928)))));   //row 32 [P2<<17 + 2 sys]
     _mm512_storeu_si512((void *)(d+6336+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+124)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+933)),_mm512_loadu_si512((const void *)(cb+2867)))));   //row 33 [P0<<124 + 2 sys]
     _mm512_storeu_si512((void *)(d+6528+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+945)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1164)),_mm512_loadu_si512((const void *)(cb+173)))));   //row 34 [P2<<177+P3<<12 + 1 sys]
     _mm512_storeu_si512((void *)(d+6720+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+402)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+461)),_mm512_loadu_si512((const void *)(cb+2104)))));   //row 35 [P1<<18 + 2 sys]
     _mm512_storeu_si512((void *)(d+7104+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+37)),_mm512_loadu_si512((const void *)(kb+1183))));   //row 37 [P0<<37+P3<<31 + 0 sys]
     _mm512_storeu_si512((void *)(d+7296+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+574)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+468)),_mm512_loadu_si512((const void *)(cb+2071)))));   //row 38 [P1<<190 + 2 sys]
     _mm512_storeu_si512((void *)(d+7488+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+825)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+93)),_mm512_loadu_si512((const void *)(cb+2820)))));   //row 39 [P2<<57 + 2 sys]
     _mm512_storeu_si512((void *)(d+7680+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+107)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1315)),_mm512_loadu_si512((const void *)(cb+871)))));   //row 40 [P0<<107+P3<<163 + 1 sys]
     _mm512_storeu_si512((void *)(d+7872+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+444)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+531)),_mm512_loadu_si512((const void *)(cb+1927)))));   //row 41 [P1<<60 + 2 sys]
  }
}
