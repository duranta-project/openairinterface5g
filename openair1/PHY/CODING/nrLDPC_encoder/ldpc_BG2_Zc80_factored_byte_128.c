#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=80, byte encoding, factored
// 173 terms vs 1471 expanded (8.50x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc80_byte_128(uint8_t *c,uint8_t *d) {
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
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[21],c2[20],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[42],c2[41],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[61],c2[60],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[82],c2[81],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[84],c2[83],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[85],c2[84],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[93],c2[92],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[94],c2[93],6),simde_mm_alignr_epi8(c2[95],c2[94],15)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[50]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[65],c2[64],12),simde_mm_alignr_epi8(c2[72],c2[71],4))));

//row: 18  [direct, 3 terms]
     d2[90]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[62],c2[61],2),simde_mm_alignr_epi8(c2[71],c2[70],6)));

//row: 22  [direct, 2 terms]
     d2[110]=simde_mm_xor_si128(c2[10],simde_mm_alignr_epi8(c2[21],c2[20],4));

//row: 23  [direct, 3 terms]
     d2[115]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],11),simde_mm_alignr_epi8(c2[55],c2[54],14)));

//row: 24  [direct, 3 terms]
     d2[120]=simde_mm_xor_si128(c2[10],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[25],c2[24],5),simde_mm_alignr_epi8(c2[91],c2[90],7)));

//row: 25  [direct, 2 terms]
     d2[125]=simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[55],c2[54],1));

//row: 27  [direct, 2 terms]
     d2[135]=simde_mm_xor_si128(c2[0],c2[62]);

//row: 28  [direct, 3 terms]
     d2[140]=simde_mm_xor_si128(c2[10],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[23],c2[22],14),simde_mm_alignr_epi8(c2[52],c2[51],14)));

//row: 29  [direct, 2 terms]
     d2[145]=simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[45],c2[44],10));

//row: 30  [direct, 4 terms]
     d2[150]=simde_mm_xor_si128(c2[20],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[74],c2[73],3),simde_mm_alignr_epi8(c2[94],c2[93],6))));

//row: 36  [direct, 3 terms]
     d2[180]=simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[24],c2[23],15),simde_mm_alignr_epi8(c2[71],c2[70],2)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[5]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[10],simde_mm_xor_si128(cm[20],simde_mm_xor_si128(cm[30],simde_mm_xor_si128(cm[60],cm[90]))))));
       dm[10]=simde_mm_xor_si128(dm[5],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[33],cm[32],12),simde_mm_xor_si128(cm[40],simde_mm_xor_si128(cm[50],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[61],cm[60],8),simde_mm_xor_si128(cm[70],simde_mm_xor_si128(cm[80],simde_mm_alignr_epi8(cm[94],cm[93],7)))))))));
       dm[15]=simde_mm_xor_si128(dm[10],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[11],cm[10],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],9),simde_mm_alignr_epi8(cm[82],cm[81],12)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[4]; dm=&dsimd[4];
  dm[5]=simde_mm_xor_si128(dsimd[4],simde_mm_xor_si128(cm[0],simde_mm_xor_si128(cm[10],simde_mm_xor_si128(cm[20],simde_mm_xor_si128(cm[30],simde_mm_xor_si128(cm[60],cm[90]))))));
  dm[10]=simde_mm_xor_si128(dsimd[9],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[4],cm[3],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[33],cm[32],12),simde_mm_xor_si128(cm[40],simde_mm_xor_si128(cm[50],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[61],cm[60],8),simde_mm_xor_si128(cm[70],simde_mm_xor_si128(cm[80],simde_mm_alignr_epi8(cm[94],cm[93],7)))))))));
  dm[15]=simde_mm_xor_si128(dsimd[14],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[4],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[11],cm[10],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[32],cm[31],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[41],cm[40],9),simde_mm_alignr_epi8(cm[82],cm[81],12)))))));

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

//row: 4  [P1<<77 + 2 sys]
     d2[20]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+237)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[14],c2[13],8)));

//row: 5  [P1<<64 + 4 sys]
     d2[25]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+224)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[54],c2[53],14),simde_mm_alignr_epi8(c2[74],c2[73],13)))));

