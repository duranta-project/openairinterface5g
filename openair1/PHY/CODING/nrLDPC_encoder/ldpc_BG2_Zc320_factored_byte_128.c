#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=320, byte encoding, factored
// 173 terms vs 1483 expanded (8.57x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc320_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[2560] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<20; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[127],c2[126],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[128],c2[127],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[140],c2[139],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[167],c2[166],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[180],c2[179],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[210],c2[209],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[220],c2[219],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[241],c2[240],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[246],c2[245],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[260],c2[259],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[284],c2[283],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[300],c2[299],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[324],c2[323],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[327],c2[326],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[340],c2[339],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[363],c2[362],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[364],c2[363],6),simde_mm_alignr_epi8(c2[380],c2[379],15)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[200]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[250],c2[249],12),simde_mm_alignr_epi8(c2[282],c2[281],4))));

//row: 18  [direct, 3 terms]
     d2[360]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],2),simde_mm_alignr_epi8(c2[286],c2[285],6)));

//row: 22  [direct, 2 terms]
     d2[440]=simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[81],c2[80],4));

//row: 23  [direct, 3 terms]
     d2[460]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[125],c2[124],11),simde_mm_alignr_epi8(c2[210],c2[209],14)));

//row: 24  [direct, 3 terms]
     d2[480]=simde_mm_xor_si128(c2[40],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],5),simde_mm_alignr_epi8(c2[366],c2[365],7)));

//row: 25  [direct, 2 terms]
     d2[500]=simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[205],c2[204],1));

//row: 27  [direct, 2 terms]
     d2[540]=simde_mm_xor_si128(c2[0],c2[242]);

//row: 28  [direct, 3 terms]
     d2[560]=simde_mm_xor_si128(c2[40],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],14),simde_mm_alignr_epi8(c2[207],c2[206],14)));

//row: 29  [direct, 2 terms]
     d2[580]=simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[170],c2[169],10));

//row: 30  [direct, 4 terms]
     d2[600]=simde_mm_xor_si128(c2[80],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[284],c2[283],3),simde_mm_alignr_epi8(c2[369],c2[368],6))));

//row: 36  [direct, 3 terms]
     d2[720]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[84],c2[83],15),simde_mm_alignr_epi8(c2[286],c2[285],2)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[20]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[40],simde_mm_xor_si128(cm[80],simde_mm_xor_si128(cm[120],simde_mm_xor_si128(cm[240],cm[360]))))));
       dm[40]=simde_mm_xor_si128(dm[20],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[128],cm[127],12),simde_mm_xor_si128(cm[160],simde_mm_xor_si128(cm[200],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[246],cm[245],8),simde_mm_xor_si128(cm[280],simde_mm_xor_si128(cm[320],simde_mm_alignr_epi8(cm[364],cm[363],7)))))))));
       dm[60]=simde_mm_xor_si128(dm[40],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[46],cm[45],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[127],cm[126],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[161],cm[160],9),simde_mm_alignr_epi8(cm[327],cm[326],12)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[19]; dm=&dsimd[19];
  dm[20]=simde_mm_xor_si128(dsimd[19],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[40],simde_mm_xor_si128(cm[80],simde_mm_xor_si128(cm[120],simde_mm_xor_si128(cm[240],cm[360]))))));
  dm[40]=simde_mm_xor_si128(dsimd[39],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[128],cm[127],12),simde_mm_xor_si128(cm[160],simde_mm_xor_si128(cm[200],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[246],cm[245],8),simde_mm_xor_si128(cm[280],simde_mm_xor_si128(cm[320],simde_mm_alignr_epi8(cm[364],cm[363],7)))))))));
  dm[60]=simde_mm_xor_si128(dsimd[59],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[19],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[46],cm[45],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[127],cm[126],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[161],cm[160],9),simde_mm_alignr_epi8(cm[327],cm[326],12)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*640,     d+k*320, 320);
    memcpy(kd+k*640+320, d+k*320, 320);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<20; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<157 + 2 sys]
     d2[80]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+797)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[49],c2[48],8)));

//row: 5  [P1<<64 + 4 sys]
     d2[100]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+704)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],14),simde_mm_alignr_epi8(c2[289],c2[288],13)))));

//row: 6  [P1<<148 + 4 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+788)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[208],c2[207],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[287],c2[286],3),simde_mm_alignr_epi8(c2[363],c2[362],13)))));

//row: 7  [P1<<96+P3<<78 + 3 sys]
     d2[140]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+736)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1998)),simde_mm_xor_si128(c2[40],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],13),simde_mm_alignr_epi8(c2[290],c2[289],4)))));

//row: 8  [P2<<87 + 2 sys]
     d2[160]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1367)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[45],c2[44],1)));

//row: 9  [P0<<51+P1<<85 + 2 sys]
     d2[180]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+51)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+725)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[327],c2[326],1))));

//row: 11  [P3<<2 + 3 sys]
     d2[220]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1922)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[281],c2[280],7),simde_mm_alignr_epi8(c2[361],c2[360],4))));

//row: 12  [P1<<48 + 2 sys]
     d2[240]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+688)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[128],c2[127],1)));

//row: 13  [P3<<26 + 3 sys]
     d2[260]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1946)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(c2[47],simde_mm_alignr_epi8(c2[327],c2[326],6))));

//row: 14  [P1<<57+P3<<27 + 2 sys]
     d2[280]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+697)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1947)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[249],c2[248],10))));

//row: 15  [P0<<73+P1<<99 + 1 sys]
     d2[300]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+73)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+739)),c2[0]));

//row: 16  [P1<<111+P2<<143 + 2 sys]
     d2[320]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+751)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1423)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[365],c2[364],15))));

//row: 17  [P1<<109+P2<<18 + 2 sys]
     d2[340]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+749)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1298)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[202],c2[201],8))));

//row: 19  [P0<<154 + 2 sys]
     d2[380]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+154)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[50],c2[49],14)));

//row: 20  [P1<<104 + 2 sys]
     d2[400]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+744)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[170],c2[169],4)));

//row: 21  [P3<<33 + 2 sys]
     d2[420]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1953)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[322],c2[321],1)));

//row: 26  [P2<<13+P3<<7 + 2 sys]
     d2[520]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1293)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1927)),simde_mm_xor_si128(c2[80],simde_mm_alignr_epi8(c2[287],c2[286],4))));

//row: 31  [P3<<20 + 1 sys]
     d2[620]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1940)),c2[40]);

//row: 32  [P2<<122 + 2 sys]
     d2[640]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1402)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[202],c2[201],4)));

//row: 33  [P0<<13 + 2 sys]
     d2[660]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+13)),simde_mm_xor_si128(c2[80],simde_mm_alignr_epi8(c2[286],c2[285],8)));

//row: 34  [P2<<19+P3<<78 + 1 sys]
     d2[680]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1299)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1998)),c2[0]));

//row: 35  [P1<<6 + 2 sys]
     d2[700]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+646)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[210],c2[209],13)));

//row: 37  [P0<<0+P3<<144 + 0 sys]
     d2[740]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+0)),simde_mm_loadu_si128((const simde__m128i *)(kb+2064)));

//row: 38  [P1<<19 + 2 sys]
     d2[760]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+659)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[206],c2[205],13)));

//row: 39  [P2<<138 + 2 sys]
     d2[780]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1418)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[282],c2[281],8)));

//row: 40  [P0<<36+P3<<143 + 1 sys]
     d2[800]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+36)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2063)),c2[80]));

//row: 41  [P1<<55 + 2 sys]
     d2[820]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+695)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[201],c2[200],2)));
  }
}
