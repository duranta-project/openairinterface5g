#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=176, byte encoding, factored
// 319 terms vs 2107 expanded (6.61x); pipeline lag 1 chunk(s)
static inline void ldpc176_byte(uint8_t *c,uint8_t *d) {
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
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[70],c2[69],1),simde_mm_xor_si128(c2[72],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[76],c2[75],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[95],c2[94],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[96],c2[95],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[112],c2[111],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[116],c2[115],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[117],c2[116],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[138],c2[137],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[141],c2[140],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[143],c2[142],5),simde_mm_xor_si128(c2[161],simde_mm_xor_si128(c2[162],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[165],c2[164],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[180],c2[179],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[182],c2[181],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[184],c2[183],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[202],c2[201],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[206],c2[205],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[225],c2[224],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[227],c2[226],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[228],c2[227],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[243],c2[242],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[247],c2[246],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[253],c2[252],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[269],c2[268],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[275],c2[274],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[275],c2[274],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[288],c2[287],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[292],c2[291],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[293],c2[292],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[309],c2[308],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[315],c2[314],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[318],c2[317],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[334],c2[333],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[335],c2[334],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[338],c2[337],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[357],c2[356],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[358],c2[357],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[363],c2[362],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[377],c2[376],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[383],c2[382],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[383],c2[382],5),simde_mm_xor_si128(c2[404],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[405],c2[404],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[407],c2[406],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[419],c2[418],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[421],c2[420],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[429],c2[428],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[441],c2[440],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[443],c2[442],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[448],c2[447],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[463],c2[462],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[470],c2[469],9),simde_mm_alignr_epi8(c2[470],c2[469],14)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[44]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],10),c2[27]);

//row: 6  [direct, 8 terms]
     d2[66]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[230],c2[229],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[297],c2[296],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[384],c2[383],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[403],c2[402],12),simde_mm_alignr_epi8(c2[449],c2[448],1))))))));

//row: 7  [direct, 6 terms]
     d2[77]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],7),simde_mm_xor_si128(c2[91],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[162],c2[161],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[179],c2[178],7),simde_mm_alignr_epi8(c2[311],c2[310],7))))));

//row: 9  [direct, 8 terms]
     d2[99]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[228],c2[227],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[292],c2[291],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[384],c2[383],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[398],c2[397],7),simde_mm_alignr_epi8(c2[451],c2[450],1))))))));

//row: 10  [direct, 6 terms]
     d2[110]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[158],c2[157],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[180],c2[179],1),simde_mm_alignr_epi8(c2[314],c2[313],11))))));

//row: 12  [direct, 6 terms]
     d2[132]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[229],c2[228],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],14),simde_mm_xor_si128(c2[286],simde_mm_alignr_epi8(c2[401],c2[400],12))))));

//row: 14  [direct, 6 terms]
     d2[154]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[274],c2[273],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[337],c2[336],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[362],c2[361],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[379],c2[378],2),simde_mm_alignr_epi8(c2[467],c2[466],14))))));

//row: 17  [direct, 5 terms]
     d2[187]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[312],c2[311],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[357],c2[356],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[382],c2[381],4),simde_mm_alignr_epi8(c2[463],c2[462],14)))));

//row: 18  [direct, 5 terms]
     d2[198]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[271],c2[270],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[291],c2[290],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[397],c2[396],5),simde_mm_alignr_epi8(c2[423],c2[422],9)))));

//row: 19  [direct, 5 terms]
     d2[209]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[164],c2[163],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[182],c2[181],2),simde_mm_alignr_epi8(c2[222],c2[221],12)))));

//row: 21  [direct, 5 terms]
     d2[231]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[116],c2[115],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[363],c2[362],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[449],c2[448],15),simde_mm_alignr_epi8(c2[469],c2[468],13)))));

//row: 22  [direct, 4 terms]
     d2[242]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[274],c2[273],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],12),simde_mm_alignr_epi8(c2[382],c2[381],1))));

