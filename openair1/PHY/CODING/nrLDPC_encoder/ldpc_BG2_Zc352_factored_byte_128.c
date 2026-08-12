#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=352, byte encoding, factored
// 173 terms vs 1479 expanded (8.55x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc352_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[2816] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<22; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(c2[1],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[133],c2[132],2),simde_mm_xor_si128(c2[136],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[139],c2[138],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[180],c2[179],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[182],c2[181],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[187],c2[186],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[228],c2[227],14),simde_mm_xor_si128(c2[220],simde_mm_xor_si128(c2[265],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[267],c2[266],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[268],c2[267],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[311],c2[310],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[314],c2[313],8),simde_mm_xor_si128(c2[353],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[361],c2[360],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[363],c2[362],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[403],c2[402],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[404],c2[403],10),simde_mm_alignr_epi8(c2[405],c2[404],4)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[220]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[275],c2[274],14),simde_mm_alignr_epi8(c2[310],c2[309],7))));

//row: 18  [direct, 3 terms]
     d2[396]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[266],c2[265],15),simde_mm_alignr_epi8(c2[310],c2[309],6)));

//row: 22  [direct, 2 terms]
     d2[484]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],2),simde_mm_alignr_epi8(c2[99],c2[98],3));

//row: 23  [direct, 3 terms]
     d2[506]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[141],c2[140],4),simde_mm_alignr_epi8(c2[230],c2[229],6)));

//row: 24  [direct, 3 terms]
     d2[528]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[95],c2[94],12),simde_mm_alignr_epi8(c2[400],c2[399],13)));

//row: 25  [direct, 2 terms]
     d2[550]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_alignr_epi8(c2[229],c2[228],8));

//row: 27  [direct, 2 terms]
     d2[594]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],10),simde_mm_alignr_epi8(c2[271],c2[270],8));

//row: 28  [direct, 3 terms]
     d2[616]=simde_mm_xor_si128(c2[48],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],8),simde_mm_alignr_epi8(c2[230],c2[229],5)));

//row: 29  [direct, 2 terms]
     d2[638]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],11),simde_mm_alignr_epi8(c2[187],c2[186],1));

//row: 30  [direct, 4 terms]
     d2[660]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[94],c2[93],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[231],c2[230],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[314],c2[313],13),simde_mm_alignr_epi8(c2[398],c2[397],13))));

//row: 36  [direct, 3 terms]
     d2[792]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],10),simde_mm_alignr_epi8(c2[317],c2[316],1)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[22]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[10],cm[9],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[89],cm[88],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[133],cm[132],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[267],cm[266],8),simde_mm_alignr_epi8(cm[404],cm[403],11)))))));
       dm[44]=simde_mm_xor_si128(dm[22],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[137],cm[136],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[180],cm[179],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[221],cm[220],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[268],cm[267],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[311],cm[310],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[363],cm[362],11),simde_mm_alignr_epi8(cm[405],cm[404],5)))))))));
       dm[66]=simde_mm_xor_si128(dm[44],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[55],cm[54],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[139],cm[138],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],2),simde_mm_alignr_epi8(cm[361],cm[360],14)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[21]; dm=&dsimd[21];
  dm[22]=simde_mm_xor_si128(dsimd[21],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[10],cm[9],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[89],cm[88],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[133],cm[132],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[267],cm[266],8),simde_mm_alignr_epi8(cm[404],cm[403],11)))))));
  dm[44]=simde_mm_xor_si128(dsimd[43],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[137],cm[136],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[180],cm[179],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[221],cm[220],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[268],cm[267],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[311],cm[310],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[363],cm[362],11),simde_mm_alignr_epi8(cm[405],cm[404],5)))))))));
  dm[66]=simde_mm_xor_si128(dsimd[65],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[21],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[55],cm[54],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[139],cm[138],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],2),simde_mm_alignr_epi8(cm[361],cm[360],14)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*704,     d+k*352, 352);
    memcpy(kd+k*704+352, d+k*352, 352);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<22; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<83 + 2 sys]
     d2[88]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+787)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],6),simde_mm_alignr_epi8(c2[49],c2[48],3)));

//row: 5  [P1<<60 + 4 sys]
     d2[110]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+764)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[223],c2[222],3),simde_mm_alignr_epi8(c2[315],c2[314],7)))));

