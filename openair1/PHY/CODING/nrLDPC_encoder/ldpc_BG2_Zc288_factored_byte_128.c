#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=288, byte encoding, factored
// 173 terms vs 1481 expanded (8.56x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc288_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[2304] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<18; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[76],c2[75],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[111],c2[110],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[114],c2[113],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[146],c2[145],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[151],c2[150],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[151],c2[150],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[181],c2[180],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[185],c2[184],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[221],c2[220],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[222],c2[221],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[224],c2[223],2),simde_mm_xor_si128(c2[255],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[259],c2[258],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[293],c2[292],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[295],c2[294],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[296],c2[295],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[328],c2[327],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[329],c2[328],14),simde_mm_alignr_epi8(c2[332],c2[331],14)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[180]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[223],c2[222],5),c2[258])));

//row: 18  [direct, 3 terms]
     d2[324]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[223],c2[222],10),simde_mm_alignr_epi8(c2[261],c2[260],14)));

//row: 22  [direct, 2 terms]
     d2[396]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],6),simde_mm_alignr_epi8(c2[81],c2[80],4));

//row: 23  [direct, 3 terms]
     d2[414]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[116],c2[115],3),simde_mm_alignr_epi8(c2[184],c2[183],8)));

//row: 24  [direct, 3 terms]
     d2[432]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[75],c2[74],10),simde_mm_alignr_epi8(c2[331],c2[330],5)));

//row: 25  [direct, 2 terms]
     d2[450]=simde_mm_xor_si128(c2[8],simde_mm_alignr_epi8(c2[184],c2[183],15));

//row: 27  [direct, 2 terms]
     d2[486]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],13),simde_mm_alignr_epi8(c2[217],c2[216],10));

//row: 28  [direct, 3 terms]
     d2[504]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[77],c2[76],13),simde_mm_alignr_epi8(c2[183],c2[182],11)));

//row: 29  [direct, 2 terms]
     d2[522]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],5),simde_mm_alignr_epi8(c2[146],c2[145],9));

//row: 30  [direct, 4 terms]
     d2[540]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[78],c2[77],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[184],c2[183],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[259],c2[258],8),simde_mm_alignr_epi8(c2[329],c2[328],6))));

//row: 36  [direct, 3 terms]
     d2[648]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],1),simde_mm_alignr_epi8(c2[254],c2[253],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[18]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[76],cm[75],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[111],cm[110],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[224],cm[223],3),simde_mm_alignr_epi8(cm[332],cm[331],15)))))));
       dm[36]=simde_mm_xor_si128(dm[18],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[151],cm[150],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[185],cm[184],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[222],cm[221],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[259],cm[258],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[293],cm[292],5),simde_mm_alignr_epi8(cm[328],cm[327],2)))))))));
       dm[54]=simde_mm_xor_si128(dm[36],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[43],cm[42],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[151],cm[150],14),simde_mm_alignr_epi8(cm[295],cm[294],15)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[17]; dm=&dsimd[17];
  dm[18]=simde_mm_xor_si128(dsimd[17],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[38],cm[37],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[76],cm[75],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[111],cm[110],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[224],cm[223],3),simde_mm_alignr_epi8(cm[332],cm[331],15)))))));
  dm[36]=simde_mm_xor_si128(dsimd[35],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[151],cm[150],8),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[185],cm[184],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[222],cm[221],4),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[259],cm[258],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[293],cm[292],5),simde_mm_alignr_epi8(cm[328],cm[327],2)))))))));
  dm[54]=simde_mm_xor_si128(dsimd[53],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[17],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[43],cm[42],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[114],cm[113],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[151],cm[150],14),simde_mm_alignr_epi8(cm[295],cm[294],15)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*576,     d+k*288, 288);
    memcpy(kd+k*576+288, d+k*288, 288);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<18; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<51 + 2 sys]
     d2[72]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+627)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],10),simde_mm_alignr_epi8(c2[38],c2[37],8)));

//row: 5  [P1<<71 + 4 sys]
     d2[90]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+647)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[186],c2[185],4),simde_mm_alignr_epi8(c2[261],c2[260],9)))));

