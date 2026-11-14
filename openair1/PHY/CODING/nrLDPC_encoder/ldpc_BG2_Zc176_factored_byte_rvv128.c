/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 16 bytes: needs VLEN >= 128
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 16)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 16)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 16)
// generated: BG2 Zc=176, rvv128-bit, factored
// 173 terms vs 1475 expanded (8.53x); 11 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc176_byte_rvv128(uint8_t *c,uint8_t *d) {
  uint8_t kd[1408] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<11; i2++) {
     const uint8_t *cb=c+16*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+16)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+97)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+155)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+404)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+494)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+519)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+717)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+877)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1058)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1120)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1162)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1470)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1489)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1581)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1886)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1760)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2128)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2151)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2166)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2500)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2552)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2832)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2957)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2986)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3272)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3290)),LGF_RVL((const uint8_t *)(cb+3300))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+16*i2),p0);
     LGF_RVS((uint8_t *)(d+1760+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+111)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+377)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2286)),LGF_RVL((const uint8_t *)(cb+2487))))));   //row 10
     LGF_RVS((uint8_t *)(d+3168+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+173)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2143)),LGF_RVL((const uint8_t *)(cb+2486)))));   //row 18
     LGF_RVS((uint8_t *)(d+3872+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+370)),LGF_RVL((const uint8_t *)(cb+867))));   //row 22
     LGF_RVS((uint8_t *)(d+4048+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+110)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1188)),LGF_RVL((const uint8_t *)(cb+1910)))));   //row 23
     LGF_RVS((uint8_t *)(d+4224+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+465)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+812)),LGF_RVL((const uint8_t *)(cb+3229)))));   //row 24
     LGF_RVS((uint8_t *)(d+4400+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+72)),LGF_RVL((const uint8_t *)(cb+1896))));   //row 25
     LGF_RVS((uint8_t *)(d+4752+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+42)),LGF_RVL((const uint8_t *)(cb+2216))));   //row 27
     LGF_RVS((uint8_t *)(d+4928+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+416)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+728)),LGF_RVL((const uint8_t *)(cb+1909)))));   //row 28
     LGF_RVS((uint8_t *)(d+5104+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+139)),LGF_RVL((const uint8_t *)(cb+1569))));   //row 29
     LGF_RVS((uint8_t *)(d+5280+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+788)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1933)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2557)),LGF_RVL((const uint8_t *)(cb+3197))))));   //row 30
     LGF_RVS((uint8_t *)(d+6336+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+73)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+858)),LGF_RVL((const uint8_t *)(cb+2593)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+16*(i2-1);
       uint8_t *dm=d+16*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+156)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+495)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+718)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1059)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2152)),LGF_RVL((const uint8_t *)(cm+3291))))))));
       LGF_RVS((uint8_t *)(dm+176),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+176)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+17)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1121)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1471)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1761)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2167)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2501)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2987)),LGF_RVL((const uint8_t *)(cm+3301))))))))));
       LGF_RVS((uint8_t *)(dm+352),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+352)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+98)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+520)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1163)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1490)),LGF_RVL((const uint8_t *)(cm+2958))))))));
       LGF_RVS((uint8_t *)(dm+528),p3);
     }
  }

  memcpy(kd,d,176); memcpy(kd+176,d,176);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+160, *km=kd+160; uint8_t *dm=d+160;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+156)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+495)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+718)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1059)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2152)),LGF_RVL((const uint8_t *)(cm+3291))))))));
    LGF_RVS((uint8_t *)(dm+176),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+176)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+17)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1121)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1471)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1761)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2167)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2501)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2987)),LGF_RVL((const uint8_t *)(cm+3301))))))))));
    LGF_RVS((uint8_t *)(dm+352),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+352)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+98)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+520)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1163)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1490)),LGF_RVL((const uint8_t *)(cm+2958))))))));
    LGF_RVS((uint8_t *)(dm+528),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*352,d+k*176,176); memcpy(kd+k*352+176,d+k*176,176); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<11; i2++) {
     const uint8_t *cb=c+16*i2, *kb=kd+16*i2;
     LGF_RVS((uint8_t *)(d+704+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+435)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+86)),LGF_RVL((const uint8_t *)(cb+419)))));   //row 4 [P1<<83 + 2 sys]
     LGF_RVS((uint8_t *)(d+880+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+412)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+79)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+436)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1795)),LGF_RVL((const uint8_t *)(cb+2567)))))));   //row 5 [P1<<60 + 4 sys]
     LGF_RVS((uint8_t *)(d+1056+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+381)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+47)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1914)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2474)),LGF_RVL((const uint8_t *)(cb+3323)))))));   //row 6 [P1<<29 + 4 sys]
     LGF_RVS((uint8_t *)(d+1232+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+399)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1111)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+400)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1885)),LGF_RVL((const uint8_t *)(cb+2488)))))));   //row 7 [P1<<47+P3<<55 + 3 sys]
     LGF_RVS((uint8_t *)(d+1408+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+865)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+53)),LGF_RVL((const uint8_t *)(cb+383)))));   //row 8 [P2<<161 + 2 sys]
     LGF_RVS((uint8_t *)(d+1584+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+99)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+416)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+456)),LGF_RVL((const uint8_t *)(cb+2958))))));   //row 9 [P0<<99+P1<<64 + 2 sys]
     LGF_RVS((uint8_t *)(d+1936+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1197)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+91)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2639)),LGF_RVL((const uint8_t *)(cb+3192))))));   //row 11 [P3<<141 + 3 sys]
     LGF_RVS((uint8_t *)(d+2112+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+356)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+474)),LGF_RVL((const uint8_t *)(cb+1067)))));   //row 12 [P1<<4 + 2 sys]
     LGF_RVS((uint8_t *)(d+2288+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1183)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+29)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+443)),LGF_RVL((const uint8_t *)(cb+2843))))));   //row 13 [P3<<127 + 3 sys]
     LGF_RVS((uint8_t *)(d+2464+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+360)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1222)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+363)),LGF_RVL((const uint8_t *)(cb+2257))))));   //row 14 [P1<<8+P3<<166 + 2 sys]
     LGF_RVS((uint8_t *)(d+2640+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+103)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+392)),LGF_RVL((const uint8_t *)(cb+137)))));   //row 15 [P0<<103+P1<<40 + 1 sys]
     LGF_RVS((uint8_t *)(d+2816+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+369)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+869)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+430)),LGF_RVL((const uint8_t *)(cb+3326))))));   //row 16 [P1<<17+P2<<165 + 2 sys]
     LGF_RVS((uint8_t *)(d+2992+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+414)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+867)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+486)),LGF_RVL((const uint8_t *)(cb+1783))))));   //row 17 [P1<<62+P2<<163 + 2 sys]
     LGF_RVS((uint8_t *)(d+3344+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+145)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+13)),LGF_RVL((const uint8_t *)(cb+487)))));   //row 19 [P0<<145 + 2 sys]
     LGF_RVS((uint8_t *)(d+3520+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+525)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+480)),LGF_RVL((const uint8_t *)(cb+1460)))));   //row 20 [P1<<173 + 2 sys]
     LGF_RVS((uint8_t *)(d+3696+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1096)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+156)),LGF_RVL((const uint8_t *)(cb+2982)))));   //row 21 [P3<<40 + 2 sys]
     LGF_RVS((uint8_t *)(d+4576+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+757)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1201)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+740)),LGF_RVL((const uint8_t *)(cb+2502))))));   //row 26 [P2<<53+P3<<145 + 2 sys]
     LGF_RVS((uint8_t *)(d+5456+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1204)),LGF_RVL((const uint8_t *)(cb+469))));   //row 31 [P3<<148 + 1 sys]
     LGF_RVS((uint8_t *)(d+5632+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+846)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+116)),LGF_RVL((const uint8_t *)(cb+1833)))));   //row 32 [P2<<142 + 2 sys]
     LGF_RVS((uint8_t *)(d+5808+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+29)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+809)),LGF_RVL((const uint8_t *)(cb+2601)))));   //row 33 [P0<<29 + 2 sys]
     LGF_RVS((uint8_t *)(d+5984+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+745)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1218)),LGF_RVL((const uint8_t *)(cb+11)))));   //row 34 [P2<<41+P3<<162 + 1 sys]
     LGF_RVS((uint8_t *)(d+6160+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+524)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+478)),LGF_RVL((const uint8_t *)(cb+1912)))));   //row 35 [P1<<172 + 2 sys]
     LGF_RVS((uint8_t *)(d+6512+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+167)),LGF_RVL((const uint8_t *)(kb+1094))));   //row 37 [P0<<167+P3<<38 + 0 sys]
     LGF_RVS((uint8_t *)(d+6688+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+371)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+464)),LGF_RVL((const uint8_t *)(cb+1767)))));   //row 38 [P1<<19 + 2 sys]
     LGF_RVS((uint8_t *)(d+6864+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+809)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+109)),LGF_RVL((const uint8_t *)(cb+2470)))));   //row 39 [P2<<105 + 2 sys]
     LGF_RVS((uint8_t *)(d+7040+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+156)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1138)),LGF_RVL((const uint8_t *)(cb+864)))));   //row 40 [P0<<156+P3<<82 + 1 sys]
     LGF_RVS((uint8_t *)(d+7216+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+360)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+484)),LGF_RVL((const uint8_t *)(cb+1766)))));   //row 41 [P1<<8 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
