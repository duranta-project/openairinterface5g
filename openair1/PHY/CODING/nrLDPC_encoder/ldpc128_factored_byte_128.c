#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=128, byte encoding, factored
// 319 terms vs 2087 expanded (6.54x); pipeline lag 1 chunk(s)
static inline void ldpc128_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1024] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<8; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[72],c2[71],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[97],c2[96],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[113],c2[112],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[118],c2[117],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[119],c2[118],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[135],c2[134],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[136],c2[135],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[147],c2[146],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[150],c2[149],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[163],c2[162],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[167],c2[166],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[182],c2[181],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[182],c2[181],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[196],c2[195],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[199],c2[198],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[200],c2[199],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[214],c2[213],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[230],c2[229],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[231],c2[230],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[231],c2[230],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[241],c2[240],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[245],c2[244],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[247],c2[246],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[257],c2[256],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[258],c2[257],7),simde_mm_xor_si128(c2[263],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[274],c2[273],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[277],c2[276],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[280],c2[279],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[292],c2[291],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[293],c2[292],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[296],c2[295],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[307],c2[306],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[312],c2[311],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[312],c2[311],15),simde_mm_xor_si128(c2[321],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[327],c2[326],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[328],c2[327],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[337],c2[336],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[338],c2[337],12),simde_mm_alignr_epi8(c2[338],c2[337],15)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[32]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_alignr_epi8(c2[23],c2[22],6));

//row: 6  [direct, 8 terms]
     d2[48]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[162],c2[161],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[181],c2[180],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[216],c2[215],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[273],c2[272],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],13),simde_mm_alignr_epi8(c2[326],c2[325],3))))))));

//row: 7  [direct, 6 terms]
     d2[56]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],7),simde_mm_alignr_epi8(c2[231],c2[230],8))))));

//row: 9  [direct, 8 terms]
     d2[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[167],c2[166],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[178],c2[177],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[273],c2[272],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[292],c2[291],13),simde_mm_alignr_epi8(c2[326],c2[325],8))))))));

//row: 10  [direct, 6 terms]
     d2[80]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],7),simde_mm_xor_si128(c2[114],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],1),simde_mm_alignr_epi8(c2[228],c2[227],10))))));

//row: 12  [direct, 6 terms]
     d2[96]=simde_mm_xor_si128(c2[2],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[162],c2[161],5),simde_mm_xor_si128(c2[178],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[215],c2[214],10),simde_mm_alignr_epi8(c2[289],c2[288],7))))));

//row: 14  [direct, 6 terms]
     d2[112]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[196],c2[195],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[245],c2[244],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[264],c2[263],15),simde_mm_xor_si128(c2[273],simde_mm_alignr_epi8(c2[343],c2[342],5))))));

//row: 17  [direct, 5 terms]
     d2[136]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[227],c2[226],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[260],c2[259],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[273],c2[272],1),c2[337]))));

//row: 18  [direct, 5 terms]
     d2[144]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[199],c2[198],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],11),simde_mm_alignr_epi8(c2[306],c2[305],3)))));

//row: 19  [direct, 5 terms]
     d2[152]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[117],c2[116],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[136],c2[135],15),c2[166]))));

//row: 21  [direct, 5 terms]
     d2[168]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[263],c2[262],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[321],c2[320],3),simde_mm_alignr_epi8(c2[339],c2[338],11)))));

//row: 22  [direct, 4 terms]
     d2[176]=simde_mm_xor_si128(c2[4],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[193],c2[192],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[212],c2[211],12),simde_mm_alignr_epi8(c2[274],c2[273],14))));

//row: 23  [direct, 4 terms]
     d2[184]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[163],c2[162],10),simde_mm_alignr_epi8(c2[290],c2[289],8))));

//row: 25  [direct, 4 terms]
     d2[200]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[97],c2[96],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[120],c2[119],4),simde_mm_alignr_epi8(c2[228],c2[227],6))));

//row: 26  [direct, 4 terms]
     d2[208]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[70],c2[69],7),simde_mm_alignr_epi8(c2[244],c2[243],13))));

//row: 27  [direct, 3 terms]
     d2[216]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],8),simde_mm_alignr_epi8(c2[133],c2[132],2)));

//row: 28  [direct, 4 terms]
     d2[224]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[308],c2[307],5),simde_mm_alignr_epi8(c2[340],c2[339],15))));

//row: 33  [direct, 4 terms]
     d2[264]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[179],c2[178],9),simde_mm_alignr_epi8(c2[342],c2[341],3))));

//row: 34  [direct, 4 terms]
     d2[272]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[243],c2[242],4),simde_mm_alignr_epi8(c2[274],c2[273],15))));

//row: 36  [direct, 4 terms]
     d2[288]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[231],c2[230],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[244],c2[243],3),simde_mm_alignr_epi8(c2[296],c2[295],8))));

//row: 38  [direct, 4 terms]
     d2[304]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[162],c2[161],13),simde_mm_alignr_epi8(c2[195],c2[194],3))));

//row: 39  [direct, 4 terms]
     d2[312]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],5),c2[304])));

//row: 40  [direct, 3 terms]
     d2[320]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[129],c2[128],9),simde_mm_alignr_epi8(c2[274],c2[273],5)));

//row: 41  [direct, 4 terms]
     d2[328]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],11),simde_mm_alignr_epi8(c2[290],c2[289],7))));

