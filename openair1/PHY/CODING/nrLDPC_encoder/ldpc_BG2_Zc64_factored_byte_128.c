#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=64, byte encoding, factored
// 173 terms vs 1451 expanded (8.39x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc64_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[512] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<4; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_xor_si128(c2[1],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],12),simde_mm_xor_si128(c2[49],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[76],c2[75],11),simde_mm_alignr_epi8(c2[76],c2[75],15)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[40]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],9),simde_mm_xor_si128(c2[48],simde_mm_alignr_epi8(c2[60],c2[59],5))));

//row: 18  [direct, 3 terms]
     d2[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],2),simde_mm_alignr_epi8(c2[60],c2[59],2)));

//row: 22  [direct, 2 terms]
     d2[88]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],14),simde_mm_alignr_epi8(c2[20],c2[19],15));

//row: 23  [direct, 3 terms]
     d2[92]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],11),simde_mm_alignr_epi8(c2[43],c2[42],14)));

//row: 24  [direct, 3 terms]
     d2[96]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],11),simde_mm_alignr_epi8(c2[73],c2[72],8)));

//row: 25  [direct, 2 terms]
     d2[100]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],4),simde_mm_alignr_epi8(c2[42],c2[41],12));

//row: 27  [direct, 2 terms]
     d2[108]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_alignr_epi8(c2[50],c2[49],7));

//row: 28  [direct, 3 terms]
     d2[112]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],5),simde_mm_alignr_epi8(c2[41],c2[40],7)));

//row: 29  [direct, 2 terms]
     d2[116]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_alignr_epi8(c2[34],c2[33],12));

//row: 30  [direct, 4 terms]
     d2[120]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],7),simde_mm_xor_si128(c2[43],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],9),simde_mm_alignr_epi8(c2[74],c2[73],4))));

//row: 36  [direct, 3 terms]
     d2[144]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],6),simde_mm_alignr_epi8(c2[58],c2[57],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[4]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[12],cm[11],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[17],cm[16],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[52],cm[51],13),simde_mm_alignr_epi8(cm[73],cm[72],13)))))));
       dm[8]=simde_mm_xor_si128(dm[4],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[36],cm[35],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[44],cm[43],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[58],cm[57],12),simde_mm_xor_si128(cm[66],simde_mm_alignr_epi8(cm[76],cm[75],12)))))))));
       dm[12]=simde_mm_xor_si128(dm[8],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[12],cm[11],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[36],cm[35],4),cm[67]))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[3]; dm=&dsimd[3];
  dm[4]=simde_mm_xor_si128(dsimd[3],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[12],cm[11],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[17],cm[16],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[52],cm[51],13),simde_mm_alignr_epi8(cm[73],cm[72],13)))))));
  dm[8]=simde_mm_xor_si128(dsimd[7],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[36],cm[35],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[44],cm[43],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[58],cm[57],12),simde_mm_xor_si128(cm[66],simde_mm_alignr_epi8(cm[76],cm[75],12)))))))));
  dm[12]=simde_mm_xor_si128(dsimd[11],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[12],cm[11],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[36],cm[35],4),cm[67]))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*128,     d+k*64, 64);
    memcpy(kd+k*128+64, d+k*64, 64);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<4; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<7 + 2 sys]
     d2[16]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+135)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],3),simde_mm_alignr_epi8(c2[10],c2[9],6)));

//row: 5  [P1<<39 + 4 sys]
     d2[20]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+167)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],2),simde_mm_alignr_epi8(c2[58],c2[57],15)))));

//row: 6  [P1<<30 + 4 sys]
     d2[24]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+158)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],13),simde_mm_alignr_epi8(c2[74],c2[73],12)))));

//row: 7  [P1<<3+P3<<52 + 3 sys]
     d2[28]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+131)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+436)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],3),simde_mm_alignr_epi8(c2[57],c2[56],12)))));

//row: 8  [P2<<38 + 2 sys]
     d2[32]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+294)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],14),simde_mm_alignr_epi8(c2[10],c2[9],14)));

//row: 9  [P0<<61+P1<<55 + 2 sys]
     d2[36]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+61)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+183)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],11),simde_mm_alignr_epi8(c2[65],c2[64],13))));

//row: 11  [P3<<56 + 3 sys]
     d2[44]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+440)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],12),simde_mm_alignr_epi8(c2[74],c2[73],2))));

//row: 12  [P1<<14 + 2 sys]
     d2[48]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+142)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],15),simde_mm_alignr_epi8(c2[27],c2[26],15)));

//row: 13  [P3<<30 + 3 sys]
     d2[52]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+414)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],2),simde_mm_alignr_epi8(c2[67],c2[66],6))));

//row: 14  [P1<<3+P3<<40 + 2 sys]
     d2[56]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+131)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+424)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],3),simde_mm_alignr_epi8(c2[50],c2[49],1))));

//row: 15  [P0<<47+P1<<21 + 1 sys]
     d2[60]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+47)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+149)),simde_mm_alignr_epi8(c2[4],c2[3],3)));

//row: 16  [P1<<8+P2<<50 + 2 sys]
     d2[64]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+136)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+306)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],11),simde_mm_alignr_epi8(c2[73],c2[72],14))));

//row: 17  [P1<<50+P2<<0 + 2 sys]
     d2[68]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+178)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+256)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],14),simde_mm_alignr_epi8(c2[44],c2[43],12))));

//row: 19  [P0<<57 + 2 sys]
     d2[76]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+57)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_alignr_epi8(c2[10],c2[9],4)));

//row: 20  [P1<<29 + 2 sys]
     d2[80]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+157)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],10),simde_mm_alignr_epi8(c2[35],c2[34],9)));

//row: 21  [P3<<18 + 2 sys]
     d2[84]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+402)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],12),simde_mm_alignr_epi8(c2[67],c2[66],10)));

//row: 26  [P2<<32+P3<<58 + 2 sys]
     d2[104]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+288)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+442)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],13),simde_mm_alignr_epi8(c2[57],c2[56],15))));

//row: 31  [P3<<1 + 1 sys]
     d2[124]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+385)),simde_mm_alignr_epi8(c2[11],c2[10],10));

//row: 32  [P2<<38 + 2 sys]
     d2[128]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+294)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],2),simde_mm_alignr_epi8(c2[43],c2[42],12)));

//row: 33  [P0<<43 + 2 sys]
     d2[132]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+43)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],4),simde_mm_alignr_epi8(c2[59],c2[58],4)));

//row: 34  [P2<<21+P3<<36 + 1 sys]
     d2[136]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+277)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+420)),simde_mm_alignr_epi8(c2[2],c2[1],3)));

//row: 35  [P1<<63 + 2 sys]
     d2[140]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+191)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],9),simde_mm_alignr_epi8(c2[43],c2[42],8)));

//row: 37  [P0<<27+P3<<23 + 0 sys]
     d2[148]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+27)),simde_mm_loadu_si128((const simde__m128i *)(kb+407)));

//row: 38  [P1<<38 + 2 sys]
     d2[152]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+166)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],15),simde_mm_alignr_epi8(c2[41],c2[40],2)));

//row: 39  [P2<<34 + 2 sys]
     d2[156]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+290)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],15),simde_mm_alignr_epi8(c2[59],c2[58],12)));

//row: 40  [P0<<11+P3<<56 + 1 sys]
     d2[160]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+11)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+440)),c2[16]));

//row: 41  [P1<<54 + 2 sys]
     d2[164]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+182)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],1),simde_mm_alignr_epi8(c2[43],c2[42],5)));
  }
}
