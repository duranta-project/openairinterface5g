#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=240, byte encoding, factored
// 173 terms vs 1477 expanded (8.54x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc240_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1920] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<15; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[91],c2[90],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[92],c2[91],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[101],c2[100],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[124],c2[123],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[126],c2[125],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[152],c2[151],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[157],c2[156],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[182],c2[181],7),simde_mm_xor_si128(c2[186],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[190],c2[189],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[212],c2[211],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[223],c2[222],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[241],c2[240],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[243],c2[242],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[250],c2[249],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[275],c2[274],11),simde_mm_xor_si128(c2[277],simde_mm_alignr_epi8(c2[281],c2[280],7)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[150]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],9),simde_mm_xor_si128(c2[193],simde_mm_alignr_epi8(c2[225],c2[224],8))));

//row: 18  [direct, 3 terms]
     d2[270]=simde_mm_xor_si128(c2[8],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[194],c2[193],3),simde_mm_alignr_epi8(c2[224],c2[223],2)));

//row: 22  [direct, 2 terms]
     d2[330]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],3),simde_mm_alignr_epi8(c2[63],c2[62],12));

//row: 23  [direct, 3 terms]
     d2[345]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[96],c2[95],14),simde_mm_alignr_epi8(c2[157],c2[156],15)));

//row: 24  [direct, 3 terms]
     d2[360]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],3),simde_mm_alignr_epi8(c2[279],c2[278],2)));

//row: 25  [direct, 2 terms]
     d2[375]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],2),simde_mm_alignr_epi8(c2[156],c2[155],15));

//row: 27  [direct, 2 terms]
     d2[405]=simde_mm_xor_si128(c2[4],simde_mm_alignr_epi8(c2[192],c2[191],5));

//row: 28  [direct, 3 terms]
     d2[420]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[31],c2[30],7),simde_mm_xor_si128(c2[69],c2[151]));

//row: 29  [direct, 2 terms]
     d2[435]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],9),simde_mm_alignr_epi8(c2[124],c2[123],9));

//row: 30  [direct, 4 terms]
     d2[450]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[159],c2[158],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[224],c2[223],13),simde_mm_alignr_epi8(c2[272],c2[271],1))));

//row: 36  [direct, 3 terms]
     d2[540]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],2),simde_mm_xor_si128(c2[67],simde_mm_alignr_epi8(c2[217],c2[216],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[15]=simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[10],cm[9],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[39],cm[38],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[65],cm[64],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[92],cm[91],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],7),cm[277]))))));
       dm[30]=simde_mm_xor_si128(dm[15],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[132],cm[131],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[152],cm[151],11),simde_mm_xor_si128(cm[186],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[212],cm[211],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[241],cm[240],9),simde_mm_alignr_epi8(cm[281],cm[280],7)))))))));
       dm[45]=simde_mm_xor_si128(dm[30],simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[91],cm[90],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[124],cm[123],5),simde_mm_alignr_epi8(cm[250],cm[249],11)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[14]; dm=&dsimd[14];
  dm[15]=simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[14],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[10],cm[9],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[39],cm[38],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[65],cm[64],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[92],cm[91],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[182],cm[181],7),cm[277]))))));
  dm[30]=simde_mm_xor_si128(dsimd[29],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[101],cm[100],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[132],cm[131],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[152],cm[151],11),simde_mm_xor_si128(cm[186],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[212],cm[211],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[241],cm[240],9),simde_mm_alignr_epi8(cm[281],cm[280],7)))))))));
  dm[45]=simde_mm_xor_si128(dsimd[44],simde_mm_xor_si128(dsimd[14],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[5],cm[4],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[91],cm[90],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[124],cm[123],5),simde_mm_alignr_epi8(cm[250],cm[249],11)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*480,     d+k*240, 240);
    memcpy(kd+k*480+240, d+k*240, 240);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<15; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<180 + 2 sys]
     d2[60]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+660)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],13),simde_mm_alignr_epi8(c2[39],c2[38],12)));

//row: 5  [P1<<207 + 4 sys]
     d2[75]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+687)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[161],c2[160],9),simde_mm_alignr_epi8(c2[216],c2[215],8)))));

