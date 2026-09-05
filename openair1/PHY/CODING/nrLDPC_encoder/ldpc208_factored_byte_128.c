#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=208, byte encoding, factored
// 319 terms vs 2105 expanded (6.60x); pipeline lag 7 chunk(s)
static inline void ldpc208_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1664] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 7 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<13; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[109],c2[108],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[111],c2[110],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[138],c2[137],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[159],c2[158],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[163],c2[162],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[166],c2[165],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[189],c2[188],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[190],c2[189],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[192],c2[191],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[215],c2[214],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[215],c2[214],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[217],c2[216],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[236],c2[235],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[241],c2[240],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[241],c2[240],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[263],c2[262],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[266],c2[265],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[267],c2[266],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[293],c2[292],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[294],c2[293],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[298],c2[297],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[316],c2[315],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[319],c2[318],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[323],c2[322],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[342],c2[341],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[345],c2[344],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[350],c2[349],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[371],c2[370],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[371],c2[370],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[373],c2[372],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[397],c2[396],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[397],c2[396],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[402],c2[401],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[423],c2[422],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[425],c2[424],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[429],c2[428],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[447],c2[446],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[449],c2[448],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[452],c2[451],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[469],c2[468],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[473],c2[472],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[475],c2[474],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[498],c2[497],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[501],c2[500],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[504],c2[503],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[527],c2[526],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[527],c2[526],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[531],c2[530],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[549],c2[548],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[553],c2[552],7),simde_mm_alignr_epi8(c2[555],c2[554],6)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[52]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],8),simde_mm_alignr_epi8(c2[39],c2[38],12));

//row: 6  [direct, 8 terms]
     d2[78]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[158],c2[157],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[271],c2[270],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],2),simde_mm_xor_si128(c2[341],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[444],c2[443],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[471],c2[470],6),simde_mm_alignr_epi8(c2[526],c2[525],11))))))));

//row: 7  [direct, 6 terms]
     d2[91]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],8),simde_mm_xor_si128(c2[111],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[192],c2[191],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[218],c2[217],15),simde_mm_alignr_epi8(c2[369],c2[368],12))))));

//row: 9  [direct, 8 terms]
     d2[117]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[270],c2[269],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[351],c2[350],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[443],c2[442],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[479],c2[478],4),simde_mm_alignr_epi8(c2[531],c2[530],13))))))));

//row: 10  [direct, 6 terms]
     d2[130]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],12),simde_mm_xor_si128(c2[183],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],12),simde_mm_alignr_epi8(c2[369],c2[368],6))))));

//row: 12  [direct, 6 terms]
     d2[156]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[273],c2[272],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[296],c2[295],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[345],c2[344],8),simde_mm_alignr_epi8(c2[481],c2[480],15))))));

//row: 14  [direct, 6 terms]
     d2[182]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[325],c2[324],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[398],c2[397],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[425],c2[424],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[443],c2[442],1),simde_mm_alignr_epi8(c2[547],c2[546],2))))));

//row: 17  [direct, 5 terms]
     d2[221]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[376],c2[375],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[418],c2[417],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[443],c2[442],3),simde_mm_alignr_epi8(c2[556],c2[555],11)))));

//row: 18  [direct, 5 terms]
     d2[234]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[315],c2[314],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[349],c2[348],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[473],c2[472],4),simde_mm_alignr_epi8(c2[504],c2[503],4)))));

//row: 19  [direct, 5 terms]
     d2[247]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[193],c2[192],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[220],c2[219],8),simde_mm_alignr_epi8(c2[272],c2[271],15)))));

//row: 21  [direct, 5 terms]
     d2[273]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[136],c2[135],6),simde_mm_xor_si128(c2[422],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[523],c2[522],10),simde_mm_alignr_epi8(c2[559],c2[558],7)))));

//row: 22  [direct, 4 terms]
     d2[286]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[316],c2[315],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[347],c2[346],2),simde_mm_alignr_epi8(c2[451],c2[450],3))));

//row: 23  [direct, 4 terms]
     d2[299]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[269],c2[268],4),simde_mm_alignr_epi8(c2[475],c2[474],4))));

//row: 25  [direct, 4 terms]
     d2[325]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],10),simde_mm_xor_si128(c2[183],simde_mm_alignr_epi8(c2[366],c2[365],12))));

//row: 26  [direct, 4 terms]
     d2[338]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],14),simde_mm_xor_si128(c2[116],simde_mm_alignr_epi8(c2[403],c2[402],5))));