//row: 23  [direct, 4 terms]
     d2[253]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[221],c2[220],5),c2[404])));

//row: 25  [direct, 4 terms]
     d2[275]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],15),simde_mm_alignr_epi8(c2[312],c2[311],6))));

//row: 26  [direct, 4 terms]
     d2[286]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[92],c2[91],2),simde_mm_alignr_epi8(c2[337],c2[336],3))));

//row: 27  [direct, 3 terms]
     d2[297]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[142],c2[141],6),simde_mm_alignr_epi8(c2[182],c2[181],12)));

//row: 28  [direct, 4 terms]
     d2[308]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[421],c2[420],8),simde_mm_alignr_epi8(c2[473],c2[472],7))));

//row: 33  [direct, 4 terms]
     d2[363]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],8),simde_mm_xor_si128(c2[52],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[251],c2[250],13),simde_mm_alignr_epi8(c2[469],c2[468],5))));

//row: 34  [direct, 4 terms]
     d2[374]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],12),simde_mm_xor_si128(c2[163],simde_mm_xor_si128(c2[337],simde_mm_alignr_epi8(c2[378],c2[377],6))));

//row: 36  [direct, 4 terms]
     d2[396]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[309],c2[308],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[333],c2[332],3),simde_mm_alignr_epi8(c2[397],c2[396],12))));

//row: 38  [direct, 4 terms]
     d2[418]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[200],c2[199],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[223],c2[222],1),simde_mm_alignr_epi8(c2[269],c2[268],6))));

//row: 39  [direct, 4 terms]
     d2[429]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[72],c2[71],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[157],c2[156],5),c2[424])));

//row: 40  [direct, 3 terms]
     d2[440]=simde_mm_xor_si128(c2[8],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[180],c2[179],13),simde_mm_alignr_epi8(c2[383],c2[382],7)));

//row: 41  [direct, 4 terms]
     d2[451]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[200],c2[199],9),c2[402])));

//row: 45  [direct, 3 terms]
     d2[495]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],3),simde_mm_alignr_epi8(c2[224],c2[223],11)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[11]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[55],cm[54],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[76],cm[75],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[112],cm[111],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[143],cm[142],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[203],cm[202],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[225],cm[224],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[243],cm[242],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[275],cm[274],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[288],cm[287],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[334],cm[333],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[363],cm[362],11),simde_mm_xor_si128(cm[404],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[429],cm[428],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[443],cm[442],15),simde_mm_alignr_epi8(cm[463],cm[462],12))))))))))))))))));
       dm[22]=simde_mm_xor_si128(dm[11],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[48],cm[47],1),simde_mm_xor_si128(cm[72],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[116],cm[115],12),simde_mm_xor_si128(cm[161],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[202],cm[201],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[253],cm[252],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[269],cm[268],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[318],cm[317],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[335],cm[334],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[358],cm[357],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[383],cm[382],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[419],cm[418],9),simde_mm_alignr_epi8(cm[470],cm[469],14))))))))))))))))));
       dm[33]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[28],cm[27],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[70],cm[69],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[95],cm[94],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[141],cm[140],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[165],cm[164],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[227],cm[226],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[247],cm[246],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[275],cm[274],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[309],cm[308],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[357],cm[356],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[377],cm[376],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[405],cm[404],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[448],cm[447],9),simde_mm_alignr_epi8(cm[470],cm[469],9))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[10]; dm=&dsimd[10];
  dm[11]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[10],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[30],cm[29],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[55],cm[54],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[76],cm[75],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[112],cm[111],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[143],cm[142],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[203],cm[202],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[225],cm[224],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[243],cm[242],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[275],cm[274],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[288],cm[287],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[334],cm[333],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[363],cm[362],11),simde_mm_xor_si128(cm[404],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[429],cm[428],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[443],cm[442],15),simde_mm_alignr_epi8(cm[463],cm[462],12))))))))))))))))));
  dm[22]=simde_mm_xor_si128(dsimd[21],simde_mm_xor_si128(dsimd[10],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[48],cm[47],1),simde_mm_xor_si128(cm[72],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[116],cm[115],12),simde_mm_xor_si128(cm[161],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[202],cm[201],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[253],cm[252],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[269],cm[268],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[318],cm[317],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[335],cm[334],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[358],cm[357],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[383],cm[382],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[419],cm[418],9),simde_mm_alignr_epi8(cm[470],cm[469],14))))))))))))))))));
  dm[33]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[10],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[28],cm[27],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[70],cm[69],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[95],cm[94],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[141],cm[140],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[165],cm[164],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[227],cm[226],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[247],cm[246],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[275],cm[274],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[309],cm[308],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[357],cm[356],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[377],cm[376],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[405],cm[404],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[448],cm[447],9),simde_mm_alignr_epi8(cm[470],cm[469],9))))))))))))))))));

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

