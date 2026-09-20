#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=352, byte encoding, factored
// 319 terms vs 2109 expanded (6.61x); pipeline lag 1 chunk(s)
static inline void ldpc352_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[2816] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<22; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[136],c2[135],1),simde_mm_xor_si128(c2[138],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[153],c2[152],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[194],c2[193],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[195],c2[194],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[198],c2[197],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[227],c2[226],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[233],c2[232],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[237],c2[236],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[273],c2[272],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[275],c2[274],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[281],c2[280],9),simde_mm_xor_si128(c2[315],simde_mm_xor_si128(c2[316],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[319],c2[318],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[358],c2[357],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[367],c2[366],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[371],c2[370],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[400],c2[399],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[412],c2[411],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[415],c2[414],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[447],c2[446],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[448],c2[447],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[456],c2[455],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[485],c2[484],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[495],c2[494],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[500],c2[499],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[544],c2[543],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[550],c2[549],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[550],c2[549],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[579],c2[578],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[585],c2[584],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[589],c2[588],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[628],c2[627],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[634],c2[633],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[637],c2[636],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[664],c2[663],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[668],c2[667],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[676],c2[675],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[720],c2[719],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[721],c2[720],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[726],c2[725],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[757],c2[756],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[757],c2[756],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[762],c2[761],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[801],c2[800],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[803],c2[802],3),simde_mm_xor_si128(c2[811],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[837],c2[836],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[847],c2[846],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[850],c2[849],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[881],c2[880],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[883],c2[882],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[888],c2[887],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[932],c2[931],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[936],c2[935],12),simde_mm_alignr_epi8(c2[943],c2[942],14)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[88]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],10),c2[60]);

//row: 6  [direct, 8 terms]
     d2[132]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[269],c2[268],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[461],c2[460],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[491],c2[490],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[583],c2[582],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[758],c2[757],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[810],c2[809],12),simde_mm_alignr_epi8(c2[900],c2[899],1))))))));

//row: 7  [direct, 6 terms]
     d2[154]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],7),simde_mm_xor_si128(c2[190],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[327],c2[326],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[366],c2[365],7),simde_mm_alignr_epi8(c2[619],c2[618],7))))));

//row: 9  [direct, 8 terms]
     d2[198]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[459],c2[458],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[491],c2[490],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[578],c2[577],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[758],c2[757],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[794],c2[793],7),simde_mm_alignr_epi8(c2[902],c2[901],1))))))));

//row: 10  [direct, 6 terms]
     d2[220]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[178],c2[177],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[323],c2[322],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[356],c2[355],1),simde_mm_alignr_epi8(c2[633],c2[632],11))))));

//row: 12  [direct, 6 terms]
     d2[264]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[449],c2[448],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[491],c2[490],14),simde_mm_xor_si128(c2[583],simde_mm_alignr_epi8(c2[797],c2[796],12))))));

//row: 14  [direct, 6 terms]
     d2[308]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[549],c2[548],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[678],c2[677],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[714],c2[713],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[753],c2[752],2),simde_mm_alignr_epi8(c2[929],c2[928],14))))));

//row: 17  [direct, 5 terms]
     d2[374]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[631],c2[630],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[720],c2[719],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[756],c2[755],4),simde_mm_alignr_epi8(c2[936],c2[935],14)))));

//row: 18  [direct, 5 terms]
     d2[396]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[546],c2[545],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[588],c2[587],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[804],c2[803],5),simde_mm_alignr_epi8(c2[841],c2[840],9)))));

//row: 19  [direct, 5 terms]
     d2[418]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[318],c2[317],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[369],c2[368],2),simde_mm_alignr_epi8(c2[453],c2[452],12)))));

//row: 21  [direct, 5 terms]
     d2[462]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[237],c2[236],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[726],c2[725],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[889],c2[888],15),simde_mm_alignr_epi8(c2[931],c2[930],13)))));

//row: 22  [direct, 4 terms]
     d2[484]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[538],c2[537],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[587],c2[586],12),simde_mm_alignr_epi8(c2[756],c2[755],1))));

//row: 23  [direct, 4 terms]
     d2[506]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[452],c2[451],5),c2[811])));

