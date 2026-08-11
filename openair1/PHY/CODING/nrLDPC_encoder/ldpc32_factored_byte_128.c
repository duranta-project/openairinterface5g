#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=32, byte encoding, factored
// 319 terms vs 1999 expanded (6.27x); pipeline lag 1 chunk(s)
static inline void ldpc32_byte(uint8_t *c,uint8_t *d) {
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
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],14),simde_mm_xor_si128(c2[65],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[70],c2[69],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[70],c2[69],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[77],c2[76],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[78],c2[77],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[78],c2[77],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],15),simde_mm_xor_si128(c2[81],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[86],c2[85],12),simde_mm_alignr_epi8(c2[86],c2[85],15)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[8]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_alignr_epi8(c2[5],c2[4],6));

//row: 6  [direct, 8 terms]
     d2[12]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],13),simde_mm_alignr_epi8(c2[82],c2[81],3))))))));

//row: 7  [direct, 6 terms]
     d2[14]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],7),simde_mm_alignr_epi8(c2[57],c2[56],8))))));

//row: 9  [direct, 8 terms]
     d2[18]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],13),simde_mm_alignr_epi8(c2[82],c2[81],8))))))));

//row: 10  [direct, 6 terms]
     d2[20]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],7),simde_mm_xor_si128(c2[28],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],1),simde_mm_alignr_epi8(c2[58],c2[57],10))))));

//row: 12  [direct, 6 terms]
     d2[24]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],5),simde_mm_xor_si128(c2[44],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],10),simde_mm_alignr_epi8(c2[73],c2[72],7))))));

//row: 14  [direct, 6 terms]
     d2[28]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],15),simde_mm_xor_si128(c2[69],simde_mm_alignr_epi8(c2[85],c2[84],5))))));

//row: 17  [direct, 5 terms]
     d2[34]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],1),c2[85]))));

//row: 18  [direct, 5 terms]
     d2[36]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],11),simde_mm_alignr_epi8(c2[78],c2[77],3)))));

//row: 19  [direct, 5 terms]
     d2[38]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],15),c2[40]))));

//row: 21  [direct, 5 terms]
     d2[42]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],3),simde_mm_alignr_epi8(c2[85],c2[84],11)))));

//row: 22  [direct, 4 terms]
     d2[44]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],12),simde_mm_alignr_epi8(c2[70],c2[69],14))));

//row: 23  [direct, 4 terms]
     d2[46]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],10),simde_mm_alignr_epi8(c2[74],c2[73],8))));

//row: 25  [direct, 4 terms]
     d2[50]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],4),simde_mm_alignr_epi8(c2[58],c2[57],6))));

//row: 26  [direct, 4 terms]
     d2[52]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],7),simde_mm_alignr_epi8(c2[62],c2[61],13))));

//row: 27  [direct, 3 terms]
     d2[54]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],8),simde_mm_alignr_epi8(c2[33],c2[32],2)));

//row: 28  [direct, 4 terms]
     d2[56]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[78],c2[77],5),simde_mm_alignr_epi8(c2[86],c2[85],15))));

//row: 33  [direct, 4 terms]
     d2[66]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],9),simde_mm_alignr_epi8(c2[86],c2[85],3))));

//row: 34  [direct, 4 terms]
     d2[68]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],4),simde_mm_alignr_epi8(c2[70],c2[69],15))));

//row: 36  [direct, 4 terms]
     d2[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],3),simde_mm_alignr_epi8(c2[74],c2[73],8))));

//row: 38  [direct, 4 terms]
     d2[76]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],13),simde_mm_alignr_epi8(c2[49],c2[48],3))));

//row: 39  [direct, 4 terms]
     d2[78]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],5),c2[76])));

//row: 40  [direct, 3 terms]
     d2[80]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],9),simde_mm_alignr_epi8(c2[70],c2[69],5)));

//row: 41  [direct, 4 terms]
     d2[82]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],11),simde_mm_alignr_epi8(c2[74],c2[73],7))));

