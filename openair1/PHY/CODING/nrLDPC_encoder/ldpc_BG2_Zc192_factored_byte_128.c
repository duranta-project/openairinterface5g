#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=192, byte encoding, factored
// 173 terms vs 1469 expanded (8.49x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc192_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1536] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<12; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],13),simde_mm_xor_si128(c2[30],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[77],c2[76],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],13),simde_mm_xor_si128(c2[103],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[125],c2[124],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[126],c2[125],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[152],c2[151],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[176],c2[175],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[180],c2[179],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[194],c2[193],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[200],c2[199],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[204],c2[203],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[217],c2[216],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[227],c2[226],11),simde_mm_alignr_epi8(c2[228],c2[227],9)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[120]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],6),simde_mm_alignr_epi8(c2[172],c2[171],4))));

//row: 18  [direct, 3 terms]
     d2[216]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],6),simde_mm_alignr_epi8(c2[171],c2[170],14)));

//row: 22  [direct, 2 terms]
     d2[264]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],4),simde_mm_alignr_epi8(c2[52],c2[51],4));

//row: 23  [direct, 3 terms]
     d2[276]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[78],c2[77],6),simde_mm_alignr_epi8(c2[126],c2[125],15)));

//row: 24  [direct, 3 terms]
     d2[288]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],9),c2[220]));

//row: 25  [direct, 2 terms]
     d2[300]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],13),simde_mm_alignr_epi8(c2[122],c2[121],5));

//row: 27  [direct, 2 terms]
     d2[324]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],7),simde_mm_alignr_epi8(c2[148],c2[147],2));

//row: 28  [direct, 3 terms]
     d2[336]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],15),simde_mm_alignr_epi8(c2[131],c2[130],8)));

//row: 29  [direct, 2 terms]
     d2[348]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_alignr_epi8(c2[98],c2[97],1));

//row: 30  [direct, 4 terms]
     d2[360]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[130],c2[129],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[172],c2[171],4),simde_mm_alignr_epi8(c2[220],c2[219],8))));

//row: 36  [direct, 3 terms]
     d2[432]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],7),simde_mm_alignr_epi8(c2[179],c2[178],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[12]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[11],cm[10],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[31],cm[30],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[59],cm[58],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[77],cm[76],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[149],cm[148],7),simde_mm_alignr_epi8(cm[227],cm[226],12)))))));
       dm[24]=simde_mm_xor_si128(dm[12],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[75],cm[74],4),simde_mm_xor_si128(cm[99],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[126],cm[125],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[146],cm[145],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[180],cm[179],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[204],cm[203],9),simde_mm_alignr_epi8(cm[217],cm[216],3)))))))));
       dm[36]=simde_mm_xor_si128(dm[24],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[80],cm[79],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[103],cm[102],14),simde_mm_alignr_epi8(cm[200],cm[199],2)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[11]; dm=&dsimd[11];
  dm[12]=simde_mm_xor_si128(dsimd[11],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[11],cm[10],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[31],cm[30],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[59],cm[58],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[77],cm[76],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[149],cm[148],7),simde_mm_alignr_epi8(cm[227],cm[226],12)))))));
  dm[24]=simde_mm_xor_si128(dsimd[23],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[75],cm[74],4),simde_mm_xor_si128(cm[99],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[126],cm[125],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[146],cm[145],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[180],cm[179],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[204],cm[203],9),simde_mm_alignr_epi8(cm[217],cm[216],3)))))))));
  dm[36]=simde_mm_xor_si128(dsimd[35],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[11],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[80],cm[79],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[103],cm[102],14),simde_mm_alignr_epi8(cm[200],cm[199],2)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*384,     d+k*192, 192);
    memcpy(kd+k*384+192, d+k*192, 192);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<12; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<29 + 2 sys]
     d2[48]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+413)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_alignr_epi8(c2[29],c2[28],10)));

//row: 5  [P1<<48 + 4 sys]
     d2[60]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+432)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[128],c2[127],9),c2[173]))));

//row: 6  [P1<<184 + 4 sys]
     d2[72]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+568)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[126],c2[125],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[175],c2[174],4),simde_mm_alignr_epi8(c2[220],c2[219],1)))));

