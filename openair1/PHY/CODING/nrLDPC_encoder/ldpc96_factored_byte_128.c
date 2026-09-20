#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=96, byte encoding, factored
// 319 terms vs 2093 expanded (6.56x); pipeline lag 1 chunk(s)
static inline void ldpc96_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[768] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<6; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],1),simde_mm_xor_si128(c2[36],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],3),simde_mm_xor_si128(c2[48],simde_mm_xor_si128(c2[63],simde_mm_xor_si128(c2[64],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[77],c2[76],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],8),simde_mm_xor_si128(c2[100],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[110],c2[109],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[112],c2[111],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],4),simde_mm_xor_si128(c2[120],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[133],c2[132],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[133],c2[132],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[147],c2[146],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[157],c2[156],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[160],c2[159],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[170],c2[169],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[170],c2[169],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[174],c2[173],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[181],c2[180],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[182],c2[181],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[184],c2[183],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[193],c2[192],10),simde_mm_xor_si128(c2[193],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[197],c2[196],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[205],c2[204],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[206],c2[205],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[207],c2[206],3),simde_mm_xor_si128(c2[219],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[220],c2[219],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[221],c2[220],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[229],c2[228],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[231],c2[230],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[234],c2[233],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[241],c2[240],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[243],c2[242],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[243],c2[242],10),simde_mm_xor_si128(c2[253],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[256],c2[255],10),simde_mm_alignr_epi8(c2[257],c2[256],5)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[24]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],12),simde_mm_alignr_epi8(c2[18],c2[17],5));

//row: 6  [direct, 8 terms]
     d2[36]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[77],c2[76],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[136],c2[135],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[162],c2[161],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[208],c2[207],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[218],c2[217],2),simde_mm_alignr_epi8(c2[243],c2[242],1))))))));

//row: 7  [direct, 6 terms]
     d2[42]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[97],c2[96],2),simde_mm_alignr_epi8(c2[170],c2[169],2))))));

//row: 9  [direct, 8 terms]
     d2[54]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[135],c2[134],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[160],c2[159],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[205],c2[204],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[220],c2[219],15),simde_mm_alignr_epi8(c2[246],c2[245],2))))))));

//row: 10  [direct, 6 terms]
     d2[60]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],15),simde_mm_alignr_epi8(c2[173],c2[172],2))))));

//row: 12  [direct, 6 terms]
     d2[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[125],c2[124],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[135],c2[134],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[160],c2[159],2),simde_mm_alignr_epi8(c2[221],c2[220],10))))));

//row: 14  [direct, 6 terms]
     d2[84]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[198],c2[197],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[208],c2[207],11),simde_mm_alignr_epi8(c2[253],c2[252],12))))));

//row: 17  [direct, 5 terms]
     d2[102]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[169],c2[168],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[198],c2[197],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],6),simde_mm_alignr_epi8(c2[258],c2[257],7)))));

//row: 18  [direct, 5 terms]
     d2[108]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[162],c2[161],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[219],c2[218],4),simde_mm_alignr_epi8(c2[229],c2[228],4)))));

//row: 19  [direct, 5 terms]
     d2[114]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],2),simde_mm_alignr_epi8(c2[121],c2[120],5)))));

//row: 21  [direct, 5 terms]
     d2[126]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[195],c2[194],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],5),simde_mm_alignr_epi8(c2[253],c2[252],1)))));

//row: 22  [direct, 4 terms]
     d2[132]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[159],c2[158],9),simde_mm_alignr_epi8(c2[206],c2[205],6))));

//row: 23  [direct, 4 terms]
     d2[138]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],13),simde_mm_alignr_epi8(c2[218],c2[217],11))));

//row: 25  [direct, 4 terms]
     d2[150]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],15),simde_mm_alignr_epi8(c2[170],c2[169],8))));

//row: 26  [direct, 4 terms]
     d2[156]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],12),simde_mm_alignr_epi8(c2[183],c2[182],8))));

//row: 27  [direct, 3 terms]
     d2[162]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],2),simde_mm_alignr_epi8(c2[97],c2[96],5)));

//row: 28  [direct, 4 terms]
     d2[168]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[232],c2[231],4),simde_mm_alignr_epi8(c2[258],c2[257],2))));

//row: 33  [direct, 4 terms]
     d2[198]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],9),simde_mm_alignr_epi8(c2[253],c2[252],9))));

//row: 34  [direct, 4 terms]
     d2[204]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[181],c2[180],10),simde_mm_alignr_epi8(c2[206],c2[205],8))));

//row: 36  [direct, 4 terms]
     d2[216]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[170],c2[169],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[181],c2[180],9),simde_mm_alignr_epi8(c2[218],c2[217],5))));

//row: 38  [direct, 4 terms]
     d2[228]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],1),simde_mm_alignr_epi8(c2[146],c2[145],6))));

//row: 39  [direct, 4 terms]
     d2[234]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],10),simde_mm_alignr_epi8(c2[229],c2[228],9))));

//row: 40  [direct, 3 terms]
     d2[240]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],5),simde_mm_alignr_epi8(c2[206],c2[205],8)));

//row: 41  [direct, 4 terms]
     d2[246]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[111],c2[110],11),c2[216])));

