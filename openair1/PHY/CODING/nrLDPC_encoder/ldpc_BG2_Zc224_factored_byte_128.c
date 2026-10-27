#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=224, byte encoding, factored
// 173 terms vs 1473 expanded (8.51x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc224_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1792] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<14; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[58],c2[57],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[94],c2[93],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[96],c2[95],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[120],c2[119],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[148],c2[147],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[150],c2[149],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[174],c2[173],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[176],c2[175],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[176],c2[175],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[203],c2[202],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[226],c2[225],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[230],c2[229],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[234],c2[233],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[253],c2[252],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[254],c2[253],15),simde_mm_alignr_epi8(c2[261],c2[260],5)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[140]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[169],c2[168],8),simde_mm_alignr_epi8(c2[200],c2[199],8))));

//row: 18  [direct, 3 terms]
     d2[252]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],4),simde_mm_xor_si128(c2[169],simde_mm_alignr_epi8(c2[206],c2[205],12)));

//row: 22  [direct, 2 terms]
     d2[308]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],1),simde_mm_alignr_epi8(c2[57],c2[56],1));

//row: 23  [direct, 3 terms]
     d2[322]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],6),simde_mm_alignr_epi8(c2[149],c2[148],6)));

//row: 24  [direct, 3 terms]
     d2[336]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],8),simde_mm_alignr_epi8(c2[256],c2[255],15)));

//row: 25  [direct, 2 terms]
     d2[350]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],3),simde_mm_alignr_epi8(c2[146],c2[145],14));

//row: 27  [direct, 2 terms]
     d2[378]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],11),simde_mm_alignr_epi8(c2[176],c2[175],6));

//row: 28  [direct, 3 terms]
     d2[392]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[70],c2[69],4),simde_mm_alignr_epi8(c2[153],c2[152],1)));

//row: 29  [direct, 2 terms]
     d2[406]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],12),simde_mm_alignr_epi8(c2[116],c2[115],13));

//row: 30  [direct, 4 terms]
     d2[420]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[143],c2[142],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[208],c2[207],9),c2[263])));

//row: 36  [direct, 3 terms]
     d2[504]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],15),simde_mm_alignr_epi8(c2[202],c2[201],3)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[14]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[35],cm[34],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[58],cm[57],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[96],cm[95],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[174],cm[173],15),simde_mm_alignr_epi8(cm[253],cm[252],8)))))));
       dm[28]=simde_mm_xor_si128(dm[14],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[94],cm[93],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[120],cm[119],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[150],cm[149],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[176],cm[175],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[226],cm[225],13),simde_mm_alignr_epi8(cm[254],cm[253],15)))))))));
       dm[42]=simde_mm_xor_si128(dm[28],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[10],cm[9],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[124],cm[123],15),simde_mm_alignr_epi8(cm[230],cm[229],11)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[13]; dm=&dsimd[13];
  dm[14]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[13],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[35],cm[34],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[58],cm[57],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[96],cm[95],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[174],cm[173],15),simde_mm_alignr_epi8(cm[253],cm[252],8)))))));
  dm[28]=simde_mm_xor_si128(dsimd[27],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[94],cm[93],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[120],cm[119],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[150],cm[149],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[176],cm[175],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[226],cm[225],13),simde_mm_alignr_epi8(cm[254],cm[253],15)))))))));
  dm[42]=simde_mm_xor_si128(dsimd[41],simde_mm_xor_si128(dsimd[13],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[10],cm[9],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[87],cm[86],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[124],cm[123],15),simde_mm_alignr_epi8(cm[230],cm[229],11)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*448,     d+k*224, 224);
    memcpy(kd+k*448+224, d+k*224, 224);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<14; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<101 + 2 sys]
     d2[56]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+549)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],8),c2[29]));

//row: 5  [P1<<193 + 4 sys]
     d2[70]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+641)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[151],c2[150],10),simde_mm_alignr_epi8(c2[210],c2[209],11)))));

//row: 6  [P1<<209 + 4 sys]
     d2[84]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+657)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[144],c2[143],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[198],c2[197],15),simde_mm_alignr_epi8(c2[266],c2[265],14)))));