//row: 6  [P1<<29 + 4 sys]
     d2[132]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+733)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[230],c2[229],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[309],c2[308],10),simde_mm_alignr_epi8(c2[406],c2[405],11)))));

//row: 7  [P1<<47+P3<<55 + 3 sys]
     d2[154]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+751)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2167)),simde_mm_xor_si128(c2[47],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[228],c2[227],13),simde_mm_alignr_epi8(c2[310],c2[309],8)))));

//row: 8  [P2<<161 + 2 sys]
     d2[176]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1569)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],5),simde_mm_alignr_epi8(c2[46],c2[45],15)));

//row: 9  [P0<<99+P1<<64 + 2 sys]
     d2[198]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+99)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+768)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],8),simde_mm_alignr_epi8(c2[361],c2[360],14))));

//row: 11  [P3<<141 + 3 sys]
     d2[242]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2253)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[319],c2[318],15),simde_mm_alignr_epi8(c2[398],c2[397],8))));

//row: 12  [P1<<4 + 2 sys]
     d2[264]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+708)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],10),simde_mm_alignr_epi8(c2[133],c2[132],11)));

//row: 13  [P3<<127 + 3 sys]
     d2[286]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2239)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],11),simde_mm_alignr_epi8(c2[354],c2[353],11))));

//row: 14  [P1<<8+P3<<166 + 2 sys]
     d2[308]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+712)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2278)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],11),simde_mm_alignr_epi8(c2[274],c2[273],1))));

//row: 15  [P0<<103+P1<<40 + 1 sys]
     d2[330]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+103)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+744)),simde_mm_alignr_epi8(c2[9],c2[8],9)));

//row: 16  [P1<<17+P2<<165 + 2 sys]
     d2[352]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+721)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1573)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],14),simde_mm_alignr_epi8(c2[406],c2[405],14))));

//row: 17  [P1<<62+P2<<163 + 2 sys]
     d2[374]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+766)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1571)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],6),simde_mm_alignr_epi8(c2[222],c2[221],7))));

//row: 19  [P0<<145 + 2 sys]
     d2[418]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+145)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_alignr_epi8(c2[53],c2[52],7)));

//row: 20  [P1<<173 + 2 sys]
     d2[440]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+877)),simde_mm_xor_si128(c2[52],simde_mm_alignr_epi8(c2[180],c2[179],4)));

//row: 21  [P3<<40 + 2 sys]
     d2[462]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2152)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],12),simde_mm_alignr_epi8(c2[363],c2[362],6)));

//row: 26  [P2<<53+P3<<145 + 2 sys]
     d2[572]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1461)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2257)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],4),simde_mm_alignr_epi8(c2[311],c2[310],6))));

//row: 31  [P3<<148 + 1 sys]
     d2[682]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2260)),simde_mm_alignr_epi8(c2[52],c2[51],5));

//row: 32  [P2<<142 + 2 sys]
     d2[704]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1550)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],4),simde_mm_alignr_epi8(c2[225],c2[224],9)));

//row: 33  [P0<<29 + 2 sys]
     d2[726]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+29)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[95],c2[94],9),simde_mm_alignr_epi8(c2[317],c2[316],9)));

//row: 34  [P2<<41+P3<<162 + 1 sys]
     d2[748]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1449)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2274)),simde_mm_alignr_epi8(c2[1],c2[0],11)));

//row: 35  [P1<<172 + 2 sys]
     d2[770]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+876)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],14),simde_mm_alignr_epi8(c2[230],c2[229],8)));

//row: 37  [P0<<167+P3<<38 + 0 sys]
     d2[814]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+167)),simde_mm_loadu_si128((const simde__m128i *)(kb+2150)));

//row: 38  [P1<<19 + 2 sys]
     d2[836]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+723)),simde_mm_xor_si128(c2[51],simde_mm_alignr_epi8(c2[221],c2[220],7)));

//row: 39  [P2<<105 + 2 sys]
     d2[858]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1513)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],13),simde_mm_alignr_epi8(c2[309],c2[308],6)));

//row: 40  [P0<<156+P3<<82 + 1 sys]
     d2[880]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+156)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2194)),c2[98]));

//row: 41  [P1<<8 + 2 sys]
     d2[902]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+712)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],4),simde_mm_alignr_epi8(c2[221],c2[220],6)));
  }
}
