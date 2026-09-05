#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=160, byte encoding, factored
// 319 terms vs 2101 expanded (6.59x); pipeline lag 1 chunk(s)
static inline void ldpc160_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1280] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<10; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],15),simde_mm_xor_si128(c2[23],simde_mm_xor_si128(c2[45],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[64],c2[63],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[83],c2[82],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],6),simde_mm_xor_si128(c2[105],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[123],c2[122],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[125],c2[124],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[141],c2[140],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[143],c2[142],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[163],c2[162],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[165],c2[164],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[181],c2[180],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[185],c2[184],7),simde_mm_xor_si128(c2[185],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[201],c2[200],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[207],c2[206],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[208],c2[207],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[224],c2[223],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[229],c2[228],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[229],c2[228],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[241],c2[240],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[246],c2[245],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[261],c2[260],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[269],c2[268],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[269],c2[268],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[284],c2[283],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[285],c2[284],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[288],c2[287],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[307],c2[306],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[309],c2[308],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[309],c2[308],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[323],c2[322],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[325],c2[324],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[327],c2[326],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[342],c2[341],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[342],c2[341],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[346],c2[345],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[365],c2[364],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[366],c2[365],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[368],c2[367],1),simde_mm_xor_si128(c2[381],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[385],c2[384],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[387],c2[386],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[402],c2[401],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[408],c2[407],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[410],c2[409],15),simde_mm_xor_si128(c2[422],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[425],c2[424],4),simde_mm_alignr_epi8(c2[429],c2[428],13)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[40]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_alignr_epi8(c2[23],c2[22],13));

//row: 6  [direct, 8 terms]
     d2[60]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[221],c2[220],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[265],c2[264],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[349],c2[348],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[369],c2[368],10),simde_mm_alignr_epi8(c2[405],c2[404],11))))))));

//row: 7  [direct, 6 terms]
     d2[70]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[83],c2[82],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[144],c2[143],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[162],c2[161],9),simde_mm_alignr_epi8(c2[285],c2[284],12))))));

//row: 9  [direct, 8 terms]
     d2[90]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[228],c2[227],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[270],c2[269],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[347],c2[346],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[369],c2[368],7),simde_mm_alignr_epi8(c2[404],c2[403],1))))))));

//row: 10  [direct, 6 terms]
     d2[100]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],14),simde_mm_xor_si128(c2[45],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[84],c2[83],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],1),simde_mm_alignr_epi8(c2[290],c2[289],7))))));

//row: 12  [direct, 6 terms]
     d2[120]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[201],c2[200],9),simde_mm_xor_si128(c2[223],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[267],c2[266],9),simde_mm_alignr_epi8(c2[364],c2[363],4))))));

//row: 14  [direct, 6 terms]
     d2[140]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[304],c2[303],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[324],c2[323],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[342],c2[341],3),simde_mm_alignr_epi8(c2[427],c2[426],2))))));

//row: 17  [direct, 5 terms]
     d2[170]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],3),simde_mm_xor_si128(c2[328],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[344],c2[343],3),simde_mm_alignr_epi8(c2[425],c2[424],4)))));

//row: 18  [direct, 5 terms]
     d2[180]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[269],c2[268],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[369],c2[368],13),simde_mm_alignr_epi8(c2[389],c2[388],7)))));

//row: 19  [direct, 5 terms]
     d2[190]=simde_mm_xor_si128(c2[4],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[147],c2[146],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[167],c2[166],14),simde_mm_alignr_epi8(c2[203],c2[202],9)))));

//row: 21  [direct, 5 terms]
     d2[210]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[329],c2[328],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[408],c2[407],11),simde_mm_alignr_epi8(c2[427],c2[426],7)))));

//row: 22  [direct, 4 terms]
     d2[220]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[264],c2[263],7),simde_mm_alignr_epi8(c2[350],c2[349],12))));

