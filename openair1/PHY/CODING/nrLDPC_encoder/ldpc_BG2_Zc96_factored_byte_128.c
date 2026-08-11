#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=96, byte encoding, factored
// 173 terms vs 1461 expanded (8.45x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc96_byte(uint8_t *c,uint8_t *d) {
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
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],7),simde_mm_xor_si128(c2[12],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[49],c2[48],13),simde_mm_xor_si128(c2[49],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[77],c2[76],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[86],c2[85],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[102],c2[101],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[109],c2[108],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[113],c2[112],11),simde_mm_alignr_epi8(c2[114],c2[113],9)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[60]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],6),simde_mm_alignr_epi8(c2[88],c2[87],4))));

//row: 18  [direct, 3 terms]
     d2[108]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],6),simde_mm_alignr_epi8(c2[87],c2[86],14)));

//row: 22  [direct, 2 terms]
     d2[132]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],4),simde_mm_alignr_epi8(c2[28],c2[27],4));

//row: 23  [direct, 3 terms]
     d2[138]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],6),simde_mm_alignr_epi8(c2[66],c2[65],15)));

//row: 24  [direct, 3 terms]
     d2[144]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],9),c2[112]));

//row: 25  [direct, 2 terms]
     d2[150]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],13),simde_mm_alignr_epi8(c2[62],c2[61],5));

//row: 27  [direct, 2 terms]
     d2[162]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],7),simde_mm_alignr_epi8(c2[76],c2[75],2));

//row: 28  [direct, 3 terms]
     d2[168]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],15),simde_mm_alignr_epi8(c2[65],c2[64],8)));

//row: 29  [direct, 2 terms]
     d2[174]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],14),simde_mm_alignr_epi8(c2[50],c2[49],1));

//row: 30  [direct, 4 terms]
     d2[180]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[64],c2[63],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],4),simde_mm_alignr_epi8(c2[112],c2[111],8))));

//row: 36  [direct, 3 terms]
     d2[216]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],7),simde_mm_alignr_epi8(c2[89],c2[88],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[6]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[13],cm[12],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[29],cm[28],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[77],cm[76],7),simde_mm_alignr_epi8(cm[113],cm[112],12)))))));
       dm[12]=simde_mm_xor_si128(dm[6],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[39],cm[38],4),simde_mm_xor_si128(cm[51],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[66],cm[65],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[90],cm[89],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[102],cm[101],9),simde_mm_alignr_epi8(cm[109],cm[108],3)))))))));
       dm[18]=simde_mm_xor_si128(dm[12],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],14),simde_mm_alignr_epi8(cm[98],cm[97],2)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[5]; dm=&dsimd[5];
  dm[6]=simde_mm_xor_si128(dsimd[5],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[13],cm[12],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[29],cm[28],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[77],cm[76],7),simde_mm_alignr_epi8(cm[113],cm[112],12)))))));
  dm[12]=simde_mm_xor_si128(dsimd[11],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[39],cm[38],4),simde_mm_xor_si128(cm[51],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[66],cm[65],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[74],cm[73],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[90],cm[89],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[102],cm[101],9),simde_mm_alignr_epi8(cm[109],cm[108],3)))))))));
  dm[18]=simde_mm_xor_si128(dsimd[17],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[14],cm[13],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[49],cm[48],14),simde_mm_alignr_epi8(cm[98],cm[97],2)))))));

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

//row: 4  [P1<<29 + 2 sys]
     d2[24]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+221)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],8),simde_mm_alignr_epi8(c2[17],c2[16],10)));

//row: 5  [P1<<48 + 4 sys]
     d2[30]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+240)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],9),c2[89]))));

//row: 6  [P1<<88 + 4 sys]
     d2[36]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+280)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],4),simde_mm_alignr_epi8(c2[112],c2[111],1)))));

//row: 7  [P1<<6+P3<<47 + 3 sys]
     d2[42]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+198)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+623)),simde_mm_xor_si128(c2[17],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],10),c2[85]))));

