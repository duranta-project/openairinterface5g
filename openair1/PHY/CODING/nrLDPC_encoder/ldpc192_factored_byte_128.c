#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=192, byte encoding, factored
// 319 terms vs 2097 expanded (6.57x); pipeline lag 1 chunk(s)
static inline void ldpc192_byte(uint8_t *c,uint8_t *d) {
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
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[77],c2[76],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[84],c2[83],1),simde_mm_xor_si128(c2[72],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],3),simde_mm_xor_si128(c2[102],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],12),simde_mm_xor_si128(c2[124],simde_mm_xor_si128(c2[129],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[155],c2[154],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[173],c2[172],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[177],c2[176],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[178],c2[177],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[196],c2[195],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[201],c2[200],11),simde_mm_xor_si128(c2[202],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[220],c2[219],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[224],c2[223],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[228],c2[227],2),simde_mm_xor_si128(c2[246],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[271],c2[270],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[271],c2[270],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[272],c2[271],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[291],c2[290],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[298],c2[297],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[313],c2[312],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[322],c2[321],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[323],c2[322],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[338],c2[337],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[338],c2[337],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[342],c2[341],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[362],c2[361],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[364],c2[363],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[367],c2[366],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[391],c2[390],10),simde_mm_xor_si128(c2[391],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[395],c2[394],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[415],c2[414],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[416],c2[415],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[417],c2[416],3),simde_mm_xor_si128(c2[435],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[436],c2[435],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[437],c2[436],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[457],c2[456],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[465],c2[464],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[468],c2[467],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[481],c2[480],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[483],c2[482],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[489],c2[488],10),simde_mm_xor_si128(c2[511],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[514],c2[513],10),simde_mm_alignr_epi8(c2[515],c2[514],5)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[48]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],12),simde_mm_alignr_epi8(c2[36],c2[35],5));

//row: 6  [direct, 8 terms]
     d2[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[241],c2[240],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[274],c2[273],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[318],c2[317],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[412],c2[411],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[434],c2[433],2),simde_mm_alignr_epi8(c2[483],c2[482],1))))))));

//row: 7  [direct, 6 terms]
     d2[84]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[177],c2[176],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[199],c2[198],2),simde_mm_alignr_epi8(c2[344],c2[343],2))))));

//row: 9  [direct, 8 terms]
     d2[108]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[273],c2[272],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[316],c2[315],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[415],c2[414],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[436],c2[435],15),simde_mm_alignr_epi8(c2[486],c2[485],2))))))));

//row: 10  [direct, 6 terms]
     d2[120]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[175],c2[174],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[204],c2[203],15),simde_mm_alignr_epi8(c2[347],c2[346],2))))));

//row: 12  [direct, 6 terms]
     d2[144]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[251],c2[250],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[267],c2[266],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[316],c2[315],2),simde_mm_alignr_epi8(c2[437],c2[436],10))))));

//row: 14  [direct, 6 terms]
     d2[168]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[292],c2[291],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[369],c2[368],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[390],c2[389],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[418],c2[417],11),simde_mm_alignr_epi8(c2[505],c2[504],12))))));

//row: 17  [direct, 5 terms]
     d2[204]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[343],c2[342],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[390],c2[389],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[419],c2[418],6),simde_mm_alignr_epi8(c2[516],c2[515],7)))));

//row: 18  [direct, 5 terms]
     d2[216]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[299],c2[298],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[318],c2[317],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[441],c2[440],4),simde_mm_alignr_epi8(c2[457],c2[456],4)))));

//row: 19  [direct, 5 terms]
     d2[228]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[178],c2[177],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[196],c2[195],2),simde_mm_alignr_epi8(c2[241],c2[240],5)))));

//row: 21  [direct, 5 terms]
     d2[252]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[127],c2[126],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[393],c2[392],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[482],c2[481],5),simde_mm_alignr_epi8(c2[511],c2[510],1)))));

//row: 22  [direct, 4 terms]
     d2[264]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[289],c2[288],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[315],c2[314],9),simde_mm_alignr_epi8(c2[410],c2[409],6))));