//row: 23  [direct, 4 terms]
     d2[230]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],5),simde_mm_alignr_epi8(c2[367],c2[366],9))));

//row: 25  [direct, 4 terms]
     d2[250]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[129],c2[128],7),simde_mm_xor_si128(c2[146],simde_mm_alignr_epi8(c2[283],c2[282],14))));

//row: 26  [direct, 4 terms]
     d2[260]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],14),simde_mm_alignr_epi8(c2[305],c2[304],3))));

//row: 27  [direct, 3 terms]
     d2[270]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],13),c2[169]));

//row: 28  [direct, 4 terms]
     d2[280]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[384],c2[383],2),simde_mm_alignr_epi8(c2[425],c2[424],10))));

//row: 33  [direct, 4 terms]
     d2[330]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[229],c2[228],12),simde_mm_alignr_epi8(c2[423],c2[422],1))));

//row: 34  [direct, 4 terms]
     d2[340]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[301],c2[300],5),simde_mm_alignr_epi8(c2[343],c2[342],12))));

//row: 36  [direct, 4 terms]
     d2[360]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[282],c2[281],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[302],c2[301],2),c2[360])));

//row: 38  [direct, 4 terms]
     d2[380]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[182],c2[181],3),simde_mm_xor_si128(c2[204],simde_mm_alignr_epi8(c2[242],c2[241],5))));

//row: 39  [direct, 4 terms]
     d2[390]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],13),simde_mm_xor_si128(c2[142],c2[383])));

//row: 40  [direct, 3 terms]
     d2[400]=simde_mm_xor_si128(c2[2],simde_mm_xor_si128(c2[165],simde_mm_alignr_epi8(c2[343],c2[342],5)));

//row: 41  [direct, 4 terms]
     d2[410]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[188],c2[187],12),simde_mm_alignr_epi8(c2[363],c2[362],15))));

//row: 45  [direct, 3 terms]
     d2[450]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[125],c2[124],4),simde_mm_alignr_epi8(c2[208],c2[207],14)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[10]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[47],cm[46],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[64],cm[63],1),simde_mm_xor_si128(cm[105],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[123],cm[122],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[181],cm[180],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[201],cm[200],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[224],cm[223],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[241],cm[240],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[269],cm[268],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[309],cm[308],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[327],cm[326],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[368],cm[367],1),simde_mm_xor_si128(cm[381],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[402],cm[401],13),cm[422])))))))))))))))));
       dm[20]=simde_mm_xor_si128(dm[10],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[62],cm[61],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[149],cm[148],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[161],cm[160],4),simde_mm_xor_si128(cm[185],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[229],cm[228],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[249],cm[248],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[285],cm[284],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[307],cm[306],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[325],cm[324],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[346],cm[345],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[387],cm[386],4),simde_mm_alignr_epi8(cm[429],cm[428],13))))))))))))))))));
       dm[30]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],12),simde_mm_xor_si128(cm[23],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[62],cm[61],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[83],cm[82],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[124],cm[123],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[141],cm[140],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[165],cm[164],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[208],cm[207],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[229],cm[228],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[246],cm[245],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[261],cm[260],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[284],cm[283],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[323],cm[322],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[342],cm[341],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[410],cm[409],15),simde_mm_alignr_epi8(cm[425],cm[424],4))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[9]; dm=&dsimd[9];
  dm[10]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[9],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[21],cm[20],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[47],cm[46],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[64],cm[63],1),simde_mm_xor_si128(cm[105],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[123],cm[122],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[181],cm[180],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[201],cm[200],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[224],cm[223],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[241],cm[240],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[269],cm[268],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[309],cm[308],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[327],cm[326],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[368],cm[367],1),simde_mm_xor_si128(cm[381],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[402],cm[401],13),cm[422])))))))))))))))));
  dm[20]=simde_mm_xor_si128(dsimd[19],simde_mm_xor_si128(dsimd[9],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[62],cm[61],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[149],cm[148],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[161],cm[160],4),simde_mm_xor_si128(cm[185],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[229],cm[228],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[249],cm[248],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[285],cm[284],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[307],cm[306],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[325],cm[324],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[346],cm[345],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[387],cm[386],4),simde_mm_alignr_epi8(cm[429],cm[428],13))))))))))))))))));
  dm[30]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[9],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],12),simde_mm_xor_si128(cm[23],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[62],cm[61],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[83],cm[82],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[124],cm[123],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[141],cm[140],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[165],cm[164],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[208],cm[207],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[229],cm[228],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[246],cm[245],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[261],cm[260],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[284],cm[283],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[323],cm[322],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[342],cm[341],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[365],cm[364],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[410],cm[409],15),simde_mm_alignr_epi8(cm[425],cm[424],4))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*320,     d+k*160, 160);
    memcpy(kd+k*320+160, d+k*160, 160);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<10; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<119 + 6 sys]
     d2[50]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+119)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[64],c2[63],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[250],c2[249],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[323],c2[322],9),simde_mm_alignr_epi8(c2[427],c2[426],11)))))));