//row: 27  [direct, 3 terms]
     d2[351]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[165],c2[164],12),simde_mm_alignr_epi8(c2[210],c2[209],6)));

//row: 28  [direct, 4 terms]
     d2[364]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[497],c2[496],8),simde_mm_alignr_epi8(c2[552],c2[551],13))));

//row: 33  [direct, 4 terms]
     d2[429]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[287],c2[286],6),simde_mm_alignr_epi8(c2[552],c2[551],12))));

//row: 34  [direct, 4 terms]
     d2[442]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[192],c2[191],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[403],c2[402],5),simde_mm_alignr_epi8(c2[452],c2[451],11))));

//row: 36  [direct, 4 terms]
     d2[468]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[377],c2[376],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[402],c2[401],1),simde_mm_alignr_epi8(c2[471],c2[470],11))));

//row: 38  [direct, 4 terms]
     d2[494]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],11),simde_mm_xor_si128(c2[238],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[273],c2[272],6),simde_mm_alignr_epi8(c2[319],c2[318],4))));

//row: 39  [direct, 4 terms]
     d2[507]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[189],c2[188],13),simde_mm_alignr_epi8(c2[506],c2[505],12))));

//row: 40  [direct, 3 terms]
     d2[520]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[214],c2[213],4),simde_mm_alignr_epi8(c2[443],c2[442],12)));

//row: 41  [direct, 4 terms]
     d2[533]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[83],c2[82],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[243],c2[242],14),c2[476])));

