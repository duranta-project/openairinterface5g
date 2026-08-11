#include <string.h>
#include "PHY/sse_intrin.h"
// generated code for BG2 Zc=208, byte encoding, factored
// 173 terms vs 1483 expanded (8.57x); pipeline lag 1 chunk(s)
static inline void ldpc_BG2_Zc208_byte(uint8_t *c,uint8_t *d) {
  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;
  simde__m128i *c2,*d2,*cm,*dm;
  uint8_t kd[1664] __attribute__((aligned(64)));
  int i2;

  // stage 1: P0, the kernel-free extension rows, and the kernel rows
  // pipelined 1 chunk(s) behind P0 so no loop split is needed
  for (i2=0; i2<13; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];

//row: 0
     d2[0]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[2],c2[1],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[29],c2[28],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[55],c2[54],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],15),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[80],c2[79],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[81],c2[80],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[105],c2[104],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[106],c2[105],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[116],c2[115],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[137],c2[136],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[168],c2[167],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[169],c2[168],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[169],c2[168],10),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[183],c2[182],2),simde_mm_xor_si128(c2[183],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[209],c2[208],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[217],c2[216],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[220],c2[219],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[235],c2[234],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[244],c2[243],15),simde_mm_alignr_epi8(c2[246],c2[245],3)))))))))))))))))))))))))));

//row: 10  [direct, 4 terms]
     d2[130]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[8],c2[7],13),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],12),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[168],c2[167],1),simde_mm_alignr_epi8(c2[186],c2[185],3))));

//row: 18  [direct, 3 terms]
     d2[234]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[169],c2[168],11),simde_mm_alignr_epi8(c2[191],c2[190],12)));

//row: 22  [direct, 2 terms]
     d2[286]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],10),simde_mm_alignr_epi8(c2[60],c2[59],14));

//row: 23  [direct, 3 terms]
     d2[299]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],4),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[89],c2[88],10),simde_mm_alignr_epi8(c2[131],c2[130],13)));

//row: 24  [direct, 3 terms]
     d2[312]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],1),simde_mm_alignr_epi8(c2[240],c2[239],8)));

//row: 25  [direct, 2 terms]
     d2[325]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],5),simde_mm_alignr_epi8(c2[143],c2[142],2));

//row: 27  [direct, 2 terms]
     d2[351]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[11],c2[10],8),simde_mm_alignr_epi8(c2[169],c2[168],1));

//row: 28  [direct, 3 terms]
     d2[364]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[27],c2[26],14),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[64],c2[63],10),simde_mm_alignr_epi8(c2[133],c2[132],14)));

//row: 29  [direct, 2 terms]
     d2[377]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[4],c2[3],2),simde_mm_alignr_epi8(c2[106],c2[105],11));

//row: 30  [direct, 4 terms]
     d2[390]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[57],c2[56],6),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[132],c2[131],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[186],c2[185],2),simde_mm_alignr_epi8(c2[235],c2[234],6))));

//row: 36  [direct, 3 terms]
     d2[468]=simde_mm_xor_si128(simde_mm_alignr_epi8(c2[13],c2[12],5),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],7),simde_mm_alignr_epi8(c2[194],c2[193],3)));

     if (i2>=1) {
       cm=&csimd[i2-1]; dm=&dsimd[i2-1];
       dm[13]=simde_mm_xor_si128(dm[0],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[28],cm[27],3),simde_mm_xor_si128(cm[63],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[89],cm[88],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[169],cm[168],4),simde_mm_alignr_epi8(cm[235],cm[234],13)))))));
       dm[26]=simde_mm_xor_si128(dm[13],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[80],cm[79],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[105],cm[104],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[137],cm[136],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[168],cm[167],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],14),simde_mm_alignr_epi8(cm[246],cm[245],4)))))))));
       dm[39]=simde_mm_xor_si128(dm[26],simde_mm_xor_si128(simde_mm_alignr_epi8(dm[1],dm[0],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[116],cm[115],7),simde_mm_alignr_epi8(cm[217],cm[216],4)))))));
     }
  }

  // epilogue: kernel rows for the final 1 chunk(s), wrapping
  cm=&csimd[12]; dm=&dsimd[12];
  dm[13]=simde_mm_xor_si128(dsimd[12],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[9],cm[8],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[28],cm[27],3),simde_mm_xor_si128(cm[63],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[89],cm[88],5),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[169],cm[168],4),simde_mm_alignr_epi8(cm[235],cm[234],13)))))));
  dm[26]=simde_mm_xor_si128(dsimd[25],simde_mm_xor_si128(simde_mm_alignr_epi8(cm[2],cm[1],2),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[80],cm[79],11),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[105],cm[104],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[137],cm[136],6),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[168],cm[167],9),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[184],cm[183],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[209],cm[208],14),simde_mm_alignr_epi8(cm[246],cm[245],4)))))))));
  dm[39]=simde_mm_xor_si128(dsimd[38],simde_mm_xor_si128(simde_mm_alignr_epi8(dsimd[0],dsimd[12],1),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[8],cm[7],14),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[37],cm[36],3),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[81],cm[80],15),simde_mm_xor_si128(simde_mm_alignr_epi8(cm[116],cm[115],7),simde_mm_alignr_epi8(cm[217],cm[216],4)))))));

  // double P0..P3 so any rotation becomes one unaligned load
  for (int k=0;k<4;k++) {
    memcpy(kd+k*416,     d+k*208, 208);
    memcpy(kd+k*416+208, d+k*208, 208);
  }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<13; i2++) {
     c2=&csimd[i2];
     d2=&dsimd[i2];
     const uint8_t *kb=kd+16*i2;

//row: 4  [P1<<117 + 2 sys]
     d2[52]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+533)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],11),simde_mm_alignr_epi8(c2[28],c2[27],11)));