//row: 45  [direct, 3 terms]
     d2[270]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[76],c2[75],5),simde_mm_alignr_epi8(c2[121],c2[120],15)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[6]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[28],cm[27],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[42],cm[41],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[66],cm[65],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[110],cm[109],13),simde_mm_xor_si128(cm[120],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[133],cm[132],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[146],cm[145],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[161],cm[160],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[193],cm[192],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[220],cm[219],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[234],cm[233],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[243],cm[242],10),simde_mm_alignr_epi8(cm[256],cm[255],10))))))))))))))))));
       dm[12]=simde_mm_xor_si128(dm[6],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[29],cm[28],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],9),simde_mm_xor_si128(cm[48],simde_mm_xor_si128(cm[63],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[133],cm[132],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[170],cm[169],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[181],cm[180],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[197],cm[196],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[206],cm[205],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[231],cm[230],11),cm[253])))))))))))))))));
       dm[18]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],7),simde_mm_xor_si128(cm[36],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[54],cm[53],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[73],cm[72],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[88],cm[87],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[123],cm[122],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[134],cm[133],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[147],cm[146],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[170],cm[169],4),simde_mm_xor_si128(cm[193],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[205],cm[204],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[221],cm[220],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[243],cm[242],7),simde_mm_alignr_epi8(cm[257],cm[256],5))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[5]; dm=&dsimd[5];
  dm[6]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[28],cm[27],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[42],cm[41],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[66],cm[65],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[110],cm[109],13),simde_mm_xor_si128(cm[120],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[133],cm[132],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[146],cm[145],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[161],cm[160],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[193],cm[192],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[220],cm[219],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[234],cm[233],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[243],cm[242],10),simde_mm_alignr_epi8(cm[256],cm[255],10))))))))))))))))));
  dm[12]=simde_mm_xor_si128(dsimd[11],simde_mm_xor_si128(dsimd[5],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[29],cm[28],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],9),simde_mm_xor_si128(cm[48],simde_mm_xor_si128(cm[63],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[133],cm[132],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[148],cm[147],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[170],cm[169],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[181],cm[180],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[197],cm[196],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[206],cm[205],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[231],cm[230],11),cm[253])))))))))))))))));
  dm[18]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],7),simde_mm_xor_si128(cm[36],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[54],cm[53],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[73],cm[72],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[88],cm[87],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[123],cm[122],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[134],cm[133],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[147],cm[146],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[170],cm[169],4),simde_mm_xor_si128(cm[193],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[205],cm[204],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[221],cm[220],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[243],cm[242],7),simde_mm_alignr_epi8(cm[257],cm[256],5))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*192,     d+k*96, 96);
    memcpy(kd+k*192+96, d+k*96, 96);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<6; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<61 + 6 sys]
     d2[30]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+61)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[196],c2[195],1),simde_mm_alignr_epi8(c2[256],c2[255],3)))))));

//row: 8  [P0<<67+P2<<74 + 7 sys]
     d2[48]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+67)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+458)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[195],c2[194],7),simde_mm_xor_si128(c2[230],c2[257]))))))));

//row: 11  [P0<<46+P1<<19 + 5 sys]
     d2[66]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+46)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+211)),simde_mm_xor_si128(c2[3],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[195],c2[194],15),simde_mm_alignr_epi8(c2[258],c2[257],12)))))));

//row: 13  [P1<<82 + 4 sys]
     d2[78]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+274)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],10),simde_mm_alignr_epi8(c2[242],c2[241],3)))));

//row: 15  [P3<<77 + 5 sys]
     d2[90]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+653)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[158],c2[157],14),simde_mm_alignr_epi8(c2[220],c2[219],7))))));

//row: 16  [P0<<57 + 4 sys]
     d2[96]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+57)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[136],c2[135],9),simde_mm_alignr_epi8(c2[241],c2[240],1)))));

//row: 20  [P0<<59 + 4 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+59)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[113],c2[112],8),simde_mm_alignr_epi8(c2[136],c2[135],5)))));

//row: 24  [P0<<42 + 4 sys]
     d2[144]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+42)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],11),simde_mm_alignr_epi8(c2[136],c2[135],3)))));

//row: 29  [P3<<78 + 3 sys]
     d2[174]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+654)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[169],c2[168],5),simde_mm_alignr_epi8(c2[220],c2[219],3))));

//row: 30  [P2<<90 + 3 sys]
     d2[180]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+474)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],5),simde_mm_alignr_epi8(c2[161],c2[160],4))));

//row: 31  [P0<<66+P3<<64 + 2 sys]
     d2[186]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+66)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+640)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],4),simde_mm_alignr_epi8(c2[86],c2[85],7))));

//row: 32  [P2<<95 + 3 sys]
     d2[192]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+479)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],9),simde_mm_alignr_epi8(c2[173],c2[172],15))));

//row: 35  [P0<<74 + 3 sys]
     d2[210]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+74)),simde_mm_xor_si128(c2[14],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],15),simde_mm_alignr_epi8(c2[145],c2[144],2))));

//row: 37  [P1<<19 + 2 sys]
     d2[222]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+211)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],13),simde_mm_alignr_epi8(c2[162],c2[161],2)));

//row: 42  [P2<<26 + 2 sys]
     d2[252]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+410)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_alignr_epi8(c2[49],c2[48],14)));

//row: 43  [P3<<72 + 3 sys]
     d2[258]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+648)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[195],c2[194],4),simde_mm_alignr_epi8(c2[218],c2[217],2))));

//row: 44  [P0<<82 + 3 sys]
     d2[264]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+82)),simde_mm_xor_si128(c2[5],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],14),simde_mm_alignr_epi8(c2[113],c2[112],3))));
  }
}
