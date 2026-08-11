#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=288, byte encoding, factored
// 319 terms vs 2105 expanded (6.60x); pipeline lag 1 chunk(s)
static inline void ldpc288_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[2304] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<18; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[83],c2[82],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[84],c2[83],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[86],c2[85],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[119],c2[118],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[120],c2[119],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[151],c2[150],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[153],c2[152],5),simde_mm_xor_si128(c2[160],simde_mm_xor_si128(c2[190],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[194],c2[193],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[196],c2[195],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[217],c2[216],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[222],c2[221],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[229],c2[228],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[257],c2[256],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[261],c2[260],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[266],c2[265],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[292],c2[291],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[301],c2[300],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[301],c2[300],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[326],c2[325],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[332],c2[331],5),simde_mm_xor_si128(c2[324],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[361],c2[360],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[363],c2[362],9),simde_mm_xor_si128(c2[369],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[403],c2[402],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[404],c2[403],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[407],c2[406],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[433],c2[432],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[436],c2[435],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[446],c2[445],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[471],c2[470],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[473],c2[472],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[476],c2[475],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[509],c2[508],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[512],c2[511],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[519],c2[518],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[550],c2[549],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[555],c2[554],9),simde_mm_xor_si128(c2[540],simde_mm_xor_si128(c2[585],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[586],c2[585],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[592],c2[591],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[622],c2[621],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[623],c2[622],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[626],c2[625],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[654],c2[653],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[665],c2[664],13),simde_mm_xor_si128(c2[648],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[694],c2[693],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[697],c2[696],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[698],c2[697],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[725],c2[724],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[728],c2[727],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[735],c2[734],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[763],c2[762],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[764],c2[763],7),simde_mm_alignr_epi8(c2[773],c2[772],5)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],6),simde_mm_alignr_epi8(c2[51],c2[50],11));

//row: 6  [direct, 8 terms]
     d2[108]=simde_mm_xor_si128(c2[5],simde_mm_xor_si128(c2[225],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[371],c2[370],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[402],c2[401],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[472],c2[471],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[624],c2[623],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[658],c2[657],7),simde_mm_alignr_epi8(c2[727],c2[726],12))))))));

//row: 7  [direct, 6 terms]
     d2[126]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[154],c2[153],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[264],c2[263],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[295],c2[294],8),simde_mm_alignr_epi8(c2[515],c2[514],4))))));

//row: 9  [direct, 8 terms]
     d2[162]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[366],c2[365],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[405],c2[404],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[471],c2[470],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[621],c2[620],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[653],c2[652],12),simde_mm_alignr_epi8(c2[734],c2[733],1))))))));

//row: 10  [direct, 6 terms]
     d2[180]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[269],c2[268],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[303],c2[302],10),simde_mm_alignr_epi8(c2[517],c2[516],9))))));

//row: 12  [direct, 6 terms]
     d2[216]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[376],c2[375],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[406],c2[405],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[483],c2[482],14),simde_mm_alignr_epi8(c2[664],c2[663],3))))));

//row: 14  [direct, 6 terms]
     d2[252]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[444],c2[443],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[556],c2[555],13),simde_mm_xor_si128(c2[577],simde_mm_xor_si128(c2[612],simde_mm_alignr_epi8(c2[761],c2[760],15))))));

//row: 17  [direct, 5 terms]
     d2[306]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[513],c2[512],9),simde_mm_xor_si128(c2[576],simde_mm_xor_si128(c2[612],simde_mm_alignr_epi8(c2[767],c2[766],2)))));

//row: 18  [direct, 5 terms]
     d2[324]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[442],c2[441],7),simde_mm_xor_si128(c2[468],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[664],c2[663],1),c2[693]))));

//row: 19  [direct, 5 terms]
     d2[342]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(c2[36],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[260],c2[259],6),simde_mm_xor_si128(c2[297],c2[360]))));

//row: 21  [direct, 5 terms]
     d2[378]=simde_mm_xor_si128(c2[40],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[593],c2[592],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[721],c2[720],9),simde_mm_alignr_epi8(c2[758],c2[757],2)))));

//row: 22  [direct, 4 terms]
     d2[396]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[444],c2[443],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[473],c2[472],8),simde_mm_alignr_epi8(c2[629],c2[628],1))));

