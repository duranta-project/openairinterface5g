#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=256, byte encoding, factored
// 319 terms vs 2099 expanded (6.58x); pipeline lag 1 chunk(s)
static inline void ldpc256_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[2048] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<16; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[76],c2[75],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[79],c2[78],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[79],c2[78],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[106],c2[105],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[130],c2[129],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[136],c2[135],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[165],c2[164],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[167],c2[166],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[168],c2[167],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[193],c2[192],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[198],c2[197],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[202],c2[201],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[233],c2[232],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[238],c2[237],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[239],c2[238],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[263],c2[262],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[268],c2[267],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[272],c2[271],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[292],c2[291],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[294],c2[293],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[299],c2[298],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[323],c2[322],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[329],c2[328],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[335],c2[334],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[358],c2[357],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[359],c2[358],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[366],c2[365],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[391],c2[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[396],c2[395],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[400],c2[399],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[417],c2[416],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[425],c2[424],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[430],c2[429],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[455],c2[454],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[462],c2[461],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[463],c2[462],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[489],c2[488],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[493],c2[492],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[495],c2[494],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[514],c2[513],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[521],c2[520],14),simde_mm_xor_si128(c2[527],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[549],c2[548],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[554],c2[553],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[560],c2[559],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[588],c2[587],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[589],c2[588],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[592],c2[591],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[611],c2[610],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[624],c2[623],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[624],c2[623],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[648],c2[647],5),simde_mm_xor_si128(c2[649],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[655],c2[654],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[673],c2[672],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[674],c2[673],12),simde_mm_alignr_epi8(c2[674],c2[673],15)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[64]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],13),simde_mm_alignr_epi8(c2[39],c2[38],6));

//row: 6  [direct, 8 terms]
     d2[96]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[194],c2[193],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[322],c2[321],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[357],c2[356],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[432],c2[431],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[545],c2[544],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[586],c2[585],13),simde_mm_alignr_epi8(c2[654],c2[653],3))))))));

//row: 7  [direct, 6 terms]
     d2[112]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[138],c2[137],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[226],c2[225],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[267],c2[266],7),simde_mm_alignr_epi8(c2[455],c2[454],8))))));

//row: 9  [direct, 8 terms]
     d2[144]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[327],c2[326],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[354],c2[353],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[425],c2[424],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[545],c2[544],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[580],c2[579],13),simde_mm_alignr_epi8(c2[654],c2[653],8))))))));

//row: 10  [direct, 6 terms]
     d2[160]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[139],c2[138],7),simde_mm_xor_si128(c2[234],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[260],c2[259],1),simde_mm_alignr_epi8(c2[452],c2[451],10))))));

//row: 12  [direct, 6 terms]
     d2[192]=simde_mm_xor_si128(c2[10],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[322],c2[321],5),simde_mm_xor_si128(c2[354],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[431],c2[430],10),simde_mm_alignr_epi8(c2[577],c2[576],7))))));

//row: 14  [direct, 6 terms]
     d2[224]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[388],c2[387],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[493],c2[492],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[520],c2[519],15),simde_mm_xor_si128(c2[545],simde_mm_alignr_epi8(c2[687],c2[686],5))))));

//row: 17  [direct, 5 terms]
     d2[272]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[459],c2[458],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[516],c2[515],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[545],c2[544],1),c2[681]))));

//row: 18  [direct, 5 terms]
     d2[288]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[399],c2[398],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[417],c2[416],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[586],c2[585],11),simde_mm_alignr_epi8(c2[618],c2[617],3)))));

//row: 19  [direct, 5 terms]
     d2[304]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[229],c2[228],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[264],c2[263],15),c2[334]))));

//row: 21  [direct, 5 terms]
     d2[336]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[168],c2[167],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[519],c2[518],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[649],c2[648],3),simde_mm_alignr_epi8(c2[683],c2[682],11)))));

//row: 22  [direct, 4 terms]
     d2[352]=simde_mm_xor_si128(c2[4],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[393],c2[392],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[428],c2[427],12),simde_mm_alignr_epi8(c2[554],c2[553],14))));

//row: 23  [direct, 4 terms]
     d2[368]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[331],c2[330],10),simde_mm_alignr_epi8(c2[586],c2[585],8))));