//row: 45  [direct, 3 terms]
     d2[360]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],7),simde_mm_alignr_epi8(c2[163],c2[162],7)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[8]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[39],cm[38],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[97],cm[96],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[167],cm[166],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[183],cm[182],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[196],cm[195],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[245],cm[244],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[258],cm[257],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[307],cm[306],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[327],cm[326],15),simde_mm_alignr_epi8(cm[338],cm[337],15))))))))))))))))));
       dm[16]=simde_mm_xor_si128(dm[8],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[39],cm[38],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[56],cm[55],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[72],cm[71],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[85],cm[84],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[118],cm[117],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[135],cm[134],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[147],cm[146],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[199],cm[198],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[231],cm[230],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[241],cm[240],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[257],cm[256],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[274],cm[273],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[312],cm[311],15),simde_mm_alignr_epi8(cm[338],cm[337],12))))))))))))))))));
       dm[24]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[22],cm[21],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[54],cm[53],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[66],cm[65],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[98],cm[97],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[113],cm[112],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[136],cm[135],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[161],cm[160],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[200],cm[199],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[214],cm[213],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[230],cm[229],3),simde_mm_xor_si128(cm[263],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[277],cm[276],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[296],cm[295],4),simde_mm_xor_si128(cm[321],simde_mm_alignr_epi8(cm[337],cm[336],12))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[7]; dm=&dsimd[7];
  dm[8]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[7],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[39],cm[38],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[97],cm[96],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[167],cm[166],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[183],cm[182],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[196],cm[195],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[245],cm[244],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[258],cm[257],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[307],cm[306],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[327],cm[326],15),simde_mm_alignr_epi8(cm[338],cm[337],15))))))))))))))))));
  dm[16]=simde_mm_xor_si128(dsimd[15],simde_mm_xor_si128(dsimd[7],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[39],cm[38],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[56],cm[55],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[72],cm[71],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[85],cm[84],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[118],cm[117],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[135],cm[134],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[147],cm[146],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[199],cm[198],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[231],cm[230],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[241],cm[240],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[257],cm[256],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[274],cm[273],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[312],cm[311],15),simde_mm_alignr_epi8(cm[338],cm[337],12))))))))))))))))));
  dm[24]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[7],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[22],cm[21],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[54],cm[53],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[66],cm[65],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[98],cm[97],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[113],cm[112],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[136],cm[135],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[161],cm[160],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[200],cm[199],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[214],cm[213],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[230],cm[229],3),simde_mm_xor_si128(cm[263],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[277],cm[276],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[296],cm[295],4),simde_mm_xor_si128(cm[321],simde_mm_alignr_epi8(cm[337],cm[336],12))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*256,     d+k*128, 128);
    memcpy(kd+k*256+128, d+k*128, 128);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<8; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<115 + 6 sys]
     d2[40]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+115)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[199],c2[198],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[258],c2[257],12),simde_mm_alignr_epi8(c2[344],c2[343],11)))))));

//row: 8  [P0<<113+P2<<90 + 7 sys]
     d2[64]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+113)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+602)),simde_mm_xor_si128(c2[7],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[198],c2[197],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[263],c2[262],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[307],c2[306],4),simde_mm_alignr_epi8(c2[343],c2[342],13)))))))));

//row: 11  [P0<<124+P1<<22 + 5 sys]
     d2[88]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+124)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+278)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[198],c2[197],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[260],c2[259],6),simde_mm_alignr_epi8(c2[341],c2[340],14)))))));

//row: 13  [P1<<62 + 4 sys]
     d2[104]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+318)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],7),simde_mm_alignr_epi8(c2[324],c2[323],9)))));

//row: 15  [P3<<51 + 5 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+819)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],8),simde_mm_xor_si128(c2[22],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[165],c2[164],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[212],c2[211],15),simde_mm_alignr_epi8(c2[293],c2[292],11))))));

//row: 16  [P0<<26 + 4 sys]
     d2[128]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+26)),simde_mm_xor_si128(c2[20],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[180],c2[179],1),simde_mm_alignr_epi8(c2[324],c2[323],3)))));

//row: 20  [P0<<32 + 4 sys]
     d2[160]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+32)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[150],c2[149],9),simde_mm_alignr_epi8(c2[179],c2[178],15)))));

//row: 24  [P0<<94 + 4 sys]
     d2[192]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+94)),simde_mm_xor_si128(c2[7],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],12),simde_mm_alignr_epi8(c2[184],c2[183],4)))));

//row: 29  [P3<<6 + 3 sys]
     d2[232]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+774)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[231],c2[230],12),simde_mm_alignr_epi8(c2[294],c2[293],4))));

//row: 30  [P2<<9 + 3 sys]
     d2[240]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+521)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[165],c2[164],9),simde_mm_alignr_epi8(c2[216],c2[215],8))));

//row: 31  [P0<<44+P3<<61 + 2 sys]
     d2[248]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+44)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+829)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],15),simde_mm_alignr_epi8(c2[116],c2[115],1))));

//row: 32  [P2<<121 + 3 sys]
     d2[256]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+633)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],13),simde_mm_xor_si128(c2[199],simde_mm_alignr_epi8(c2[229],c2[228],7))));

//row: 35  [P0<<103 + 3 sys]
     d2[280]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+103)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],5),simde_mm_alignr_epi8(c2[197],c2[196],15))));

//row: 37  [P1<<122 + 2 sys]
     d2[296]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+378)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],6),simde_mm_alignr_epi8(c2[214],c2[213],12)));

//row: 42  [P2<<9 + 2 sys]
     d2[336]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+521)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],5),simde_mm_alignr_epi8(c2[66],c2[65],13)));

//row: 43  [P3<<11 + 3 sys]
     d2[344]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+779)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[259],c2[258],3),simde_mm_alignr_epi8(c2[291],c2[290],13))));

//row: 44  [P0<<45 + 3 sys]
     d2[352]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+45)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],13),simde_mm_alignr_epi8(c2[147],c2[146],3))));
  }
}
