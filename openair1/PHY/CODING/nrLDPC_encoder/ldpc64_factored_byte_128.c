#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=64, byte encoding, factored
// 319 terms vs 2071 expanded (6.49x); pipeline lag 1 chunk(s)
static inline void ldpc64_byte_128(uint8_t *c,uint8_t *d) {
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
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[76],c2[75],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[83],c2[82],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[83],c2[82],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[106],c2[105],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[129],c2[128],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[130],c2[129],7),simde_mm_xor_si128(c2[131],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[138],c2[137],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[140],c2[139],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[155],c2[154],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[156],c2[155],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[156],c2[155],15),simde_mm_xor_si128(c2[161],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[163],c2[162],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[164],c2[163],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[169],c2[168],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[170],c2[169],12),simde_mm_alignr_epi8(c2[170],c2[169],15)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[16]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_alignr_epi8(c2[11],c2[10],6));

//row: 6  [direct, 8 terms]
     d2[24]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[108],c2[107],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],13),simde_mm_alignr_epi8(c2[162],c2[161],3))))))));

//row: 7  [direct, 6 terms]
     d2[28]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],7),simde_mm_alignr_epi8(c2[115],c2[114],8))))));

//row: 9  [direct, 8 terms]
     d2[36]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[83],c2[82],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],13),simde_mm_alignr_epi8(c2[162],c2[161],8))))))));

//row: 10  [direct, 6 terms]
     d2[40]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],7),simde_mm_xor_si128(c2[58],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],1),simde_mm_alignr_epi8(c2[116],c2[115],10))))));

//row: 12  [direct, 6 terms]
     d2[48]=simde_mm_xor_si128(c2[2],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],5),simde_mm_xor_si128(c2[90],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[107],c2[106],10),simde_mm_alignr_epi8(c2[145],c2[144],7))))));

//row: 14  [direct, 6 terms]
     d2[56]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],15),simde_mm_xor_si128(c2[137],simde_mm_alignr_epi8(c2[171],c2[170],5))))));

//row: 17  [direct, 5 terms]
     d2[68]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],1),c2[169]))));

//row: 18  [direct, 5 terms]
     d2[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],11),simde_mm_alignr_epi8(c2[154],c2[153],3)))));

//row: 19  [direct, 5 terms]
     d2[76]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],15),c2[82]))));

//row: 21  [direct, 5 terms]
     d2[84]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],3),simde_mm_alignr_epi8(c2[171],c2[170],11)))));

//row: 22  [direct, 4 terms]
     d2[88]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[97],c2[96],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[108],c2[107],12),simde_mm_alignr_epi8(c2[138],c2[137],14))));

//row: 23  [direct, 4 terms]
     d2[92]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[83],c2[82],10),simde_mm_alignr_epi8(c2[146],c2[145],8))));

//row: 25  [direct, 4 terms]
     d2[100]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],4),simde_mm_alignr_epi8(c2[116],c2[115],6))));

//row: 26  [direct, 4 terms]
     d2[104]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],7),simde_mm_alignr_epi8(c2[124],c2[123],13))));

//row: 27  [direct, 3 terms]
     d2[108]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],8),simde_mm_alignr_epi8(c2[65],c2[64],2)));

//row: 28  [direct, 4 terms]
     d2[112]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[156],c2[155],5),simde_mm_alignr_epi8(c2[172],c2[171],15))));

//row: 33  [direct, 4 terms]
     d2[132]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],9),simde_mm_alignr_epi8(c2[170],c2[169],3))));

//row: 34  [direct, 4 terms]
     d2[136]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],4),simde_mm_alignr_epi8(c2[138],c2[137],15))));

//row: 36  [direct, 4 terms]
     d2[144]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],3),simde_mm_alignr_epi8(c2[148],c2[147],8))));

//row: 38  [direct, 4 terms]
     d2[152]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],13),simde_mm_alignr_epi8(c2[99],c2[98],3))));

//row: 39  [direct, 4 terms]
     d2[156]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],5),c2[152])));

//row: 40  [direct, 3 terms]
     d2[160]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],9),simde_mm_alignr_epi8(c2[138],c2[137],5)));

//row: 41  [direct, 4 terms]
     d2[164]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],11),simde_mm_alignr_epi8(c2[146],c2[145],7))));

