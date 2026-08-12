#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG1 Zc=80, byte encoding, factored
// 319 terms vs 2095 expanded (6.57x); pipeline lag 1 chunk(s)
static inline void ldpc80_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[640] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<5; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],15),simde_mm_xor_si128(c2[13],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],6),simde_mm_xor_si128(c2[20],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],6),simde_mm_xor_si128(c2[50],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[64],c2[63],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[83],c2[82],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[95],c2[94],7),simde_mm_xor_si128(c2[90],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[131],c2[130],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[143],c2[142],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[144],c2[143],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[152],c2[151],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[154],c2[153],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[154],c2[153],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[162],c2[161],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[163],c2[162],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[165],c2[164],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[171],c2[170],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[172],c2[171],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[172],c2[171],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[181],c2[180],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[185],c2[184],13),simde_mm_xor_si128(c2[191],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[192],c2[191],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[195],c2[194],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[202],c2[201],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[205],c2[204],15),simde_mm_xor_si128(c2[212],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[214],c2[213],13),simde_mm_alignr_epi8(c2[215],c2[214],4)))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))))));

//row: 4  [direct, 2 terms]
     d2[20]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],9),simde_mm_alignr_epi8(c2[13],c2[12],13));

//row: 6  [direct, 8 terms]
     d2[30]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[111],c2[110],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[135],c2[134],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[174],c2[173],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[184],c2[183],10),simde_mm_alignr_epi8(c2[205],c2[204],11))))))));

//row: 7  [direct, 6 terms]
     d2[35]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],9),simde_mm_alignr_epi8(c2[145],c2[144],12))))));

//row: 9  [direct, 8 terms]
     d2[45]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[113],c2[112],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[135],c2[134],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[172],c2[171],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[184],c2[183],7),simde_mm_alignr_epi8(c2[204],c2[203],1))))))));

//row: 10  [direct, 6 terms]
     d2[50]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],14),simde_mm_xor_si128(c2[20],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],1),simde_mm_alignr_epi8(c2[145],c2[144],7))))));

//row: 12  [direct, 6 terms]
     d2[60]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],9),simde_mm_xor_si128(c2[113],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],9),simde_mm_alignr_epi8(c2[184],c2[183],4))))));

//row: 14  [direct, 6 terms]
     d2[70]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[154],c2[153],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[164],c2[163],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[172],c2[171],3),simde_mm_alignr_epi8(c2[212],c2[211],2))))));

//row: 17  [direct, 5 terms]
     d2[85]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],3),simde_mm_xor_si128(c2[163],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[174],c2[173],3),simde_mm_alignr_epi8(c2[215],c2[214],4)))));

//row: 18  [direct, 5 terms]
     d2[90]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[184],c2[183],13),simde_mm_alignr_epi8(c2[194],c2[193],7)))));

//row: 19  [direct, 5 terms]
     d2[95]=simde_mm_xor_si128(c2[4],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[72],c2[71],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],14),simde_mm_alignr_epi8(c2[103],c2[102],9)))));

//row: 21  [direct, 5 terms]
     d2[105]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[164],c2[163],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],11),simde_mm_alignr_epi8(c2[212],c2[211],7)))));

//row: 22  [direct, 4 terms]
     d2[110]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],7),simde_mm_alignr_epi8(c2[175],c2[174],12))));

//row: 23  [direct, 4 terms]
     d2[115]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],5),simde_mm_alignr_epi8(c2[182],c2[181],9))));

//row: 25  [direct, 4 terms]
     d2[125]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[64],c2[63],7),simde_mm_xor_si128(c2[71],simde_mm_alignr_epi8(c2[143],c2[142],14))));

//row: 26  [direct, 4 terms]
     d2[130]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],14),simde_mm_alignr_epi8(c2[155],c2[154],3))));

//row: 27  [direct, 3 terms]
     d2[135]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],13),c2[84]));

//row: 28  [direct, 4 terms]
     d2[140]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[194],c2[193],2),simde_mm_alignr_epi8(c2[215],c2[214],10))));

//row: 33  [direct, 4 terms]
     d2[165]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],12),simde_mm_alignr_epi8(c2[213],c2[212],1))));

//row: 34  [direct, 4 terms]
     d2[170]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[151],c2[150],5),simde_mm_alignr_epi8(c2[173],c2[172],12))));

//row: 36  [direct, 4 terms]
     d2[180]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[142],c2[141],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[152],c2[151],2),c2[180])));

//row: 38  [direct, 4 terms]
     d2[190]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[92],c2[91],3),simde_mm_xor_si128(c2[104],simde_mm_alignr_epi8(c2[122],c2[121],5))));

//row: 39  [direct, 4 terms]
     d2[195]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],13),simde_mm_xor_si128(c2[72],c2[193])));

//row: 40  [direct, 3 terms]
     d2[200]=simde_mm_xor_si128(c2[2],simde_mm_xor_si128(c2[80],simde_mm_alignr_epi8(c2[173],c2[172],5)));

//row: 41  [direct, 4 terms]
     d2[205]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[93],c2[92],12),simde_mm_alignr_epi8(c2[183],c2[182],15))));