//row: 8  [P0<<85+P2<<154 + 7 sys]
     d2[80]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+85)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+794)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[245],c2[244],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[329],c2[328],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[387],c2[386],14),simde_mm_alignr_epi8(c2[427],c2[426],13)))))))));

//row: 11  [P0<<43+P1<<120 + 5 sys]
     d2[110]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+43)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+440)),simde_mm_xor_si128(c2[1],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[249],c2[248],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[329],c2[328],1),simde_mm_alignr_epi8(c2[422],c2[421],1)))))));

//row: 13  [P1<<37 + 4 sys]
     d2[130]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+357)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],15),c2[400]))));

//row: 15  [P3<<51 + 5 sys]
     d2[150]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1011)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[204],c2[203],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[269],c2[268],2),simde_mm_alignr_epi8(c2[362],c2[361],8))))));

//row: 16  [P0<<140 + 4 sys]
     d2[160]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+140)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[223],c2[222],13),simde_mm_alignr_epi8(c2[408],c2[407],3)))));

//row: 20  [P0<<10 + 4 sys]
     d2[200]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+10)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],8),simde_mm_alignr_epi8(c2[229],c2[228],2)))));

//row: 24  [P0<<121 + 4 sys]
     d2[240]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+121)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],14),simde_mm_alignr_epi8(c2[222],c2[221],11)))));

//row: 29  [P3<<29 + 3 sys]
     d2[290]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+989)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],4),simde_mm_alignr_epi8(c2[368],c2[367],5))));

//row: 30  [P2<<135 + 3 sys]
     d2[300]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+775)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[202],c2[201],7),simde_mm_alignr_epi8(c2[267],c2[266],9))));

//row: 31  [P0<<66+P3<<2 + 2 sys]
     d2[310]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+66)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+962)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],14),simde_mm_alignr_epi8(c2[150],c2[149],4))));

//row: 32  [P2<<57 + 3 sys]
     d2[320]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+697)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[242],c2[241],6),simde_mm_alignr_epi8(c2[287],c2[286],15))));

//row: 35  [P0<<125 + 3 sys]
     d2[350]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+125)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[130],c2[129],14),simde_mm_alignr_epi8(c2[244],c2[243],7))));

//row: 37  [P1<<115 + 2 sys]
     d2[370]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+435)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],10),simde_mm_alignr_epi8(c2[261],c2[260],15)));

//row: 42  [P2<<126 + 2 sys]
     d2[420]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+766)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],2),simde_mm_alignr_epi8(c2[85],c2[84],1)));

//row: 43  [P3<<102 + 3 sys]
     d2[430]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1062)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[325],c2[324],5),c2[361])));

//row: 44  [P0<<100 + 3 sys]
     d2[440]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+100)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],3),simde_mm_alignr_epi8(c2[187],c2[186],3))));
  }
}
