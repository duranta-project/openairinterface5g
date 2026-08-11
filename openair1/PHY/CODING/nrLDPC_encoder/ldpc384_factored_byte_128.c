#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=384, byte encoding, factored
// 319 terms vs 2105 expanded (6.60x); pipeline lag 1 chunk(s)
static inline void ldpc384_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[3072] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<24; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[64],c2[63],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[117],c2[116],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[168],c2[167],1),simde_mm_xor_si128(c2[144],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[210],c2[209],3),simde_mm_xor_si128(c2[210],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[213],c2[212],12),simde_mm_xor_si128(c2[249],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[252],c2[251],5),simde_mm_xor_si128(c2[256],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[299],c2[298],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[301],c2[300],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[302],c2[301],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[346],c2[345],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[353],c2[352],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[357],c2[356],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[388],c2[387],8),simde_mm_xor_si128(c2[394],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[405],c2[404],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[436],c2[435],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[444],c2[443],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[452],c2[451],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[489],c2[488],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[489],c2[488],4),simde_mm_xor_si128(c2[498],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[535],c2[534],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[547],c2[546],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[548],c2[547],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[578],c2[577],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[591],c2[590],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[598],c2[597],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[637],c2[636],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[646],c2[645],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[647],c2[646],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[678],c2[677],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[686],c2[685],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[686],c2[685],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[724],c2[723],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[727],c2[726],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[734],c2[733],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[775],c2[774],10),simde_mm_xor_si128(c2[787],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[791],c2[790],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[824],c2[823],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[825],c2[824],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[835],c2[834],12),simde_mm_xor_si128(c2[879],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[880],c2[879],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[881],c2[880],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[924],c2[923],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[925],c2[924],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[933],c2[932],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[961],c2[960],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[963],c2[962],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[981],c2[980],10),simde_mm_xor_si128(c2[1015],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1030],c2[1029],10),simde_mm_alignr_epi8(c2[1031],c2[1030],5)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[96]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],12),simde_mm_alignr_epi8(c2[60],c2[59],5));

//row: 6  [direct, 8 terms]
     d2[144]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[305],c2[304],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[481],c2[480],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[550],c2[549],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[630],c2[629],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[832],c2[831],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[866],c2[865],2),simde_mm_alignr_epi8(c2[975],c2[974],1))))))));

//row: 7  [direct, 6 terms]
     d2[168]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[212],c2[211],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[357],c2[356],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[403],c2[402],2),simde_mm_alignr_epi8(c2[680],c2[679],2))))));

//row: 9  [direct, 8 terms]
     d2[216]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[501],c2[500],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[537],c2[536],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[628],c2[627],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[835],c2[834],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[868],c2[867],15),simde_mm_alignr_epi8(c2[966],c2[965],2))))))));

//row: 10  [direct, 6 terms]
     d2[240]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[118],c2[117],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[210],c2[209],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[343],c2[342],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[408],c2[407],15),simde_mm_alignr_epi8(c2[695],c2[694],2))))));

//row: 12  [direct, 6 terms]
     d2[288]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[491],c2[490],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[543],c2[542],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[628],c2[627],2),simde_mm_alignr_epi8(c2[869],c2[868],10))))));

//row: 14  [direct, 6 terms]
     d2[336]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[592],c2[591],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[729],c2[728],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[774],c2[773],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[838],c2[837],11),simde_mm_alignr_epi8(c2[1009],c2[1008],12))))));

//row: 17  [direct, 5 terms]
     d2[408]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[691],c2[690],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[774],c2[773],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[839],c2[838],6),simde_mm_alignr_epi8(c2[1032],c2[1031],7)))));

//row: 18  [direct, 5 terms]
     d2[432]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[587],c2[586],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[642],c2[641],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[873],c2[872],4),simde_mm_alignr_epi8(c2[913],c2[912],4)))));

//row: 19  [direct, 5 terms]
     d2[456]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[358],c2[357],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[400],c2[399],2),simde_mm_alignr_epi8(c2[493],c2[492],5)))));

//row: 21  [direct, 5 terms]
     d2[504]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[247],c2[246],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[789],c2[788],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[974],c2[973],5),simde_mm_alignr_epi8(c2[1015],c2[1014],1)))));

//row: 22  [direct, 4 terms]
     d2[528]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[577],c2[576],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[639],c2[638],9),simde_mm_alignr_epi8(c2[818],c2[817],6))));