//row: 25  [direct, 4 terms]
     d2[400]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[201],c2[200],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[232],c2[231],4),simde_mm_alignr_epi8(c2[460],c2[459],6))));

//row: 26  [direct, 4 terms]
     d2[416]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[142],c2[141],7),simde_mm_alignr_epi8(c2[484],c2[483],13))));

//row: 27  [direct, 3 terms]
     d2[432]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[199],c2[198],8),simde_mm_alignr_epi8(c2[269],c2[268],2)));

//row: 28  [direct, 4 terms]
     d2[448]=simde_mm_xor_si128(c2[8],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[139],c2[138],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[620],c2[619],5),simde_mm_alignr_epi8(c2[676],c2[675],15))));

//row: 33  [direct, 4 terms]
     d2[528]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[76],c2[75],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[355],c2[354],9),simde_mm_alignr_epi8(c2[686],c2[685],3))));

//row: 34  [direct, 4 terms]
     d2[544]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[235],c2[234],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[491],c2[490],4),simde_mm_alignr_epi8(c2[554],c2[553],15))));

//row: 36  [direct, 4 terms]
     d2[576]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[455],c2[454],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[484],c2[483],3),simde_mm_alignr_epi8(c2[584],c2[583],8))));

//row: 38  [direct, 4 terms]
     d2[608]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[298],c2[297],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[330],c2[329],13),simde_mm_alignr_epi8(c2[395],c2[394],3))));

//row: 39  [direct, 4 terms]
     d2[624]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[234],c2[233],5),c2[608])));

//row: 40  [direct, 3 terms]
     d2[640]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[265],c2[264],9),simde_mm_alignr_epi8(c2[554],c2[553],5)));

//row: 41  [direct, 4 terms]
     d2[656]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[107],c2[106],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[297],c2[296],11),simde_mm_alignr_epi8(c2[586],c2[585],7))));

//row: 45  [direct, 3 terms]
     d2[720]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[202],c2[201],7),simde_mm_alignr_epi8(c2[331],c2[330],7)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[16]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[16],cm[15],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[106],cm[105],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[167],cm[166],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[193],cm[192],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[335],cm[334],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[359],cm[358],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[396],cm[395],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[417],cm[416],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[493],cm[492],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[514],cm[513],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[588],cm[587],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[611],cm[610],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[655],cm[654],15),simde_mm_alignr_epi8(cm[674],cm[673],15))))))))))))))))));
       dm[32]=simde_mm_xor_si128(dm[16],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[104],cm[103],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[136],cm[135],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[165],cm[164],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[238],cm[237],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[263],cm[262],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[299],cm[298],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[366],cm[365],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[455],cm[454],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[489],cm[488],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[521],cm[520],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[554],cm[553],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[624],cm[623],15),simde_mm_alignr_epi8(cm[674],cm[673],12))))))))))))))))));
       dm[48]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[102],cm[101],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[130],cm[129],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[202],cm[201],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[233],cm[232],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[272],cm[271],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[329],cm[328],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[358],cm[357],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[400],cm[399],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[430],cm[429],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[462],cm[461],3),simde_mm_xor_si128(cm[527],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[549],cm[548],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[592],cm[591],4),simde_mm_xor_si128(cm[649],simde_mm_alignr_epi8(cm[673],cm[672],12))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[15]; dm=&dsimd[15];
  dm[16]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[15],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[16],cm[15],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[106],cm[105],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[167],cm[166],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[193],cm[192],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[292],cm[291],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[335],cm[334],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[359],cm[358],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[396],cm[395],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[417],cm[416],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[493],cm[492],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[514],cm[513],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[588],cm[587],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[611],cm[610],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[655],cm[654],15),simde_mm_alignr_epi8(cm[674],cm[673],15))))))))))))))))));
  dm[32]=simde_mm_xor_si128(dsimd[31],simde_mm_xor_si128(dsimd[15],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[104],cm[103],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[136],cm[135],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[165],cm[164],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[238],cm[237],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[263],cm[262],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[299],cm[298],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[366],cm[365],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[391],cm[390],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[455],cm[454],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[489],cm[488],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[521],cm[520],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[554],cm[553],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[624],cm[623],15),simde_mm_alignr_epi8(cm[674],cm[673],12))))))))))))))))));
  dm[48]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[15],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[102],cm[101],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[130],cm[129],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[202],cm[201],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[233],cm[232],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[272],cm[271],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[329],cm[328],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[358],cm[357],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[400],cm[399],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[430],cm[429],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[462],cm[461],3),simde_mm_xor_si128(cm[527],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[549],cm[548],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[592],cm[591],4),simde_mm_xor_si128(cm[649],simde_mm_alignr_epi8(cm[673],cm[672],12))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*512,     d+k*256, 256);
    memcpy(kd+k*512+256, d+k*256, 256);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<16; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<115 + 6 sys]
     d2[80]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+115)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[47],c2[46],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[109],c2[108],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[399],c2[398],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[514],c2[513],12),simde_mm_alignr_epi8(c2[680],c2[679],11)))))));