//row: 7  [P1<<150+P3<<181 + 3 sys]
     d2[98]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+598)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1525)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[141],c2[140],13),simde_mm_alignr_epi8(c2[203],c2[202],9)))));

//row: 8  [P2<<152 + 2 sys]
     d2[112]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1048)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],3),simde_mm_alignr_epi8(c2[31],c2[30],11)));

//row: 9  [P0<<184+P1<<83 + 2 sys]
     d2[126]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+184)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+531)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],2),simde_mm_alignr_epi8(c2[226],c2[225],14))));

//row: 11  [P3<<99 + 3 sys]
     d2[154]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1443)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[205],c2[204],10),simde_mm_alignr_epi8(c2[259],c2[258],14))));

//row: 12  [P1<<109 + 2 sys]
     d2[168]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+557)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],14),simde_mm_alignr_epi8(c2[98],c2[97],9)));

//row: 13  [P3<<140 + 3 sys]
     d2[182]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1484)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],1),simde_mm_alignr_epi8(c2[233],c2[232],15))));

//row: 14  [P1<<40+P3<<116 + 2 sys]
     d2[196]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+488)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1460)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],4),simde_mm_alignr_epi8(c2[174],c2[173],15))));

//row: 15  [P0<<200+P1<<110 + 1 sys]
     d2[210]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+200)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+558)),simde_mm_alignr_epi8(c2[8],c2[7],4)));

//row: 16  [P1<<134+P2<<97 + 2 sys]
     d2[224]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+582)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+993)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],11),simde_mm_alignr_epi8(c2[262],c2[261],14))));

//row: 17  [P1<<206+P2<<2 + 2 sys]
     d2[238]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+654)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+898)),simde_mm_xor_si128(c2[31],simde_mm_alignr_epi8(c2[149],c2[148],4))));

//row: 19  [P0<<86 + 2 sys]
     d2[266]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+86)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],3),simde_mm_alignr_epi8(c2[37],c2[36],10)));

//row: 20  [P1<<141 + 2 sys]
     d2[280]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+589)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],6),simde_mm_alignr_epi8(c2[114],c2[113],4)));

//row: 21  [P3<<81 + 2 sys]
     d2[294]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1425)),simde_mm_xor_si128(c2[5],simde_mm_alignr_epi8(c2[227],c2[226],11)));

//row: 26  [P2<<221+P3<<6 + 2 sys]
     d2[364]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1117)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1350)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],10),simde_mm_alignr_epi8(c2[197],c2[196],6))));

//row: 31  [P3<<182 + 1 sys]
     d2[434]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1526)),simde_mm_alignr_epi8(c2[38],c2[37],3));

//row: 32  [P2<<110 + 2 sys]
     d2[448]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1006)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],12),simde_mm_alignr_epi8(c2[142],c2[141],5)));

//row: 33  [P0<<109 + 2 sys]
     d2[462]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+109)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],7),simde_mm_alignr_epi8(c2[197],c2[196],12)));

//row: 34  [P2<<201+P3<<69 + 1 sys]
     d2[476]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1097)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1413)),simde_mm_alignr_epi8(c2[2],c2[1],13)));

//row: 35  [P1<<55 + 2 sys]
     d2[490]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+503)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],11),simde_mm_alignr_epi8(c2[151],c2[150],5)));

//row: 37  [P0<<40+P3<<12 + 0 sys]
     d2[518]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+40)),simde_mm_loadu_si128((const simde__m128i *)(kb+1356)));

//row: 38  [P1<<46 + 2 sys]
     d2[532]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+494)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],5),simde_mm_alignr_epi8(c2[147],c2[146],1)));

//row: 39  [P2<<154 + 2 sys]
     d2[546]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1050)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],10),simde_mm_alignr_epi8(c2[208],c2[207],5)));

//row: 40  [P0<<35+P3<<36 + 1 sys]
     d2[560]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+35)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1380)),simde_mm_alignr_epi8(c2[63],c2[62],2)));

//row: 41  [P1<<81 + 2 sys]
     d2[574]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+529)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],8),simde_mm_alignr_epi8(c2[147],c2[146],5)));
  }
}