//row: 23  [direct, 4 terms]
     d2[414]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],4),simde_mm_xor_si128(c2[72],simde_mm_xor_si128(c2[360],simde_mm_alignr_epi8(c2[659],c2[658],5))));

//row: 25  [direct, 4 terms]
     d2[450]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],13),simde_mm_xor_si128(c2[216],simde_mm_xor_si128(c2[252],simde_mm_alignr_epi8(c2[516],c2[515],7))));

//row: 26  [direct, 4 terms]
     d2[468]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(c2[72],simde_mm_xor_si128(c2[144],simde_mm_alignr_epi8(c2[558],c2[557],5))));

//row: 27  [direct, 3 terms]
     d2[486]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[219],c2[218],4),simde_mm_alignr_epi8(c2[293],c2[292],8)));

//row: 28  [direct, 4 terms]
     d2[504]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],3),simde_mm_xor_si128(c2[144],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[694],c2[693],11),simde_mm_alignr_epi8(c2[760],c2[759],14))));

//row: 33  [direct, 4 terms]
     d2[594]=simde_mm_xor_si128(c2[36],simde_mm_xor_si128(c2[72],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[401],c2[400],12),simde_mm_alignr_epi8(c2[758],c2[757],2))));

//row: 34  [direct, 4 terms]
     d2[612]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],5),simde_mm_xor_si128(c2[252],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[547],c2[546],12),c2[612])));

//row: 36  [direct, 4 terms]
     d2[648]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],8),simde_mm_xor_si128(c2[505],simde_mm_xor_si128(c2[540],c2[648])));

//row: 38  [direct, 4 terms]
     d2[684]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],14),simde_mm_xor_si128(c2[324],simde_mm_xor_si128(c2[360],c2[432])));

//row: 39  [direct, 4 terms]
     d2[702]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],9),simde_mm_xor_si128(c2[108],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[263],c2[262],5),simde_mm_alignr_epi8(c2[692],c2[691],5))));

//row: 40  [direct, 3 terms]
     d2[720]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],8),simde_mm_xor_si128(c2[297],simde_mm_alignr_epi8(c2[613],c2[612],2)));

//row: 41  [direct, 4 terms]
     d2[738]=simde_mm_xor_si128(c2[36],simde_mm_xor_si128(c2[108],simde_mm_xor_si128(c2[324],simde_mm_alignr_epi8(c2[660],c2[659],7))));

//row: 45  [direct, 3 terms]
     d2[810]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],8),simde_mm_xor_si128(c2[216],c2[369]));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[18]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[84],cm[83],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[120],cm[119],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[194],cm[193],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[217],cm[216],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[326],cm[325],13),simde_mm_xor_si128(cm[369],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[404],cm[403],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[446],cm[445],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[476],cm[475],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[555],cm[554],9),simde_mm_xor_si128(cm[585],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[654],cm[653],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[698],cm[697],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[725],cm[724],9),simde_mm_alignr_epi8(cm[773],cm[772],5))))))))))))))))));
       dm[36]=simde_mm_xor_si128(dm[18],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[12],cm[11],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[83],cm[82],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[122],cm[121],15),simde_mm_xor_si128(cm[160],simde_mm_xor_si128(cm[190],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[257],cm[256],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[301],cm[300],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[332],cm[331],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[403],cm[402],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[433],cm[432],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[509],cm[508],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[550],cm[549],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[586],cm[585],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[622],cm[621],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[694],cm[693],12),simde_mm_alignr_epi8(cm[764],cm[763],7))))))))))))))))));
       dm[54]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[12],cm[11],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[46],cm[45],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[119],cm[118],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[151],cm[150],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[222],cm[221],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[261],cm[260],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[301],cm[300],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[363],cm[362],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[407],cm[406],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[436],cm[435],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[471],cm[470],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[512],cm[511],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[592],cm[591],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[623],cm[622],5),simde_mm_xor_si128(cm[648],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[728],cm[727],1),simde_mm_alignr_epi8(cm[763],cm[762],12))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[17]; dm=&dsimd[17];
  dm[18]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[17],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[84],cm[83],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[120],cm[119],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[194],cm[193],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[217],cm[216],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[326],cm[325],13),simde_mm_xor_si128(cm[369],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[404],cm[403],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[446],cm[445],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[476],cm[475],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[555],cm[554],9),simde_mm_xor_si128(cm[585],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[654],cm[653],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[698],cm[697],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[725],cm[724],9),simde_mm_alignr_epi8(cm[773],cm[772],5))))))))))))))))));
  dm[36]=simde_mm_xor_si128(dsimd[35],simde_mm_xor_si128(dsimd[17],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[12],cm[11],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[83],cm[82],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[122],cm[121],15),simde_mm_xor_si128(cm[160],simde_mm_xor_si128(cm[190],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[257],cm[256],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[301],cm[300],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[332],cm[331],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[403],cm[402],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[433],cm[432],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[509],cm[508],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[550],cm[549],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[586],cm[585],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[622],cm[621],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[694],cm[693],12),simde_mm_alignr_epi8(cm[764],cm[763],7))))))))))))))))));
  dm[54]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[17],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[12],cm[11],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[46],cm[45],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[119],cm[118],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[151],cm[150],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[222],cm[221],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[261],cm[260],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[301],cm[300],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[363],cm[362],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[407],cm[406],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[436],cm[435],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[471],cm[470],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[512],cm[511],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[592],cm[591],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[623],cm[622],5),simde_mm_xor_si128(cm[648],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[728],cm[727],1),simde_mm_alignr_epi8(cm[763],cm[762],12))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*576,     d+k*288, 288);
    memcpy(kd+k*576+288, d+k*288, 288);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<18; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<144 + 6 sys]
     d2[90]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+144)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[113],c2[112],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[450],c2[449],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[592],c2[591],14),simde_mm_alignr_epi8(c2[761],c2[760],15)))))));

