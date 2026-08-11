#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=256, byte encoding, factored
// 173 terms vs 1477 expanded (8.54x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc256_byte(uint8_t *c,uint8_t *d) {
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
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_xor_si128(c2[5],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[68],c2[67],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[77],c2[76],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[98],c2[97],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[99],c2[98],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[107],c2[106],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[138],c2[137],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[144],c2[143],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[167],c2[166],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[168],c2[167],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[204],c2[203],12),simde_mm_xor_si128(c2[205],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[207],c2[206],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[228],c2[227],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[234],c2[233],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[258],c2[257],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[270],c2[269],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[271],c2[270],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[296],c2[295],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[301],c2[300],12),simde_mm_alignr_epi8(c2[304],c2[303],11)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[160]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[44],c2[43],9),simde_mm_xor_si128(c2[192],simde_mm_alignr_epi8(c2[232],c2[231],5))));

//row: 18  [direct, 3 terms]
     d2[288]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[14],c2[13],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[205],c2[204],2),simde_mm_alignr_epi8(c2[228],c2[227],2)));

//row: 22  [direct, 2 terms]
     d2[352]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[46],c2[45],14),simde_mm_alignr_epi8(c2[68],c2[67],15));

//row: 23  [direct, 3 terms]
     d2[368]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[111],c2[110],11),simde_mm_alignr_epi8(c2[175],c2[174],14)));

//row: 24  [direct, 3 terms]
     d2[384]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],11),simde_mm_alignr_epi8(c2[289],c2[288],8)));

//row: 25  [direct, 2 terms]
     d2[400]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],4),simde_mm_alignr_epi8(c2[170],c2[169],12));

//row: 27  [direct, 2 terms]
     d2[432]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],8),simde_mm_alignr_epi8(c2[202],c2[201],7));

//row: 28  [direct, 3 terms]
     d2[448]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[71],c2[70],5),simde_mm_alignr_epi8(c2[169],c2[168],7)));

//row: 29  [direct, 2 terms]
     d2[464]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],2),simde_mm_alignr_epi8(c2[130],c2[129],12));

//row: 30  [direct, 4 terms]
     d2[480]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[69],c2[68],7),simde_mm_xor_si128(c2[175],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[225],c2[224],9),simde_mm_alignr_epi8(c2[294],c2[293],4))));

//row: 36  [direct, 3 terms]
     d2[576]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[67],c2[66],6),simde_mm_alignr_epi8(c2[234],c2[233],10)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[16]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[40],cm[39],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[77],cm[76],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[98],cm[97],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[204],cm[203],13),simde_mm_alignr_epi8(cm[301],cm[300],13)))))));
       dm[32]=simde_mm_xor_si128(dm[16],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[11],cm[10],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[107],cm[106],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[144],cm[143],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[168],cm[167],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[207],cm[206],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[234],cm[233],12),simde_mm_xor_si128(cm[270],simde_mm_alignr_epi8(cm[304],cm[303],12)))))))));
       dm[48]=simde_mm_xor_si128(dm[32],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[40],cm[39],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[132],cm[131],4),cm[271]))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[15]; dm=&dsimd[15];
  dm[16]=simde_mm_xor_si128(dsimd[15],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[1],cm[0],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[40],cm[39],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[77],cm[76],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[98],cm[97],10),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[204],cm[203],13),simde_mm_alignr_epi8(cm[301],cm[300],13)))))));
  dm[32]=simde_mm_xor_si128(dsimd[31],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[11],cm[10],7),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[107],cm[106],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[144],cm[143],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[168],cm[167],13),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[207],cm[206],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[234],cm[233],12),simde_mm_xor_si128(cm[270],simde_mm_alignr_epi8(cm[304],cm[303],12)))))))));
  dm[48]=simde_mm_xor_si128(dsimd[47],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[15],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[6],cm[5],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[40],cm[39],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[99],cm[98],12),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[132],cm[131],4),cm[271]))))));

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

//row: 4  [P1<<71 + 2 sys]
     d2[64]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+583)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],3),simde_mm_alignr_epi8(c2[46],c2[45],6)));

//row: 5  [P1<<103 + 4 sys]
     d2[80]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+615)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[35],c2[34],9),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[173],c2[172],2),simde_mm_alignr_epi8(c2[234],c2[233],15)))));

