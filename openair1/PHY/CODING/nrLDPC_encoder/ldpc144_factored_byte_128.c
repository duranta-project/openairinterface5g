#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=144, byte encoding, factored
// 319 terms vs 2095 expanded (6.57x); pipeline lag 1 chunk(s)
static inline void ldpc144_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1152] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<9; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[79],c2[78],12),simde_mm_xor_si128(c2[79],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],5),simde_mm_xor_si128(c2[91],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[95],c2[94],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[97],c2[96],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[109],c2[108],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[112],c2[111],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[135],c2[134],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[164],c2[163],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[170],c2[169],5),simde_mm_xor_si128(c2[162],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[181],c2[180],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],9),simde_mm_xor_si128(c2[180],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[200],c2[199],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[205],c2[204],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[206],c2[205],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[217],c2[216],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[220],c2[219],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[221],c2[220],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[237],c2[236],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[239],c2[238],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[257],c2[256],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[258],c2[257],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[260],c2[259],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[271],c2[270],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[276],c2[275],9),simde_mm_xor_si128(c2[270],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[289],c2[288],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[295],c2[294],2),simde_mm_xor_si128(c2[288],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[307],c2[306],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[308],c2[307],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[311],c2[310],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[330],c2[329],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[332],c2[331],13),simde_mm_xor_si128(c2[324],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[343],c2[342],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[346],c2[345],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[347],c2[346],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[365],c2[364],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[366],c2[365],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[368],c2[367],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[385],c2[384],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[386],c2[385],5),simde_mm_alignr_epi8(c2[386],c2[385],7)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[36]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],6),simde_mm_alignr_epi8(c2[24],c2[23],11));

//row: 6  [direct, 8 terms]
     d2[54]=simde_mm_xor_si128(c2[5],simde_mm_xor_si128(c2[108],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[182],c2[181],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[204],c2[203],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[238],c2[237],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[309],c2[308],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[325],c2[324],7),simde_mm_alignr_epi8(c2[367],c2[366],12))))))));

//row: 7  [direct, 6 terms]
     d2[63]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[129],c2[128],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[151],c2[150],8),simde_mm_alignr_epi8(c2[254],c2[253],4))))));

//row: 9  [direct, 8 terms]
     d2[81]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[186],c2[185],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[207],c2[206],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[237],c2[236],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[315],c2[314],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[329],c2[328],12),simde_mm_alignr_epi8(c2[365],c2[364],1))))))));

//row: 10  [direct, 6 terms]
     d2[90]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[150],c2[149],10),simde_mm_alignr_epi8(c2[256],c2[255],9))))));

//row: 12  [direct, 6 terms]
     d2[108]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[187],c2[186],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[199],c2[198],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[240],c2[239],14),simde_mm_alignr_epi8(c2[331],c2[330],3))))));

//row: 14  [direct, 6 terms]
     d2[126]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[219],c2[218],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[277],c2[276],13),simde_mm_xor_si128(c2[289],simde_mm_xor_si128(c2[306],simde_mm_alignr_epi8(c2[383],c2[382],15))))));

//row: 17  [direct, 5 terms]
     d2[153]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[261],c2[260],9),simde_mm_xor_si128(c2[288],simde_mm_xor_si128(c2[306],simde_mm_alignr_epi8(c2[380],c2[379],2)))));

//row: 18  [direct, 5 terms]
     d2[162]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[217],c2[216],7),simde_mm_xor_si128(c2[234],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[331],c2[330],1),c2[342]))));

//row: 19  [direct, 5 terms]
     d2[171]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(c2[18],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],6),simde_mm_xor_si128(c2[144],c2[180]))));

//row: 21  [direct, 5 terms]
     d2[189]=simde_mm_xor_si128(c2[22],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[93],c2[92],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[296],c2[295],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[361],c2[360],9),simde_mm_alignr_epi8(c2[380],c2[379],2)))));

//row: 22  [direct, 4 terms]
     d2[198]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[219],c2[218],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[239],c2[238],8),simde_mm_alignr_epi8(c2[314],c2[313],1))));