//row: 5  [P1<<62 + 4 sys]
     d2[65]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+478)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[9],c2[8],8),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],1),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[133],c2[132],4),simde_mm_alignr_epi8(c2[191],c2[190],4)))));

//row: 6  [P1<<12 + 4 sys]
     d2[78]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+428)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[133],c2[132],2),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[195],c2[194],6),simde_mm_alignr_epi8(c2[245],c2[244],8)))));

//row: 7  [P1<<107+P3<<58 + 3 sys]
     d2[91]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+523)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1306)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[37],c2[36],3),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[135],c2[134],14),simde_mm_alignr_epi8(c2[191],c2[190],15)))));

//row: 8  [P2<<22 + 2 sys]
     d2[104]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+854)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],5),simde_mm_alignr_epi8(c2[38],c2[37],1)));

//row: 9  [P0<<205+P1<<81 + 2 sys]
     d2[117]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+205)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+497)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],10),simde_mm_alignr_epi8(c2[210],c2[209],11))));

//row: 11  [P3<<8 + 3 sys]
     d2[143]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1256)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[3],c2[2],7),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[184],c2[183],13),simde_mm_alignr_epi8(c2[237],c2[236],3))));

//row: 12  [P1<<49 + 2 sys]
     d2[156]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+465)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[28],c2[27],2),simde_mm_alignr_epi8(c2[88],c2[87],11)));

//row: 13  [P3<<186 + 3 sys]
     d2[169]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1434)),simde_mm_xor_si128(c2[2],simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],5),simde_mm_alignr_epi8(c2[214],c2[213],15))));

//row: 14  [P1<<52+P3<<109 + 2 sys]
     d2[182]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+468)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1357)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[32],c2[31],11),simde_mm_alignr_epi8(c2[158],c2[157],4))));

//row: 15  [P0<<108+P1<<102 + 1 sys]
     d2[195]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+108)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+518)),simde_mm_alignr_epi8(c2[11],c2[10],14)));

//row: 16  [P1<<54+P2<<176 + 2 sys]
     d2[208]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+470)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1008)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[34],c2[33],13),simde_mm_alignr_epi8(c2[236],c2[235],15))));

//row: 17  [P1<<142+P2<<35 + 2 sys]
     d2[221]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+558)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+867)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[30],c2[29],9),simde_mm_alignr_epi8(c2[143],c2[142],9))));

//row: 19  [P0<<52 + 2 sys]
     d2[247]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+52)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),simde_mm_alignr_epi8(c2[34],c2[33],12)));

//row: 20  [P1<<114 + 2 sys]
     d2[260]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+530)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[39],c2[38],4),simde_mm_alignr_epi8(c2[107],c2[106],3)));

//row: 21  [P3<<23 + 2 sys]
     d2[273]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1271)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[1],c2[0],10),simde_mm_alignr_epi8(c2[216],c2[215],10)));

//row: 26  [P2<<49+P3<<161 + 2 sys]
     d2[338]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+881)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1409)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],4),simde_mm_alignr_epi8(c2[193],c2[192],12))));

//row: 31  [P3<<189 + 1 sys]
     d2[403]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1437)),simde_mm_alignr_epi8(c2[34],c2[33],3));

//row: 32  [P2<<163 + 2 sys]
     d2[416]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+995)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[7],c2[6],14),c2[130]));

//row: 33  [P0<<179 + 2 sys]
     d2[429]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+179)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[63],c2[62],3),simde_mm_alignr_epi8(c2[193],c2[192],13)));

//row: 34  [P2<<191+P3<<193 + 1 sys]
     d2[442]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1023)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1441)),simde_mm_alignr_epi8(c2[13],c2[12],5)));

//row: 35  [P1<<181 + 2 sys]
     d2[455]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+597)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],13),simde_mm_alignr_epi8(c2[141],c2[140],7)));

//row: 37  [P0<<181+P3<<193 + 0 sys]
     d2[481]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+181)),simde_mm_loadu_si128((const simde__m128i *)(kb+1441)));

//row: 38  [P1<<191 + 2 sys]
     d2[494]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+607)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[36],c2[35],13),simde_mm_alignr_epi8(c2[141],c2[140],13)));

//row: 39  [P2<<173 + 2 sys]
     d2[507]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1005)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[12],c2[11],5),simde_mm_alignr_epi8(c2[192],c2[191],13)));

//row: 40  [P0<<163+P3<<179 + 1 sys]
     d2[520]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+163)),simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+1427)),simde_mm_alignr_epi8(c2[65],c2[64],1)));

//row: 41  [P1<<167 + 2 sys]
     d2[533]=simde_mm_xor_si128(simde_mm_loadu_si128((const simde__m128i *)(kb+583)),simde_mm_xor_si128(simde_mm_alignr_epi8(c2[38],c2[37],15),simde_mm_alignr_epi8(c2[143],c2[142],5)));
  }
}
