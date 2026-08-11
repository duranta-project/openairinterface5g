#include <string.h>
#include "PHY/sse_intrin.h"
// generated: BG2 Zc=320, 512-bit, factored
// 173 terms vs 1483 expanded (8.57x); 5 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc320_byte(uint8_t *c,uint8_t *d) {
  uint8_t kd[2560] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<5; i2++) {
     const uint8_t *cb=c+64*i2;
     __m512i p0=_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+19)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+136)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+319)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+677)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+733)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+959)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1294)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1599)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2018)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2043)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2239)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2568)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2661)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2879)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3345)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3519)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3851)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3927)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4159)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4536)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4799)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+5172)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+5227)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+5439)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+5805)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+5814)),_mm512_loadu_si512((const void *)(cb+6079))))))))))))))))))))))))))));
     _mm512_storeu_si512((void *)(d+64*i2),p0);
     _mm512_storeu_si512((void *)(d+3200+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+657)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3996)),_mm512_loadu_si512((const void *)(cb+4500))))));   //row 10
     _mm512_storeu_si512((void *)(d+5760+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3858)),_mm512_loadu_si512((const void *)(cb+4566)))));   //row 18
     _mm512_storeu_si512((void *)(d+7040+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_loadu_si512((const void *)(cb+1284))));   //row 22
     _mm512_storeu_si512((void *)(d+7360+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1995)),_mm512_loadu_si512((const void *)(cb+3358)))));   //row 23
     _mm512_storeu_si512((void *)(d+7680+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1349)),_mm512_loadu_si512((const void *)(cb+5847)))));   //row 24
     _mm512_storeu_si512((void *)(d+8000+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_loadu_si512((const void *)(cb+3265))));   //row 25
     _mm512_storeu_si512((void *)(d+8640+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_loadu_si512((const void *)(cb+3872))));   //row 27
     _mm512_storeu_si512((void *)(d+8960+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1406)),_mm512_loadu_si512((const void *)(cb+3310)))));   //row 28
     _mm512_storeu_si512((void *)(d+9280+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_loadu_si512((const void *)(cb+2714))));   //row 29
     _mm512_storeu_si512((void *)(d+9600+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1280)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3235)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4531)),_mm512_loadu_si512((const void *)(cb+5894))))));   //row 30
     _mm512_storeu_si512((void *)(d+11520+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1343)),_mm512_loadu_si512((const void *)(cb+4562)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+64*(i2-1);
       uint8_t *dm=d+64*(i2-1);
       __m512i p1=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+640)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1280)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1920)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3840)),_mm512_loadu_si512((const void *)(cm+5760))))))));
       _mm512_storeu_si512((void *)(dm+320),p1);
       __m512i p2=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+320)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+137)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2044)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2560)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3200)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3928)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+4480)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+5120)),_mm512_loadu_si512((const void *)(cm+5815))))))))));
       _mm512_storeu_si512((void *)(dm+640),p2);
       __m512i p3=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+640)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+1)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+20)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+734)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2019)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2569)),_mm512_loadu_si512((const void *)(cm+5228))))))));
       _mm512_storeu_si512((void *)(dm+960),p3);
     }
  }

  memcpy(kd,d,320); memcpy(kd+320,d,320);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+256, *km=kd+256; uint8_t *dm=d+256;
    __m512i p1=_mm512_xor_si512(_mm512_loadu_si512((const void *)(km+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+640)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1280)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1920)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3840)),_mm512_loadu_si512((const void *)(cm+5760))))))));
    _mm512_storeu_si512((void *)(dm+320),p1);
    __m512i p2=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+320)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+137)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2044)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2560)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3200)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3928)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+4480)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+5120)),_mm512_loadu_si512((const void *)(cm+5815))))))))));
    _mm512_storeu_si512((void *)(dm+640),p2);
    __m512i p3=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+640)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(km+1)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+20)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+734)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2019)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2569)),_mm512_loadu_si512((const void *)(cm+5228))))))));
    _mm512_storeu_si512((void *)(dm+960),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*640,d+k*320,320); memcpy(kd+k*640+320,d+k*320,320); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<5; i2++) {
     const uint8_t *cb=c+64*i2, *kb=kd+64*i2;
     _mm512_storeu_si512((void *)(d+1280+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+797)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_loadu_si512((const void *)(cb+776)))));   //row 4 [P1<<157 + 2 sys]
     _mm512_storeu_si512((void *)(d+1600+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+704)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+771)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3342)),_mm512_loadu_si512((const void *)(cb+4621)))))));   //row 5 [P1<<64 + 4 sys]
     _mm512_storeu_si512((void *)(d+1920+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+788)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3324)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4579)),_mm512_loadu_si512((const void *)(cb+5805)))))));   //row 6 [P1<<148 + 4 sys]
     _mm512_storeu_si512((void *)(d+2240+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+736)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1998)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3245)),_mm512_loadu_si512((const void *)(cb+4628)))))));   //row 7 [P1<<96+P3<<78 + 3 sys]
     _mm512_storeu_si512((void *)(d+2560+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1367)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_loadu_si512((const void *)(cb+705)))));   //row 8 [P2<<87 + 2 sys]
     _mm512_storeu_si512((void *)(d+2880+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+51)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+725)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_loadu_si512((const void *)(cb+5217))))));   //row 9 [P0<<51+P1<<85 + 2 sys]
     _mm512_storeu_si512((void *)(d+3520+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1922)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4487)),_mm512_loadu_si512((const void *)(cb+5764))))));   //row 11 [P3<<2 + 3 sys]
     _mm512_storeu_si512((void *)(d+3840+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+688)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_loadu_si512((const void *)(cb+2033)))));   //row 12 [P1<<48 + 2 sys]
     _mm512_storeu_si512((void *)(d+4160+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1946)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+752)),_mm512_loadu_si512((const void *)(cb+5222))))));   //row 13 [P3<<26 + 3 sys]
     _mm512_storeu_si512((void *)(d+4480+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+697)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1947)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_loadu_si512((const void *)(cb+3978))))));   //row 14 [P1<<57+P3<<27 + 2 sys]
     _mm512_storeu_si512((void *)(d+4800+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+73)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+739)),_mm512_loadu_si512((const void *)(cb+0)))));   //row 15 [P0<<73+P1<<99 + 1 sys]
     _mm512_storeu_si512((void *)(d+5120+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+751)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1423)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_loadu_si512((const void *)(cb+5839))))));   //row 16 [P1<<111+P2<<143 + 2 sys]
     _mm512_storeu_si512((void *)(d+5440+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+749)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1298)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_loadu_si512((const void *)(cb+3224))))));   //row 17 [P1<<109+P2<<18 + 2 sys]
     _mm512_storeu_si512((void *)(d+6080+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+154)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_loadu_si512((const void *)(cb+798)))));   //row 19 [P0<<154 + 2 sys]
     _mm512_storeu_si512((void *)(d+6400+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+744)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_loadu_si512((const void *)(cb+2708)))));   //row 20 [P1<<104 + 2 sys]
     _mm512_storeu_si512((void *)(d+6720+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1953)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_loadu_si512((const void *)(cb+5137)))));   //row 21 [P3<<33 + 2 sys]
     _mm512_storeu_si512((void *)(d+8320+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1293)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1927)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1280)),_mm512_loadu_si512((const void *)(cb+4580))))));   //row 26 [P2<<13+P3<<7 + 2 sys]
     _mm512_storeu_si512((void *)(d+9920+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1940)),_mm512_loadu_si512((const void *)(cb+640))));   //row 31 [P3<<20 + 1 sys]
     _mm512_storeu_si512((void *)(d+10240+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1402)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_loadu_si512((const void *)(cb+3220)))));   //row 32 [P2<<122 + 2 sys]
     _mm512_storeu_si512((void *)(d+10560+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+13)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1280)),_mm512_loadu_si512((const void *)(cb+4568)))));   //row 33 [P0<<13 + 2 sys]
     _mm512_storeu_si512((void *)(d+10880+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1299)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1998)),_mm512_loadu_si512((const void *)(cb+0)))));   //row 34 [P2<<19+P3<<78 + 1 sys]
     _mm512_storeu_si512((void *)(d+11200+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+646)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_loadu_si512((const void *)(cb+3357)))));   //row 35 [P1<<6 + 2 sys]
     _mm512_storeu_si512((void *)(d+11840+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+0)),_mm512_loadu_si512((const void *)(kb+2064))));   //row 37 [P0<<0+P3<<144 + 0 sys]
     _mm512_storeu_si512((void *)(d+12160+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+659)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_loadu_si512((const void *)(cb+3293)))));   //row 38 [P1<<19 + 2 sys]
     _mm512_storeu_si512((void *)(d+12480+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1418)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+0)),_mm512_loadu_si512((const void *)(cb+4504)))));   //row 39 [P2<<138 + 2 sys]
     _mm512_storeu_si512((void *)(d+12800+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+36)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+2063)),_mm512_loadu_si512((const void *)(cb+1280)))));   //row 40 [P0<<36+P3<<143 + 1 sys]
     _mm512_storeu_si512((void *)(d+13120+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+695)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+640)),_mm512_loadu_si512((const void *)(cb+3202)))));   //row 41 [P1<<55 + 2 sys]
  }
}