//row: 45  [direct, 3 terms]
     d2[585]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[159],c2[158],13),simde_mm_alignr_epi8(c2[270],c2[269],9)));

     if (i2>=7) {
       cm=&csimd[i2-7]; dm=&dsimd[i2-7];
       dm[13]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[26],simde_mm_xor_si128(cm[52],simde_mm_xor_si128(cm[78],simde_mm_xor_si128(cm[130],simde_mm_xor_si128(cm[156],simde_mm_xor_si128(cm[234],simde_mm_xor_si128(cm[260],simde_mm_xor_si128(cm[286],simde_mm_xor_si128(cm[312],simde_mm_xor_si128(cm[338],simde_mm_xor_si128(cm[390],simde_mm_xor_si128(cm[416],simde_mm_xor_si128(cm[468],simde_mm_xor_si128(cm[494],simde_mm_xor_si128(cm[520],cm[546])))))))))))))))));
       dm[26]=simde_mm_xor_si128(dm[13],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[7],dm[6],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],12),simde_mm_xor_si128(cm[104],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],10),simde_mm_xor_si128(cm[182],simde_mm_xor_si128(cm[208],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[235],cm[234],2),simde_mm_xor_si128(cm[287],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[316],cm[315],12),simde_mm_xor_si128(cm[364],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[418],cm[417],14),simde_mm_xor_si128(cm[442],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[505],cm[504],8),simde_mm_alignr_epi8(cm[548],cm[547],15))))))))))))))))));
       dm[39]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[112],cm[111],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[270],cm[269],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[344],cm[343],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[422],cm[421],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[453],cm[452],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[476],cm[475],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[521],cm[520],2),simde_mm_alignr_epi8(cm[555],cm[554],14))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 7 chunk(s), wrapping
  cm=&csimd[6]; dm=&dsimd[6];
  dm[13]=simde_mm_xor_si128(dsimd[6],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[26],simde_mm_xor_si128(cm[52],simde_mm_xor_si128(cm[78],simde_mm_xor_si128(cm[130],simde_mm_xor_si128(cm[156],simde_mm_xor_si128(cm[234],simde_mm_xor_si128(cm[260],simde_mm_xor_si128(cm[286],simde_mm_xor_si128(cm[312],simde_mm_xor_si128(cm[338],simde_mm_xor_si128(cm[390],simde_mm_xor_si128(cm[416],simde_mm_xor_si128(cm[468],simde_mm_xor_si128(cm[494],simde_mm_xor_si128(cm[520],cm[546])))))))))))))))));
  dm[26]=simde_mm_xor_si128(dsimd[19],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[12],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],12),simde_mm_xor_si128(cm[104],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],10),simde_mm_xor_si128(cm[182],simde_mm_xor_si128(cm[208],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[235],cm[234],2),simde_mm_xor_si128(cm[287],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[316],cm[315],12),simde_mm_xor_si128(cm[364],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[418],cm[417],14),simde_mm_xor_si128(cm[442],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[505],cm[504],8),simde_mm_alignr_epi8(cm[548],cm[547],15))))))))))))))))));
  dm[39]=simde_mm_xor_si128(dsimd[6],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[112],cm[111],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[270],cm[269],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[344],cm[343],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[422],cm[421],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[453],cm[452],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[476],cm[475],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[521],cm[520],2),simde_mm_alignr_epi8(cm[555],cm[554],14))))))))))))))))));
  cm=&csimd[7]; dm=&dsimd[7];
  dm[13]=simde_mm_xor_si128(dsimd[7],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[26],simde_mm_xor_si128(cm[52],simde_mm_xor_si128(cm[78],simde_mm_xor_si128(cm[130],simde_mm_xor_si128(cm[156],simde_mm_xor_si128(cm[234],simde_mm_xor_si128(cm[260],simde_mm_xor_si128(cm[286],simde_mm_xor_si128(cm[312],simde_mm_xor_si128(cm[338],simde_mm_xor_si128(cm[390],simde_mm_xor_si128(cm[416],simde_mm_xor_si128(cm[468],simde_mm_xor_si128(cm[494],simde_mm_xor_si128(cm[520],cm[546])))))))))))))))));
  dm[26]=simde_mm_xor_si128(dsimd[20],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[1],dsimd[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],12),simde_mm_xor_si128(cm[104],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],10),simde_mm_xor_si128(cm[182],simde_mm_xor_si128(cm[208],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[235],cm[234],2),simde_mm_xor_si128(cm[287],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[316],cm[315],12),simde_mm_xor_si128(cm[364],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[418],cm[417],14),simde_mm_xor_si128(cm[442],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[505],cm[504],8),simde_mm_alignr_epi8(cm[548],cm[547],15))))))))))))))))));
  dm[39]=simde_mm_xor_si128(dsimd[7],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[112],cm[111],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[270],cm[269],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[344],cm[343],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[422],cm[421],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[453],cm[452],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[476],cm[475],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[521],cm[520],2),simde_mm_alignr_epi8(cm[555],cm[554],14))))))))))))))))));
  cm=&csimd[8]; dm=&dsimd[8];
  dm[13]=simde_mm_xor_si128(dsimd[8],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[26],simde_mm_xor_si128(cm[52],simde_mm_xor_si128(cm[78],simde_mm_xor_si128(cm[130],simde_mm_xor_si128(cm[156],simde_mm_xor_si128(cm[234],simde_mm_xor_si128(cm[260],simde_mm_xor_si128(cm[286],simde_mm_xor_si128(cm[312],simde_mm_xor_si128(cm[338],simde_mm_xor_si128(cm[390],simde_mm_xor_si128(cm[416],simde_mm_xor_si128(cm[468],simde_mm_xor_si128(cm[494],simde_mm_xor_si128(cm[520],cm[546])))))))))))))))));
  dm[26]=simde_mm_xor_si128(dsimd[21],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[2],dsimd[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],12),simde_mm_xor_si128(cm[104],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],10),simde_mm_xor_si128(cm[182],simde_mm_xor_si128(cm[208],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[235],cm[234],2),simde_mm_xor_si128(cm[287],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[316],cm[315],12),simde_mm_xor_si128(cm[364],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[418],cm[417],14),simde_mm_xor_si128(cm[442],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[505],cm[504],8),simde_mm_alignr_epi8(cm[548],cm[547],15))))))))))))))))));
  dm[39]=simde_mm_xor_si128(dsimd[8],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[112],cm[111],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[270],cm[269],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[344],cm[343],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[422],cm[421],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[453],cm[452],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[476],cm[475],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[521],cm[520],2),simde_mm_alignr_epi8(cm[555],cm[554],14))))))))))))))))));
  cm=&csimd[9]; dm=&dsimd[9];
  dm[13]=simde_mm_xor_si128(dsimd[9],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[26],simde_mm_xor_si128(cm[52],simde_mm_xor_si128(cm[78],simde_mm_xor_si128(cm[130],simde_mm_xor_si128(cm[156],simde_mm_xor_si128(cm[234],simde_mm_xor_si128(cm[260],simde_mm_xor_si128(cm[286],simde_mm_xor_si128(cm[312],simde_mm_xor_si128(cm[338],simde_mm_xor_si128(cm[390],simde_mm_xor_si128(cm[416],simde_mm_xor_si128(cm[468],simde_mm_xor_si128(cm[494],simde_mm_xor_si128(cm[520],cm[546])))))))))))))))));
  dm[26]=simde_mm_xor_si128(dsimd[22],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[3],dsimd[2],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],12),simde_mm_xor_si128(cm[104],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],10),simde_mm_xor_si128(cm[182],simde_mm_xor_si128(cm[208],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[235],cm[234],2),simde_mm_xor_si128(cm[287],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[316],cm[315],12),simde_mm_xor_si128(cm[364],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[418],cm[417],14),simde_mm_xor_si128(cm[442],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[505],cm[504],8),simde_mm_alignr_epi8(cm[548],cm[547],15))))))))))))))))));
  dm[39]=simde_mm_xor_si128(dsimd[9],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[112],cm[111],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[270],cm[269],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[344],cm[343],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[422],cm[421],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[453],cm[452],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[476],cm[475],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[521],cm[520],2),simde_mm_alignr_epi8(cm[555],cm[554],14))))))))))))))))));
  cm=&csimd[10]; dm=&dsimd[10];
  dm[13]=simde_mm_xor_si128(dsimd[10],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[26],simde_mm_xor_si128(cm[52],simde_mm_xor_si128(cm[78],simde_mm_xor_si128(cm[130],simde_mm_xor_si128(cm[156],simde_mm_xor_si128(cm[234],simde_mm_xor_si128(cm[260],simde_mm_xor_si128(cm[286],simde_mm_xor_si128(cm[312],simde_mm_xor_si128(cm[338],simde_mm_xor_si128(cm[390],simde_mm_xor_si128(cm[416],simde_mm_xor_si128(cm[468],simde_mm_xor_si128(cm[494],simde_mm_xor_si128(cm[520],cm[546])))))))))))))))));
  dm[26]=simde_mm_xor_si128(dsimd[23],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[4],dsimd[3],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],12),simde_mm_xor_si128(cm[104],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],10),simde_mm_xor_si128(cm[182],simde_mm_xor_si128(cm[208],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[235],cm[234],2),simde_mm_xor_si128(cm[287],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[316],cm[315],12),simde_mm_xor_si128(cm[364],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[418],cm[417],14),simde_mm_xor_si128(cm[442],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[505],cm[504],8),simde_mm_alignr_epi8(cm[548],cm[547],15))))))))))))))))));
  dm[39]=simde_mm_xor_si128(dsimd[10],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[112],cm[111],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[270],cm[269],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[344],cm[343],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[422],cm[421],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[453],cm[452],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[476],cm[475],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[521],cm[520],2),simde_mm_alignr_epi8(cm[555],cm[554],14))))))))))))))))));
  cm=&csimd[11]; dm=&dsimd[11];
  dm[13]=simde_mm_xor_si128(dsimd[11],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[26],simde_mm_xor_si128(cm[52],simde_mm_xor_si128(cm[78],simde_mm_xor_si128(cm[130],simde_mm_xor_si128(cm[156],simde_mm_xor_si128(cm[234],simde_mm_xor_si128(cm[260],simde_mm_xor_si128(cm[286],simde_mm_xor_si128(cm[312],simde_mm_xor_si128(cm[338],simde_mm_xor_si128(cm[390],simde_mm_xor_si128(cm[416],simde_mm_xor_si128(cm[468],simde_mm_xor_si128(cm[494],simde_mm_xor_si128(cm[520],cm[546])))))))))))))))));
  dm[26]=simde_mm_xor_si128(dsimd[24],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[5],dsimd[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],12),simde_mm_xor_si128(cm[104],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],10),simde_mm_xor_si128(cm[182],simde_mm_xor_si128(cm[208],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[235],cm[234],2),simde_mm_xor_si128(cm[287],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[316],cm[315],12),simde_mm_xor_si128(cm[364],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[418],cm[417],14),simde_mm_xor_si128(cm[442],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[505],cm[504],8),simde_mm_alignr_epi8(cm[548],cm[547],15))))))))))))))))));
  dm[39]=simde_mm_xor_si128(dsimd[11],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[112],cm[111],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[270],cm[269],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[344],cm[343],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[422],cm[421],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[453],cm[452],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[476],cm[475],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[521],cm[520],2),simde_mm_alignr_epi8(cm[555],cm[554],14))))))))))))))))));
  cm=&csimd[12]; dm=&dsimd[12];
  dm[13]=simde_mm_xor_si128(dsimd[12],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[26],simde_mm_xor_si128(cm[52],simde_mm_xor_si128(cm[78],simde_mm_xor_si128(cm[130],simde_mm_xor_si128(cm[156],simde_mm_xor_si128(cm[234],simde_mm_xor_si128(cm[260],simde_mm_xor_si128(cm[286],simde_mm_xor_si128(cm[312],simde_mm_xor_si128(cm[338],simde_mm_xor_si128(cm[390],simde_mm_xor_si128(cm[416],simde_mm_xor_si128(cm[468],simde_mm_xor_si128(cm[494],simde_mm_xor_si128(cm[520],cm[546])))))))))))))))));
  dm[26]=simde_mm_xor_si128(dsimd[25],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[6],dsimd[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[53],cm[52],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],12),simde_mm_xor_si128(cm[104],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],10),simde_mm_xor_si128(cm[182],simde_mm_xor_si128(cm[208],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[235],cm[234],2),simde_mm_xor_si128(cm[287],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[316],cm[315],12),simde_mm_xor_si128(cm[364],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[418],cm[417],14),simde_mm_xor_si128(cm[442],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[505],cm[504],8),simde_mm_alignr_epi8(cm[548],cm[547],15))))))))))))))))));
  dm[39]=simde_mm_xor_si128(dsimd[12],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[112],cm[111],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[270],cm[269],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[322],cm[321],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[344],cm[343],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[422],cm[421],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[453],cm[452],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[476],cm[475],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[521],cm[520],2),simde_mm_alignr_epi8(cm[555],cm[554],14))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*416,     d+k*208, 208);
    memcpy(kd+k*416+208, d+k*208, 208);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<13; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<72 + 6 sys]
     d2[65]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+72)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[317],c2[316],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[429],c2[428],15),simde_mm_alignr_epi8(c2[557],c2[556],1)))))));