//row: 7  [P1<<102+P3<<143 + 3 sys]
     d2[84]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+486)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1295)),simde_mm_xor_si128(c2[29],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],10),c2[169]))));

//row: 8  [P2<<152 + 2 sys]
     d2[96]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+920)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],6),simde_mm_alignr_epi8(c2[29],c2[28],6)));

//row: 9  [P0<<185+P1<<178 + 2 sys]
     d2[108]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+185)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+562)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],12),simde_mm_alignr_epi8(c2[201],c2[200],4))));

//row: 11  [P3<<154 + 3 sys]
     d2[132]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1306)),simde_mm_xor_si128(c2[2],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[174],c2[173],12),simde_mm_alignr_epi8(c2[227],c2[226],14))));

//row: 12  [P1<<11 + 2 sys]
     d2[144]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+395)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],7),simde_mm_alignr_epi8(c2[78],c2[77],13)));

//row: 13  [P3<<166 + 3 sys]
     d2[156]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1318)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],13),simde_mm_alignr_epi8(c2[195],c2[194],3))));

//row: 14  [P1<<21+P3<<163 + 2 sys]
     d2[168]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+405)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1315)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],3),simde_mm_alignr_epi8(c2[152],c2[151],6))));

//row: 15  [P0<<63+P1<<81 + 1 sys]
     d2[180]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+63)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+465)),simde_mm_alignr_epi8(c2[5],c2[4],4)));

//row: 16  [P1<<135+P2<<64 + 2 sys]
     d2[192]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+519)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+832)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],7),simde_mm_alignr_epi8(c2[228],c2[227],1))));

//row: 17  [P1<<9+P2<<6 + 2 sys]
     d2[204]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+393)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+774)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],14),simde_mm_alignr_epi8(c2[122],c2[121],7))));

//row: 19  [P0<<156 + 2 sys]
     d2[228]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+156)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],10),simde_mm_alignr_epi8(c2[27],c2[26],10)));

//row: 20  [P1<<153 + 2 sys]
     d2[240]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+537)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],12),simde_mm_alignr_epi8(c2[100],c2[99],13)));

//row: 21  [P3<<67 + 2 sys]
     d2[252]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1219)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],13),simde_mm_alignr_epi8(c2[203],c2[202],15)));

//row: 26  [P2<<55+P3<<85 + 2 sys]
     d2[312]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+823)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1237)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],3),simde_mm_alignr_epi8(c2[177],c2[176],9))));

//row: 31  [P3<<170 + 1 sys]
     d2[372]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1322)),simde_mm_alignr_epi8(c2[25],c2[24],3));

//row: 32  [P2<<17 + 2 sys]
     d2[384]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+785)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],4),simde_mm_alignr_epi8(c2[121],c2[120],8)));

//row: 33  [P0<<124 + 2 sys]
     d2[396]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+124)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],5),simde_mm_alignr_epi8(c2[180],c2[179],3)));

//row: 34  [P2<<177+P3<<12 + 1 sys]
     d2[408]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+945)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1164)),simde_mm_alignr_epi8(c2[11],c2[10],13)));

//row: 35  [P1<<18 + 2 sys]
     d2[420]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+402)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],13),simde_mm_alignr_epi8(c2[132],c2[131],8)));

//row: 37  [P0<<37+P3<<31 + 0 sys]
     d2[444]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+37)),simde_mm_loadu_si128((const simde__m128i *)(kb+1183)));

//row: 38  [P1<<190 + 2 sys]
     d2[456]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+574)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],4),simde_mm_alignr_epi8(c2[130],c2[129],7)));

//row: 39  [P2<<57 + 2 sys]
     d2[468]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+825)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],13),simde_mm_alignr_epi8(c2[177],c2[176],4)));

//row: 40  [P0<<107+P3<<163 + 1 sys]
     d2[480]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+107)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1315)),simde_mm_alignr_epi8(c2[55],c2[54],7)));

//row: 41  [P1<<60 + 2 sys]
     d2[492]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+444)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],3),simde_mm_alignr_epi8(c2[121],c2[120],7)));
  }
}
