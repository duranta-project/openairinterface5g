#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=48, byte encoding, factored
// 319 terms vs 2081 expanded (6.52x); pipeline lag 1 chunk(s)
static inline void ldpc48_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[384] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<3; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],1),simde_mm_xor_si128(c2[18],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],12),simde_mm_xor_si128(c2[24],simde_mm_xor_si128(c2[31],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],5),simde_mm_xor_si128(c2[30],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],8),simde_mm_xor_si128(c2[49],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],4),simde_mm_xor_si128(c2[60],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[79],c2[78],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[79],c2[78],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[86],c2[85],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[86],c2[85],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[92],c2[91],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[97],c2[96],10),simde_mm_xor_si128(c2[97],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[109],c2[108],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[110],c2[109],15),simde_mm_xor_si128(c2[108],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[117],c2[116],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[117],c2[116],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[127],c2[126],10),simde_mm_xor_si128(c2[127],simde_mm_alignr_epi8(c2[128],c2[127],5)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[12]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],12),simde_mm_alignr_epi8(c2[9],c2[8],5));

//row: 6  [direct, 8 terms]
     d2[18]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[110],c2[109],2),simde_mm_alignr_epi8(c2[123],c2[122],1))))))));

//row: 7  [direct, 6 terms]
     d2[21]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],2),simde_mm_alignr_epi8(c2[86],c2[85],2))))));

//row: 9  [direct, 8 terms]
     d2[27]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[79],c2[78],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[109],c2[108],15),simde_mm_alignr_epi8(c2[123],c2[122],2))))))));

//row: 10  [direct, 6 terms]
     d2[30]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],15),simde_mm_alignr_epi8(c2[86],c2[85],2))))));

//row: 12  [direct, 6 terms]
     d2[36]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[79],c2[78],2),simde_mm_alignr_epi8(c2[110],c2[109],10))))));

//row: 14  [direct, 6 terms]
     d2[42]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[93],c2[92],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],11),simde_mm_alignr_epi8(c2[127],c2[126],12))))));

//row: 17  [direct, 5 terms]
     d2[51]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],6),simde_mm_alignr_epi8(c2[129],c2[128],7)))));

//row: 18  [direct, 5 terms]
     d2[54]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[111],c2[110],4),simde_mm_alignr_epi8(c2[115],c2[114],4)))));

//row: 19  [direct, 5 terms]
     d2[57]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],2),simde_mm_alignr_epi8(c2[61],c2[60],5)))));

//row: 21  [direct, 5 terms]
     d2[63]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],5),simde_mm_alignr_epi8(c2[127],c2[126],1)))));

//row: 22  [direct, 4 terms]
     d2[66]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],9),simde_mm_alignr_epi8(c2[104],c2[103],6))));

//row: 23  [direct, 4 terms]
     d2[69]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],13),simde_mm_alignr_epi8(c2[110],c2[109],11))));

//row: 25  [direct, 4 terms]
     d2[75]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],15),simde_mm_alignr_epi8(c2[86],c2[85],8))));

//row: 26  [direct, 4 terms]
     d2[78]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],12),simde_mm_alignr_epi8(c2[93],c2[92],8))));

//row: 27  [direct, 3 terms]
     d2[81]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],2),simde_mm_alignr_epi8(c2[49],c2[48],5)));

//row: 28  [direct, 4 terms]
     d2[84]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],4),simde_mm_alignr_epi8(c2[129],c2[128],2))));

//row: 33  [direct, 4 terms]
     d2[99]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],9),simde_mm_alignr_epi8(c2[127],c2[126],9))));

//row: 34  [direct, 4 terms]
     d2[102]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],10),simde_mm_alignr_epi8(c2[104],c2[103],8))));

//row: 36  [direct, 4 terms]
     d2[108]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[86],c2[85],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],9),simde_mm_alignr_epi8(c2[110],c2[109],5))));

//row: 38  [direct, 4 terms]
     d2[114]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],1),simde_mm_alignr_epi8(c2[74],c2[73],6))));

//row: 39  [direct, 4 terms]
     d2[117]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],10),simde_mm_alignr_epi8(c2[115],c2[114],9))));

//row: 40  [direct, 3 terms]
     d2[120]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],5),simde_mm_alignr_epi8(c2[104],c2[103],8)));

//row: 41  [direct, 4 terms]
     d2[123]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],11),c2[108])));

