#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=384, byte encoding, factored
// 173 terms vs 1473 expanded (8.51x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc384_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[3072] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<24; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],13),simde_mm_xor_si128(c2[54],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[107],c2[106],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[107],c2[106],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[147],c2[146],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[152],c2[151],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[195],c2[194],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[199],c2[198],13),simde_mm_xor_si128(c2[199],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[245],c2[244],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[246],c2[245],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[293],c2[292],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[296],c2[295],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[344],c2[343],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[348],c2[347],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[386],c2[385],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[392],c2[391],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[396],c2[395],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[433],c2[432],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[443],c2[442],11),simde_mm_alignr_epi8(c2[444],c2[443],9)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[240]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],6),simde_mm_alignr_epi8(c2[340],c2[339],4))));

//row: 18  [direct, 3 terms]
     d2[432]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[289],c2[288],6),simde_mm_alignr_epi8(c2[339],c2[338],14)));

//row: 22  [direct, 2 terms]
     d2[528]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],4),simde_mm_alignr_epi8(c2[100],c2[99],4));

//row: 23  [direct, 3 terms]
     d2[552]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[150],c2[149],6),simde_mm_alignr_epi8(c2[246],c2[245],15)));

//row: 24  [direct, 3 terms]
     d2[576]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[106],c2[105],9),c2[436]));

//row: 25  [direct, 2 terms]
     d2[600]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],13),simde_mm_alignr_epi8(c2[242],c2[241],5));

//row: 27  [direct, 2 terms]
     d2[648]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],7),simde_mm_alignr_epi8(c2[292],c2[291],2));

//row: 28  [direct, 3 terms]
     d2[672]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],15),simde_mm_alignr_epi8(c2[251],c2[250],8)));

//row: 29  [direct, 2 terms]
     d2[696]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_alignr_epi8(c2[194],c2[193],1));

//row: 30  [direct, 4 terms]
     d2[720]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[250],c2[249],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[340],c2[339],4),simde_mm_alignr_epi8(c2[436],c2[435],8))));

//row: 36  [direct, 3 terms]
     d2[864]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[106],c2[105],7),simde_mm_alignr_epi8(c2[347],c2[346],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[24]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[11],cm[10],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[55],cm[54],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[107],cm[106],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[149],cm[148],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[293],cm[292],7),simde_mm_alignr_epi8(cm[443],cm[442],12)))))));
       dm[48]=simde_mm_xor_si128(dm[24],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[147],cm[146],4),simde_mm_xor_si128(cm[195],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[246],cm[245],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[290],cm[289],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[348],cm[347],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[396],cm[395],9),simde_mm_alignr_epi8(cm[433],cm[432],3)))))))));
       dm[72]=simde_mm_xor_si128(dm[48],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[56],cm[55],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[152],cm[151],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[199],cm[198],14),simde_mm_alignr_epi8(cm[392],cm[391],2)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[23]; dm=&dsimd[23];
  dm[24]=simde_mm_xor_si128(dsimd[23],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[11],cm[10],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[55],cm[54],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[107],cm[106],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[149],cm[148],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[293],cm[292],7),simde_mm_alignr_epi8(cm[443],cm[442],12)))))));
  dm[48]=simde_mm_xor_si128(dsimd[47],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[147],cm[146],4),simde_mm_xor_si128(cm[195],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[246],cm[245],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[290],cm[289],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[348],cm[347],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[396],cm[395],9),simde_mm_alignr_epi8(cm[433],cm[432],3)))))))));
  dm[72]=simde_mm_xor_si128(dsimd[71],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[23],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[56],cm[55],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[152],cm[151],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[199],cm[198],14),simde_mm_alignr_epi8(cm[392],cm[391],2)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*768,     d+k*384, 384);
    memcpy(kd+k*768+384, d+k*384, 384);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<24; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<29 + 2 sys]
     d2[96]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+797)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_alignr_epi8(c2[53],c2[52],10)));

//row: 5  [P1<<48 + 4 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+816)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[248],c2[247],9),c2[341]))));

//row: 6  [P1<<184 + 4 sys]
     d2[144]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+952)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[246],c2[245],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[343],c2[342],4),simde_mm_alignr_epi8(c2[436],c2[435],1)))));