//row: 6  [P1<<56 + 4 sys]
     d2[90]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+536)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[155],c2[154],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[221],c2[220],12),simde_mm_alignr_epi8(c2[278],c2[277],12)))));

//row: 7  [P1<<172+P3<<154 + 3 sys]
     d2[105]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+652)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1594)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[162],c2[161],10),simde_mm_alignr_epi8(c2[216],c2[215],7)))));

//row: 8  [P2<<225 + 2 sys]
     d2[120]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1185)),simde_mm_xor_si128(c2[11],simde_mm_alignr_epi8(c2[41],c2[40],9)));

//row: 9  [P0<<48+P1<<68 + 2 sys]
     d2[135]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+48)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+548)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],7),simde_mm_alignr_epi8(c2[255],c2[254],14))));

//row: 11  [P3<<51 + 3 sys]
     d2[165]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1491)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[224],c2[223],6),simde_mm_alignr_epi8(c2[281],c2[280],8))));

//row: 12  [P1<<72 + 2 sys]
     d2[180]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+552)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],12),simde_mm_alignr_epi8(c2[98],c2[97],10)));

//row: 13  [P3<<219 + 3 sys]
     d2[195]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1659)),simde_mm_xor_si128(c2[3],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],9),simde_mm_alignr_epi8(c2[251],c2[250],7))));

//row: 14  [P1<<204+P3<<162 + 2 sys]
     d2[210]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+684)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1602)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],2),simde_mm_alignr_epi8(c2[195],c2[194],8))));

//row: 15  [P0<<217+P1<<157 + 1 sys]
     d2[225]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+217)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+637)),simde_mm_alignr_epi8(c2[3],c2[2],6)));

//row: 16  [P1<<175+P2<<202 + 2 sys]
     d2[240]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+655)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1162)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],10),simde_mm_alignr_epi8(c2[272],c2[271],7))));

//row: 17  [P1<<195+P2<<218 + 2 sys]
     d2[255]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+675)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1178)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],4),simde_mm_alignr_epi8(c2[161],c2[160],13))));

//row: 19  [P0<<88 + 2 sys]
     d2[285]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+88)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],7),simde_mm_alignr_epi8(c2[36],c2[35],4)));

//row: 20  [P1<<6 + 2 sys]
     d2[300]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+486)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],5),simde_mm_alignr_epi8(c2[135],c2[134],3)));

//row: 21  [P3<<11 + 2 sys]
     d2[315]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1451)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],14),simde_mm_alignr_epi8(c2[241],c2[240],13)));

//row: 26  [P2<<190+P3<<86 + 2 sys]
     d2[390]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1150)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1526)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[70],c2[69],2),simde_mm_alignr_epi8(c2[215],c2[214],2))));

//row: 31  [P3<<46 + 1 sys]
     d2[465]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1486)),simde_mm_alignr_epi8(c2[43],c2[42],9));

//row: 32  [P2<<116 + 2 sys]
     d2[480]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1076)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],3),simde_mm_alignr_epi8(c2[151],c2[150],14)));

//row: 33  [P0<<106 + 2 sys]
     d2[495]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+106)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],14),simde_mm_alignr_epi8(c2[211],c2[210],2)));

//row: 34  [P2<<135+P3<<141 + 1 sys]
     d2[510]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1095)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1581)),simde_mm_alignr_epi8(c2[12],c2[11],8)));

//row: 35  [P1<<175 + 2 sys]
     d2[525]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+655)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],5),simde_mm_alignr_epi8(c2[165],c2[164],1)));

//row: 37  [P0<<154+P3<<114 + 0 sys]
     d2[555]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+154)),simde_mm_loadu_si128((const simde__m128i *)(kb+1554)));

//row: 38  [P1<<105 + 2 sys]
     d2[570]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+585)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],10),simde_mm_alignr_epi8(c2[153],c2[152],9)));

//row: 39  [P2<<189 + 2 sys]
     d2[585]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1149)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],7),simde_mm_alignr_epi8(c2[213],c2[212],13)));

//row: 40  [P0<<67+P3<<180 + 1 sys]
     d2[600]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+67)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1620)),simde_mm_alignr_epi8(c2[65],c2[64],14)));

//row: 41  [P1<<230 + 2 sys]
     d2[615]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+710)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],5),simde_mm_alignr_epi8(c2[164],c2[163],7)));
  }
}