//row: 45  [direct, 3 terms]
     d2[135]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],5),simde_mm_alignr_epi8(c2[61],c2[60],15)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[3]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[13],cm[12],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[33],cm[32],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[56],cm[55],13),simde_mm_xor_si128(cm[60],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[67],cm[66],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[80],cm[79],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[92],cm[91],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[97],cm[96],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[109],cm[108],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[117],cm[116],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[123],cm[122],10),simde_mm_alignr_epi8(cm[127],cm[126],10))))))))))))))))));
       dm[6]=simde_mm_xor_si128(dm[3],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[20],cm[19],9),simde_mm_xor_si128(cm[24],simde_mm_xor_si128(cm[30],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[67],cm[66],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[73],cm[72],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[91],cm[90],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[98],cm[97],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[104],cm[103],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[117],cm[116],11),cm[127])))))))))))))))));
       dm[9]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],7),simde_mm_xor_si128(cm[18],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[43],cm[42],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[63],cm[62],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[68],cm[67],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[75],cm[74],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],4),simde_mm_xor_si128(cm[97],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[103],cm[102],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[110],cm[109],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[123],cm[122],7),simde_mm_alignr_epi8(cm[128],cm[127],5))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[2]; dm=&dsimd[2];
  dm[3]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[2],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[13],cm[12],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[33],cm[32],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[56],cm[55],13),simde_mm_xor_si128(cm[60],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[67],cm[66],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[80],cm[79],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[92],cm[91],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[97],cm[96],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[109],cm[108],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[117],cm[116],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[123],cm[122],10),simde_mm_alignr_epi8(cm[127],cm[126],10))))))))))))))))));
  dm[6]=simde_mm_xor_si128(dsimd[5],simde_mm_xor_si128(dsimd[2],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[20],cm[19],9),simde_mm_xor_si128(cm[24],simde_mm_xor_si128(cm[30],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[67],cm[66],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[73],cm[72],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[91],cm[90],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[98],cm[97],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[104],cm[103],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[117],cm[116],11),cm[127])))))))))))))))));
  dm[9]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[2],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],7),simde_mm_xor_si128(cm[18],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[27],cm[26],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[43],cm[42],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[63],cm[62],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[68],cm[67],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[75],cm[74],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[86],cm[85],4),simde_mm_xor_si128(cm[97],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[103],cm[102],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[110],cm[109],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[123],cm[122],7),simde_mm_alignr_epi8(cm[128],cm[127],5))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*96,     d+k*48, 48);
    memcpy(kd+k*96+48, d+k*48, 48);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<3; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<13 + 6 sys]
     d2[15]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+13)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[97],c2[96],1),simde_mm_alignr_epi8(c2[127],c2[126],3)))))));

//row: 8  [P0<<19+P2<<26 + 7 sys]
     d2[24]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+19)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+218)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],7),simde_mm_xor_si128(c2[116],c2[128]))))))));

//row: 11  [P0<<46+P1<<19 + 5 sys]
     d2[33]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+46)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+115)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],15),simde_mm_alignr_epi8(c2[129],c2[128],12)))))));

//row: 13  [P1<<34 + 4 sys]
     d2[39]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+130)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],10),simde_mm_alignr_epi8(c2[122],c2[121],3)))));

//row: 15  [P3<<29 + 5 sys]
     d2[45]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+317)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],14),simde_mm_alignr_epi8(c2[109],c2[108],7))))));

//row: 16  [P0<<9 + 4 sys]
     d2[48]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+9)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],9),simde_mm_alignr_epi8(c2[121],c2[120],1)))));

//row: 20  [P0<<11 + 4 sys]
     d2[60]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+11)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],8),simde_mm_alignr_epi8(c2[67],c2[66],5)))));

//row: 24  [P0<<42 + 4 sys]
     d2[72]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+42)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],11),simde_mm_alignr_epi8(c2[67],c2[66],3)))));

//row: 29  [P3<<30 + 3 sys]
     d2[87]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+318)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],5),simde_mm_alignr_epi8(c2[109],c2[108],3))));

//row: 30  [P2<<42 + 3 sys]
     d2[90]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+234)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],5),simde_mm_alignr_epi8(c2[80],c2[79],4))));

//row: 31  [P0<<18+P3<<16 + 2 sys]
     d2[93]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+18)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+304)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],4),simde_mm_alignr_epi8(c2[44],c2[43],7))));

//row: 32  [P2<<47 + 3 sys]
     d2[96]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+239)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],9),simde_mm_alignr_epi8(c2[86],c2[85],15))));

//row: 35  [P0<<26 + 3 sys]
     d2[105]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+26)),simde_mm_xor_si128(c2[8],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],15),simde_mm_alignr_epi8(c2[73],c2[72],2))));

//row: 37  [P1<<19 + 2 sys]
     d2[111]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+115)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],13),simde_mm_alignr_epi8(c2[81],c2[80],2)));

//row: 42  [P2<<26 + 2 sys]
     d2[126]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+218)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_alignr_epi8(c2[25],c2[24],14)));

//row: 43  [P3<<24 + 3 sys]
     d2[129]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+312)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],4),simde_mm_alignr_epi8(c2[110],c2[109],2))));

//row: 44  [P0<<34 + 3 sys]
     d2[132]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+34)),simde_mm_xor_si128(c2[2],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],14),simde_mm_alignr_epi8(c2[56],c2[55],3))));
  }
}