//row: 25  [direct, 4 terms]
     d2[550]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[280],c2[279],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[315],c2[314],15),simde_mm_alignr_epi8(c2[620],c2[619],6))));

//row: 26  [direct, 4 terms]
     d2[572]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[109],c2[108],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[191],c2[190],2),simde_mm_alignr_epi8(c2[667],c2[666],3))));

//row: 27  [direct, 3 terms]
     d2[594]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[285],c2[284],6),simde_mm_alignr_epi8(c2[369],c2[368],12)));

//row: 28  [direct, 4 terms]
     d2[616]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[177],c2[176],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[839],c2[838],8),simde_mm_alignr_epi8(c2[935],c2[934],7))));

//row: 33  [direct, 4 terms]
     d2[726]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],8),simde_mm_xor_si128(c2[107],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[493],c2[492],13),simde_mm_alignr_epi8(c2[931],c2[930],5))));

//row: 34  [direct, 4 terms]
     d2[748]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],12),simde_mm_xor_si128(c2[328],simde_mm_xor_si128(c2[667],simde_mm_alignr_epi8(c2[752],c2[751],6))));

//row: 36  [direct, 4 terms]
     d2[792]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[617],c2[616],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[663],c2[662],3),simde_mm_alignr_epi8(c2[804],c2[803],12))));

//row: 38  [direct, 4 terms]
     d2[836]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[409],c2[408],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[454],c2[453],1),simde_mm_alignr_epi8(c2[544],c2[543],6))));

//row: 39  [direct, 4 terms]
     d2[858]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[59],c2[58],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[311],c2[310],5),c2[853])));

//row: 40  [direct, 3 terms]
     d2[880]=simde_mm_xor_si128(c2[19],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[367],c2[366],13),simde_mm_alignr_epi8(c2[757],c2[756],7)));

//row: 41  [direct, 4 terms]
     d2[902]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[398],c2[397],9),c2[809])));

//row: 45  [direct, 3 terms]
     d2[990]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[269],c2[268],3),simde_mm_alignr_epi8(c2[455],c2[454],11)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[22]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[19],cm[18],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[52],cm[51],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[153],cm[152],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[233],cm[232],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[275],cm[274],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[412],cm[411],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[456],cm[455],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[485],cm[484],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[550],cm[549],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[585],cm[584],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[664],cm[663],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[726],cm[725],11),simde_mm_xor_si128(cm[811],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[847],cm[846],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[883],cm[882],15),simde_mm_alignr_epi8(cm[936],cm[935],12))))))))))))))))));
       dm[44]=simde_mm_xor_si128(dm[22],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[103],cm[102],1),simde_mm_xor_si128(cm[138],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[198],cm[197],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[237],cm[236],12),simde_mm_xor_si128(cm[315],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[371],cm[370],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[400],cm[399],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[495],cm[494],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[544],cm[543],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[637],cm[636],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[676],cm[675],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[721],cm[720],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[757],cm[756],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[837],cm[836],9),simde_mm_alignr_epi8(cm[943],cm[942],14))))))))))))))))));
       dm[66]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[136],cm[135],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[194],cm[193],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[273],cm[272],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[319],cm[318],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[358],cm[357],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[447],cm[446],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[500],cm[499],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[550],cm[549],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[589],cm[588],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[628],cm[627],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[720],cm[719],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[762],cm[761],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[801],cm[800],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[888],cm[887],9),simde_mm_alignr_epi8(cm[932],cm[931],9))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[21]; dm=&dsimd[21];
  dm[22]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[21],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[19],cm[18],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[52],cm[51],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[153],cm[152],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[233],cm[232],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[275],cm[274],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[412],cm[411],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[456],cm[455],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[485],cm[484],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[550],cm[549],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[585],cm[584],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[664],cm[663],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[726],cm[725],11),simde_mm_xor_si128(cm[811],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[847],cm[846],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[883],cm[882],15),simde_mm_alignr_epi8(cm[936],cm[935],12))))))))))))))))));
  dm[44]=simde_mm_xor_si128(dsimd[43],simde_mm_xor_si128(dsimd[21],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[103],cm[102],1),simde_mm_xor_si128(cm[138],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[198],cm[197],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[237],cm[236],12),simde_mm_xor_si128(cm[315],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[371],cm[370],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[400],cm[399],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[495],cm[494],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[544],cm[543],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[637],cm[636],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[676],cm[675],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[721],cm[720],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[757],cm[756],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[837],cm[836],9),simde_mm_alignr_epi8(cm[943],cm[942],14))))))))))))))))));
  dm[66]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[21],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[7],cm[6],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[136],cm[135],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[194],cm[193],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[273],cm[272],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[319],cm[318],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[358],cm[357],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[447],cm[446],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[500],cm[499],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[550],cm[549],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[589],cm[588],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[628],cm[627],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[720],cm[719],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[762],cm[761],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[801],cm[800],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[888],cm[887],9),simde_mm_alignr_epi8(cm[932],cm[931],9))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*704,     d+k*352, 352);
    memcpy(kd+k*704+352, d+k*352, 352);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<22; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<283 + 6 sys]
     d2[110]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+283)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[549],c2[548],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[723],c2[722],7),simde_mm_alignr_epi8(c2[941],c2[940],2)))))));