//row: 23  [direct, 4 terms]
     d2[207]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],4),simde_mm_xor_si128(c2[36],simde_mm_xor_si128(c2[180],simde_mm_alignr_epi8(c2[326],c2[325],5))));

//row: 25  [direct, 4 terms]
     d2[225]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],13),simde_mm_xor_si128(c2[108],simde_mm_xor_si128(c2[126],simde_mm_alignr_epi8(c2[255],c2[254],7))));

//row: 26  [direct, 4 terms]
     d2[234]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(c2[36],simde_mm_xor_si128(c2[72],simde_mm_alignr_epi8(c2[279],c2[278],5))));

//row: 27  [direct, 3 terms]
     d2[243]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[111],c2[110],4),simde_mm_alignr_epi8(c2[149],c2[148],8)));

//row: 28  [direct, 4 terms]
     d2[252]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],3),simde_mm_xor_si128(c2[72],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[343],c2[342],11),simde_mm_alignr_epi8(c2[382],c2[381],14))));

//row: 33  [direct, 4 terms]
     d2[297]=simde_mm_xor_si128(c2[18],simde_mm_xor_si128(c2[36],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],12),simde_mm_alignr_epi8(c2[380],c2[379],2))));

//row: 34  [direct, 4 terms]
     d2[306]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],5),simde_mm_xor_si128(c2[126],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[277],c2[276],12),c2[306])));

//row: 36  [direct, 4 terms]
     d2[324]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_xor_si128(c2[253],simde_mm_xor_si128(c2[270],c2[324])));

//row: 38  [direct, 4 terms]
     d2[342]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],14),simde_mm_xor_si128(c2[162],simde_mm_xor_si128(c2[180],c2[216])));

//row: 39  [direct, 4 terms]
     d2[351]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],9),simde_mm_xor_si128(c2[54],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[128],c2[127],5),simde_mm_alignr_epi8(c2[350],c2[349],5))));

//row: 40  [direct, 3 terms]
     d2[360]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_xor_si128(c2[144],simde_mm_alignr_epi8(c2[307],c2[306],2)));

//row: 41  [direct, 4 terms]
     d2[369]=simde_mm_xor_si128(c2[18],simde_mm_xor_si128(c2[54],simde_mm_xor_si128(c2[162],simde_mm_alignr_epi8(c2[327],c2[326],7))));

//row: 45  [direct, 3 terms]
     d2[405]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],8),simde_mm_xor_si128(c2[108],c2[180]));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[9]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[22],cm[21],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[39],cm[38],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[95],cm[94],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[109],cm[108],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[164],cm[163],13),simde_mm_xor_si128(cm[180],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[206],cm[205],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[221],cm[220],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[242],cm[241],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[276],cm[275],9),simde_mm_xor_si128(cm[288],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[330],cm[329],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[347],cm[346],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],9),simde_mm_alignr_epi8(cm[386],cm[385],5))))))))))))))))));
       dm[18]=simde_mm_xor_si128(dm[9],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[59],cm[58],15),simde_mm_xor_si128(cm[79],simde_mm_xor_si128(cm[91],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[170],cm[169],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[205],cm[204],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[217],cm[216],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[257],cm[256],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[271],cm[270],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[289],cm[288],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[307],cm[306],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[343],cm[342],12),simde_mm_alignr_epi8(cm[386],cm[385],7))))))))))))))))));
       dm[27]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[19],cm[18],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[56],cm[55],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[135],cm[134],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[183],cm[182],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[200],cm[199],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[220],cm[219],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[237],cm[236],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[260],cm[259],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[295],cm[294],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[308],cm[307],5),simde_mm_xor_si128(cm[324],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[368],cm[367],1),simde_mm_alignr_epi8(cm[385],cm[384],12))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[8]; dm=&dsimd[8];
  dm[9]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[8],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[22],cm[21],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[39],cm[38],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[95],cm[94],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[109],cm[108],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[164],cm[163],13),simde_mm_xor_si128(cm[180],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[206],cm[205],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[221],cm[220],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[242],cm[241],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[276],cm[275],9),simde_mm_xor_si128(cm[288],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[330],cm[329],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[347],cm[346],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],9),simde_mm_alignr_epi8(cm[386],cm[385],5))))))))))))))))));
  dm[18]=simde_mm_xor_si128(dsimd[17],simde_mm_xor_si128(dsimd[8],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[59],cm[58],15),simde_mm_xor_si128(cm[79],simde_mm_xor_si128(cm[91],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[170],cm[169],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[205],cm[204],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[217],cm[216],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[257],cm[256],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[271],cm[270],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[289],cm[288],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[307],cm[306],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[343],cm[342],12),simde_mm_alignr_epi8(cm[386],cm[385],7))))))))))))))))));
  dm[27]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[8],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[19],cm[18],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[56],cm[55],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[135],cm[134],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[183],cm[182],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[200],cm[199],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[220],cm[219],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[237],cm[236],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[260],cm[259],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[295],cm[294],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[308],cm[307],5),simde_mm_xor_si128(cm[324],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[368],cm[367],1),simde_mm_alignr_epi8(cm[385],cm[384],12))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*288,     d+k*144, 144);
    memcpy(kd+k*288+144, d+k*144, 144);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<9; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<0 + 6 sys]
     d2[45]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+0)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[225],c2[224],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[295],c2[294],14),simde_mm_alignr_epi8(c2[383],c2[382],15)))))));