//row: 8  [P0<<35+P2<<116 + 7 sys]
     d2[104]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+35)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+948)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[321],c2[320],13),simde_mm_xor_si128(c2[422],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[501],c2[500],3),simde_mm_alignr_epi8(c2[553],c2[552],5)))))))));

//row: 11  [P0<<109+P1<<124 + 5 sys]
     d2[143]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+109)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+540)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[320],c2[319],11),simde_mm_xor_si128(c2[417],simde_mm_alignr_epi8(c2[553],c2[552],8)))))));

//row: 13  [P1<<46 + 4 sys]
     d2[169]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+462)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],1),c2[521]))));

//row: 15  [P3<<46 + 5 sys]
     d2[195]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1294)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[266],c2[265],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[350],c2[349],6),simde_mm_alignr_epi8(c2[472],c2[471],5))))));

//row: 16  [P0<<182 + 4 sys]
     d2[208]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+182)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],6),simde_mm_xor_si128(c2[296],simde_mm_alignr_epi8(c2[528],c2[527],10)))));

//row: 20  [P0<<30 + 4 sys]
     d2[260]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+30)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[79],c2[78],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[235],c2[234],15),simde_mm_alignr_epi8(c2[287],c2[286],5)))));

//row: 24  [P0<<6 + 4 sys]
     d2[312]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+6)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[86],c2[85],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[116],c2[115],15),simde_mm_alignr_epi8(c2[288],c2[287],12)))));