//row: 8  [P0<<314+P2<<244 + 7 sys]
     d2[176]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+314)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1652)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[548],c2[547],8),simde_mm_xor_si128(c2[718],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[838],c2[837],1),simde_mm_alignr_epi8(c2[928],c2[927],11)))))))));

//row: 11  [P0<<338+P1<<192 + 5 sys]
     d2[242]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+338)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+896)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[541],c2[540],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[726],c2[725],15),simde_mm_alignr_epi8(c2[935],c2[934],6)))))));

//row: 13  [P1<<81 + 4 sys]
     d2[286]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+785)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[325],c2[324],9),simde_mm_alignr_epi8(c2[886],c2[885],14)))));

//row: 15  [P3<<113 + 5 sys]
     d2[330]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2225)),simde_mm_xor_si128(c2[11],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[441],c2[440],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[578],c2[577],1),c2[803])))));

//row: 16  [P0<<114 + 4 sys]
     d2[352]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+114)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[151],c2[150],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[505],c2[504],12),simde_mm_alignr_epi8(c2[901],c2[900],11)))));

//row: 20  [P0<<30 + 4 sys]
     d2[440]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+30)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[406],c2[405],11),simde_mm_alignr_epi8(c2[500],c2[499],4)))));

//row: 24  [P0<<194 + 4 sys]
     d2[528]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+194)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[143],c2[142],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[192],c2[191],9),c2[502]))));

//row: 29  [P3<<107 + 3 sys]
     d2[638]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2219)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[623],c2[622],8),simde_mm_alignr_epi8(c2[808],c2[807],3))));

//row: 30  [P2<<212 + 3 sys]
     d2[660]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1620)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],11),simde_mm_xor_si128(c2[441],simde_mm_alignr_epi8(c2[578],c2[577],15))));

//row: 31  [P0<<279+P3<<222 + 2 sys]
     d2[682]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+279)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2334)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],5),simde_mm_alignr_epi8(c2[327],c2[326],9))));

//row: 32  [P2<<83 + 3 sys]
     d2[704]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1491)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[545],c2[544],9),simde_mm_alignr_epi8(c2[638],c2[637],2))));

//row: 35  [P0<<268 + 3 sys]
     d2[770]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+268)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[278],c2[277],2),simde_mm_alignr_epi8(c2[541],c2[540],3))));

//row: 37  [P1<<85 + 2 sys]
     d2[814]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+789)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[64],c2[63],15),simde_mm_alignr_epi8(c2[587],c2[586],12)));

//row: 42  [P2<<17 + 2 sys]
     d2[924]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1425)),simde_mm_xor_si128(c2[18],simde_mm_alignr_epi8(c2[182],c2[181],3)));

//row: 43  [P3<<167 + 3 sys]
     d2[946]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2279)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[705],c2[704],3),simde_mm_alignr_epi8(c2[796],c2[795],5))));

//row: 44  [P0<<272 + 3 sys]
     d2[968]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+272)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[324],c2[323],4),simde_mm_alignr_epi8(c2[415],c2[414],5))));
  }
}
