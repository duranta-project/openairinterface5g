#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=320, byte encoding, factored
// 319 terms vs 2101 expanded (6.59x); pipeline lag 1 chunk(s)
static inline void ldpc320_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[2560] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<20; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],15),simde_mm_xor_si128(c2[53],simde_mm_xor_si128(c2[85],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[173],c2[172],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[177],c2[176],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[178],c2[177],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[211],c2[210],1),simde_mm_xor_si128(c2[215],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[243],c2[242],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[244],c2[243],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[255],c2[254],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[289],c2[288],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[291],c2[290],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[293],c2[292],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[321],c2[320],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[325],c2[324],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[333],c2[332],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[361],c2[360],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[365],c2[364],7),simde_mm_xor_si128(c2[365],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[407],c2[406],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[411],c2[410],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[418],c2[417],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[449],c2[448],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[454],c2[453],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[459],c2[458],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[491],c2[490],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[496],c2[495],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[499],c2[498],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[529],c2[528],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[531],c2[530],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[539],c2[538],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[564],c2[563],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[565],c2[564],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[578],c2[577],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[617],c2[616],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[619],c2[618],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[619],c2[618],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[643],c2[642],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[645],c2[644],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[647],c2[646],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[682],c2[681],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[686],c2[685],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[692],c2[691],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[725],c2[724],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[728],c2[727],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[736],c2[735],6),simde_mm_xor_si128(c2[761],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[775],c2[774],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[777],c2[776],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[812],c2[811],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[818],c2[817],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[820],c2[819],15),simde_mm_xor_si128(c2[842],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[845],c2[844],4),simde_mm_alignr_epi8(c2[859],c2[858],13)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[80]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],9),simde_mm_alignr_epi8(c2[53],c2[52],13));

//row: 6  [direct, 8 terms]
     d2[120]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[419],c2[418],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[441],c2[440],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[535],c2[534],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[699],c2[698],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[729],c2[728],10),simde_mm_alignr_epi8(c2[815],c2[814],11))))))));

//row: 7  [direct, 6 terms]
     d2[140]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[173],c2[172],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[284],c2[283],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[322],c2[321],9),simde_mm_alignr_epi8(c2[565],c2[564],12))))));

//row: 9  [direct, 8 terms]
     d2[180]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[403],c2[402],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[458],c2[457],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[530],c2[529],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[697],c2[696],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[729],c2[728],7),simde_mm_alignr_epi8(c2[814],c2[813],1))))))));

//row: 10  [direct, 6 terms]
     d2[200]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],14),simde_mm_xor_si128(c2[85],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[174],c2[173],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[285],c2[284],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[331],c2[330],1),simde_mm_alignr_epi8(c2[580],c2[579],7))))));

//row: 12  [direct, 6 terms]
     d2[240]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[411],c2[410],9),simde_mm_xor_si128(c2[443],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[527],c2[526],9),simde_mm_alignr_epi8(c2[724],c2[723],4))))));

//row: 14  [direct, 6 terms]
     d2[280]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[499],c2[498],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[604],c2[603],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[644],c2[643],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[692],c2[691],3),simde_mm_alignr_epi8(c2[857],c2[856],2))))));

//row: 17  [direct, 5 terms]
     d2[340]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[570],c2[569],3),simde_mm_xor_si128(c2[648],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[684],c2[683],3),simde_mm_alignr_epi8(c2[855],c2[854],4)))));

//row: 18  [direct, 5 terms]
     d2[360]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[499],c2[498],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[539],c2[538],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[729],c2[728],13),simde_mm_alignr_epi8(c2[779],c2[778],7)))));

//row: 19  [direct, 5 terms]
     d2[380]=simde_mm_xor_si128(c2[4],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[287],c2[286],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[337],c2[336],14),simde_mm_alignr_epi8(c2[403],c2[402],9)))));

//row: 21  [direct, 5 terms]
     d2[420]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[211],c2[210],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[649],c2[648],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[818],c2[817],11),simde_mm_alignr_epi8(c2[847],c2[846],7)))));

//row: 22  [direct, 4 terms]
     d2[440]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[482],c2[481],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[524],c2[523],7),simde_mm_alignr_epi8(c2[700],c2[699],12))));