//row: 23  [direct, 4 terms]
     d2[276]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[244],c2[243],13),simde_mm_alignr_epi8(c2[434],c2[433],11))));

//row: 25  [direct, 4 terms]
     d2[300]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[180],c2[179],15),simde_mm_alignr_epi8(c2[344],c2[343],8))));

//row: 26  [direct, 4 terms]
     d2[312]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],12),simde_mm_alignr_epi8(c2[369],c2[368],8))));

//row: 27  [direct, 3 terms]
     d2[324]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],2),simde_mm_alignr_epi8(c2[199],c2[198],5)));

//row: 28  [direct, 4 terms]
     d2[336]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[460],c2[459],4),simde_mm_alignr_epi8(c2[510],c2[509],2))));

//row: 33  [direct, 4 terms]
     d2[396]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[275],c2[274],9),simde_mm_alignr_epi8(c2[511],c2[510],9))));

//row: 34  [direct, 4 terms]
     d2[408]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[178],c2[177],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[361],c2[360],10),simde_mm_alignr_epi8(c2[416],c2[415],8))));

//row: 36  [direct, 4 terms]
     d2[432]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[344],c2[343],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[367],c2[366],9),simde_mm_alignr_epi8(c2[434],c2[433],5))));

//row: 38  [direct, 4 terms]
     d2[456]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[228],c2[227],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[247],c2[246],1),simde_mm_alignr_epi8(c2[290],c2[289],6))));

//row: 39  [direct, 4 terms]
     d2[468]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[78],c2[77],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[178],c2[177],10),simde_mm_alignr_epi8(c2[463],c2[462],9))));

//row: 40  [direct, 3 terms]
     d2[480]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[195],c2[194],5),simde_mm_alignr_epi8(c2[416],c2[415],8)));

//row: 41  [direct, 4 terms]
     d2[492]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[225],c2[224],11),c2[438])));

//row: 45  [direct, 3 terms]
     d2[540]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[154],c2[153],5),simde_mm_alignr_epi8(c2[241],c2[240],15)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[12]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[52],cm[51],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[84],cm[83],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[132],cm[131],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[146],cm[145],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[224],cm[223],13),simde_mm_xor_si128(cm[246],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[271],cm[270],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[290],cm[289],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[323],cm[322],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[362],cm[361],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[436],cm[435],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[468],cm[467],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[489],cm[488],10),simde_mm_alignr_epi8(cm[514],cm[513],10))))))))))))))))));
       dm[24]=simde_mm_xor_si128(dm[12],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[77],cm[76],9),simde_mm_xor_si128(cm[102],simde_mm_xor_si128(cm[129],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[177],cm[176],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[201],cm[200],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[228],cm[227],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[271],cm[270],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[298],cm[297],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[338],cm[337],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[367],cm[366],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[395],cm[394],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[416],cm[415],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[465],cm[464],11),cm[511])))))))))))))))));
       dm[36]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],7),simde_mm_xor_si128(cm[72],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[102],cm[101],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[145],cm[144],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[178],cm[177],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[196],cm[195],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[249],cm[248],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[272],cm[271],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[291],cm[290],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[338],cm[337],4),simde_mm_xor_si128(cm[391],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[415],cm[414],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[437],cm[436],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[483],cm[482],7),simde_mm_alignr_epi8(cm[515],cm[514],5))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[11]; dm=&dsimd[11];
  dm[12]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[11],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[52],cm[51],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[84],cm[83],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[132],cm[131],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[146],cm[145],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[224],cm[223],13),simde_mm_xor_si128(cm[246],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[271],cm[270],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[290],cm[289],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[323],cm[322],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[362],cm[361],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[436],cm[435],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[468],cm[467],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[489],cm[488],10),simde_mm_alignr_epi8(cm[514],cm[513],10))))))))))))))))));
  dm[24]=simde_mm_xor_si128(dsimd[23],simde_mm_xor_si128(dsimd[11],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[77],cm[76],9),simde_mm_xor_si128(cm[102],simde_mm_xor_si128(cm[129],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[177],cm[176],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[201],cm[200],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[228],cm[227],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[271],cm[270],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[298],cm[297],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[338],cm[337],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[367],cm[366],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[395],cm[394],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[416],cm[415],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[465],cm[464],11),cm[511])))))))))))))))));
  dm[36]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[11],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],7),simde_mm_xor_si128(cm[72],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[102],cm[101],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[145],cm[144],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[178],cm[177],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[196],cm[195],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[249],cm[248],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[272],cm[271],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[291],cm[290],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[338],cm[337],4),simde_mm_xor_si128(cm[391],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[415],cm[414],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[437],cm[436],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[483],cm[482],7),simde_mm_alignr_epi8(cm[515],cm[514],5))))))))))))))))));

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