//row: 45  [direct, 3 terms]
     d2[90]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],7),simde_mm_alignr_epi8(c2[41],c2[40],7)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[2]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[25],cm[24],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[61],cm[60],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[66],cm[65],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[77],cm[76],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],15),simde_mm_alignr_epi8(cm[86],cm[85],15))))))))))))))))));
       dm[4]=simde_mm_xor_si128(dm[2],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[33],cm[32],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[46],cm[45],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[61],cm[60],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[65],cm[64],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[70],cm[69],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[78],cm[77],15),simde_mm_alignr_epi8(cm[86],cm[85],12))))))))))))))))));
       dm[6]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[29],cm[28],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[34],cm[33],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[46],cm[45],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[54],cm[53],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[58],cm[57],3),simde_mm_xor_si128(cm[65],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[69],cm[68],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],4),simde_mm_xor_si128(cm[81],simde_mm_alignr_epi8(cm[85],cm[84],12))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[1]; dm=&dsimd[1];
  dm[2]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[25],cm[24],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[61],cm[60],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[66],cm[65],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[77],cm[76],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],15),simde_mm_alignr_epi8(cm[86],cm[85],15))))))))))))))))));
  dm[4]=simde_mm_xor_si128(dsimd[3],simde_mm_xor_si128(dsimd[1],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[33],cm[32],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[46],cm[45],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[61],cm[60],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[65],cm[64],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[70],cm[69],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[78],cm[77],15),simde_mm_alignr_epi8(cm[86],cm[85],12))))))))))))))))));
  dm[6]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[29],cm[28],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[34],cm[33],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[46],cm[45],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[54],cm[53],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[58],cm[57],3),simde_mm_xor_si128(cm[65],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[69],cm[68],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],4),simde_mm_xor_si128(cm[81],simde_mm_alignr_epi8(cm[85],cm[84],12))))))))))))))))));

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

//row: 5  [P0<<19 + 6 sys]
     d2[10]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+19)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],12),simde_mm_alignr_epi8(c2[86],c2[85],11)))))));

//row: 8  [P2<<26+P0<<17 + 7 sys]
     d2[16]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+154)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+17)),simde_mm_xor_si128(c2[1],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[77],c2[76],4),simde_mm_alignr_epi8(c2[85],c2[84],13)))))))));

//row: 11  [P0<<28+P1<<22 + 5 sys]
     d2[22]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+28)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+86)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],6),simde_mm_alignr_epi8(c2[85],c2[84],14)))))));

//row: 13  [P1<<30 + 4 sys]
     d2[26]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+94)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],7),simde_mm_alignr_epi8(c2[82],c2[81],9)))));

//row: 15  [P3<<19 + 5 sys]
     d2[30]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+211)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_xor_si128(c2[4],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],15),simde_mm_alignr_epi8(c2[73],c2[72],11))))));

//row: 16  [P0<<26 + 4 sys]
     d2[32]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+26)),simde_mm_xor_si128(c2[4],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],1),simde_mm_alignr_epi8(c2[82],c2[81],3)))));

//row: 20  [P0<<0 + 4 sys]
     d2[40]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+0)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],9),simde_mm_alignr_epi8(c2[45],c2[44],15)))));

//row: 24  [P0<<30 + 4 sys]
     d2[48]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+30)),simde_mm_xor_si128(c2[1],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],12),simde_mm_alignr_epi8(c2[46],c2[45],4)))));

//row: 29  [P3<<6 + 3 sys]
     d2[58]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+198)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],12),simde_mm_alignr_epi8(c2[74],c2[73],4))));

//row: 30  [P2<<9 + 3 sys]
     d2[60]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+137)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],9),simde_mm_alignr_epi8(c2[54],c2[53],8))));

//row: 31  [P0<<12+P3<<29 + 2 sys]
     d2[62]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+12)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+221)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],15),simde_mm_alignr_epi8(c2[30],c2[29],1))));

//row: 32  [P2<<25 + 3 sys]
     d2[64]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+153)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(c2[49],simde_mm_alignr_epi8(c2[57],c2[56],7))));

//row: 35  [P0<<7 + 3 sys]
     d2[70]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+7)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],5),simde_mm_alignr_epi8(c2[49],c2[48],15))));

//row: 37  [P1<<26 + 2 sys]
     d2[74]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+90)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],6),simde_mm_alignr_epi8(c2[54],c2[53],12)));

//row: 42  [P2<<9 + 2 sys]
     d2[84]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+137)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],5),simde_mm_alignr_epi8(c2[18],c2[17],13)));

//row: 43  [P3<<11 + 3 sys]
     d2[86]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+203)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],3),simde_mm_alignr_epi8(c2[73],c2[72],13))));

//row: 44  [P0<<13 + 3 sys]
     d2[88]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+13)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],13),simde_mm_alignr_epi8(c2[37],c2[36],3))));
  }
}