//row: 8  [P0<<230+P2<<54 + 7 sys]
     d2[144]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+230)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1206)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],6),simde_mm_xor_si128(c2[36],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[435],c2[434],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[583],c2[582],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[687],c2[686],14),simde_mm_alignr_epi8(c2[757],c2[756],15)))))))));

//row: 11  [P0<<39+P1<<26 + 5 sys]
     d2[198]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+39)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+602)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[450],c2[449],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[588],c2[587],5),simde_mm_alignr_epi8(c2[774],c2[773],1)))))));

//row: 13  [P1<<36 + 4 sys]
     d2[234]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+612)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],7),simde_mm_xor_si128(c2[108],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[266],c2[265],8),simde_mm_alignr_epi8(c2[723],c2[722],15)))));

//row: 15  [P3<<64 + 5 sys]
     d2[270]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1792)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],10),simde_mm_xor_si128(c2[36],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[372],c2[371],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[475],c2[474],12),simde_mm_alignr_epi8(c2[653],c2[652],4))))));

//row: 16  [P0<<0 + 4 sys]
     d2[288]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+0)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[109],c2[108],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[403],c2[402],3),simde_mm_alignr_epi8(c2[724],c2[723],6)))));

//row: 20  [P0<<228 + 4 sys]
     d2[360]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+228)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[330],c2[329],10),c2[405]))));

//row: 24  [P0<<0 + 4 sys]
     d2[432]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+0)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],7),simde_mm_xor_si128(c2[144],simde_mm_alignr_epi8(c2[413],c2[412],10)))));

//row: 29  [P3<<42 + 3 sys]
     d2[522]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1770)),simde_mm_xor_si128(c2[36],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[516],c2[515],4),c2[648])));

//row: 30  [P2<<173 + 3 sys]
     d2[540]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1325)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[366],c2[365],10),simde_mm_alignr_epi8(c2[484],c2[483],12))));

//row: 31  [P0<<166+P3<<19 + 2 sys]
     d2[558]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+166)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1747)),simde_mm_xor_si128(c2[45],c2[261])));

//row: 32  [P2<<162 + 3 sys]
     d2[576]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1314)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[446],c2[445],3),simde_mm_alignr_epi8(c2[507],c2[506],4))));

//row: 35  [P0<<205 + 3 sys]
     d2[630]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+205)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[234],c2[233],6),c2[432])));

//row: 37  [P1<<0 + 2 sys]
     d2[666]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+576)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],8),c2[477]));

//row: 42  [P2<<35 + 2 sys]
     d2[756]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1187)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],11),c2[144]));

//row: 43  [P3<<270 + 3 sys]
     d2[774]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1998)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[592],c2[591],3),c2[648])));

//row: 44  [P0<<57 + 3 sys]
     d2[792]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+57)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_xor_si128(c2[252],c2[324])));
  }
}
