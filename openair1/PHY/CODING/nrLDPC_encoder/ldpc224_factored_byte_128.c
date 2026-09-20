#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=224, byte encoding, factored
// 319 terms vs 2099 expanded (6.58x); pipeline lag 1 chunk(s)
static inline void ldpc224_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1792] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<14; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],15),simde_mm_xor_si128(c2[29],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[94],c2[93],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[95],c2[94],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[113],c2[112],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],14),simde_mm_xor_si128(c2[123],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[152],c2[151],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[169],c2[168],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[171],c2[170],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[180],c2[179],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[202],c2[201],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[205],c2[204],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[206],c2[205],13),simde_mm_xor_si128(c2[225],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[233],c2[232],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[234],c2[233],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[258],c2[257],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[264],c2[263],1),simde_mm_xor_si128(c2[252],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[283],c2[282],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[285],c2[284],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[293],c2[292],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[318],c2[317],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[321],c2[320],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[322],c2[321],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[338],c2[337],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[342],c2[341],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[350],c2[349],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[369],c2[368],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[374],c2[373],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[378],c2[377],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[397],c2[396],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[397],c2[396],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[406],c2[405],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[421],c2[420],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[421],c2[420],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[425],c2[424],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[453],c2[452],6),simde_mm_xor_si128(c2[454],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[456],c2[455],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[481],c2[480],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[489],c2[488],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[489],c2[488],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[511],c2[510],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[512],c2[511],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[513],c2[512],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[534],c2[533],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[545],c2[544],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[546],c2[545],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[562],c2[561],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[566],c2[565],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[567],c2[566],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[594],c2[593],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[598],c2[597],14),simde_mm_alignr_epi8(c2[600],c2[599],11)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[56]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],10),simde_mm_alignr_epi8(c2[29],c2[28],10));

//row: 6  [direct, 8 terms]
     d2[84]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[176],c2[175],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[288],c2[287],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[310],c2[309],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[368],c2[367],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[480],c2[479],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[513],c2[512],8),simde_mm_alignr_epi8(c2[568],c2[567],4))))))));

//row: 7  [direct, 6 terms]
     d2[98]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[119],c2[118],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[234],c2[233],6),simde_mm_alignr_epi8(c2[402],c2[401],14))))));

//row: 9  [direct, 8 terms]
     d2[126]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[294],c2[293],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[315],c2[314],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[370],c2[369],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[488],c2[487],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[511],c2[510],13),simde_mm_alignr_epi8(c2[574],c2[573],10))))))));

//row: 10  [direct, 6 terms]
     d2[140]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[198],c2[197],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[237],c2[236],2),simde_mm_alignr_epi8(c2[399],c2[398],7))))));

//row: 12  [direct, 6 terms]
     d2[168]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[289],c2[288],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[309],c2[308],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[366],c2[365],12),simde_mm_alignr_epi8(c2[516],c2[515],6))))));

//row: 14  [direct, 6 terms]
     d2[196]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[347],c2[346],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[434],c2[433],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[460],c2[459],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[480],c2[479],3),simde_mm_alignr_epi8(c2[591],c2[590],11))))));

//row: 17  [direct, 5 terms]
     d2[238]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[399],c2[398],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[461],c2[460],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[480],c2[479],15),simde_mm_alignr_epi8(c2[589],c2[588],4)))));

//row: 18  [direct, 5 terms]
     d2[252]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[343],c2[342],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[377],c2[376],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[513],c2[512],15),simde_mm_alignr_epi8(c2[544],c2[543],10)))));

//row: 19  [direct, 5 terms]
     d2[266]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[237],c2[236],6),simde_mm_alignr_epi8(c2[281],c2[280],8)))));

//row: 21  [direct, 5 terms]
     d2[294]=simde_mm_xor_si128(c2[40],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[462],c2[461],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[564],c2[563],2),simde_mm_alignr_epi8(c2[595],c2[594],10)))));

//row: 22  [direct, 4 terms]
     d2[308]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],5),simde_mm_xor_si128(c2[336],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[365],c2[364],3),simde_mm_alignr_epi8(c2[486],c2[485],4))));

//row: 23  [direct, 4 terms]
     d2[322]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[291],c2[290],8),simde_mm_alignr_epi8(c2[512],c2[511],10))));