//row: 23  [direct, 4 terms]
     d2[460]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[84],c2[83],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[409],c2[408],5),simde_mm_alignr_epi8(c2[727],c2[726],9))));

//row: 25  [direct, 4 terms]
     d2[500]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[259],c2[258],7),simde_mm_xor_si128(c2[286],simde_mm_alignr_epi8(c2[563],c2[562],14))));

//row: 26  [direct, 4 terms]
     d2[520]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[180],c2[179],14),simde_mm_alignr_epi8(c2[605],c2[604],3))));

//row: 27  [direct, 3 terms]
     d2[540]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],13),c2[339]));

//row: 28  [direct, 4 terms]
     d2[560]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[179],c2[178],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[764],c2[763],2),simde_mm_alignr_epi8(c2[855],c2[854],10))));

//row: 33  [direct, 4 terms]
     d2[660]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[449],c2[448],12),simde_mm_alignr_epi8(c2[843],c2[842],1))));

//row: 34  [direct, 4 terms]
     d2[680]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[299],c2[298],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[601],c2[600],5),simde_mm_alignr_epi8(c2[683],c2[682],12))));

//row: 36  [direct, 4 terms]
     d2[720]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[572],c2[571],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[612],c2[611],2),c2[730])));

//row: 38  [direct, 4 terms]
     d2[760]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[372],c2[371],3),simde_mm_xor_si128(c2[404],simde_mm_alignr_epi8(c2[492],c2[491],5))));

//row: 39  [direct, 4 terms]
     d2[780]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[125],c2[124],13),simde_mm_xor_si128(c2[292],c2[773])));

//row: 40  [direct, 3 terms]
     d2[800]=simde_mm_xor_si128(c2[2],simde_mm_xor_si128(c2[325],simde_mm_alignr_epi8(c2[693],c2[692],5)));

//row: 41  [direct, 4 terms]
     d2[820]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[368],c2[367],12),simde_mm_alignr_epi8(c2[733],c2[732],15))));

//row: 45  [direct, 3 terms]
     d2[900]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[255],c2[254],4),simde_mm_alignr_epi8(c2[408],c2[407],14)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[20]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[124],cm[123],1),simde_mm_xor_si128(cm[215],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[243],cm[242],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[361],cm[360],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[411],cm[410],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[454],cm[453],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[491],cm[490],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[529],cm[528],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[619],cm[618],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[647],cm[646],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[728],cm[727],1),simde_mm_xor_si128(cm[761],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[812],cm[811],13),cm[842])))))))))))))))));
       dm[40]=simde_mm_xor_si128(dm[20],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[19],cm[18],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[122],cm[121],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[177],cm[176],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[211],cm[210],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[289],cm[288],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[321],cm[320],4),simde_mm_xor_si128(cm[365],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[449],cm[448],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[499],cm[498],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[565],cm[564],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[617],cm[616],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[645],cm[644],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[686],cm[685],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[777],cm[776],4),simde_mm_alignr_epi8(cm[859],cm[858],13))))))))))))))))));
       dm[60]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],12),simde_mm_xor_si128(cm[53],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[122],cm[121],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[173],cm[172],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[244],cm[243],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[291],cm[290],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[325],cm[324],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[418],cm[417],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[459],cm[458],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[496],cm[495],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[531],cm[530],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[564],cm[563],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[643],cm[642],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[682],cm[681],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[725],cm[724],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[820],cm[819],15),simde_mm_alignr_epi8(cm[845],cm[844],4))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[19]; dm=&dsimd[19];
  dm[20]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[19],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[124],cm[123],1),simde_mm_xor_si128(cm[215],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[243],cm[242],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[361],cm[360],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[411],cm[410],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[454],cm[453],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[491],cm[490],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[529],cm[528],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[619],cm[618],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[647],cm[646],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[728],cm[727],1),simde_mm_xor_si128(cm[761],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[812],cm[811],13),cm[842])))))))))))))))));
  dm[40]=simde_mm_xor_si128(dsimd[39],simde_mm_xor_si128(dsimd[19],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[19],cm[18],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[122],cm[121],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[177],cm[176],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[211],cm[210],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[289],cm[288],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[321],cm[320],4),simde_mm_xor_si128(cm[365],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[449],cm[448],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[499],cm[498],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[565],cm[564],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[617],cm[616],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[645],cm[644],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[686],cm[685],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[777],cm[776],4),simde_mm_alignr_epi8(cm[859],cm[858],13))))))))))))))))));
  dm[60]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[19],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],12),simde_mm_xor_si128(cm[53],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[122],cm[121],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[173],cm[172],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[244],cm[243],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[291],cm[290],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[325],cm[324],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[418],cm[417],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[459],cm[458],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[496],cm[495],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[531],cm[530],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[564],cm[563],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[643],cm[642],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[682],cm[681],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[725],cm[724],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[820],cm[819],15),simde_mm_alignr_epi8(cm[845],cm[844],4))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*640,     d+k*320, 320);
    memcpy(kd+k*640+320, d+k*320, 320);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<20; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<279 + 6 sys]
     d2[100]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+279)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[500],c2[499],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[653],c2[652],9),simde_mm_alignr_epi8(c2[857],c2[856],11)))))));

