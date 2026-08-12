#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=32, byte encoding, factored
// 173 terms vs 1391 expanded (8.04x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc32_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[256] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<2; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_xor_si128(c2[1],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],1),simde_mm_xor_si128(c2[25],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],11),simde_mm_alignr_epi8(c2[38],c2[37],15)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[20]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],9),simde_mm_xor_si128(c2[24],simde_mm_alignr_epi8(c2[30],c2[29],5))));

//row: 18  [direct, 3 terms]
     d2[36]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],2),simde_mm_alignr_epi8(c2[30],c2[29],2)));

//row: 22  [direct, 2 terms]
     d2[44]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],14),simde_mm_alignr_epi8(c2[10],c2[9],15));

//row: 23  [direct, 3 terms]
     d2[46]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],11),simde_mm_alignr_epi8(c2[21],c2[20],14)));

//row: 24  [direct, 3 terms]
     d2[48]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],11),simde_mm_alignr_epi8(c2[37],c2[36],8)));

//row: 25  [direct, 2 terms]
     d2[50]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],4),simde_mm_alignr_epi8(c2[22],c2[21],12));

//row: 27  [direct, 2 terms]
     d2[54]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_alignr_epi8(c2[26],c2[25],7));

//row: 28  [direct, 3 terms]
     d2[56]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],5),simde_mm_alignr_epi8(c2[21],c2[20],7)));

//row: 29  [direct, 2 terms]
     d2[58]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_alignr_epi8(c2[18],c2[17],12));

//row: 30  [direct, 4 terms]
     d2[60]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_xor_si128(c2[21],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],9),simde_mm_alignr_epi8(c2[38],c2[37],4))));

//row: 36  [direct, 3 terms]
     d2[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],6),simde_mm_alignr_epi8(c2[30],c2[29],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[2]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],13),simde_mm_alignr_epi8(cm[37],cm[36],13)))))));
       dm[4]=simde_mm_xor_si128(dm[2],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[13],cm[12],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[22],cm[21],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[25],cm[24],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],12),simde_mm_xor_si128(cm[32],simde_mm_alignr_epi8(cm[38],cm[37],12)))))))));
       dm[6]=simde_mm_xor_si128(dm[4],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[13],cm[12],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],4),cm[33]))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[1]; dm=&dsimd[1];
  dm[2]=simde_mm_xor_si128(dsimd[1],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],13),simde_mm_alignr_epi8(cm[37],cm[36],13)))))));
  dm[4]=simde_mm_xor_si128(dsimd[3],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[13],cm[12],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[22],cm[21],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[25],cm[24],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],12),simde_mm_xor_si128(cm[32],simde_mm_alignr_epi8(cm[38],cm[37],12)))))))));
  dm[6]=simde_mm_xor_si128(dsimd[5],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[13],cm[12],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],4),cm[33]))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*64,     d+k*32, 32);
    memcpy(kd+k*64+32, d+k*32, 32);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<2; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<7 + 2 sys]
     d2[8]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+71)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_alignr_epi8(c2[6],c2[5],6)));

//row: 5  [P1<<7 + 4 sys]
     d2[10]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+71)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],2),simde_mm_alignr_epi8(c2[30],c2[29],15)))));

//row: 6  [P1<<30 + 4 sys]
     d2[12]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+94)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],13),simde_mm_alignr_epi8(c2[38],c2[37],12)))));

//row: 7  [P1<<3+P3<<20 + 3 sys]
     d2[14]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+67)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+212)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],3),simde_mm_alignr_epi8(c2[29],c2[28],12)))));

//row: 8  [P2<<6 + 2 sys]
     d2[16]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+134)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],14),simde_mm_alignr_epi8(c2[6],c2[5],14)));

//row: 9  [P0<<29+P1<<23 + 2 sys]
     d2[18]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+29)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+87)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],11),simde_mm_alignr_epi8(c2[33],c2[32],13))));

//row: 11  [P3<<24 + 3 sys]
     d2[22]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+216)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],12),simde_mm_alignr_epi8(c2[38],c2[37],2))));

//row: 12  [P1<<14 + 2 sys]
     d2[24]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+78)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],15),simde_mm_alignr_epi8(c2[13],c2[12],15)));

//row: 13  [P3<<30 + 3 sys]
     d2[26]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+222)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],2),simde_mm_alignr_epi8(c2[33],c2[32],6))));

//row: 14  [P1<<3+P3<<8 + 2 sys]
     d2[28]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+67)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+200)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],3),simde_mm_alignr_epi8(c2[26],c2[25],1))));

//row: 15  [P0<<15+P1<<21 + 1 sys]
     d2[30]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+15)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+85)),simde_mm_alignr_epi8(c2[2],c2[1],3)));

//row: 16  [P1<<8+P2<<18 + 2 sys]
     d2[32]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+72)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+146)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],11),simde_mm_alignr_epi8(c2[37],c2[36],14))));

//row: 17  [P1<<18+P2<<0 + 2 sys]
     d2[34]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+82)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+128)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],14),simde_mm_alignr_epi8(c2[22],c2[21],12))));

//row: 19  [P0<<25 + 2 sys]
     d2[38]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+25)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_alignr_epi8(c2[6],c2[5],4)));

//row: 20  [P1<<29 + 2 sys]
     d2[40]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+93)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],10),simde_mm_alignr_epi8(c2[17],c2[16],9)));

//row: 21  [P3<<18 + 2 sys]
     d2[42]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+210)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],12),simde_mm_alignr_epi8(c2[33],c2[32],10)));

//row: 26  [P2<<0+P3<<26 + 2 sys]
     d2[52]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+128)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+218)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],13),simde_mm_alignr_epi8(c2[29],c2[28],15))));

//row: 31  [P3<<1 + 1 sys]
     d2[62]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+193)),simde_mm_alignr_epi8(c2[5],c2[4],10));

//row: 32  [P2<<6 + 2 sys]
     d2[64]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+134)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_alignr_epi8(c2[21],c2[20],12)));

//row: 33  [P0<<11 + 2 sys]
     d2[66]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+11)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],4),simde_mm_alignr_epi8(c2[29],c2[28],4)));

//row: 34  [P2<<21+P3<<4 + 1 sys]
     d2[68]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+149)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+196)),simde_mm_alignr_epi8(c2[2],c2[1],3)));

//row: 35  [P1<<31 + 2 sys]
     d2[70]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+95)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],9),simde_mm_alignr_epi8(c2[21],c2[20],8)));

//row: 37  [P0<<27+P3<<23 + 0 sys]
     d2[74]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+27)),simde_mm_loadu_si128((const simde__m128i *)(kb+215)));

//row: 38  [P1<<6 + 2 sys]
     d2[76]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+70)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],15),simde_mm_alignr_epi8(c2[21],c2[20],2)));

//row: 39  [P2<<2 + 2 sys]
     d2[78]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+130)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],15),simde_mm_alignr_epi8(c2[29],c2[28],12)));

//row: 40  [P0<<11+P3<<24 + 1 sys]
     d2[80]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+11)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+216)),c2[8]));

//row: 41  [P1<<22 + 2 sys]
     d2[82]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+86)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],1),simde_mm_alignr_epi8(c2[21],c2[20],5)));
  }
}