//row: 25  [direct, 4 terms]
     d2[350]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[179],c2[178],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[201],c2[200],1),simde_mm_alignr_epi8(c2[398],c2[397],1))));

//row: 26  [direct, 4 terms]
     d2[364]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],11),simde_mm_xor_si128(c2[67],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[126],c2[125],4),simde_mm_alignr_epi8(c2[428],c2[427],15))));

//row: 27  [direct, 3 terms]
     d2[378]=simde_mm_xor_si128(c2[41],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[177],c2[176],13),simde_mm_alignr_epi8(c2[235],c2[234],14)));

//row: 28  [direct, 4 terms]
     d2[392]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[546],c2[545],9),simde_mm_alignr_epi8(c2[596],c2[595],2))));

//row: 33  [direct, 4 terms]
     d2[462]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[319],c2[318],1),simde_mm_alignr_epi8(c2[597],c2[596],9))));

//row: 34  [direct, 4 terms]
     d2[476]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[208],c2[207],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[425],c2[424],4),simde_mm_alignr_epi8(c2[486],c2[485],6))));

//row: 36  [direct, 4 terms]
     d2[504]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[402],c2[401],13),simde_mm_xor_si128(c2[420],simde_mm_alignr_epi8(c2[505],c2[504],6))));

//row: 38  [direct, 4 terms]
     d2[532]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[258],c2[257],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[285],c2[284],9),simde_mm_alignr_epi8(c2[337],c2[336],10))));

//row: 39  [direct, 4 terms]
     d2[546]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[200],c2[199],1),simde_mm_alignr_epi8(c2[540],c2[539],2))));

//row: 40  [direct, 3 terms]
     d2[560]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[227],c2[226],13),c2[482]));

//row: 41  [direct, 4 terms]
     d2[574]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[256],c2[255],12),simde_mm_alignr_epi8(c2[515],c2[514],7))));

//row: 45  [direct, 3 terms]
     d2[630]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[176],c2[175],9),simde_mm_alignr_epi8(c2[282],c2[281],13)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[14]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],15),simde_mm_xor_si128(cm[29],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[62],cm[61],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[90],cm[89],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[145],cm[144],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[169],cm[168],10),simde_mm_xor_si128(cm[252],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[293],cm[292],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[338],cm[337],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[378],cm[377],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[421],cm[420],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[453],cm[452],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[513],cm[512],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[545],cm[544],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[567],cm[566],8),simde_mm_alignr_epi8(cm[594],cm[593],1))))))))))))))))));
       dm[28]=simde_mm_xor_si128(dm[14],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[59],cm[58],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[94],cm[93],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[115],cm[114],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[206],cm[205],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[233],cm[232],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[258],cm[257],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[321],cm[320],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[342],cm[341],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[397],cm[396],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[421],cm[420],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[456],cm[455],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[489],cm[488],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[534],cm[533],15),simde_mm_alignr_epi8(cm[600],cm[599],11))))))))))))))))));
       dm[42]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[13],cm[12],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[95],cm[94],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[113],cm[112],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[171],cm[170],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[205],cm[204],14),simde_mm_xor_si128(cm[225],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[283],cm[282],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[318],cm[317],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[350],cm[349],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[374],cm[373],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[397],cm[396],5),simde_mm_xor_si128(cm[454],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[481],cm[480],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[511],cm[510],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[562],cm[561],14),simde_mm_alignr_epi8(cm[598],cm[597],14))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[13]; dm=&dsimd[13];
  dm[14]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[13],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],15),simde_mm_xor_si128(cm[29],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[62],cm[61],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[90],cm[89],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[145],cm[144],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[169],cm[168],10),simde_mm_xor_si128(cm[252],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[293],cm[292],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[338],cm[337],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[378],cm[377],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[421],cm[420],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[453],cm[452],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[513],cm[512],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[545],cm[544],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[567],cm[566],8),simde_mm_alignr_epi8(cm[594],cm[593],1))))))))))))))))));
  dm[28]=simde_mm_xor_si128(dsimd[27],simde_mm_xor_si128(dsimd[13],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[59],cm[58],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[94],cm[93],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[115],cm[114],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[206],cm[205],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[233],cm[232],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[258],cm[257],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[321],cm[320],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[342],cm[341],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[397],cm[396],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[421],cm[420],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[456],cm[455],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[489],cm[488],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[534],cm[533],15),simde_mm_alignr_epi8(cm[600],cm[599],11))))))))))))))))));
  dm[42]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[13],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[13],cm[12],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[95],cm[94],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[113],cm[112],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[171],cm[170],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[205],cm[204],14),simde_mm_xor_si128(cm[225],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[283],cm[282],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[318],cm[317],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[350],cm[349],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[374],cm[373],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[397],cm[396],5),simde_mm_xor_si128(cm[454],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[481],cm[480],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[511],cm[510],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[562],cm[561],14),simde_mm_alignr_epi8(cm[598],cm[597],14))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*448,     d+k*224, 224);
    memcpy(kd+k*448+224, d+k*224, 224);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<14; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<153 + 6 sys]
     d2[70]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+153)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],6),simde_mm_xor_si128(c2[341],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[460],c2[459],6),simde_mm_alignr_epi8(c2[597],c2[596],2)))))));