//row: 5  [P0<<157 + 6 sys]
     d2[60]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+157)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[299],c2[298],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[388],c2[387],1),simde_mm_alignr_epi8(c2[508],c2[507],3)))))));

//row: 8  [P0<<67+P2<<170 + 7 sys]
     d2[96]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+67)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+938)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[83],c2[82],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[387],c2[386],7),simde_mm_xor_si128(c2[458],c2[515]))))))));

//row: 11  [P0<<142+P1<<115 + 5 sys]
     d2[132]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+142)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+499)),simde_mm_xor_si128(c2[3],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[289],c2[288],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[387],c2[386],15),simde_mm_alignr_epi8(c2[516],c2[515],12)))))));

//row: 13  [P1<<178 + 4 sys]
     d2[156]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+562)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[84],c2[83],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[173],c2[172],10),simde_mm_alignr_epi8(c2[488],c2[487],3)))));

//row: 15  [P3<<77 + 5 sys]
     d2[180]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1229)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[320],c2[319],14),simde_mm_alignr_epi8(c2[436],c2[435],7))))));

//row: 16  [P0<<57 + 4 sys]
     d2[192]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+57)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[268],c2[267],9),simde_mm_alignr_epi8(c2[487],c2[486],1)))));

//row: 20  [P0<<59 + 4 sys]
     d2[240]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+59)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[221],c2[220],8),simde_mm_alignr_epi8(c2[274],c2[273],5)))));

//row: 24  [P0<<42 + 4 sys]
     d2[288]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+42)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],11),simde_mm_alignr_epi8(c2[274],c2[273],3)))));

//row: 29  [P3<<78 + 3 sys]
     d2[348]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1230)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[337],c2[336],5),simde_mm_alignr_epi8(c2[442],c2[441],3))));

//row: 30  [P2<<90 + 3 sys]
     d2[360]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+858)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[243],c2[242],5),simde_mm_alignr_epi8(c2[317],c2[316],4))));

//row: 31  [P0<<66+P3<<64 + 2 sys]
     d2[372]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+66)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1216)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],4),simde_mm_alignr_epi8(c2[170],c2[169],7))));

//row: 32  [P2<<95 + 3 sys]
     d2[384]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+863)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[289],c2[288],9),simde_mm_alignr_epi8(c2[347],c2[346],15))));

//row: 35  [P0<<74 + 3 sys]
     d2[420]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+74)),simde_mm_xor_si128(c2[32],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[153],c2[152],15),simde_mm_alignr_epi8(c2[289],c2[288],2))));

//row: 37  [P1<<115 + 2 sys]
     d2[444]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+499)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],13),simde_mm_alignr_epi8(c2[318],c2[317],2)));

//row: 42  [P2<<26 + 2 sys]
     d2[504]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+794)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],1),simde_mm_alignr_epi8(c2[97],c2[96],14)));

//row: 43  [P3<<168 + 3 sys]
     d2[516]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1320)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[393],c2[392],4),simde_mm_alignr_epi8(c2[440],c2[439],2))));

//row: 44  [P0<<82 + 3 sys]
     d2[528]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+82)),simde_mm_xor_si128(c2[5],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[173],c2[172],14),simde_mm_alignr_epi8(c2[227],c2[226],3))));
  }
}
