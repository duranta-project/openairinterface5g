#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=112, byte encoding, factored
// 173 terms vs 1459 expanded (8.43x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc112_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[896] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<7; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[118],c2[117],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[127],c2[126],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[128],c2[127],5),simde_mm_alignr_epi8(c2[128],c2[127],15)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[70]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],8),simde_mm_alignr_epi8(c2[102],c2[101],8))));

//row: 18  [direct, 3 terms]
     d2[126]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],4),simde_mm_xor_si128(c2[85],simde_mm_alignr_epi8(c2[101],c2[100],12)));

//row: 22  [direct, 2 terms]
     d2[154]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],1),simde_mm_alignr_epi8(c2[29],c2[28],1));

//row: 23  [direct, 3 terms]
     d2[161]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],6),simde_mm_alignr_epi8(c2[72],c2[71],6)));

//row: 24  [direct, 3 terms]
     d2[168]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],8),simde_mm_alignr_epi8(c2[130],c2[129],15)));

//row: 25  [direct, 2 terms]
     d2[175]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],3),simde_mm_alignr_epi8(c2[76],c2[75],14));

//row: 27  [direct, 2 terms]
     d2[189]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],11),simde_mm_alignr_epi8(c2[85],c2[84],6));

//row: 28  [direct, 3 terms]
     d2[196]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],4),simde_mm_alignr_epi8(c2[76],c2[75],1)));

//row: 29  [direct, 2 terms]
     d2[203]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],12),simde_mm_alignr_epi8(c2[60],c2[59],13));

//row: 30  [direct, 4 terms]
     d2[210]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],9),c2[130])));

//row: 36  [direct, 3 terms]
     d2[252]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],15),simde_mm_alignr_epi8(c2[104],c2[103],3)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[7]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[47],cm[46],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[90],cm[89],15),simde_mm_alignr_epi8(cm[127],cm[126],8)))))));
       dm[14]=simde_mm_xor_si128(dm[7],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[73],cm[72],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[85],cm[84],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[104],cm[103],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],13),simde_mm_alignr_epi8(cm[128],cm[127],15)))))))));
       dm[21]=simde_mm_xor_si128(dm[14],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[16],cm[15],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[61],cm[60],15),simde_mm_alignr_epi8(cm[118],cm[117],11)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[6]; dm=&dsimd[6];
  dm[7]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[47],cm[46],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[90],cm[89],15),simde_mm_alignr_epi8(cm[127],cm[126],8)))))));
  dm[14]=simde_mm_xor_si128(dsimd[13],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[73],cm[72],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[85],cm[84],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[104],cm[103],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],13),simde_mm_alignr_epi8(cm[128],cm[127],15)))))))));
  dm[21]=simde_mm_xor_si128(dsimd[20],simde_mm_xor_si128(dsimd[6],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[16],cm[15],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[61],cm[60],15),simde_mm_alignr_epi8(cm[118],cm[117],11)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*224,     d+k*112, 112);
    memcpy(kd+k*224+112, d+k*112, 112);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<7; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<101 + 2 sys]
     d2[28]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+325)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],8),c2[15]));

//row: 5  [P1<<81 + 4 sys]
     d2[35]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+305)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],10),simde_mm_alignr_epi8(c2[105],c2[104],11)))));

//row: 6  [P1<<97 + 4 sys]
     d2[42]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+321)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],15),simde_mm_alignr_epi8(c2[133],c2[132],14)))));

//row: 7  [P1<<38+P3<<69 + 3 sys]
     d2[49]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+262)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+741)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],13),simde_mm_alignr_epi8(c2[105],c2[104],9)))));

//row: 8  [P2<<40 + 2 sys]
     d2[56]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+488)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],3),simde_mm_alignr_epi8(c2[17],c2[16],11)));

//row: 9  [P0<<72+P1<<83 + 2 sys]
     d2[63]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+72)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+307)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],2),simde_mm_alignr_epi8(c2[114],c2[113],14))));

//row: 11  [P3<<99 + 3 sys]
     d2[77]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+771)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],10),simde_mm_alignr_epi8(c2[133],c2[132],14))));

//row: 12  [P1<<109 + 2 sys]
     d2[84]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+333)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],14),simde_mm_alignr_epi8(c2[49],c2[48],9)));

//row: 13  [P3<<28 + 3 sys]
     d2[91]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+700)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],1),simde_mm_alignr_epi8(c2[114],c2[113],15))));

//row: 14  [P1<<40+P3<<4 + 2 sys]
     d2[98]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+264)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+676)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],4),simde_mm_alignr_epi8(c2[90],c2[89],15))));

//row: 15  [P0<<88+P1<<110 + 1 sys]
     d2[105]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+88)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+334)),simde_mm_alignr_epi8(c2[1],c2[0],4)));

//row: 16  [P1<<22+P2<<97 + 2 sys]
     d2[112]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+246)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+545)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],11),simde_mm_alignr_epi8(c2[129],c2[128],14))));

//row: 17  [P1<<94+P2<<2 + 2 sys]
     d2[119]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+318)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+450)),simde_mm_xor_si128(c2[17],simde_mm_alignr_epi8(c2[72],c2[71],4))));

//row: 19  [P0<<86 + 2 sys]
     d2[133]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+86)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],3),simde_mm_alignr_epi8(c2[16],c2[15],10)));

//row: 20  [P1<<29 + 2 sys]
     d2[140]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+253)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],6),simde_mm_alignr_epi8(c2[58],c2[57],4)));

//row: 21  [P3<<81 + 2 sys]
     d2[147]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+753)),simde_mm_xor_si128(c2[5],simde_mm_alignr_epi8(c2[115],c2[114],11)));

//row: 26  [P2<<109+P3<<6 + 2 sys]
     d2[182]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+557)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+678)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],10),simde_mm_alignr_epi8(c2[99],c2[98],6))));

//row: 31  [P3<<70 + 1 sys]
     d2[217]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+742)),simde_mm_alignr_epi8(c2[17],c2[16],3));

//row: 32  [P2<<110 + 2 sys]
     d2[224]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+558)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],12),simde_mm_alignr_epi8(c2[72],c2[71],5)));

//row: 33  [P0<<109 + 2 sys]
     d2[231]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+109)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],7),simde_mm_alignr_epi8(c2[99],c2[98],12)));

//row: 34  [P2<<89+P3<<69 + 1 sys]
     d2[238]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+537)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+741)),simde_mm_alignr_epi8(c2[2],c2[1],13)));

//row: 35  [P1<<55 + 2 sys]
     d2[245]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+279)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],11),simde_mm_alignr_epi8(c2[74],c2[73],5)));

//row: 37  [P0<<40+P3<<12 + 0 sys]
     d2[259]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+40)),simde_mm_loadu_si128((const simde__m128i *)(kb+684)));

//row: 38  [P1<<46 + 2 sys]
     d2[266]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+270)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],5),simde_mm_alignr_epi8(c2[77],c2[76],1)));

//row: 39  [P2<<42 + 2 sys]
     d2[273]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+490)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],10),simde_mm_alignr_epi8(c2[103],c2[102],5)));

//row: 40  [P0<<35+P3<<36 + 1 sys]
     d2[280]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+35)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+708)),simde_mm_alignr_epi8(c2[35],c2[34],2)));

//row: 41  [P1<<81 + 2 sys]
     d2[287]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+305)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],8),simde_mm_alignr_epi8(c2[77],c2[76],5)));
  }
}