//row: 8  [P0<<241+P2<<90 + 7 sys]
     d2[128]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+241)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1114)),simde_mm_xor_si128(c2[7],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[97],c2[96],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[398],c2[397],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[519],c2[518],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[619],c2[618],4),simde_mm_alignr_epi8(c2[679],c2[678],13)))))))));

//row: 11  [P0<<252+P1<<22 + 5 sys]
     d2[176]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+252)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+534)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[390],c2[389],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[524],c2[523],6),simde_mm_alignr_epi8(c2[677],c2[676],14)))))));

//row: 13  [P1<<62 + 4 sys]
     d2[208]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+574)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[112],c2[111],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[234],c2[233],7),simde_mm_alignr_epi8(c2[652],c2[651],9)))));

//row: 15  [P3<<179 + 5 sys]
     d2[240]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1715)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],8),simde_mm_xor_si128(c2[38],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[325],c2[324],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[420],c2[419],15),simde_mm_alignr_epi8(c2[581],c2[580],11))))));

//row: 16  [P0<<154 + 4 sys]
     d2[256]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+154)),simde_mm_xor_si128(c2[36],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[356],c2[355],1),simde_mm_alignr_epi8(c2[644],c2[643],3)))));

//row: 20  [P0<<160 + 4 sys]
     d2[320]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+160)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[108],c2[107],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[302],c2[301],9),simde_mm_alignr_epi8(c2[355],c2[354],15)))));

//row: 24  [P0<<222 + 4 sys]
     d2[384]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+222)),simde_mm_xor_si128(c2[7],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[143],c2[142],12),simde_mm_alignr_epi8(c2[360],c2[359],4)))));

//row: 29  [P3<<6 + 3 sys]
     d2[464]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1542)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[463],c2[462],12),simde_mm_alignr_epi8(c2[582],c2[581],4))));

//row: 30  [P2<<9 + 3 sys]
     d2[480]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1033)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[325],c2[324],9),simde_mm_alignr_epi8(c2[424],c2[423],8))));

//row: 31  [P0<<172+P3<<61 + 2 sys]
     d2[496]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+172)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1597)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],15),simde_mm_alignr_epi8(c2[236],c2[235],1))));

//row: 32  [P2<<121 + 3 sys]
     d2[512]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1145)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],13),simde_mm_xor_si128(c2[391],simde_mm_alignr_epi8(c2[461],c2[460],7))));

//row: 35  [P0<<103 + 3 sys]
     d2[560]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+103)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[205],c2[204],5),simde_mm_alignr_epi8(c2[397],c2[396],15))));

//row: 37  [P1<<122 + 2 sys]
     d2[592]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+634)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],6),simde_mm_alignr_epi8(c2[430],c2[429],12)));

//row: 42  [P2<<137 + 2 sys]
     d2[672]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1161)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],5),simde_mm_alignr_epi8(c2[138],c2[137],13)));

//row: 43  [P3<<139 + 3 sys]
     d2[688]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1675)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[523],c2[522],3),simde_mm_alignr_epi8(c2[587],c2[586],13))));

//row: 44  [P0<<173 + 3 sys]
     d2[704]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+173)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[234],c2[233],13),simde_mm_alignr_epi8(c2[299],c2[298],3))));
  }
}