//row: 7  [P1<<102+P3<<143 + 3 sys]
     d2[168]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+870)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2447)),simde_mm_xor_si128(c2[53],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[252],c2[251],10),c2[337]))));

//row: 8  [P2<<152 + 2 sys]
     d2[192]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1688)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],6),simde_mm_alignr_epi8(c2[53],c2[52],6)));

//row: 9  [P0<<185+P1<<178 + 2 sys]
     d2[216]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+185)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+946)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],12),simde_mm_alignr_epi8(c2[393],c2[392],4))));

//row: 11  [P3<<154 + 3 sys]
     d2[264]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2458)),simde_mm_xor_si128(c2[2],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[342],c2[341],12),simde_mm_alignr_epi8(c2[443],c2[442],14))));

//row: 12  [P1<<11 + 2 sys]
     d2[288]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+779)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],7),simde_mm_alignr_epi8(c2[150],c2[149],13)));

//row: 13  [P3<<166 + 3 sys]
     d2[312]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2470)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],13),simde_mm_alignr_epi8(c2[387],c2[386],3))));

//row: 14  [P1<<21+P3<<163 + 2 sys]
     d2[336]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+789)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2467)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],3),simde_mm_alignr_epi8(c2[296],c2[295],6))));

//row: 15  [P0<<63+P1<<81 + 1 sys]
     d2[360]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+63)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+849)),simde_mm_alignr_epi8(c2[5],c2[4],4)));

//row: 16  [P1<<135+P2<<64 + 2 sys]
     d2[384]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+903)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1600)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],7),simde_mm_alignr_epi8(c2[444],c2[443],1))));

//row: 17  [P1<<9+P2<<6 + 2 sys]
     d2[408]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+777)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1542)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],14),simde_mm_alignr_epi8(c2[242],c2[241],7))));

//row: 19  [P0<<156 + 2 sys]
     d2[456]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+156)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],10),simde_mm_alignr_epi8(c2[51],c2[50],10)));

//row: 20  [P1<<153 + 2 sys]
     d2[480]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+921)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],12),simde_mm_alignr_epi8(c2[196],c2[195],13)));

//row: 21  [P3<<67 + 2 sys]
     d2[504]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2371)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],13),simde_mm_alignr_epi8(c2[395],c2[394],15)));

//row: 26  [P2<<55+P3<<85 + 2 sys]
     d2[624]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1591)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2389)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],3),simde_mm_alignr_epi8(c2[345],c2[344],9))));

//row: 31  [P3<<170 + 1 sys]
     d2[744]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2474)),simde_mm_alignr_epi8(c2[49],c2[48],3));

//row: 32  [P2<<17 + 2 sys]
     d2[768]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1553)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],4),simde_mm_alignr_epi8(c2[241],c2[240],8)));

//row: 33  [P0<<124 + 2 sys]
     d2[792]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+124)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[107],c2[106],5),simde_mm_alignr_epi8(c2[348],c2[347],3)));

//row: 34  [P2<<177+P3<<12 + 1 sys]
     d2[816]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1713)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2316)),simde_mm_alignr_epi8(c2[11],c2[10],13)));

//row: 35  [P1<<18 + 2 sys]
     d2[840]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+786)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],13),simde_mm_alignr_epi8(c2[252],c2[251],8)));

//row: 37  [P0<<37+P3<<31 + 0 sys]
     d2[888]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+37)),simde_mm_loadu_si128((const simde__m128i *)(kb+2335)));

//row: 38  [P1<<190 + 2 sys]
     d2[912]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+958)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],4),simde_mm_alignr_epi8(c2[250],c2[249],7)));

//row: 39  [P2<<57 + 2 sys]
     d2[936]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1593)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],13),simde_mm_alignr_epi8(c2[345],c2[344],4)));

//row: 40  [P0<<107+P3<<163 + 1 sys]
     d2[960]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+107)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2467)),simde_mm_alignr_epi8(c2[103],c2[102],7)));

//row: 41  [P1<<60 + 2 sys]
     d2[984]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+828)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],3),simde_mm_alignr_epi8(c2[241],c2[240],7)));
  }
}