//row: 6  [P1<<158 + 4 sys]
     d2[96]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+670)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[10],c2[9],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[175],c2[174],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[227],c2[226],13),simde_mm_alignr_epi8(c2[290],c2[289],12)))));

//row: 7  [P1<<3+P3<<116 + 3 sys]
     d2[112]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+515)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1652)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[170],c2[169],3),simde_mm_alignr_epi8(c2[233],c2[232],12)))));

//row: 8  [P2<<230 + 2 sys]
     d2[128]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1254)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],14),simde_mm_alignr_epi8(c2[38],c2[37],14)));

//row: 9  [P0<<61+P1<<247 + 2 sys]
     d2[144]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+61)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+759)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],11),simde_mm_alignr_epi8(c2[269],c2[268],13))));

//row: 11  [P3<<56 + 3 sys]
     d2[176]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1592)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],11),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[239],c2[238],12),simde_mm_alignr_epi8(c2[302],c2[301],2))));

//row: 12  [P1<<14 + 2 sys]
     d2[192]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+526)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],15),simde_mm_alignr_epi8(c2[103],c2[102],15)));

//row: 13  [P3<<222 + 3 sys]
     d2[208]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1758)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[33],c2[32],2),simde_mm_alignr_epi8(c2[259],c2[258],6))));

//row: 14  [P1<<3+P3<<232 + 2 sys]
     d2[224]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+515)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1768)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[40],c2[39],3),simde_mm_alignr_epi8(c2[202],c2[201],1))));

//row: 15  [P0<<175+P1<<213 + 1 sys]
     d2[240]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+175)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+725)),simde_mm_alignr_epi8(c2[4],c2[3],3)));

//row: 16  [P1<<8+P2<<242 + 2 sys]
     d2[256]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+520)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1266)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[45],c2[44],11),simde_mm_alignr_epi8(c2[297],c2[296],14))));

//row: 17  [P1<<114+P2<<64 + 2 sys]
     d2[272]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+626)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1088)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[48],c2[47],14),simde_mm_alignr_epi8(c2[168],c2[167],12))));

//row: 19  [P0<<185 + 2 sys]
     d2[304]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+185)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[6],c2[5],7),simde_mm_alignr_epi8(c2[34],c2[33],4)));

//row: 20  [P1<<29 + 2 sys]
     d2[320]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+541)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],10),simde_mm_alignr_epi8(c2[135],c2[134],9)));

//row: 21  [P3<<210 + 2 sys]
     d2[336]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1746)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[5],c2[4],12),simde_mm_alignr_epi8(c2[259],c2[258],10)));

//row: 26  [P2<<160+P3<<122 + 2 sys]
     d2[416]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1184)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1658)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[66],c2[65],13),simde_mm_alignr_epi8(c2[233],c2[232],15))));

//row: 31  [P3<<1 + 1 sys]
     d2[496]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1537)),simde_mm_alignr_epi8(c2[39],c2[38],10));

//row: 32  [P2<<166 + 2 sys]
     d2[512]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1190)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[16],c2[15],2),simde_mm_alignr_epi8(c2[163],c2[162],12)));

//row: 33  [P0<<235 + 2 sys]
     d2[528]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+235)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[73],c2[72],4),simde_mm_alignr_epi8(c2[235],c2[234],4)));

//row: 34  [P2<<85+P3<<36 + 1 sys]
     d2[544]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1109)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1572)),simde_mm_alignr_epi8(c2[10],c2[9],3)));

//row: 35  [P1<<63 + 2 sys]
     d2[560]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+575)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],9),simde_mm_alignr_epi8(c2[163],c2[162],8)));

//row: 37  [P0<<219+P3<<151 + 0 sys]
     d2[592]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+219)),simde_mm_loadu_si128((const simde__m128i *)(kb+1687)));

//row: 38  [P1<<38 + 2 sys]
     d2[608]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+550)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],15),simde_mm_alignr_epi8(c2[165],c2[164],2)));

//row: 39  [P2<<34 + 2 sys]
     d2[624]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1058)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[15],c2[14],15),simde_mm_alignr_epi8(c2[235],c2[234],12)));

//row: 40  [P0<<75+P3<<120 + 1 sys]
     d2[640]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+75)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1656)),c2[64]));

//row: 41  [P1<<118 + 2 sys]
     d2[656]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+630)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[41],c2[40],1),simde_mm_alignr_epi8(c2[175],c2[174],5)));
  }
}
