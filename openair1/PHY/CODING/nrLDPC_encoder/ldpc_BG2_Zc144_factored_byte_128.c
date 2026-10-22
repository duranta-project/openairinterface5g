#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=144, byte encoding, factored
// 173 terms vs 1481 expanded (8.56x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc144_byte_128(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1152] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<9; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[60],c2[59],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[79],c2[78],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[79],c2[78],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[95],c2[94],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[113],c2[112],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[116],c2[115],2),simde_mm_xor_si128(c2[129],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[133],c2[132],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[151],c2[150],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[152],c2[151],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[166],c2[165],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[167],c2[166],14),simde_mm_alignr_epi8(c2[170],c2[169],14)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[90]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],5),c2[132])));

//row: 18  [direct, 3 terms]
     d2[162]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],10),simde_mm_alignr_epi8(c2[135],c2[134],14)));

//row: 22  [direct, 2 terms]
     d2[198]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],6),simde_mm_alignr_epi8(c2[45],c2[44],4));

//row: 23  [direct, 3 terms]
     d2[207]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],3),simde_mm_alignr_epi8(c2[94],c2[93],8)));

//row: 24  [direct, 3 terms]
     d2[216]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],10),simde_mm_alignr_epi8(c2[169],c2[168],5)));

//row: 25  [direct, 2 terms]
     d2[225]=simde_mm_xor_si128(c2[8],simde_mm_alignr_epi8(c2[94],c2[93],15));

//row: 27  [direct, 2 terms]
     d2[243]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_alignr_epi8(c2[109],c2[108],10));

//row: 28  [direct, 3 terms]
     d2[252]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],13),simde_mm_alignr_epi8(c2[93],c2[92],11)));

//row: 29  [direct, 2 terms]
     d2[261]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],5),simde_mm_alignr_epi8(c2[74],c2[73],9));

//row: 30  [direct, 4 terms]
     d2[270]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[94],c2[93],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[133],c2[132],8),simde_mm_alignr_epi8(c2[167],c2[166],6))));

//row: 36  [direct, 3 terms]
     d2[324]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],1),simde_mm_alignr_epi8(c2[128],c2[127],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[9]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[20],cm[19],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[40],cm[39],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[116],cm[115],3),simde_mm_alignr_epi8(cm[170],cm[169],15)))))));
       dm[18]=simde_mm_xor_si128(dm[9],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[60],cm[59],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[95],cm[94],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[133],cm[132],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[149],cm[148],5),simde_mm_alignr_epi8(cm[166],cm[165],2)))))))));
       dm[27]=simde_mm_xor_si128(dm[18],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[25],cm[24],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[60],cm[59],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],14),simde_mm_alignr_epi8(cm[151],cm[150],15)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[8]; dm=&dsimd[8];
  dm[9]=simde_mm_xor_si128(dsimd[8],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[20],cm[19],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[40],cm[39],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[57],cm[56],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[116],cm[115],3),simde_mm_alignr_epi8(cm[170],cm[169],15)))))));
  dm[18]=simde_mm_xor_si128(dsimd[17],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[60],cm[59],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[95],cm[94],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[133],cm[132],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[149],cm[148],5),simde_mm_alignr_epi8(cm[166],cm[165],2)))))))));
  dm[27]=simde_mm_xor_si128(dsimd[26],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[8],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[25],cm[24],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[60],cm[59],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[79],cm[78],14),simde_mm_alignr_epi8(cm[151],cm[150],15)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*288,     d+k*144, 144);
    memcpy(kd+k*288+144, d+k*144, 144);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<9; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<51 + 2 sys]
     d2[36]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+339)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],10),simde_mm_alignr_epi8(c2[20],c2[19],8)));

//row: 5  [P1<<71 + 4 sys]
     d2[45]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+359)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[96],c2[95],4),simde_mm_alignr_epi8(c2[135],c2[134],9)))));

//row: 6  [P1<<139 + 4 sys]
     d2[54]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+427)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[96],c2[95],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[133],c2[132],11),simde_mm_alignr_epi8(c2[171],c2[170],5)))));

