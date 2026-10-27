#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=176, byte encoding, factored
// 173 terms vs 1475 expanded (8.53x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc176_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1408] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<11; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(c2[1],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],2),simde_mm_xor_si128(c2[70],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[92],c2[91],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[94],c2[93],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[118],c2[117],14),simde_mm_xor_si128(c2[110],simde_mm_xor_si128(c2[133],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[135],c2[134],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[136],c2[135],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[157],c2[156],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[160],c2[159],8),simde_mm_xor_si128(c2[177],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[185],c2[184],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[187],c2[186],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[205],c2[204],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[206],c2[205],10),simde_mm_alignr_epi8(c2[207],c2[206],4)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[110]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[143],c2[142],14),simde_mm_alignr_epi8(c2[156],c2[155],7))));

//row: 18  [direct, 3 terms]
     d2[198]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],15),simde_mm_alignr_epi8(c2[156],c2[155],6)));

//row: 22  [direct, 2 terms]
     d2[242]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],2),simde_mm_alignr_epi8(c2[55],c2[54],3));

//row: 23  [direct, 3 terms]
     d2[253]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],4),simde_mm_alignr_epi8(c2[120],c2[119],6)));

//row: 24  [direct, 3 terms]
     d2[264]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],12),simde_mm_alignr_epi8(c2[202],c2[201],13)));

//row: 25  [direct, 2 terms]
     d2[275]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_alignr_epi8(c2[119],c2[118],8));

//row: 27  [direct, 2 terms]
     d2[297]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],10),simde_mm_alignr_epi8(c2[139],c2[138],8));

//row: 28  [direct, 3 terms]
     d2[308]=simde_mm_xor_si128(c2[26],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],8),simde_mm_alignr_epi8(c2[120],c2[119],5)));

//row: 29  [direct, 2 terms]
     d2[319]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],11),simde_mm_alignr_epi8(c2[99],c2[98],1));

//row: 30  [direct, 4 terms]
     d2[330]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[160],c2[159],13),simde_mm_alignr_epi8(c2[200],c2[199],13))));

//row: 36  [direct, 3 terms]
     d2[396]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],10),simde_mm_alignr_epi8(c2[163],c2[162],1)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[11]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[10],cm[9],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[31],cm[30],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[67],cm[66],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[135],cm[134],8),simde_mm_alignr_epi8(cm[206],cm[205],11)))))));
       dm[22]=simde_mm_xor_si128(dm[11],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[71],cm[70],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[92],cm[91],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[111],cm[110],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[136],cm[135],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[157],cm[156],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[187],cm[186],11),simde_mm_alignr_epi8(cm[207],cm[206],5)))))))));
       dm[33]=simde_mm_xor_si128(dm[22],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[33],cm[32],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[73],cm[72],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[94],cm[93],2),simde_mm_alignr_epi8(cm[185],cm[184],14)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[10]; dm=&dsimd[10];
  dm[11]=simde_mm_xor_si128(dsimd[10],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[10],cm[9],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[31],cm[30],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[67],cm[66],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[135],cm[134],8),simde_mm_alignr_epi8(cm[206],cm[205],11)))))));
  dm[22]=simde_mm_xor_si128(dsimd[21],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[71],cm[70],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[92],cm[91],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[111],cm[110],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[136],cm[135],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[157],cm[156],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[187],cm[186],11),simde_mm_alignr_epi8(cm[207],cm[206],5)))))))));
  dm[33]=simde_mm_xor_si128(dsimd[32],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[10],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[33],cm[32],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[73],cm[72],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[94],cm[93],2),simde_mm_alignr_epi8(cm[185],cm[184],14)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*352,     d+k*176, 176);
    memcpy(kd+k*352+176, d+k*176, 176);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<11; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<83 + 2 sys]
     d2[44]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+435)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],6),simde_mm_alignr_epi8(c2[27],c2[26],3)));

//row: 5  [P1<<60 + 4 sys]
     d2[55]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+412)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[113],c2[112],3),simde_mm_alignr_epi8(c2[161],c2[160],7)))));

//row: 6  [P1<<29 + 4 sys]
     d2[66]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+381)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[120],c2[119],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[155],c2[154],10),simde_mm_alignr_epi8(c2[208],c2[207],11)))));

