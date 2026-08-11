#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=128, byte encoding, factored
// 173 terms vs 1469 expanded (8.49x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc128_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1024] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<8; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],6),simde_mm_xor_si128(c2[5],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[51],c2[50],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[72],c2[71],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[88],c2[87],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[100],c2[99],12),simde_mm_xor_si128(c2[101],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[116],c2[115],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[130],c2[129],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[134],c2[133],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[135],c2[134],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[149],c2[148],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[152],c2[151],11),simde_mm_alignr_epi8(c2[152],c2[151],15)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[80]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],9),simde_mm_xor_si128(c2[96],simde_mm_alignr_epi8(c2[120],c2[119],5))));

//row: 18  [direct, 3 terms]
     d2[144]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],2),simde_mm_alignr_epi8(c2[116],c2[115],2)));

//row: 22  [direct, 2 terms]
     d2[176]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],14),simde_mm_alignr_epi8(c2[36],c2[35],15));

//row: 23  [direct, 3 terms]
     d2[184]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],11),simde_mm_alignr_epi8(c2[87],c2[86],14)));

//row: 24  [direct, 3 terms]
     d2[192]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],11),simde_mm_alignr_epi8(c2[145],c2[144],8)));

//row: 25  [direct, 2 terms]
     d2[200]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],4),simde_mm_alignr_epi8(c2[82],c2[81],12));

//row: 27  [direct, 2 terms]
     d2[216]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_alignr_epi8(c2[98],c2[97],7));

//row: 28  [direct, 3 terms]
     d2[224]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],5),simde_mm_alignr_epi8(c2[81],c2[80],7)));

//row: 29  [direct, 2 terms]
     d2[232]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_alignr_epi8(c2[66],c2[65],12));

//row: 30  [direct, 4 terms]
     d2[240]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],7),simde_mm_xor_si128(c2[87],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[113],c2[112],9),simde_mm_alignr_epi8(c2[150],c2[149],4))));

//row: 36  [direct, 3 terms]
     d2[288]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],6),simde_mm_alignr_epi8(c2[114],c2[113],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[8]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[24],cm[23],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],13),simde_mm_alignr_epi8(cm[149],cm[148],13)))))));
       dm[16]=simde_mm_xor_si128(dm[8],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[72],cm[71],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[88],cm[87],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[103],cm[102],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],12),simde_mm_xor_si128(cm[134],simde_mm_alignr_epi8(cm[152],cm[151],12)))))))));
       dm[24]=simde_mm_xor_si128(dm[16],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[24],cm[23],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[68],cm[67],4),cm[135]))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[7]; dm=&dsimd[7];
  dm[8]=simde_mm_xor_si128(dsimd[7],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[24],cm[23],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[50],cm[49],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[100],cm[99],13),simde_mm_alignr_epi8(cm[149],cm[148],13)))))));
  dm[16]=simde_mm_xor_si128(dsimd[15],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[3],cm[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[72],cm[71],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[88],cm[87],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[103],cm[102],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],12),simde_mm_xor_si128(cm[134],simde_mm_alignr_epi8(cm[152],cm[151],12)))))))));
  dm[24]=simde_mm_xor_si128(dsimd[23],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[7],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[24],cm[23],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[51],cm[50],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[68],cm[67],4),cm[135]))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*256,     d+k*128, 128);
    memcpy(kd+k*256+128, d+k*128, 128);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<8; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<71 + 2 sys]
     d2[32]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+327)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],3),simde_mm_alignr_epi8(c2[22],c2[21],6)));

//row: 5  [P1<<103 + 4 sys]
     d2[40]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+359)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[19],c2[18],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],2),simde_mm_alignr_epi8(c2[114],c2[113],15)))));

//row: 6  [P1<<30 + 4 sys]
     d2[48]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+286)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[115],c2[114],13),simde_mm_alignr_epi8(c2[146],c2[145],12)))));