//row: 8  [P2<<54+P0<<86 + 7 sys]
     d2[72]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+630)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+86)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],6),simde_mm_xor_si128(c2[18],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[219],c2[218],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[295],c2[294],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[345],c2[344],14),simde_mm_alignr_epi8(c2[379],c2[378],15)))))))));

//row: 11  [P0<<39+P1<<26 + 5 sys]
     d2[99]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+39)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+314)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[225],c2[224],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[291],c2[290],5),simde_mm_alignr_epi8(c2[387],c2[386],1)))))));

//row: 13  [P1<<36 + 4 sys]
     d2[117]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+324)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],7),simde_mm_xor_si128(c2[54],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],8),simde_mm_alignr_epi8(c2[363],c2[362],15)))));

//row: 15  [P3<<64 + 5 sys]
     d2[135]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+928)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],10),simde_mm_xor_si128(c2[18],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[241],c2[240],12),simde_mm_alignr_epi8(c2[329],c2[328],4))))));

//row: 16  [P0<<0 + 4 sys]
     d2[144]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+0)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[205],c2[204],3),simde_mm_alignr_epi8(c2[364],c2[363],6)))));

//row: 20  [P0<<84 + 4 sys]
     d2[180]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+84)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[168],c2[167],10),c2[198]))));

//row: 24  [P0<<0 + 4 sys]
     d2[216]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+0)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],7),simde_mm_xor_si128(c2[72],simde_mm_alignr_epi8(c2[206],c2[205],10)))));

//row: 29  [P3<<42 + 3 sys]
     d2[261]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+906)),simde_mm_xor_si128(c2[18],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[255],c2[254],4),c2[324])));

//row: 30  [P2<<29 + 3 sys]
     d2[270]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+605)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[186],c2[185],10),simde_mm_alignr_epi8(c2[241],c2[240],12))));

//row: 31  [P0<<22+P3<<19 + 2 sys]
     d2[279]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+22)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+883)),simde_mm_xor_si128(c2[18],c2[126])));

//row: 32  [P2<<18 + 3 sys]
     d2[288]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+594)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[221],c2[220],3),simde_mm_alignr_epi8(c2[255],c2[254],4))));

//row: 35  [P0<<61 + 3 sys]
     d2[315]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+61)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[117],c2[116],6),c2[216])));

//row: 37  [P1<<0 + 2 sys]
     d2[333]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+288)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],8),c2[234]));

//row: 42  [P2<<35 + 2 sys]
     d2[378]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+611)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],11),c2[72]));

//row: 43  [P3<<126 + 3 sys]
     d2[387]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+990)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[295],c2[294],3),c2[324])));

//row: 44  [P0<<57 + 3 sys]
     d2[396]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+57)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_xor_si128(c2[126],c2[162])));
  }
}
