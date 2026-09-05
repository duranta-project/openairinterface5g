#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=112, byte encoding, factored
// 319 terms vs 2095 expanded (6.57x); pipeline lag 1 chunk(s)
static inline void ldpc112_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[896] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<7; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],15),simde_mm_xor_si128(c2[15],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],14),simde_mm_xor_si128(c2[60],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],15),simde_mm_xor_si128(c2[113],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],7),simde_mm_xor_si128(c2[126],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[143],c2[142],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[157],c2[156],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[160],c2[159],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[170],c2[169],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[174],c2[173],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[175],c2[174],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[185],c2[184],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[187],c2[186],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[189],c2[188],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[201],c2[200],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[201],c2[200],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[211],c2[210],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[211],c2[210],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[215],c2[214],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[225],c2[224],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[229],c2[228],6),simde_mm_xor_si128(c2[230],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[243],c2[242],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[244],c2[243],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[244],c2[243],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[253],c2[252],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[254],c2[253],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[259],c2[258],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[268],c2[267],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[272],c2[271],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[273],c2[272],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[282],c2[281],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[286],c2[285],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[287],c2[286],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[297],c2[296],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[299],c2[298],11),simde_mm_alignr_epi8(c2[300],c2[299],1)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[28]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],10),simde_mm_alignr_epi8(c2[15],c2[14],10));

//row: 6  [direct, 8 terms]
     d2[42]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[141],c2[140],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[156],c2[155],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[186],c2[185],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[254],c2[253],8),simde_mm_alignr_epi8(c2[281],c2[280],4))))))));

//row: 7  [direct, 6 terms]
     d2[49]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],6),simde_mm_alignr_epi8(c2[199],c2[198],14))))));

//row: 9  [direct, 8 terms]
     d2[63]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[147],c2[146],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[188],c2[187],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[243],c2[242],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[259],c2[258],13),simde_mm_alignr_epi8(c2[287],c2[286],10))))))));

//row: 10  [direct, 6 terms]
     d2[70]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[118],c2[117],2),simde_mm_alignr_epi8(c2[203],c2[202],7))))));

//row: 12  [direct, 6 terms]
     d2[84]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[142],c2[141],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[155],c2[154],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[184],c2[183],12),simde_mm_alignr_epi8(c2[257],c2[256],6))))));

//row: 14  [direct, 6 terms]
     d2[98]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[172],c2[171],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[217],c2[216],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[229],c2[228],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],3),simde_mm_alignr_epi8(c2[297],c2[296],11))))));

//row: 17  [direct, 5 terms]
     d2[119]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[230],c2[229],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],15),simde_mm_alignr_epi8(c2[295],c2[294],4)))));

//row: 18  [direct, 5 terms]
     d2[126]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[175],c2[174],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[188],c2[187],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[254],c2[253],15),simde_mm_alignr_epi8(c2[271],c2[270],10)))));

//row: 19  [direct, 5 terms]
     d2[133]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[118],c2[117],6),simde_mm_alignr_epi8(c2[141],c2[140],8)))));

//row: 21  [direct, 5 terms]
     d2[147]=simde_mm_xor_si128(c2[19],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[72],c2[71],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[231],c2[230],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[284],c2[283],2),simde_mm_alignr_epi8(c2[301],c2[300],10)))));

//row: 22  [direct, 4 terms]
     d2[154]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],5),simde_mm_xor_si128(c2[168],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],3),simde_mm_alignr_epi8(c2[241],c2[240],4))));

//row: 23  [direct, 4 terms]
     d2[161]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[144],c2[143],8),simde_mm_alignr_epi8(c2[253],c2[252],10))));

//row: 25  [direct, 4 terms]
     d2[175]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],1),simde_mm_alignr_epi8(c2[202],c2[201],1))));

//row: 26  [direct, 4 terms]
     d2[182]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],11),simde_mm_xor_si128(c2[32],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],4),simde_mm_alignr_epi8(c2[211],c2[210],15))));

//row: 27  [direct, 3 terms]
     d2[189]=simde_mm_xor_si128(c2[20],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[86],c2[85],13),simde_mm_alignr_epi8(c2[116],c2[115],14)));

//row: 28  [direct, 4 terms]
     d2[196]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[273],c2[272],9),simde_mm_alignr_epi8(c2[295],c2[294],2))));

//row: 33  [direct, 4 terms]
     d2[231]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[158],c2[157],1),simde_mm_alignr_epi8(c2[296],c2[295],9))));

//row: 34  [direct, 4 terms]
     d2[238]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[215],c2[214],4),simde_mm_alignr_epi8(c2[241],c2[240],6))));

//row: 36  [direct, 4 terms]
     d2[252]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[199],c2[198],13),simde_mm_xor_si128(c2[210],simde_mm_alignr_epi8(c2[253],c2[252],6))));

//row: 38  [direct, 4 terms]
     d2[266]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],9),simde_mm_alignr_epi8(c2[169],c2[168],10))));

//row: 39  [direct, 4 terms]
     d2[273]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],1),simde_mm_alignr_epi8(c2[267],c2[266],2))));

//row: 40  [direct, 3 terms]
     d2[280]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],13),c2[244]));

//row: 41  [direct, 4 terms]
     d2[287]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[130],c2[129],12),simde_mm_alignr_epi8(c2[256],c2[255],7))));