//row: 23  [direct, 4 terms]
     d2[552]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[484],c2[483],13),simde_mm_alignr_epi8(c2[866],c2[865],11))));

//row: 25  [direct, 4 terms]
     d2[600]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[290],c2[289],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[360],c2[359],15),simde_mm_alignr_epi8(c2[692],c2[691],8))));

//row: 26  [direct, 4 terms]
     d2[624]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[197],c2[196],12),simde_mm_alignr_epi8(c2[729],c2[728],8))));

//row: 27  [direct, 3 terms]
     d2[648]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[301],c2[300],2),simde_mm_alignr_epi8(c2[391],c2[390],5)));

//row: 28  [direct, 4 terms]
     d2[672]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[194],c2[193],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[928],c2[927],4),simde_mm_alignr_epi8(c2[1026],c2[1025],2))));

//row: 33  [direct, 4 terms]
     d2[792]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[97],c2[96],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[551],c2[550],9),simde_mm_alignr_epi8(c2[1015],c2[1014],9))));

//row: 34  [direct, 4 terms]
     d2[816]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[346],c2[345],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[733],c2[732],10),simde_mm_alignr_epi8(c2[836],c2[835],8))));

//row: 36  [direct, 4 terms]
     d2[864]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[692],c2[691],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[739],c2[738],9),simde_mm_alignr_epi8(c2[866],c2[865],5))));

//row: 38  [direct, 4 terms]
     d2[912]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[444],c2[443],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[499],c2[498],1),simde_mm_alignr_epi8(c2[590],c2[589],6))));

//row: 39  [direct, 4 terms]
     d2[936]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[150],c2[149],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[358],c2[357],10),simde_mm_alignr_epi8(c2[931],c2[930],9))));

//row: 40  [direct, 3 terms]
     d2[960]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[387],c2[386],5),simde_mm_alignr_epi8(c2[836],c2[835],8)));

//row: 41  [direct, 4 terms]
     d2[984]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[52],c2[51],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[164],c2[163],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[441],c2[440],11),c2[882])));

//row: 45  [direct, 3 terms]
     d2[1080]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[298],c2[297],5),simde_mm_alignr_epi8(c2[481],c2[480],15)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[24]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[20],cm[19],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[168],cm[167],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[252],cm[251],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[302],cm[301],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[452],cm[451],13),simde_mm_xor_si128(cm[498],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[535],cm[534],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[578],cm[577],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[647],cm[646],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[734],cm[733],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[775],cm[774],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[880],cm[879],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[924],cm[923],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[981],cm[980],10),simde_mm_alignr_epi8(cm[1030],cm[1029],10))))))))))))))))));
       dm[48]=simde_mm_xor_si128(dm[24],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[149],cm[148],9),simde_mm_xor_si128(cm[210],simde_mm_xor_si128(cm[249],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[357],cm[356],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[405],cm[404],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[444],cm[443],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[547],cm[546],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[598],cm[597],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[686],cm[685],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[727],cm[726],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[791],cm[790],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[824],cm[823],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[933],cm[932],11),cm[1015])))))))))))))))));
       dm[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[54],cm[53],7),simde_mm_xor_si128(cm[144],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[210],cm[209],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[301],cm[300],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[346],cm[345],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[388],cm[387],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[489],cm[488],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[548],cm[547],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[591],cm[590],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[646],cm[645],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[686],cm[685],4),simde_mm_xor_si128(cm[787],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[835],cm[834],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[881],cm[880],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[963],cm[962],7),simde_mm_alignr_epi8(cm[1031],cm[1030],5))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[23]; dm=&dsimd[23];
  dm[24]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[23],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[20],cm[19],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[168],cm[167],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[252],cm[251],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[302],cm[301],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[452],cm[451],13),simde_mm_xor_si128(cm[498],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[535],cm[534],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[578],cm[577],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[647],cm[646],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[734],cm[733],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[775],cm[774],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[880],cm[879],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[924],cm[923],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[981],cm[980],10),simde_mm_alignr_epi8(cm[1030],cm[1029],10))))))))))))))))));
  dm[48]=simde_mm_xor_si128(dsimd[47],simde_mm_xor_si128(dsimd[23],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[149],cm[148],9),simde_mm_xor_si128(cm[210],simde_mm_xor_si128(cm[249],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[357],cm[356],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[405],cm[404],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[444],cm[443],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[547],cm[546],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[598],cm[597],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[686],cm[685],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[727],cm[726],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[791],cm[790],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[824],cm[823],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[933],cm[932],11),cm[1015])))))))))))))))));
  dm[72]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[23],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[18],cm[17],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[54],cm[53],7),simde_mm_xor_si128(cm[144],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[210],cm[209],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[301],cm[300],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[346],cm[345],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[388],cm[387],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[489],cm[488],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[548],cm[547],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[591],cm[590],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[646],cm[645],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[686],cm[685],4),simde_mm_xor_si128(cm[787],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[835],cm[834],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[881],cm[880],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[963],cm[962],7),simde_mm_alignr_epi8(cm[1031],cm[1030],5))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*768,     d+k*384, 384);
    memcpy(kd+k*768+384, d+k*384, 384);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<24; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<157 + 6 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+157)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[152],c2[151],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[587],c2[586],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[784],c2[783],1),simde_mm_alignr_epi8(c2[1012],c2[1011],3)))))));