//row: 6  [P1<<68 + 4 sys]
     d2[30]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+228)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[72],c2[71],3),simde_mm_alignr_epi8(c2[93],c2[92],13)))));

//row: 7  [P1<<16+P3<<78 + 3 sys]
     d2[35]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+176)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+558)),simde_mm_xor_si128(c2[10],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[53],c2[52],13),simde_mm_alignr_epi8(c2[75],c2[74],4)))));

//row: 8  [P2<<7 + 2 sys]
     d2[40]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+327)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[15],c2[14],1)));

//row: 9  [P0<<51+P1<<5 + 2 sys]
     d2[45]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+51)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+165)),simde_mm_xor_si128(c2[10],simde_mm_alignr_epi8(c2[82],c2[81],1))));

//row: 11  [P3<<2 + 3 sys]
     d2[55]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+482)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],7),simde_mm_alignr_epi8(c2[91],c2[90],4))));

//row: 12  [P1<<48 + 2 sys]
     d2[60]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+208)),simde_mm_xor_si128(c2[10],simde_mm_alignr_epi8(c2[33],c2[32],1)));

//row: 13  [P3<<26 + 3 sys]
     d2[65]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+506)),simde_mm_xor_si128(c2[0],simde_mm_xor_si128(c2[12],simde_mm_alignr_epi8(c2[82],c2[81],6))));

//row: 14  [P1<<57+P3<<27 + 2 sys]
     d2[70]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+217)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+507)),simde_mm_xor_si128(c2[10],simde_mm_alignr_epi8(c2[64],c2[63],10))));

//row: 15  [P0<<73+P1<<19 + 1 sys]
     d2[75]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+73)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+179)),c2[0]));

//row: 16  [P1<<31+P2<<63 + 2 sys]
     d2[80]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+191)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+383)),simde_mm_xor_si128(c2[10],simde_mm_alignr_epi8(c2[95],c2[94],15))));

//row: 17  [P1<<29+P2<<18 + 2 sys]
     d2[85]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+189)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+338)),simde_mm_xor_si128(c2[10],simde_mm_alignr_epi8(c2[52],c2[51],8))));

//row: 19  [P0<<74 + 2 sys]
     d2[95]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+74)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[15],c2[14],14)));

//row: 20  [P1<<24 + 2 sys]
     d2[100]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+184)),simde_mm_xor_si128(c2[10],simde_mm_alignr_epi8(c2[45],c2[44],4)));

//row: 21  [P3<<33 + 2 sys]
     d2[105]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+513)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[82],c2[81],1)));

//row: 26  [P2<<13+P3<<7 + 2 sys]
     d2[130]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+333)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+487)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[72],c2[71],4))));

//row: 31  [P3<<20 + 1 sys]
     d2[155]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+500)),c2[10]);

//row: 32  [P2<<42 + 2 sys]
     d2[160]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+362)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[52],c2[51],4)));

//row: 33  [P0<<13 + 2 sys]
     d2[165]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+13)),simde_mm_xor_si128(c2[20],simde_mm_alignr_epi8(c2[71],c2[70],8)));

//row: 34  [P2<<19+P3<<78 + 1 sys]
     d2[170]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+339)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+558)),c2[0]));

//row: 35  [P1<<6 + 2 sys]
     d2[175]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+166)),simde_mm_xor_si128(c2[10],simde_mm_alignr_epi8(c2[55],c2[54],13)));

//row: 37  [P0<<0+P3<<64 + 0 sys]
     d2[185]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+0)),simde_mm_loadu_si128((const simde__m128i *)(kb+544)));

//row: 38  [P1<<19 + 2 sys]
     d2[190]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+179)),simde_mm_xor_si128(c2[10],simde_mm_alignr_epi8(c2[51],c2[50],13)));

//row: 39  [P2<<58 + 2 sys]
     d2[195]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+378)),simde_mm_xor_si128(c2[0],simde_mm_alignr_epi8(c2[72],c2[71],8)));

//row: 40  [P0<<36+P3<<63 + 1 sys]
     d2[200]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+36)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+543)),c2[20]));

//row: 41  [P1<<55 + 2 sys]
     d2[205]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+215)),simde_mm_xor_si128(c2[10],simde_mm_alignr_epi8(c2[51],c2[50],2)));
  }
}