//row: 7  [P1<<108+P3<<65 + 3 sys]
     d2[63]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+396)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+929)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],7),simde_mm_alignr_epi8(c2[129],c2[128],3)))));

//row: 8  [P2<<88 + 2 sys]
     d2[72]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+664)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],6),simde_mm_alignr_epi8(c2[23],c2[22],5)));

//row: 9  [P0<<24+P1<<49 + 2 sys]
     d2[81]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+24)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+337)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],1),simde_mm_alignr_epi8(c2[147],c2[146],8))));

//row: 11  [P3<<64 + 3 sys]
     d2[99]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+928)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[128],c2[127],14),simde_mm_alignr_epi8(c2[170],c2[169],4))));

//row: 12  [P1<<131 + 2 sys]
     d2[108]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+419)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],1),simde_mm_alignr_epi8(c2[62],c2[61],10)));

//row: 13  [P3<<47 + 3 sys]
     d2[117]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+911)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],5),simde_mm_alignr_epi8(c2[148],c2[147],14))));

//row: 14  [P1<<130+P3<<97 + 2 sys]
     d2[126]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+418)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+961)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],15),simde_mm_alignr_epi8(c2[112],c2[111],3))));

//row: 15  [P0<<96+P1<<128 + 1 sys]
     d2[135]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+96)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+416)),simde_mm_alignr_epi8(c2[9],c2[8],11)));

//row: 16  [P1<<28+P2<<8 + 2 sys]
     d2[144]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+316)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+584)),simde_mm_xor_si128(c2[21],simde_mm_alignr_epi8(c2[163],c2[162],9))));

//row: 17  [P1<<65+P2<<42 + 2 sys]
     d2[153]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+353)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+618)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],8),simde_mm_alignr_epi8(c2[93],c2[92],11))));

//row: 19  [P0<<41 + 2 sys]
     d2[171]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+41)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],15),simde_mm_alignr_epi8(c2[20],c2[19],12)));

//row: 20  [P1<<78 + 2 sys]
     d2[180]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+366)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],2),simde_mm_alignr_epi8(c2[79],c2[78],7)));

//row: 21  [P3<<81 + 2 sys]
     d2[189]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+945)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],11),simde_mm_alignr_epi8(c2[149],c2[148],11)));

//row: 26  [P2<<100+P3<<133 + 2 sys]
     d2[234]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+676)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+997)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],14),simde_mm_alignr_epi8(c2[128],c2[127],12))));

//row: 31  [P3<<139 + 1 sys]
     d2[279]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1003)),c2[23]);

//row: 32  [P2<<71 + 2 sys]
     d2[288]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+647)),simde_mm_xor_si128(c2[2],simde_mm_alignr_epi8(c2[96],c2[95],9)));

//row: 33  [P0<<2 + 2 sys]
     d2[297]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],7),simde_mm_alignr_epi8(c2[127],c2[126],6)));

//row: 34  [P2<<25+P3<<114 + 1 sys]
     d2[306]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+601)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+978)),simde_mm_alignr_epi8(c2[3],c2[2],5)));

//row: 35  [P1<<93 + 2 sys]
     d2[315]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+381)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],12),simde_mm_alignr_epi8(c2[99],c2[98],9)));

//row: 37  [P0<<97+P3<<56 + 0 sys]
     d2[333]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+97)),simde_mm_loadu_si128((const simde__m128i *)(kb+920)));

//row: 38  [P1<<1 + 2 sys]
     d2[342]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+289)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],1),simde_mm_alignr_epi8(c2[95],c2[94],6)));

//row: 39  [P2<<142 + 2 sys]
     d2[351]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+718)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],7),c2[128]));

//row: 40  [P0<<73+P3<<102 + 1 sys]
     d2[360]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+73)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+966)),simde_mm_alignr_epi8(c2[37],c2[36],6)));

//row: 41  [P1<<19 + 2 sys]
     d2[369]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+307)),simde_mm_xor_si128(c2[21],simde_mm_alignr_epi8(c2[93],c2[92],15)));
  }
}