//row: 8  [P0<<67+P2<<170 + 7 sys]
     d2[192]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+67)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1706)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[155],c2[154],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[578],c2[577],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[771],c2[770],7),simde_mm_xor_si128(c2[926],c2[1031]))))))));

//row: 11  [P0<<334+P1<<115 + 5 sys]
     d2[264]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+334)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+883)),simde_mm_xor_si128(c2[3],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[577],c2[576],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[771],c2[770],15),simde_mm_alignr_epi8(c2[1020],c2[1019],12)))))));

//row: 13  [P1<<370 + 4 sys]
     d2[312]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1138)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[156],c2[155],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[353],c2[352],10),simde_mm_alignr_epi8(c2[968],c2[967],3)))));

//row: 15  [P3<<269 + 5 sys]
     d2[360]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2573)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[494],c2[493],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[644],c2[643],14),simde_mm_alignr_epi8(c2[868],c2[867],7))))));

//row: 16  [P0<<57 + 4 sys]
     d2[384]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+57)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[166],c2[165],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[532],c2[531],9),simde_mm_alignr_epi8(c2[979],c2[978],1)))));

//row: 20  [P0<<59 + 4 sys]
     d2[480]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+59)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[157],c2[156],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[449],c2[448],8),simde_mm_alignr_epi8(c2[550],c2[549],5)))));

//row: 24  [P0<<234 + 4 sys]
     d2[576]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+234)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[154],c2[153],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[207],c2[206],11),simde_mm_alignr_epi8(c2[550],c2[549],3)))));

//row: 29  [P3<<78 + 3 sys]
     d2[696]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2382)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[64],c2[63],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[673],c2[672],5),simde_mm_alignr_epi8(c2[874],c2[873],3))));

//row: 30  [P2<<90 + 3 sys]
     d2[720]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1626)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[495],c2[494],5),simde_mm_alignr_epi8(c2[641],c2[640],4))));

//row: 31  [P0<<258+P3<<256 + 2 sys]
     d2[744]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+258)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2560)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],4),simde_mm_alignr_epi8(c2[350],c2[349],7))));

//row: 32  [P2<<287 + 3 sys]
     d2[768]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1823)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[589],c2[588],9),simde_mm_alignr_epi8(c2[683],c2[682],15))));

//row: 35  [P0<<266 + 3 sys]
     d2[840]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+266)),simde_mm_xor_si128(c2[68],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[309],c2[308],15),simde_mm_alignr_epi8(c2[577],c2[576],2))));

//row: 37  [P1<<115 + 2 sys]
     d2[888]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+883)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],13),simde_mm_alignr_epi8(c2[630],c2[629],2)));

//row: 42  [P2<<218 + 2 sys]
     d2[1008]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1754)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],1),simde_mm_alignr_epi8(c2[193],c2[192],14)));

//row: 43  [P3<<168 + 3 sys]
     d2[1032]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2472)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[56],c2[55],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[777],c2[776],4),simde_mm_alignr_epi8(c2[872],c2[871],2))));

//row: 44  [P0<<274 + 3 sys]
     d2[1056]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+274)),simde_mm_xor_si128(c2[5],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[341],c2[340],14),simde_mm_alignr_epi8(c2[443],c2[442],3))));
  }
}