//row: 6  [P1<<139 + 4 sys]
     d2[108]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+715)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[186],c2[185],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[259],c2[258],11),simde_mm_alignr_epi8(c2[333],c2[332],5)))));

//row: 7  [P1<<108+P3<<65 + 3 sys]
     d2[126]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+684)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1793)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[189],c2[188],7),simde_mm_alignr_epi8(c2[255],c2[254],3)))));

//row: 8  [P2<<88 + 2 sys]
     d2[144]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1240)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],6),simde_mm_alignr_epi8(c2[41],c2[40],5)));

//row: 9  [P0<<24+P1<<49 + 2 sys]
     d2[162]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+24)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+625)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[43],c2[42],1),simde_mm_alignr_epi8(c2[291],c2[290],8))));

//row: 11  [P3<<64 + 3 sys]
     d2[198]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1792)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[254],c2[253],14),simde_mm_alignr_epi8(c2[332],c2[331],4))));

//row: 12  [P1<<131 + 2 sys]
     d2[216]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+707)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],1),simde_mm_alignr_epi8(c2[116],c2[115],10)));

//row: 13  [P3<<47 + 3 sys]
     d2[234]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1775)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],5),simde_mm_alignr_epi8(c2[292],c2[291],14))));

//row: 14  [P1<<130+P3<<97 + 2 sys]
     d2[252]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+706)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1825)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],15),simde_mm_alignr_epi8(c2[220],c2[219],3))));

//row: 15  [P0<<96+P1<<128 + 1 sys]
     d2[270]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+96)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+704)),simde_mm_alignr_epi8(c2[9],c2[8],11)));

//row: 16  [P1<<28+P2<<8 + 2 sys]
     d2[288]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+604)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1160)),simde_mm_xor_si128(c2[39],simde_mm_alignr_epi8(c2[325],c2[324],9))));

//row: 17  [P1<<65+P2<<42 + 2 sys]
     d2[306]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+641)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1194)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],8),simde_mm_alignr_epi8(c2[183],c2[182],11))));

//row: 19  [P0<<41 + 2 sys]
     d2[342]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+41)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],15),simde_mm_alignr_epi8(c2[38],c2[37],12)));

//row: 20  [P1<<78 + 2 sys]
     d2[360]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+654)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],2),simde_mm_alignr_epi8(c2[151],c2[150],7)));

//row: 21  [P3<<81 + 2 sys]
     d2[378]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1809)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],11),simde_mm_alignr_epi8(c2[293],c2[292],11)));

//row: 26  [P2<<100+P3<<133 + 2 sys]
     d2[468]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1252)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1861)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],14),simde_mm_alignr_epi8(c2[254],c2[253],12))));

//row: 31  [P3<<139 + 1 sys]
     d2[558]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1867)),c2[41]);

//row: 32  [P2<<71 + 2 sys]
     d2[576]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1223)),simde_mm_xor_si128(c2[2],simde_mm_alignr_epi8(c2[186],c2[185],9)));

//row: 33  [P0<<2 + 2 sys]
     d2[594]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+2)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],7),simde_mm_alignr_epi8(c2[253],c2[252],6)));

//row: 34  [P2<<25+P3<<114 + 1 sys]
     d2[612]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1177)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1842)),simde_mm_alignr_epi8(c2[3],c2[2],5)));

//row: 35  [P1<<93 + 2 sys]
     d2[630]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+669)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],12),simde_mm_alignr_epi8(c2[189],c2[188],9)));

//row: 37  [P0<<97+P3<<56 + 0 sys]
     d2[666]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+97)),simde_mm_loadu_si128((const simde__m128i *)(kb+1784)));

//row: 38  [P1<<1 + 2 sys]
     d2[684]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+577)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],1),simde_mm_alignr_epi8(c2[185],c2[184],6)));

//row: 39  [P2<<142 + 2 sys]
     d2[702]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1294)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],7),c2[254]));

//row: 40  [P0<<73+P3<<102 + 1 sys]
     d2[720]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+73)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1830)),simde_mm_alignr_epi8(c2[73],c2[72],6)));

//row: 41  [P1<<19 + 2 sys]
     d2[738]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+595)),simde_mm_xor_si128(c2[39],simde_mm_alignr_epi8(c2[183],c2[182],15)));
  }
}
