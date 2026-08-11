#include <string.h>
#include "PHY/sse_intrin.h"
// generated: BG2 Zc=384, 512-bit, factored
// 173 terms vs 1473 expanded (8.51x); 6 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc384_byte(uint8_t *c,uint8_t *d) {
  uint8_t kd[3072] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<6; i2++) {
     const uint8_t *cb=c+64*i2;
     __m512i p0=_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+24)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+26)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+173)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+864)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+881)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+903)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1701)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1710)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2339)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2369)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2420)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3119)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3181)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3184)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3911)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3931)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4638)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4678)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4730)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+5493)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+5562)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+6171)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+6257)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+6328)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+6914)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+7083)),_mm512_loadu_si512((const void *)(cb+7097))))))))))))))))))))))))))));
     _mm512_storeu_si512((void *)(d+64*i2),p0);
     _mm512_storeu_si512((void *)(d+3840+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+59)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+872)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4630)),_mm512_loadu_si512((const void *)(cb+5428))))));   //row 10
     _mm512_storeu_si512((void *)(d+6912+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+186)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4614)),_mm512_loadu_si512((const void *)(cb+5422)))));   //row 18
     _mm512_storeu_si512((void *)(d+8448+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+788)),_mm512_loadu_si512((const void *)(cb+1588))));   //row 22
     _mm512_storeu_si512((void *)(d+8832+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+106)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2390)),_mm512_loadu_si512((const void *)(cb+3935)))));   //row 23
     _mm512_storeu_si512((void *)(d+9216+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+950)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1689)),_mm512_loadu_si512((const void *)(cb+6976)))));   //row 24
     _mm512_storeu_si512((void *)(d+9600+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+45)),_mm512_loadu_si512((const void *)(cb+3861))));   //row 25
     _mm512_storeu_si512((void *)(d+10368+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+103)),_mm512_loadu_si512((const void *)(cb+4658))));   //row 27
     _mm512_storeu_si512((void *)(d+10752+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+838)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1647)),_mm512_loadu_si512((const void *)(cb+4008)))));   //row 28
     _mm512_storeu_si512((void *)(d+11136+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+110)),_mm512_loadu_si512((const void *)(cb+3089))));   //row 29
     _mm512_storeu_si512((void *)(d+11520+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1656)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3994)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+5428)),_mm512_loadu_si512((const void *)(cb+6968))))));   //row 30
     _mm512_storeu_si512((void *)(d+13824+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+25)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1687)),_mm512_loadu_si512((const void *)(cb+5546)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+64*(i2-1);
       uint8_t *dm=d+64*(i2-1);
       __m512i p1=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+174)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+865)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1702)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2370)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+4679)),_mm512_loadu_si512((const void *)(cm+7084))))))));
       _mm512_storeu_si512((void *)(dm+384),p1);
       __m512i p2=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+384)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+27)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2340)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3120)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3932)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+4639)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+5563)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+6329)),_mm512_loadu_si512((const void *)(cm+6915))))))))));
       _mm512_storeu_si512((void *)(dm+768),p2);
       __m512i p3=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+768)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+1)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+25)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+882)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2421)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3182)),_mm512_loadu_si512((const void *)(cm+6258))))))));
       _mm512_storeu_si512((void *)(dm+1152),p3);
     }
  }

  memcpy(kd,d,384); memcpy(kd+384,d,384);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+320, *km=kd+320; uint8_t *dm=d+320;
    __m512i p1=_mm512_xor_si512(_mm512_loadu_si512((const void *)(km+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+174)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+865)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1702)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2370)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+4679)),_mm512_loadu_si512((const void *)(cm+7084))))))));
    _mm512_storeu_si512((void *)(dm+384),p1);
    __m512i p2=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+384)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+27)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2340)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3120)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3932)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+4639)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+5563)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+6329)),_mm512_loadu_si512((const void *)(cm+6915))))))))));
    _mm512_storeu_si512((void *)(dm+768),p2);
    __m512i p3=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+768)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(km+1)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+25)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+882)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2421)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3182)),_mm512_loadu_si512((const void *)(cm+6258))))))));
    _mm512_storeu_si512((void *)(dm+1152),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*768,d+k*384,384); memcpy(kd+k*768+384,d+k*384,384); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<6; i2++) {
     const uint8_t *cb=c+64*i2, *kb=kd+64*i2;
     _mm512_storeu_si512((void *)(d+1536+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+797)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+72)),_mm512_loadu_si512((const void *)(cb+842)))));   //row 4 [P1<<29 + 2 sys]
     _mm512_storeu_si512((void *)(d+1920+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+816)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+10)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+812)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3961)),_mm512_loadu_si512((const void *)(cb+5456)))))));   //row 5 [P1<<48 + 4 sys]
     _mm512_storeu_si512((void *)(d+2304+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+952)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+129)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3932)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+5476)),_mm512_loadu_si512((const void *)(cb+6961)))))));   //row 6 [P1<<184 + 4 sys]
     _mm512_storeu_si512((void *)(d+2688+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+870)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+2447)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+848)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4026)),_mm512_loadu_si512((const void *)(cb+5392)))))));   //row 7 [P1<<102+P3<<143 + 3 sys]
     _mm512_storeu_si512((void *)(d+3072+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1688)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+118)),_mm512_loadu_si512((const void *)(cb+838)))));   //row 8 [P2<<152 + 2 sys]
     _mm512_storeu_si512((void *)(d+3456+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+185)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+946)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+796)),_mm512_loadu_si512((const void *)(cb+6276))))));   //row 9 [P0<<185+P1<<178 + 2 sys]
     _mm512_storeu_si512((void *)(d+4224+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+2458)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+32)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+5468)),_mm512_loadu_si512((const void *)(cb+7086))))));   //row 11 [P3<<154 + 3 sys]
     _mm512_storeu_si512((void *)(d+4608+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+779)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+807)),_mm512_loadu_si512((const void *)(cb+2397)))));   //row 12 [P1<<11 + 2 sys]
     _mm512_storeu_si512((void *)(d+4992+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+2470)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+49)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+893)),_mm512_loadu_si512((const void *)(cb+6179))))));   //row 13 [P3<<166 + 3 sys]
     _mm512_storeu_si512((void *)(d+5376+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+789)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+2467)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+787)),_mm512_loadu_si512((const void *)(cb+4726))))));   //row 14 [P1<<21+P3<<163 + 2 sys]
     _mm512_storeu_si512((void *)(d+5760+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+63)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+849)),_mm512_loadu_si512((const void *)(cb+68)))));   //row 15 [P0<<63+P1<<81 + 1 sys]
     _mm512_storeu_si512((void *)(d+6144+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+903)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1600)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+855)),_mm512_loadu_si512((const void *)(cb+7089))))));   //row 16 [P1<<135+P2<<64 + 2 sys]
     _mm512_storeu_si512((void *)(d+6528+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+777)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1542)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+926)),_mm512_loadu_si512((const void *)(cb+3863))))));   //row 17 [P1<<9+P2<<6 + 2 sys]
     _mm512_storeu_si512((void *)(d+7296+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+156)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+58)),_mm512_loadu_si512((const void *)(cb+810)))));   //row 19 [P0<<156 + 2 sys]
     _mm512_storeu_si512((void *)(d+7680+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+921)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+844)),_mm512_loadu_si512((const void *)(cb+3133)))));   //row 20 [P1<<153 + 2 sys]
     _mm512_storeu_si512((void *)(d+8064+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+2371)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+157)),_mm512_loadu_si512((const void *)(cb+6319)))));   //row 21 [P3<<67 + 2 sys]
     _mm512_storeu_si512((void *)(d+9984+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1591)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+2389)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1603)),_mm512_loadu_si512((const void *)(cb+5513))))));   //row 26 [P2<<55+P3<<85 + 2 sys]
     _mm512_storeu_si512((void *)(d+11904+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+2474)),_mm512_loadu_si512((const void *)(cb+771))));   //row 31 [P3<<170 + 1 sys]
     _mm512_storeu_si512((void *)(d+12288+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1553)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+84)),_mm512_loadu_si512((const void *)(cb+3848)))));   //row 32 [P2<<17 + 2 sys]
     _mm512_storeu_si512((void *)(d+12672+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+124)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1701)),_mm512_loadu_si512((const void *)(cb+5555)))));   //row 33 [P0<<124 + 2 sys]
     _mm512_storeu_si512((void *)(d+13056+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1713)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+2316)),_mm512_loadu_si512((const void *)(cb+173)))));   //row 34 [P2<<177+P3<<12 + 1 sys]
     _mm512_storeu_si512((void *)(d+13440+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+786)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+845)),_mm512_loadu_si512((const void *)(cb+4024)))));   //row 35 [P1<<18 + 2 sys]
     _mm512_storeu_si512((void *)(d+14208+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+37)),_mm512_loadu_si512((const void *)(kb+2335))));   //row 37 [P0<<37+P3<<31 + 0 sys]
     _mm512_storeu_si512((void *)(d+14592+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+958)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+852)),_mm512_loadu_si512((const void *)(cb+3991)))));   //row 38 [P1<<190 + 2 sys]
     _mm512_storeu_si512((void *)(d+14976+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1593)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+93)),_mm512_loadu_si512((const void *)(cb+5508)))));   //row 39 [P2<<57 + 2 sys]
     _mm512_storeu_si512((void *)(d+15360+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+107)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+2467)),_mm512_loadu_si512((const void *)(cb+1639)))));   //row 40 [P0<<107+P3<<163 + 1 sys]
     _mm512_storeu_si512((void *)(d+15744+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+828)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+915)),_mm512_loadu_si512((const void *)(cb+3847)))));   //row 41 [P1<<60 + 2 sys]
  }
}
