#include <string.h>
#include "PHY/sse_intrin.h"
// generated: BG2 Zc=256, 512-bit, factored
// 173 terms vs 1477 expanded (8.54x); 4 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc256_byte_512(uint8_t *c,uint8_t *d) {
  uint8_t kd[2048] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<4; i2++) {
     const uint8_t *cb=c+64*i2;
     __m512i p0=_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+8)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+80)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+166)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+519)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+625)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+628)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1081)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1227)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1561)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1579)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1701)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2099)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2205)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2300)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2663)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2684)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3260)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3280)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3297)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3637)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3739)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4113)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4319)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4335)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4735)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+4812)),_mm512_loadu_si512((const void *)(cb+4859))))))))))))))))))))))))))));
     _mm512_storeu_si512((void *)(d+64*i2),p0);
     _mm512_storeu_si512((void *)(d+2560+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+11)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+697)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3072)),_mm512_loadu_si512((const void *)(cb+3701))))));   //row 10
     _mm512_storeu_si512((void *)(d+4608+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+220)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3266)),_mm512_loadu_si512((const void *)(cb+3634)))));   //row 18
     _mm512_storeu_si512((void *)(d+5632+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+734)),_mm512_loadu_si512((const void *)(cb+1087))));   //row 22
     _mm512_storeu_si512((void *)(d+5888+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+23)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1771)),_mm512_loadu_si512((const void *)(cb+2798)))));   //row 23
     _mm512_storeu_si512((void *)(d+6144+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+558)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1163)),_mm512_loadu_si512((const void *)(cb+4616)))));   //row 24
     _mm512_storeu_si512((void *)(d+6400+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+228)),_mm512_loadu_si512((const void *)(cb+2716))));   //row 25
     _mm512_storeu_si512((void *)(d+6912+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+8)),_mm512_loadu_si512((const void *)(cb+3223))));   //row 27
     _mm512_storeu_si512((void *)(d+7168+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+610)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1125)),_mm512_loadu_si512((const void *)(cb+2695)))));   //row 28
     _mm512_storeu_si512((void *)(d+7424+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+18)),_mm512_loadu_si512((const void *)(cb+2076))));   //row 29
     _mm512_storeu_si512((void *)(d+7680+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1095)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2800)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3593)),_mm512_loadu_si512((const void *)(cb+4692))))));   //row 30
     _mm512_storeu_si512((void *)(d+9216+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+140)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1062)),_mm512_loadu_si512((const void *)(cb+3738)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+64*(i2-1);
       uint8_t *dm=d+64*(i2-1);
       __m512i p1=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+9)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+629)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1228)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1562)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3261)),_mm512_loadu_si512((const void *)(cm+4813))))))));
       _mm512_storeu_si512((void *)(dm+256),p1);
       __m512i p2=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+256)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+167)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1702)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2301)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2685)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3298)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3740)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+4320)),_mm512_loadu_si512((const void *)(cm+4860))))))))));
       _mm512_storeu_si512((void *)(dm+512),p2);
       __m512i p3=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+512)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+1)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+81)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+626)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1580)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2100)),_mm512_loadu_si512((const void *)(cm+4336))))))));
       _mm512_storeu_si512((void *)(dm+768),p3);
     }
  }

  memcpy(kd,d,256); memcpy(kd+256,d,256);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+192, *km=kd+192; uint8_t *dm=d+192;
    __m512i p1=_mm512_xor_si512(_mm512_loadu_si512((const void *)(km+0)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+9)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+629)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1228)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1562)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3261)),_mm512_loadu_si512((const void *)(cm+4813))))))));
    _mm512_storeu_si512((void *)(dm+256),p1);
    __m512i p2=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+256)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+167)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1702)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2301)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2685)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3298)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+3740)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+4320)),_mm512_loadu_si512((const void *)(cm+4860))))))))));
    _mm512_storeu_si512((void *)(dm+512),p2);
    __m512i p3=_mm512_xor_si512(_mm512_loadu_si512((const void *)(dm+512)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(km+1)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+81)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+626)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+1580)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cm+2100)),_mm512_loadu_si512((const void *)(cm+4336))))))));
    _mm512_storeu_si512((void *)(dm+768),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*512,d+k*256,256); memcpy(kd+k*512+256,d+k*256,256); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<4; i2++) {
     const uint8_t *cb=c+64*i2, *kb=kd+64*i2;
     _mm512_storeu_si512((void *)(d+1024+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+583)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+179)),_mm512_loadu_si512((const void *)(cb+726)))));   //row 4 [P1<<71 + 2 sys]
     _mm512_storeu_si512((void *)(d+1280+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+615)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+231)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+553)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2754)),_mm512_loadu_si512((const void *)(cb+3743)))))));   //row 5 [P1<<103 + 4 sys]
     _mm512_storeu_si512((void *)(d+1536+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+670)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+155)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2788)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3629)),_mm512_loadu_si512((const void *)(cb+4636)))))));   //row 6 [P1<<158 + 4 sys]
     _mm512_storeu_si512((void *)(d+1792+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+515)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1652)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+641)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+2707)),_mm512_loadu_si512((const void *)(cb+3724)))))));   //row 7 [P1<<3+P3<<116 + 3 sys]
     _mm512_storeu_si512((void *)(d+2048+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1254)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+142)),_mm512_loadu_si512((const void *)(cb+606)))));   //row 8 [P2<<230 + 2 sys]
     _mm512_storeu_si512((void *)(d+2304+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+61)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+759)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+715)),_mm512_loadu_si512((const void *)(cb+4301))))));   //row 9 [P0<<61+P1<<247 + 2 sys]
     _mm512_storeu_si512((void *)(d+2816+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1592)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+11)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+3820)),_mm512_loadu_si512((const void *)(cb+4818))))));   //row 11 [P3<<56 + 3 sys]
     _mm512_storeu_si512((void *)(d+3072+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+526)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+575)),_mm512_loadu_si512((const void *)(cb+1647)))));   //row 12 [P1<<14 + 2 sys]
     _mm512_storeu_si512((void *)(d+3328+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1758)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+83)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+514)),_mm512_loadu_si512((const void *)(cb+4134))))));   //row 13 [P3<<222 + 3 sys]
     _mm512_storeu_si512((void *)(d+3584+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+515)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1768)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+627)),_mm512_loadu_si512((const void *)(cb+3217))))));   //row 14 [P1<<3+P3<<232 + 2 sys]
     _mm512_storeu_si512((void *)(d+3840+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+175)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+725)),_mm512_loadu_si512((const void *)(cb+51)))));   //row 15 [P0<<175+P1<<213 + 1 sys]
     _mm512_storeu_si512((void *)(d+4096+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+520)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1266)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+715)),_mm512_loadu_si512((const void *)(cb+4750))))));   //row 16 [P1<<8+P2<<242 + 2 sys]
     _mm512_storeu_si512((void *)(d+4352+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+626)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1088)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+766)),_mm512_loadu_si512((const void *)(cb+2684))))));   //row 17 [P1<<114+P2<<64 + 2 sys]
     _mm512_storeu_si512((void *)(d+4864+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+185)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+87)),_mm512_loadu_si512((const void *)(cb+532)))));   //row 19 [P0<<185 + 2 sys]
     _mm512_storeu_si512((void *)(d+5120+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+541)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+538)),_mm512_loadu_si512((const void *)(cb+2153)))));   //row 20 [P1<<29 + 2 sys]
     _mm512_storeu_si512((void *)(d+5376+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1746)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+76)),_mm512_loadu_si512((const void *)(cb+4138)))));   //row 21 [P3<<210 + 2 sys]
     _mm512_storeu_si512((void *)(d+6656+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1184)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1658)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1053)),_mm512_loadu_si512((const void *)(cb+3727))))));   //row 26 [P2<<160+P3<<122 + 2 sys]
     _mm512_storeu_si512((void *)(d+7936+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1537)),_mm512_loadu_si512((const void *)(cb+618))));   //row 31 [P3<<1 + 1 sys]
     _mm512_storeu_si512((void *)(d+8192+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1190)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+242)),_mm512_loadu_si512((const void *)(cb+2604)))));   //row 32 [P2<<166 + 2 sys]
     _mm512_storeu_si512((void *)(d+8448+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+235)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+1156)),_mm512_loadu_si512((const void *)(cb+3748)))));   //row 33 [P0<<235 + 2 sys]
     _mm512_storeu_si512((void *)(d+8704+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1109)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1572)),_mm512_loadu_si512((const void *)(cb+147)))));   //row 34 [P2<<85+P3<<36 + 1 sys]
     _mm512_storeu_si512((void *)(d+8960+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+575)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+569)),_mm512_loadu_si512((const void *)(cb+2600)))));   //row 35 [P1<<63 + 2 sys]
     _mm512_storeu_si512((void *)(d+9472+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+219)),_mm512_loadu_si512((const void *)(kb+1687))));   //row 37 [P0<<219+P3<<151 + 0 sys]
     _mm512_storeu_si512((void *)(d+9728+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+550)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+543)),_mm512_loadu_si512((const void *)(cb+2626)))));   //row 38 [P1<<38 + 2 sys]
     _mm512_storeu_si512((void *)(d+9984+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1058)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+239)),_mm512_loadu_si512((const void *)(cb+3756)))));   //row 39 [P2<<34 + 2 sys]
     _mm512_storeu_si512((void *)(d+10240+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+75)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+1656)),_mm512_loadu_si512((const void *)(cb+1024)))));   //row 40 [P0<<75+P3<<120 + 1 sys]
     _mm512_storeu_si512((void *)(d+10496+64*i2),_mm512_xor_si512(_mm512_loadu_si512((const void *)(kb+630)),_mm512_xor_si512(_mm512_loadu_si512((const void *)(cb+641)),_mm512_loadu_si512((const void *)(cb+2789)))));   //row 41 [P1<<118 + 2 sys]
  }
}