//row: 7  [P1<<3+P3<<116 + 3 sys]
     d2[56]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+259)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+884)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],3),simde_mm_alignr_epi8(c2[113],c2[112],12)))));

//row: 8  [P2<<102 + 2 sys]
     d2[64]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+614)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],14),simde_mm_alignr_epi8(c2[22],c2[21],14)));

//row: 9  [P0<<61+P1<<119 + 2 sys]
     d2[72]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+61)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+375)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],11),simde_mm_alignr_epi8(c2[133],c2[132],13))));

//row: 11  [P3<<56 + 3 sys]
     d2[88]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+824)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[119],c2[118],12),simde_mm_alignr_epi8(c2[150],c2[149],2))));

//row: 12  [P1<<14 + 2 sys]
     d2[96]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+270)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],15),simde_mm_alignr_epi8(c2[55],c2[54],15)));

//row: 13  [P3<<94 + 3 sys]
     d2[104]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+862)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],2),simde_mm_alignr_epi8(c2[131],c2[130],6))));

//row: 14  [P1<<3+P3<<104 + 2 sys]
     d2[112]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+259)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+872)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],3),simde_mm_alignr_epi8(c2[98],c2[97],1))));

//row: 15  [P0<<47+P1<<85 + 1 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+47)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+341)),simde_mm_alignr_epi8(c2[4],c2[3],3)));

//row: 16  [P1<<8+P2<<114 + 2 sys]
     d2[128]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+264)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+626)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],11),simde_mm_alignr_epi8(c2[145],c2[144],14))));

//row: 17  [P1<<114+P2<<64 + 2 sys]
     d2[136]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+370)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+576)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],14),simde_mm_alignr_epi8(c2[88],c2[87],12))));

//row: 19  [P0<<57 + 2 sys]
     d2[152]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+57)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],7),simde_mm_alignr_epi8(c2[18],c2[17],4)));

//row: 20  [P1<<29 + 2 sys]
     d2[160]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+285)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],10),simde_mm_alignr_epi8(c2[71],c2[70],9)));

//row: 21  [P3<<82 + 2 sys]
     d2[168]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+850)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],12),simde_mm_alignr_epi8(c2[131],c2[130],10)));

//row: 26  [P2<<32+P3<<122 + 2 sys]
     d2[208]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+544)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+890)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],13),simde_mm_alignr_epi8(c2[113],c2[112],15))));

//row: 31  [P3<<1 + 1 sys]
     d2[248]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+769)),simde_mm_alignr_epi8(c2[23],c2[22],10));

//row: 32  [P2<<38 + 2 sys]
     d2[256]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+550)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],2),simde_mm_alignr_epi8(c2[83],c2[82],12)));

//row: 33  [P0<<107 + 2 sys]
     d2[264]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+107)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],4),simde_mm_alignr_epi8(c2[115],c2[114],4)));

//row: 34  [P2<<85+P3<<36 + 1 sys]
     d2[272]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+597)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+804)),simde_mm_alignr_epi8(c2[2],c2[1],3)));

//row: 35  [P1<<63 + 2 sys]
     d2[280]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+319)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[20],c2[19],9),simde_mm_alignr_epi8(c2[83],c2[82],8)));

//row: 37  [P0<<91+P3<<23 + 0 sys]
     d2[296]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+91)),simde_mm_loadu_si128((const simde__m128i *)(kb+791)));

//row: 38  [P1<<38 + 2 sys]
     d2[304]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+294)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[18],c2[17],15),simde_mm_alignr_epi8(c2[85],c2[84],2)));

//row: 39  [P2<<34 + 2 sys]
     d2[312]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+546)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],15),simde_mm_alignr_epi8(c2[115],c2[114],12)));

//row: 40  [P0<<75+P3<<120 + 1 sys]
     d2[320]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+75)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+888)),c2[32]));

//row: 41  [P1<<118 + 2 sys]
     d2[328]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+374)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[17],c2[16],1),simde_mm_alignr_epi8(c2[87],c2[86],5)));
  }
}