//row: 45  [direct, 3 terms]
     d2[225]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],4),simde_mm_alignr_epi8(c2[103],c2[102],14)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[5]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[11],cm[10],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[22],cm[21],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[34],cm[33],1),simde_mm_xor_si128(cm[50],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[63],cm[62],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[91],cm[90],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[121],cm[120],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[134],cm[133],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[154],cm[153],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[162],cm[161],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[183],cm[182],1),simde_mm_xor_si128(cm[191],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[202],cm[201],13),cm[212])))))))))))))))));
       dm[10]=simde_mm_xor_si128(dm[5],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[24],cm[23],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[42],cm[41],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],4),simde_mm_xor_si128(cm[90],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[124],cm[123],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[145],cm[144],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[152],cm[151],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[165],cm[164],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[171],cm[170],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[192],cm[191],4),simde_mm_alignr_epi8(cm[214],cm[213],13))))))))))))))))));
       dm[15]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],12),simde_mm_xor_si128(cm[13],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[43],cm[42],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[64],cm[63],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[71],cm[70],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[85],cm[84],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[103],cm[102],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[121],cm[120],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[144],cm[143],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[163],cm[162],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[172],cm[171],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[185],cm[184],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[205],cm[204],15),simde_mm_alignr_epi8(cm[215],cm[214],4))))))))))))))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[4]; dm=&dsimd[4];
  dm[5]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[4],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[11],cm[10],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[22],cm[21],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[34],cm[33],1),simde_mm_xor_si128(cm[50],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[63],cm[62],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[91],cm[90],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[121],cm[120],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[134],cm[133],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[154],cm[153],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[162],cm[161],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[183],cm[182],1),simde_mm_xor_si128(cm[191],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[202],cm[201],13),cm[212])))))))))))))))));
  dm[10]=simde_mm_xor_si128(dsimd[9],simde_mm_xor_si128(dsimd[4],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[24],cm[23],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[42],cm[41],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],4),simde_mm_xor_si128(cm[90],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[124],cm[123],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[145],cm[144],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[152],cm[151],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[165],cm[164],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[171],cm[170],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[192],cm[191],4),simde_mm_alignr_epi8(cm[214],cm[213],13))))))))))))))))));
  dm[15]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[4],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],12),simde_mm_xor_si128(cm[13],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[43],cm[42],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[64],cm[63],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[71],cm[70],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[85],cm[84],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[103],cm[102],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[121],cm[120],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[131],cm[130],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[144],cm[143],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[163],cm[162],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[172],cm[171],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[185],cm[184],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[205],cm[204],15),simde_mm_alignr_epi8(cm[215],cm[214],4))))))))))))))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*160,     d+k*80, 80);
    memcpy(kd+k*160+80, d+k*80, 80);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<5; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 5  [P0<<39 + 6 sys]
     d2[25]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+39)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[125],c2[124],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[163],c2[162],9),simde_mm_alignr_epi8(c2[212],c2[211],11)))))));

//row: 8  [P0<<5+P2<<74 + 7 sys]
     d2[40]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+5)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+394)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[125],c2[124],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[164],c2[163],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[192],c2[191],14),simde_mm_alignr_epi8(c2[212],c2[211],13)))))))));

//row: 11  [P0<<43+P1<<40 + 5 sys]
     d2[55]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+43)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+200)),simde_mm_xor_si128(c2[1],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[164],c2[163],1),simde_mm_alignr_epi8(c2[212],c2[211],1)))))));

//row: 13  [P1<<37 + 4 sys]
     d2[65]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+197)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],15),c2[200]))));

//row: 15  [P3<<51 + 5 sys]
     d2[75]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+531)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[104],c2[103],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],2),simde_mm_alignr_epi8(c2[182],c2[181],8))))));

//row: 16  [P0<<60 + 4 sys]
     d2[80]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+60)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[113],c2[112],13),simde_mm_alignr_epi8(c2[203],c2[202],3)))));

//row: 20  [P0<<10 + 4 sys]
     d2[100]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+10)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[93],c2[92],8),simde_mm_alignr_epi8(c2[114],c2[113],2)))));

//row: 24  [P0<<41 + 4 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+41)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],14),simde_mm_alignr_epi8(c2[112],c2[111],11)))));

//row: 29  [P3<<29 + 3 sys]
     d2[145]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+509)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[145],c2[144],4),simde_mm_alignr_epi8(c2[183],c2[182],5))));

//row: 30  [P2<<55 + 3 sys]
     d2[150]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+375)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],7),simde_mm_alignr_epi8(c2[132],c2[131],9))));

//row: 31  [P0<<66+P3<<2 + 2 sys]
     d2[155]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+66)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+482)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],14),simde_mm_alignr_epi8(c2[75],c2[74],4))));

//row: 32  [P2<<57 + 3 sys]
     d2[160]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+377)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],6),simde_mm_alignr_epi8(c2[142],c2[141],15))));

//row: 35  [P0<<45 + 3 sys]
     d2[175]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+45)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],14),simde_mm_alignr_epi8(c2[124],c2[123],7))));

//row: 37  [P1<<35 + 2 sys]
     d2[185]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+195)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],10),simde_mm_alignr_epi8(c2[131],c2[130],15)));

//row: 42  [P2<<46 + 2 sys]
     d2[210]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+366)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],2),simde_mm_alignr_epi8(c2[45],c2[44],1)));

//row: 43  [P3<<22 + 3 sys]
     d2[215]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+502)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[165],c2[164],5),c2[181])));

//row: 44  [P0<<20 + 3 sys]
     d2[220]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+20)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],3),simde_mm_alignr_epi8(c2[92],c2[91],3))));
  }
}