//row: 7  [P1<<47+P3<<55 + 3 sys]
     d2[77]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+399)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1111)),simde_mm_xor_si128(c2[25],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[118],c2[117],13),simde_mm_alignr_epi8(c2[156],c2[155],8)))));

//row: 8  [P2<<161 + 2 sys]
     d2[88]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+865)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],5),simde_mm_alignr_epi8(c2[24],c2[23],15)));

//row: 9  [P0<<99+P1<<64 + 2 sys]
     d2[99]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+99)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+416)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],8),simde_mm_alignr_epi8(c2[185],c2[184],14))));

//row: 11  [P3<<141 + 3 sys]
     d2[121]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1197)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[165],c2[164],15),simde_mm_alignr_epi8(c2[200],c2[199],8))));

//row: 12  [P1<<4 + 2 sys]
     d2[132]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+356)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],10),simde_mm_alignr_epi8(c2[67],c2[66],11)));

//row: 13  [P3<<127 + 3 sys]
     d2[143]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1183)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],11),simde_mm_alignr_epi8(c2[178],c2[177],11))));

//row: 14  [P1<<8+P3<<166 + 2 sys]
     d2[154]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+360)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1222)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],11),simde_mm_alignr_epi8(c2[142],c2[141],1))));

//row: 15  [P0<<103+P1<<40 + 1 sys]
     d2[165]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+103)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+392)),simde_mm_alignr_epi8(c2[9],c2[8],9)));

//row: 16  [P1<<17+P2<<165 + 2 sys]
     d2[176]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+369)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+869)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],14),simde_mm_alignr_epi8(c2[208],c2[207],14))));

//row: 17  [P1<<62+P2<<163 + 2 sys]
     d2[187]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+414)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+867)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],6),simde_mm_alignr_epi8(c2[112],c2[111],7))));

//row: 19  [P0<<145 + 2 sys]
     d2[209]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+145)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_alignr_epi8(c2[31],c2[30],7)));

//row: 20  [P1<<173 + 2 sys]
     d2[220]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+525)),simde_mm_xor_si128(c2[30],simde_mm_alignr_epi8(c2[92],c2[91],4)));

//row: 21  [P3<<40 + 2 sys]
     d2[231]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1096)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],12),simde_mm_alignr_epi8(c2[187],c2[186],6)));

//row: 26  [P2<<53+P3<<145 + 2 sys]
     d2[286]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+757)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1201)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],4),simde_mm_alignr_epi8(c2[157],c2[156],6))));

//row: 31  [P3<<148 + 1 sys]
     d2[341]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1204)),simde_mm_alignr_epi8(c2[30],c2[29],5));

//row: 32  [P2<<142 + 2 sys]
     d2[352]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+846)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],4),simde_mm_alignr_epi8(c2[115],c2[114],9)));

//row: 33  [P0<<29 + 2 sys]
     d2[363]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+29)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],9),simde_mm_alignr_epi8(c2[163],c2[162],9)));

//row: 34  [P2<<41+P3<<162 + 1 sys]
     d2[374]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+745)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1218)),simde_mm_alignr_epi8(c2[1],c2[0],11)));

//row: 35  [P1<<172 + 2 sys]
     d2[385]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+524)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],14),simde_mm_alignr_epi8(c2[120],c2[119],8)));

//row: 37  [P0<<167+P3<<38 + 0 sys]
     d2[407]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+167)),simde_mm_loadu_si128((const simde__m128i *)(kb+1094)));

//row: 38  [P1<<19 + 2 sys]
     d2[418]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+371)),simde_mm_xor_si128(c2[29],simde_mm_alignr_epi8(c2[111],c2[110],7)));

//row: 39  [P2<<105 + 2 sys]
     d2[429]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+809)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],13),simde_mm_alignr_epi8(c2[155],c2[154],6)));

//row: 40  [P0<<156+P3<<82 + 1 sys]
     d2[440]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+156)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1138)),c2[54]));

//row: 41  [P1<<8 + 2 sys]
     d2[451]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+360)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],4),simde_mm_alignr_epi8(c2[111],c2[110],6)));
  }
}