//row: 8  [P2<<56 + 2 sys]
     d2[48]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+440)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],6),simde_mm_alignr_epi8(c2[17],c2[16],6)));

//row: 9  [P0<<89+P1<<82 + 2 sys]
     d2[54]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+89)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+274)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],12),simde_mm_alignr_epi8(c2[99],c2[98],4))));

//row: 11  [P3<<58 + 3 sys]
     d2[66]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+634)),simde_mm_xor_si128(c2[2],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],12),simde_mm_alignr_epi8(c2[113],c2[112],14))));

//row: 12  [P1<<11 + 2 sys]
     d2[72]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+203)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],7),simde_mm_alignr_epi8(c2[42],c2[41],13)));

//row: 13  [P3<<70 + 3 sys]
     d2[78]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+646)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],13),simde_mm_alignr_epi8(c2[99],c2[98],3))));

//row: 14  [P1<<21+P3<<67 + 2 sys]
     d2[84]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+213)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+643)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],3),simde_mm_alignr_epi8(c2[74],c2[73],6))));

//row: 15  [P0<<63+P1<<81 + 1 sys]
     d2[90]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+63)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+273)),simde_mm_alignr_epi8(c2[5],c2[4],4)));

//row: 16  [P1<<39+P2<<64 + 2 sys]
     d2[96]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+231)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+448)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],7),simde_mm_alignr_epi8(c2[114],c2[113],1))));

//row: 17  [P1<<9+P2<<6 + 2 sys]
     d2[102]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+201)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+390)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],14),simde_mm_alignr_epi8(c2[62],c2[61],7))));

//row: 19  [P0<<60 + 2 sys]
     d2[114]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+60)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],10),simde_mm_alignr_epi8(c2[15],c2[14],10)));

//row: 20  [P1<<57 + 2 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+249)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],12),simde_mm_alignr_epi8(c2[52],c2[51],13)));

//row: 21  [P3<<67 + 2 sys]
     d2[126]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+643)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],13),simde_mm_alignr_epi8(c2[101],c2[100],15)));

//row: 26  [P2<<55+P3<<85 + 2 sys]
     d2[156]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+439)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+661)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],3),simde_mm_alignr_epi8(c2[87],c2[86],9))));

//row: 31  [P3<<74 + 1 sys]
     d2[186]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+650)),simde_mm_alignr_epi8(c2[13],c2[12],3));

//row: 32  [P2<<17 + 2 sys]
     d2[192]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+401)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],4),simde_mm_alignr_epi8(c2[61],c2[60],8)));

//row: 33  [P0<<28 + 2 sys]
     d2[198]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+28)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],5),simde_mm_alignr_epi8(c2[90],c2[89],3)));

//row: 34  [P2<<81+P3<<12 + 1 sys]
     d2[204]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+465)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+588)),simde_mm_alignr_epi8(c2[5],c2[4],13)));

//row: 35  [P1<<18 + 2 sys]
     d2[210]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+210)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],13),simde_mm_alignr_epi8(c2[66],c2[65],8)));

//row: 37  [P0<<37+P3<<31 + 0 sys]
     d2[222]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+37)),simde_mm_loadu_si128((const simde__m128i *)(kb+607)));

//row: 38  [P1<<94 + 2 sys]
     d2[228]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+286)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],4),simde_mm_alignr_epi8(c2[64],c2[63],7)));

//row: 39  [P2<<57 + 2 sys]
     d2[234]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+441)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],13),simde_mm_alignr_epi8(c2[87],c2[86],4)));

//row: 40  [P0<<11+P3<<67 + 1 sys]
     d2[240]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+11)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+643)),simde_mm_alignr_epi8(c2[25],c2[24],7)));

//row: 41  [P1<<60 + 2 sys]
     d2[246]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+252)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],3),simde_mm_alignr_epi8(c2[61],c2[60],7)));
  }
}
