/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 16 bytes: needs VLEN >= 128
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 16)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 16)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 16)
// generated: BG2 Zc=224, rvv128-bit, factored
// 173 terms vs 1473 expanded (8.51x); 14 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc224_byte_rvv128(uint8_t *c,uint8_t *d) {
  uint8_t kd[1792] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<14; i2++) {
     const uint8_t *cb=c+16*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+53)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+72)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+152)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+558)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+579)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+633)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+902)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+919)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1390)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1500)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1525)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1828)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1907)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1983)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2364)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2396)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2783)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2803)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2812)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3246)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3336)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3613)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3675)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3740)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4040)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4063)),LGF_RVL((const uint8_t *)(cb+4165))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+16*i2),p0);
     LGF_RVS((uint8_t *)(d+2240+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+174)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+598)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2696)),LGF_RVL((const uint8_t *)(cb+3192))))));   //row 10
     LGF_RVS((uint8_t *)(d+4032+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+68)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2704)),LGF_RVL((const uint8_t *)(cb+3292)))));   //row 18
     LGF_RVS((uint8_t *)(d+4928+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+497)),LGF_RVL((const uint8_t *)(cb+897))));   //row 22
     LGF_RVS((uint8_t *)(d+5152+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+156)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1398)),LGF_RVL((const uint8_t *)(cb+2374)))));   //row 23
     LGF_RVS((uint8_t *)(d+5376+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+601)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+984)),LGF_RVL((const uint8_t *)(cb+4095)))));   //row 24
     LGF_RVS((uint8_t *)(d+5600+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+211)),LGF_RVL((const uint8_t *)(cb+2334))));   //row 25
     LGF_RVS((uint8_t *)(d+6048+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+27)),LGF_RVL((const uint8_t *)(cb+2806))));   //row 27
     LGF_RVS((uint8_t *)(d+6272+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+664)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1108)),LGF_RVL((const uint8_t *)(cb+2433)))));   //row 28
     LGF_RVS((uint8_t *)(d+6496+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+108)),LGF_RVL((const uint8_t *)(cb+1853))));   //row 29
     LGF_RVS((uint8_t *)(d+6720+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1002)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2284)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3321)),LGF_RVL((const uint8_t *)(cb+4208))))));   //row 30
     LGF_RVS((uint8_t *)(d+8064+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1071)),LGF_RVL((const uint8_t *)(cb+3219)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+16*(i2-1);
       uint8_t *dm=d+16*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+72)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+558)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+919)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1525)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2783)),LGF_RVL((const uint8_t *)(cm+4040))))))));
       LGF_RVS((uint8_t *)(dm+224),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+224)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+53)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1500)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1907)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2396)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2803)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3336)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3613)),LGF_RVL((const uint8_t *)(cm+4063))))))))));
       LGF_RVS((uint8_t *)(dm+448),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+448)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+152)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+579)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1390)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1983)),LGF_RVL((const uint8_t *)(cm+3675))))))));
       LGF_RVS((uint8_t *)(dm+672),p3);
     }
  }

  memcpy(kd,d,224); memcpy(kd+224,d,224);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+208, *km=kd+208; uint8_t *dm=d+208;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+72)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+558)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+919)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1525)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2783)),LGF_RVL((const uint8_t *)(cm+4040))))))));
    LGF_RVS((uint8_t *)(dm+224),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+224)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+53)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1500)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1907)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2396)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2803)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3336)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3613)),LGF_RVL((const uint8_t *)(cm+4063))))))))));
    LGF_RVS((uint8_t *)(dm+448),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+448)),LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+152)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+579)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1390)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1983)),LGF_RVL((const uint8_t *)(cm+3675))))))));
    LGF_RVS((uint8_t *)(dm+672),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*448,d+k*224,224); memcpy(kd+k*448+224,d+k*224,224); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<14; i2++) {
     const uint8_t *cb=c+16*i2, *kb=kd+16*i2;
     LGF_RVS((uint8_t *)(d+896+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+549)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+200)),LGF_RVL((const uint8_t *)(cb+464)))));   //row 4 [P1<<101 + 2 sys]
     LGF_RVS((uint8_t *)(d+1120+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+641)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+185)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+586)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2410)),LGF_RVL((const uint8_t *)(cb+3355)))))));   //row 5 [P1<<193 + 4 sys]
     LGF_RVS((uint8_t *)(d+1344+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+657)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+123)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2295)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3167)),LGF_RVL((const uint8_t *)(cb+4254)))))));   //row 6 [P1<<209 + 4 sys]
     LGF_RVS((uint8_t *)(d+1568+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+598)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1525)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+551)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2253)),LGF_RVL((const uint8_t *)(cb+3241)))))));   //row 7 [P1<<150+P3<<181 + 3 sys]
     LGF_RVS((uint8_t *)(d+1792+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1048)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+147)),LGF_RVL((const uint8_t *)(cb+491)))));   //row 8 [P2<<152 + 2 sys]
     LGF_RVS((uint8_t *)(d+2016+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+184)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+531)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+450)),LGF_RVL((const uint8_t *)(cb+3614))))));   //row 9 [P0<<184+P1<<83 + 2 sys]
     LGF_RVS((uint8_t *)(d+2464+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1443)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+99)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3274)),LGF_RVL((const uint8_t *)(cb+4142))))));   //row 11 [P3<<99 + 3 sys]
     LGF_RVS((uint8_t *)(d+2688+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+557)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+494)),LGF_RVL((const uint8_t *)(cb+1561)))));   //row 12 [P1<<109 + 2 sys]
     LGF_RVS((uint8_t *)(d+2912+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1484)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+37)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+561)),LGF_RVL((const uint8_t *)(cb+3727))))));   //row 13 [P3<<140 + 3 sys]
     LGF_RVS((uint8_t *)(d+3136+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+488)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1460)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+484)),LGF_RVL((const uint8_t *)(cb+2783))))));   //row 14 [P1<<40+P3<<116 + 2 sys]
     LGF_RVS((uint8_t *)(d+3360+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+200)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+558)),LGF_RVL((const uint8_t *)(cb+116)))));   //row 15 [P0<<200+P1<<110 + 1 sys]
     LGF_RVS((uint8_t *)(d+3584+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+582)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+993)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+523)),LGF_RVL((const uint8_t *)(cb+4190))))));   //row 16 [P1<<134+P2<<97 + 2 sys]
     LGF_RVS((uint8_t *)(d+3808+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+654)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+898)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+496)),LGF_RVL((const uint8_t *)(cb+2372))))));   //row 17 [P1<<206+P2<<2 + 2 sys]
     LGF_RVS((uint8_t *)(d+4256+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+86)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+35)),LGF_RVL((const uint8_t *)(cb+586)))));   //row 19 [P0<<86 + 2 sys]
     LGF_RVS((uint8_t *)(d+4480+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+589)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+454)),LGF_RVL((const uint8_t *)(cb+1812)))));   //row 20 [P1<<141 + 2 sys]
     LGF_RVS((uint8_t *)(d+4704+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1425)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+80)),LGF_RVL((const uint8_t *)(cb+3627)))));   //row 21 [P3<<81 + 2 sys]
     LGF_RVS((uint8_t *)(d+5824+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1117)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1350)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+986)),LGF_RVL((const uint8_t *)(cb+3142))))));   //row 26 [P2<<221+P3<<6 + 2 sys]
     LGF_RVS((uint8_t *)(d+6944+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1526)),LGF_RVL((const uint8_t *)(cb+595))));   //row 31 [P3<<182 + 1 sys]
     LGF_RVS((uint8_t *)(d+7168+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1006)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+108)),LGF_RVL((const uint8_t *)(cb+2261)))));   //row 32 [P2<<110 + 2 sys]
     LGF_RVS((uint8_t *)(d+7392+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+109)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+967)),LGF_RVL((const uint8_t *)(cb+3148)))));   //row 33 [P0<<109 + 2 sys]
     LGF_RVS((uint8_t *)(d+7616+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1097)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1413)),LGF_RVL((const uint8_t *)(cb+29)))));   //row 34 [P2<<201+P3<<69 + 1 sys]
     LGF_RVS((uint8_t *)(d+7840+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+503)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+539)),LGF_RVL((const uint8_t *)(cb+2405)))));   //row 35 [P1<<55 + 2 sys]
     LGF_RVS((uint8_t *)(d+8288+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+40)),LGF_RVL((const uint8_t *)(kb+1356))));   //row 37 [P0<<40+P3<<12 + 0 sys]
     LGF_RVS((uint8_t *)(d+8512+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+494)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+485)),LGF_RVL((const uint8_t *)(cb+2337)))));   //row 38 [P1<<46 + 2 sys]
     LGF_RVS((uint8_t *)(d+8736+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1050)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+106)),LGF_RVL((const uint8_t *)(cb+3317)))));   //row 39 [P2<<154 + 2 sys]
     LGF_RVS((uint8_t *)(d+8960+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+35)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1380)),LGF_RVL((const uint8_t *)(cb+994)))));   //row 40 [P0<<35+P3<<36 + 1 sys]
     LGF_RVS((uint8_t *)(d+9184+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+529)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+568)),LGF_RVL((const uint8_t *)(cb+2341)))));   //row 41 [P1<<81 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
