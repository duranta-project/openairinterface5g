#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=48, byte encoding, factored
// 173 terms vs 1435 expanded (8.29x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc48_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[384] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<3; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_xor_si128(c2[6],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],13),simde_mm_xor_si128(c2[25],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],11),simde_mm_alignr_epi8(c2[57],c2[56],9)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[30]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],6),simde_mm_alignr_epi8(c2[43],c2[42],4))));

//row: 18  [direct, 3 terms]
     d2[54]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],6),simde_mm_alignr_epi8(c2[45],c2[44],14)));

//row: 22  [direct, 2 terms]
     d2[66]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],4),simde_mm_alignr_epi8(c2[13],c2[12],4));

//row: 23  [direct, 3 terms]
     d2[69]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],6),simde_mm_alignr_epi8(c2[33],c2[32],15)));

//row: 24  [direct, 3 terms]
     d2[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],9),c2[55]));

//row: 25  [direct, 2 terms]
     d2[75]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],13),simde_mm_alignr_epi8(c2[32],c2[31],5));

//row: 27  [direct, 2 terms]
     d2[81]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],7),simde_mm_alignr_epi8(c2[37],c2[36],2));

//row: 28  [direct, 3 terms]
     d2[84]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],15),simde_mm_alignr_epi8(c2[32],c2[31],8)));

//row: 29  [direct, 2 terms]
     d2[87]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],14),simde_mm_alignr_epi8(c2[26],c2[25],1));

//row: 30  [direct, 4 terms]
     d2[90]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],4),simde_mm_alignr_epi8(c2[55],c2[54],8))));

//row: 36  [direct, 3 terms]
     d2[108]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],7),simde_mm_alignr_epi8(c2[44],c2[43],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[3]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[20],cm[19],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],7),simde_mm_alignr_epi8(cm[56],cm[55],12)))))));
       dm[6]=simde_mm_xor_si128(dm[3],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],4),simde_mm_xor_si128(cm[24],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[33],cm[32],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],9),simde_mm_alignr_epi8(cm[55],cm[54],3)))))))));
       dm[9]=simde_mm_xor_si128(dm[6],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[20],cm[19],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[25],cm[24],14),simde_mm_alignr_epi8(cm[50],cm[49],2)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[2]; dm=&dsimd[2];
  dm[3]=simde_mm_xor_si128(dsimd[2],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[20],cm[19],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],7),simde_mm_alignr_epi8(cm[56],cm[55],12)))))));
  dm[6]=simde_mm_xor_si128(dsimd[5],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],4),simde_mm_xor_si128(cm[24],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[33],cm[32],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],9),simde_mm_alignr_epi8(cm[55],cm[54],3)))))))));
  dm[9]=simde_mm_xor_si128(dsimd[8],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[2],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[20],cm[19],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[25],cm[24],14),simde_mm_alignr_epi8(cm[50],cm[49],2)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*96,     d+k*48, 48);
    memcpy(kd+k*96+48, d+k*48, 48);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<3; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<29 + 2 sys]
     d2[12]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+125)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],8),simde_mm_alignr_epi8(c2[8],c2[7],10)));

//row: 5  [P1<<0 + 4 sys]
     d2[15]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+96)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],9),c2[44]))));

//row: 6  [P1<<40 + 4 sys]
     d2[18]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+136)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],4),simde_mm_alignr_epi8(c2[55],c2[54],1)))));

//row: 7  [P1<<6+P3<<47 + 3 sys]
     d2[21]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+102)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+335)),simde_mm_xor_si128(c2[8],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],10),c2[43]))));

//row: 8  [P2<<8 + 2 sys]
     d2[24]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+200)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],6),simde_mm_alignr_epi8(c2[8],c2[7],6)));

//row: 9  [P0<<41+P1<<34 + 2 sys]
     d2[27]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+41)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+130)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],12),simde_mm_alignr_epi8(c2[51],c2[50],4))));

//row: 11  [P3<<10 + 3 sys]
     d2[33]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+298)),simde_mm_xor_si128(c2[2],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],12),simde_mm_alignr_epi8(c2[56],c2[55],14))));

//row: 12  [P1<<11 + 2 sys]
     d2[36]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+107)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_alignr_epi8(c2[21],c2[20],13)));

//row: 13  [P3<<22 + 3 sys]
     d2[39]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+310)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],13),simde_mm_alignr_epi8(c2[51],c2[50],3))));

//row: 14  [P1<<21+P3<<19 + 2 sys]
     d2[42]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+117)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+307)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],3),simde_mm_alignr_epi8(c2[38],c2[37],6))));

//row: 15  [P0<<15+P1<<33 + 1 sys]
     d2[45]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+15)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+129)),simde_mm_alignr_epi8(c2[2],c2[1],4)));

//row: 16  [P2<<16+P1<<39 + 2 sys]
     d2[48]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+208)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+135)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_alignr_epi8(c2[57],c2[56],1))));

//row: 17  [P1<<9+P2<<6 + 2 sys]
     d2[51]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+105)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+198)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_alignr_epi8(c2[32],c2[31],7))));

//row: 19  [P0<<12 + 2 sys]
     d2[57]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+12)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_alignr_epi8(c2[9],c2[8],10)));

//row: 20  [P1<<9 + 2 sys]
     d2[60]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+105)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],12),simde_mm_alignr_epi8(c2[25],c2[24],13)));

//row: 21  [P3<<19 + 2 sys]
     d2[63]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+307)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_alignr_epi8(c2[50],c2[49],15)));

//row: 26  [P2<<7+P3<<37 + 2 sys]
     d2[78]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+199)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+325)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],3),simde_mm_alignr_epi8(c2[45],c2[44],9))));

//row: 31  [P3<<26 + 1 sys]
     d2[93]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+314)),simde_mm_alignr_epi8(c2[7],c2[6],3));

//row: 32  [P2<<17 + 2 sys]
     d2[96]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+209)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],4),simde_mm_alignr_epi8(c2[31],c2[30],8)));

//row: 33  [P0<<28 + 2 sys]
     d2[99]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+28)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],5),simde_mm_alignr_epi8(c2[45],c2[44],3)));

//row: 34  [P2<<33+P3<<12 + 1 sys]
     d2[102]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+225)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+300)),simde_mm_alignr_epi8(c2[2],c2[1],13)));

//row: 35  [P1<<18 + 2 sys]
     d2[105]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+114)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],13),simde_mm_alignr_epi8(c2[33],c2[32],8)));

//row: 37  [P0<<37+P3<<31 + 0 sys]
     d2[111]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+37)),simde_mm_loadu_si128((const simde__m128i *)(kb+319)));

//row: 38  [P1<<46 + 2 sys]
     d2[114]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+142)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],4),simde_mm_alignr_epi8(c2[31],c2[30],7)));

//row: 39  [P2<<9 + 2 sys]
     d2[117]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+201)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],13),simde_mm_alignr_epi8(c2[45],c2[44],4)));

//row: 40  [P3<<19+P0<<11 + 1 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+307)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+11)),simde_mm_alignr_epi8(c2[13],c2[12],7)));

//row: 41  [P1<<12 + 2 sys]
     d2[123]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+108)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],3),simde_mm_alignr_epi8(c2[31],c2[30],7)));
  }
}