//row: 45  [direct, 3 terms]
     d2[315]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],9),simde_mm_alignr_epi8(c2[142],c2[141],13)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[7]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],15),simde_mm_xor_si128(cm[15],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[34],cm[33],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[48],cm[47],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[75],cm[74],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[85],cm[84],10),simde_mm_xor_si128(cm[126],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[146],cm[145],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[161],cm[160],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[170],cm[169],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[189],cm[188],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[211],cm[210],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[229],cm[228],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[254],cm[253],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[272],cm[271],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[287],cm[286],8),simde_mm_alignr_epi8(cm[300],cm[299],1))))))))))))))))));
       dm[14]=simde_mm_xor_si128(dm[7],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[31],cm[30],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[59],cm[58],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[71],cm[70],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[132],cm[131],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[174],cm[173],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[201],cm[200],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[211],cm[210],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[225],cm[224],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[244],cm[243],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[268],cm[267],15),simde_mm_alignr_epi8(cm[299],cm[298],11))))))))))))))))));
       dm[21]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[16],cm[15],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[46],cm[45],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],14),simde_mm_xor_si128(cm[113],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[143],cm[142],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[157],cm[156],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[175],cm[174],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[185],cm[184],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[201],cm[200],5),simde_mm_xor_si128(cm[230],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[243],cm[242],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[259],cm[258],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[282],cm[281],14),simde_mm_alignr_epi8(cm[297],cm[296],14))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[6]; dm=&dsimd[6];
  dm[7]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],15),simde_mm_xor_si128(cm[15],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[34],cm[33],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[48],cm[47],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[75],cm[74],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[85],cm[84],10),simde_mm_xor_si128(cm[126],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[146],cm[145],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[161],cm[160],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[170],cm[169],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[189],cm[188],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[211],cm[210],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[229],cm[228],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[254],cm[253],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[272],cm[271],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[287],cm[286],8),simde_mm_alignr_epi8(cm[300],cm[299],1))))))))))))))))));
  dm[14]=simde_mm_xor_si128(dsimd[13],simde_mm_xor_si128(dsimd[6],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[31],cm[30],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[45],cm[44],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[59],cm[58],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[71],cm[70],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[132],cm[131],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[160],cm[159],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[174],cm[173],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[201],cm[200],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[211],cm[210],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[225],cm[224],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[244],cm[243],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[268],cm[267],15),simde_mm_alignr_epi8(cm[299],cm[298],11))))))))))))))))));
  dm[21]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[16],cm[15],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[46],cm[45],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],14),simde_mm_xor_si128(cm[113],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[143],cm[142],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[157],cm[156],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[175],cm[174],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[185],cm[184],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[201],cm[200],5),simde_mm_xor_si128(cm[230],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[243],cm[242],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[259],cm[258],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[282],cm[281],14),simde_mm_alignr_epi8(cm[297],cm[296],14))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*224,     d+k*112, 112);
    memcpy(kd+k*224+112, d+k*112, 112);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<7; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<41 + 6 sys]
     d2[35]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+41)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],6),simde_mm_xor_si128(c2[173],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[229],c2[228],6),simde_mm_alignr_epi8(c2[296],c2[295],2)))))));

//row: 8  [P0<<44+P2<<89 + 7 sys]
     d2[56]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+44)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+537)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[175],c2[174],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[230],c2[229],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[269],c2[268],7),simde_mm_alignr_epi8(c2[298],c2[297],10)))))))));

//row: 11  [P0<<84+P1<<89 + 5 sys]
     d2[77]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+84)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+313)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[169],c2[168],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[227],c2[226],3),c2[296]))))));

//row: 13  [P1<<78 + 4 sys]
     d2[91]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+302)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],8),simde_mm_alignr_epi8(c2[287],c2[286],9)))));

//row: 15  [P3<<81 + 5 sys]
     d2[105]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+753)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[142],c2[141],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[189],c2[188],1),simde_mm_alignr_epi8(c2[259],c2[258],1))))));

//row: 16  [P0<<101 + 4 sys]
     d2[112]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+101)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[157],c2[156],11),simde_mm_alignr_epi8(c2[285],c2[284],13)))));

//row: 20  [P0<<71 + 4 sys]
     d2[140]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+71)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[127],c2[126],9),simde_mm_alignr_epi8(c2[161],c2[160],6)))));

//row: 24  [P0<<12 + 4 sys]
     d2[168]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+12)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],13),simde_mm_xor_si128(c2[60],simde_mm_alignr_epi8(c2[160],c2[159],1)))));

//row: 29  [P3<<68 + 3 sys]
     d2[203]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+740)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[197],c2[196],11),simde_mm_alignr_epi8(c2[256],c2[255],5))));

//row: 30  [P2<<11 + 3 sys]
     d2[210]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+459)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[142],c2[141],2),simde_mm_alignr_epi8(c2[189],c2[188],2))));

//row: 31  [P0<<65+P3<<16 + 2 sys]
     d2[217]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+65)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+688)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],15),simde_mm_alignr_epi8(c2[102],c2[101],1))));

//row: 32  [P2<<30 + 3 sys]
     d2[224]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+478)),simde_mm_xor_si128(c2[5],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[175],c2[174],1),simde_mm_alignr_epi8(c2[200],c2[199],10))));

//row: 35  [P0<<75 + 3 sys]
     d2[245]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+75)),simde_mm_xor_si128(c2[19],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],13),simde_mm_alignr_epi8(c2[170],c2[169],10))));

//row: 37  [P1<<26 + 2 sys]
     d2[259]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+250)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],1),simde_mm_alignr_epi8(c2[188],c2[187],3)));

//row: 42  [P2<<78 + 2 sys]
     d2[294]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+526)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],2),simde_mm_alignr_epi8(c2[62],c2[61],11)));

//row: 43  [P3<<49 + 3 sys]
     d2[301]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+721)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[226],c2[225],6),simde_mm_alignr_epi8(c2[254],c2[253],6))));

//row: 44  [P0<<12 + 3 sys]
     d2[308]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+12)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],4),simde_mm_alignr_epi8(c2[127],c2[126],9))));
  }
}