//row: 29  [P3<<6 + 3 sys]
     d2[377]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1254)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[373],c2[372],8),simde_mm_alignr_epi8(c2[475],c2[474],10))));

//row: 30  [P2<<20 + 3 sys]
     d2[390]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+852)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[266],c2[265],8),c2[345])));

//row: 31  [P0<<125+P3<<194 + 2 sys]
     d2[403]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+125)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1442)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],4),simde_mm_alignr_epi8(c2[186],c2[185],1))));

//row: 32  [P2<<20 + 3 sys]
     d2[416]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+852)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[320],c2[319],14),simde_mm_alignr_epi8(c2[368],c2[367],15))));

//row: 35  [P0<<185 + 3 sys]
     d2[455]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+185)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[159],c2[158],13),simde_mm_alignr_epi8(c2[323],c2[322],8))));

//row: 37  [P1<<135 + 2 sys]
     d2[481]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+551)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],2),simde_mm_alignr_epi8(c2[339],c2[338],2)));

//row: 42  [P2<<162 + 2 sys]
     d2[546]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+994)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],3),simde_mm_alignr_epi8(c2[105],c2[104],10)));

//row: 43  [P3<<98 + 3 sys]
     d2[559]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1346)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[427],c2[426],3),simde_mm_alignr_epi8(c2[475],c2[474],3))));

//row: 44  [P0<<3 + 3 sys]
     d2[572]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+3)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],6),simde_mm_alignr_epi8(c2[243],c2[242],14))));
  }
}