//row: 8  [P0<<44+P2<<201 + 7 sys]
     d2[112]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+44)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1097)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[350],c2[349],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[461],c2[460],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[535],c2[534],7),simde_mm_alignr_epi8(c2[592],c2[591],10)))))))));

//row: 11  [P0<<84+P1<<201 + 5 sys]
     d2[154]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+84)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+649)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[337],c2[336],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[451],c2[450],3),c2[590]))))));

//row: 13  [P1<<78 + 4 sys]
     d2[182]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+526)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[201],c2[200],8),simde_mm_alignr_epi8(c2[574],c2[573],9)))));

//row: 15  [P3<<81 + 5 sys]
     d2[210]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1425)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[289],c2[288],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[378],c2[377],1),simde_mm_alignr_epi8(c2[518],c2[517],1))))));

//row: 16  [P0<<101 + 4 sys]
     d2[224]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+101)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[95],c2[94],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[311],c2[310],11),simde_mm_alignr_epi8(c2[572],c2[571],13)))));

//row: 20  [P0<<183 + 4 sys]
     d2[280]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+183)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[260],c2[259],9),simde_mm_alignr_epi8(c2[322],c2[321],6)))));

//row: 24  [P0<<124 + 4 sys]
     d2[336]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+124)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[94],c2[93],13),simde_mm_xor_si128(c2[116],simde_mm_alignr_epi8(c2[321],c2[320],1)))));

//row: 29  [P3<<68 + 3 sys]
     d2[406]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1412)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[393],c2[392],11),simde_mm_alignr_epi8(c2[508],c2[507],5))));

//row: 30  [P2<<123 + 3 sys]
     d2[420]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1019)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[289],c2[288],2),simde_mm_alignr_epi8(c2[378],c2[377],2))));

//row: 31  [P0<<177+P3<<128 + 2 sys]
     d2[434]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+177)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1472)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],15),simde_mm_alignr_epi8(c2[200],c2[199],1))));

//row: 32  [P2<<30 + 3 sys]
     d2[448]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+926)),simde_mm_xor_si128(c2[12],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[350],c2[349],1),simde_mm_alignr_epi8(c2[396],c2[395],10))));

//row: 35  [P0<<187 + 3 sys]
     d2[490]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+187)),simde_mm_xor_si128(c2[40],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[179],c2[178],13),simde_mm_alignr_epi8(c2[338],c2[337],10))));

//row: 37  [P1<<138 + 2 sys]
     d2[518]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+586)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],1),simde_mm_alignr_epi8(c2[377],c2[376],3)));

//row: 42  [P2<<78 + 2 sys]
     d2[588]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+974)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],2),simde_mm_alignr_epi8(c2[118],c2[117],11)));

//row: 43  [P3<<161 + 3 sys]
     d2[602]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1505)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[450],c2[449],6),simde_mm_alignr_epi8(c2[513],c2[512],6))));

//row: 44  [P0<<12 + 3 sys]
     d2[616]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+12)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[197],c2[196],4),simde_mm_alignr_epi8(c2[253],c2[252],9))));
  }
}