//row: 5  [P0<<107 + 6 sys]
     d2[55]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+107)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[274],c2[273],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[360],c2[359],7),simde_mm_alignr_epi8(c2[468],c2[467],2)))))));

//row: 8  [P0<<138+P2<<68 + 7 sys]
     d2[88]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+138)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+772)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[273],c2[272],8),simde_mm_xor_si128(c2[355],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[420],c2[419],1),simde_mm_alignr_epi8(c2[466],c2[465],11)))))))));

//row: 11  [P0<<162+P1<<16 + 5 sys]
     d2[121]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+162)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+368)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[266],c2[265],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[363],c2[362],15),simde_mm_alignr_epi8(c2[473],c2[472],6)))))));

//row: 13  [P1<<81 + 4 sys]
     d2[143]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+433)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[160],c2[159],9),simde_mm_alignr_epi8(c2[446],c2[445],14)))));

//row: 15  [P3<<113 + 5 sys]
     d2[165]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1169)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[221],c2[220],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[292],c2[291],1),c2[396])))));

//row: 16  [P0<<114 + 4 sys]
     d2[176]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+114)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[252],c2[251],12),simde_mm_alignr_epi8(c2[450],c2[449],11)))));

//row: 20  [P0<<30 + 4 sys]
     d2[220]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+30)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[208],c2[207],11),simde_mm_alignr_epi8(c2[247],c2[246],4)))));

//row: 24  [P0<<18 + 4 sys]
     d2[264]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+18)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[77],c2[76],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[93],c2[92],9),c2[249]))));

//row: 29  [P3<<107 + 3 sys]
     d2[319]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1163)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[315],c2[314],8),simde_mm_alignr_epi8(c2[401],c2[400],3))));

//row: 30  [P2<<36 + 3 sys]
     d2[330]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+740)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],11),simde_mm_xor_si128(c2[221],simde_mm_alignr_epi8(c2[292],c2[291],15))));

//row: 31  [P0<<103+P3<<46 + 2 sys]
     d2[341]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+103)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1102)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],5),simde_mm_alignr_epi8(c2[162],c2[161],9))));

//row: 32  [P2<<83 + 3 sys]
     d2[352]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+787)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[270],c2[269],9),simde_mm_alignr_epi8(c2[319],c2[318],2))));

//row: 35  [P0<<92 + 3 sys]
     d2[385]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+92)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[135],c2[134],2),simde_mm_alignr_epi8(c2[266],c2[265],3))));

//row: 37  [P1<<85 + 2 sys]
     d2[407]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+437)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],15),simde_mm_alignr_epi8(c2[290],c2[289],12)));

//row: 42  [P2<<17 + 2 sys]
     d2[462]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+721)),simde_mm_xor_si128(c2[7],simde_mm_alignr_epi8(c2[94],c2[93],3)));

//row: 43  [P3<<167 + 3 sys]
     d2[473]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1223)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[353],c2[352],3),simde_mm_alignr_epi8(c2[400],c2[399],5))));

//row: 44  [P0<<96 + 3 sys]
     d2[484]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+96)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[159],c2[158],4),simde_mm_alignr_epi8(c2[206],c2[205],5))));
  }
}