//row: 8  [P0<<245+P2<<154 + 7 sys]
     d2[160]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+245)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1434)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[129],c2[128],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[495],c2[494],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[659],c2[658],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[767],c2[766],14),simde_mm_alignr_epi8(c2[857],c2[856],13)))))))));

//row: 11  [P0<<43+P1<<280 + 5 sys]
     d2[220]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+43)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+920)),simde_mm_xor_si128(c2[1],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[499],c2[498],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[659],c2[658],1),simde_mm_alignr_epi8(c2[852],c2[851],1)))))));

//row: 13  [P1<<37 + 4 sys]
     d2[260]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+677)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[139],c2[138],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[299],c2[298],15),c2[810]))));

//row: 15  [P3<<51 + 5 sys]
     d2[300]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1971)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[404],c2[403],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[529],c2[528],2),simde_mm_alignr_epi8(c2[732],c2[731],8))))));

//row: 16  [P0<<300 + 4 sys]
     d2[320]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+300)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[129],c2[128],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[443],c2[442],13),simde_mm_alignr_epi8(c2[808],c2[807],3)))));

//row: 20  [P0<<10 + 4 sys]
     d2[400]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+10)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[363],c2[362],8),simde_mm_alignr_epi8(c2[449],c2[448],2)))));

//row: 24  [P0<<281 + 4 sys]
     d2[480]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+281)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[138],c2[137],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[167],c2[166],14),simde_mm_alignr_epi8(c2[452],c2[451],11)))));

//row: 29  [P3<<29 + 3 sys]
     d2[580]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1949)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[580],c2[579],4),simde_mm_alignr_epi8(c2[728],c2[727],5))));

//row: 30  [P2<<135 + 3 sys]
     d2[600]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1415)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[402],c2[401],7),simde_mm_alignr_epi8(c2[527],c2[526],9))));

//row: 31  [P0<<66+P3<<162 + 2 sys]
     d2[620]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+66)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2082)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],14),simde_mm_alignr_epi8(c2[300],c2[299],4))));

//row: 32  [P2<<217 + 3 sys]
     d2[640]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1497)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[482],c2[481],6),simde_mm_alignr_epi8(c2[577],c2[576],15))));

//row: 35  [P0<<285 + 3 sys]
     d2[700]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+285)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[250],c2[249],14),simde_mm_alignr_epi8(c2[484],c2[483],7))));

//row: 37  [P1<<115 + 2 sys]
     d2[740]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+755)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],10),simde_mm_alignr_epi8(c2[521],c2[520],15)));

//row: 42  [P2<<126 + 2 sys]
     d2[840]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1406)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],2),simde_mm_alignr_epi8(c2[165],c2[164],1)));

//row: 43  [P3<<102 + 3 sys]
     d2[860]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2022)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[645],c2[644],5),c2[731])));

//row: 44  [P0<<260 + 3 sys]
     d2[880]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+260)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[295],c2[294],3),simde_mm_alignr_epi8(c2[377],c2[376],3))));
  }
}