//row: 45  [direct, 3 terms]
     d2[180]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],7),simde_mm_alignr_epi8(c2[83],c2[82],7)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[4]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[19],cm[18],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[43],cm[42],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[76],cm[75],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[83],cm[82],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[91],cm[90],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[105],cm[104],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[121],cm[120],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[130],cm[129],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[155],cm[154],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[163],cm[162],15),simde_mm_alignr_epi8(cm[170],cm[169],15))))))))))))))))));
       dm[8]=simde_mm_xor_si128(dm[4],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[19],cm[18],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[28],cm[27],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[36],cm[35],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[58],cm[57],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[67],cm[66],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[75],cm[74],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[90],cm[89],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[115],cm[114],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[121],cm[120],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[129],cm[128],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[138],cm[137],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[156],cm[155],15),simde_mm_alignr_epi8(cm[170],cm[169],12))))))))))))))))));
       dm[12]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[10],cm[9],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[34],cm[33],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[68],cm[67],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[90],cm[89],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[106],cm[105],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],3),simde_mm_xor_si128(cm[131],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[137],cm[136],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],4),simde_mm_xor_si128(cm[161],simde_mm_alignr_epi8(cm[169],cm[168],12))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[3]; dm=&dsimd[3];
  dm[4]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[19],cm[18],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[43],cm[42],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[76],cm[75],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[83],cm[82],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[91],cm[90],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[105],cm[104],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[121],cm[120],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[130],cm[129],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[155],cm[154],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[163],cm[162],15),simde_mm_alignr_epi8(cm[170],cm[169],15))))))))))))))))));
  dm[8]=simde_mm_xor_si128(dsimd[7],simde_mm_xor_si128(dsimd[3],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[19],cm[18],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[28],cm[27],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[36],cm[35],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[58],cm[57],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[67],cm[66],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[75],cm[74],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[90],cm[89],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[115],cm[114],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[121],cm[120],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[129],cm[128],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[138],cm[137],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[156],cm[155],15),simde_mm_alignr_epi8(cm[170],cm[169],12))))))))))))))))));
  dm[12]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[10],cm[9],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[34],cm[33],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[68],cm[67],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[90],cm[89],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[106],cm[105],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],3),simde_mm_xor_si128(cm[131],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[137],cm[136],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],4),simde_mm_xor_si128(cm[161],simde_mm_alignr_epi8(cm[169],cm[168],12))))))))))))))))));

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

//row: 5  [P0<<51 + 6 sys]
     d2[20]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+51)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[130],c2[129],12),simde_mm_alignr_epi8(c2[172],c2[171],11)))))));

//row: 8  [P2<<26+P0<<49 + 7 sys]
     d2[32]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+282)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+49)),simde_mm_xor_si128(c2[3],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[155],c2[154],4),simde_mm_alignr_epi8(c2[171],c2[170],13)))))))));

//row: 11  [P0<<60+P1<<22 + 5 sys]
     d2[44]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+60)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+150)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],6),simde_mm_alignr_epi8(c2[169],c2[168],14)))))));

//row: 13  [P1<<62 + 4 sys]
     d2[52]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+190)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],7),simde_mm_alignr_epi8(c2[164],c2[163],9)))));

//row: 15  [P3<<51 + 5 sys]
     d2[60]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+435)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],8),simde_mm_xor_si128(c2[10],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[108],c2[107],15),simde_mm_alignr_epi8(c2[145],c2[144],11))))));

//row: 16  [P0<<26 + 4 sys]
     d2[64]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+26)),simde_mm_xor_si128(c2[8],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[92],c2[91],1),simde_mm_alignr_epi8(c2[164],c2[163],3)))));

//row: 20  [P0<<32 + 4 sys]
     d2[80]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+32)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],9),simde_mm_alignr_epi8(c2[91],c2[90],15)))));

//row: 24  [P0<<30 + 4 sys]
     d2[96]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+30)),simde_mm_xor_si128(c2[3],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],12),simde_mm_alignr_epi8(c2[92],c2[91],4)))));

//row: 29  [P3<<6 + 3 sys]
     d2[116]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+390)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],12),simde_mm_alignr_epi8(c2[146],c2[145],4))));

//row: 30  [P2<<9 + 3 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+265)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],9),simde_mm_alignr_epi8(c2[108],c2[107],8))));

//row: 31  [P0<<44+P3<<61 + 2 sys]
     d2[124]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+44)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+445)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],15),simde_mm_alignr_epi8(c2[60],c2[59],1))));

//row: 32  [P2<<57 + 3 sys]
     d2[128]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+313)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(c2[99],simde_mm_alignr_epi8(c2[113],c2[112],7))));

//row: 35  [P0<<39 + 3 sys]
     d2[140]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+39)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],5),simde_mm_alignr_epi8(c2[97],c2[96],15))));

//row: 37  [P1<<58 + 2 sys]
     d2[148]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+186)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],6),simde_mm_alignr_epi8(c2[106],c2[105],12)));

//row: 42  [P2<<9 + 2 sys]
     d2[168]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+265)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],5),simde_mm_alignr_epi8(c2[34],c2[33],13)));

//row: 43  [P3<<11 + 3 sys]
     d2[172]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+395)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],3),simde_mm_alignr_epi8(c2[147],c2[146],13))));

//row: 44  [P0<<45 + 3 sys]
     d2[176]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+45)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],13),simde_mm_alignr_epi8(c2[75],c2[74],3))));
  }
}
