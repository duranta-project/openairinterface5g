#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=160, byte encoding, factored
// 173 terms vs 1477 expanded (8.54x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc160_byte_128(uint8_t *c,uint8_t *d) {
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
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[26],c2[25],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[50],c2[49],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[70],c2[69],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[87],c2[86],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[90],c2[89],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[110],c2[109],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[110],c2[109],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[121],c2[120],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[126],c2[125],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[130],c2[129],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[144],c2[143],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[150],c2[149],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[164],c2[163],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[167],c2[166],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[170],c2[169],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[184],c2[183],6),simde_mm_alignr_epi8(c2[190],c2[189],15)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[100]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[22],c2[21],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[130],c2[129],12),simde_mm_alignr_epi8(c2[142],c2[141],4))));

//row: 18  [direct, 3 terms]
     d2[180]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[122],c2[121],2),simde_mm_alignr_epi8(c2[146],c2[145],6)));

//row: 22  [direct, 2 terms]
     d2[220]=simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[41],c2[40],4));

//row: 23  [direct, 3 terms]
     d2[230]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],11),simde_mm_alignr_epi8(c2[110],c2[109],14)));

//row: 24  [direct, 3 terms]
     d2[240]=simde_mm_xor_si128(c2[20],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],5),simde_mm_alignr_epi8(c2[186],c2[185],7)));

//row: 25  [direct, 2 terms]
     d2[250]=simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[105],c2[104],1));

//row: 27  [direct, 2 terms]
     d2[270]=simde_mm_xor_si128(c2[0],c2[122]);

//row: 28  [direct, 3 terms]
     d2[280]=simde_mm_xor_si128(c2[20],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],14),simde_mm_alignr_epi8(c2[107],c2[106],14)));

//row: 29  [direct, 2 terms]
     d2[290]=simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[90],c2[89],10));

//row: 30  [direct, 4 terms]
     d2[300]=simde_mm_xor_si128(c2[40],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[144],c2[143],3),simde_mm_alignr_epi8(c2[189],c2[188],6))));

//row: 36  [direct, 3 terms]
     d2[360]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],15),simde_mm_alignr_epi8(c2[146],c2[145],2)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[10]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[20],simde_mm_xor_si128(cm[40],simde_mm_xor_si128(cm[60],simde_mm_xor_si128(cm[120],cm[180]))))));
       dm[20]=simde_mm_xor_si128(dm[10],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[68],cm[67],12),simde_mm_xor_si128(cm[80],simde_mm_xor_si128(cm[100],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[126],cm[125],8),simde_mm_xor_si128(cm[140],simde_mm_xor_si128(cm[160],simde_mm_alignr_epi8(cm[184],cm[183],7)))))))));
       dm[30]=simde_mm_xor_si128(dm[20],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[67],cm[66],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],9),simde_mm_alignr_epi8(cm[167],cm[166],12)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[9]; dm=&dsimd[9];
  dm[10]=simde_mm_xor_si128(dsimd[9],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[20],simde_mm_xor_si128(cm[40],simde_mm_xor_si128(cm[60],simde_mm_xor_si128(cm[120],cm[180]))))));
  dm[20]=simde_mm_xor_si128(dsimd[19],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[68],cm[67],12),simde_mm_xor_si128(cm[80],simde_mm_xor_si128(cm[100],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[126],cm[125],8),simde_mm_xor_si128(cm[140],simde_mm_xor_si128(cm[160],simde_mm_alignr_epi8(cm[184],cm[183],7)))))))));
  dm[30]=simde_mm_xor_si128(dsimd[29],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[9],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[26],cm[25],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[67],cm[66],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],9),simde_mm_alignr_epi8(cm[167],cm[166],12)))))));

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

//row: 4  [P1<<157 + 2 sys]
     d2[40]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+477)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[29],c2[28],8)));

//row: 5  [P1<<64 + 4 sys]
     d2[50]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+384)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[109],c2[108],14),simde_mm_alignr_epi8(c2[149],c2[148],13)))));

//row: 6  [P1<<148 + 4 sys]
     d2[60]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+468)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[108],c2[107],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[147],c2[146],3),simde_mm_alignr_epi8(c2[183],c2[182],13)))));

//row: 7  [P1<<96+P3<<78 + 3 sys]
     d2[70]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+416)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1038)),simde_mm_xor_si128(c2[20],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[103],c2[102],13),simde_mm_alignr_epi8(c2[150],c2[149],4)))));

//row: 8  [P2<<87 + 2 sys]
     d2[80]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+727)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[25],c2[24],1)));

//row: 9  [P0<<51+P1<<85 + 2 sys]
     d2[90]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+51)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+405)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[167],c2[166],1))));

//row: 11  [P3<<2 + 3 sys]
     d2[110]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+962)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[141],c2[140],7),simde_mm_alignr_epi8(c2[181],c2[180],4))));

//row: 12  [P1<<48 + 2 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+368)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[68],c2[67],1)));

//row: 13  [P3<<26 + 3 sys]
     d2[130]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+986)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(c2[27],simde_mm_alignr_epi8(c2[167],c2[166],6))));

//row: 14  [P1<<57+P3<<27 + 2 sys]
     d2[140]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+377)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+987)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[129],c2[128],10))));

//row: 15  [P0<<73+P1<<99 + 1 sys]
     d2[150]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+73)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+419)),c2[0]));

//row: 16  [P1<<111+P2<<143 + 2 sys]
     d2[160]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+431)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+783)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[185],c2[184],15))));

//row: 17  [P1<<109+P2<<18 + 2 sys]
     d2[170]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+429)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+658)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[102],c2[101],8))));

//row: 19  [P0<<154 + 2 sys]
     d2[190]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+154)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[30],c2[29],14)));

//row: 20  [P1<<104 + 2 sys]
     d2[200]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+424)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[90],c2[89],4)));

//row: 21  [P3<<33 + 2 sys]
     d2[210]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+993)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[162],c2[161],1)));

//row: 26  [P2<<13+P3<<7 + 2 sys]
     d2[260]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+653)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+967)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[147],c2[146],4))));

//row: 31  [P3<<20 + 1 sys]
     d2[310]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+980)),c2[20]);

//row: 32  [P2<<122 + 2 sys]
     d2[320]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+762)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[102],c2[101],4)));

//row: 33  [P0<<13 + 2 sys]
     d2[330]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+13)),simde_mm_xor_si128(c2[40],simde_mm_alignr_epi8(c2[146],c2[145],8)));

//row: 34  [P2<<19+P3<<78 + 1 sys]
     d2[340]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+659)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1038)),c2[0]));

//row: 35  [P1<<6 + 2 sys]
     d2[350]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+326)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[110],c2[109],13)));

//row: 37  [P0<<0+P3<<144 + 0 sys]
     d2[370]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+0)),simde_mm_loadu_si128((const simde__m128i *)(kb+1104)));

//row: 38  [P1<<19 + 2 sys]
     d2[380]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+339)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[106],c2[105],13)));

//row: 39  [P2<<138 + 2 sys]
     d2[390]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+778)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[142],c2[141],8)));

//row: 40  [P0<<36+P3<<143 + 1 sys]
     d2[400]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+36)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1103)),c2[40]));

//row: 41  [P1<<55 + 2 sys]
     d2[410]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+375)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[101],c2[100],2)));
  }
}
