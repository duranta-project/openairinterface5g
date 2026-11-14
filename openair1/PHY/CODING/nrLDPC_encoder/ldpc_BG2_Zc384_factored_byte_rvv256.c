/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 32 bytes: needs VLEN >= 256
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 32)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 32)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 32)
// generated: BG2 Zc=384, rvv256-bit, factored
// 173 terms vs 1473 expanded (8.51x); 12 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc384_byte_rvv256(uint8_t *c,uint8_t *d) {
  uint8_t kd[3072] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<12; i2++) {
     const uint8_t *cb=c+32*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+24)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+26)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+173)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+864)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+881)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+903)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1701)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1710)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2339)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2369)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2420)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3119)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3181)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3184)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3911)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3931)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4638)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4678)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4730)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5493)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5562)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+6171)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+6257)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+6328)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+6914)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+7083)),LGF_RVL((const uint8_t *)(cb+7097))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+32*i2),p0);
     LGF_RVS((uint8_t *)(d+3840+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+59)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+872)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4630)),LGF_RVL((const uint8_t *)(cb+5428))))));   //row 10
     LGF_RVS((uint8_t *)(d+6912+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+186)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4614)),LGF_RVL((const uint8_t *)(cb+5422)))));   //row 18
     LGF_RVS((uint8_t *)(d+8448+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+788)),LGF_RVL((const uint8_t *)(cb+1588))));   //row 22
     LGF_RVS((uint8_t *)(d+8832+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+106)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2390)),LGF_RVL((const uint8_t *)(cb+3935)))));   //row 23
     LGF_RVS((uint8_t *)(d+9216+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+950)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1689)),LGF_RVL((const uint8_t *)(cb+6976)))));   //row 24
     LGF_RVS((uint8_t *)(d+9600+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+45)),LGF_RVL((const uint8_t *)(cb+3861))));   //row 25
     LGF_RVS((uint8_t *)(d+10368+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+103)),LGF_RVL((const uint8_t *)(cb+4658))));   //row 27
     LGF_RVS((uint8_t *)(d+10752+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+838)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1647)),LGF_RVL((const uint8_t *)(cb+4008)))));   //row 28
     LGF_RVS((uint8_t *)(d+11136+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+110)),LGF_RVL((const uint8_t *)(cb+3089))));   //row 29
     LGF_RVS((uint8_t *)(d+11520+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1656)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3994)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5428)),LGF_RVL((const uint8_t *)(cb+6968))))));   //row 30
     LGF_RVS((uint8_t *)(d+13824+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+25)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1687)),LGF_RVL((const uint8_t *)(cb+5546)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+32*(i2-1);
       uint8_t *dm=d+32*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+174)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+865)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1702)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2370)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4679)),LGF_RVL((const uint8_t *)(cm+7084))))))));
       LGF_RVS((uint8_t *)(dm+384),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+384)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+27)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2340)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3120)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3932)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4639)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+5563)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+6329)),LGF_RVL((const uint8_t *)(cm+6915))))))))));
       LGF_RVS((uint8_t *)(dm+768),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+768)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+25)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+882)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2421)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3182)),LGF_RVL((const uint8_t *)(cm+6258))))))));
       LGF_RVS((uint8_t *)(dm+1152),p3);
     }
  }

  memcpy(kd,d,384); memcpy(kd+384,d,384);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+352, *km=kd+352; uint8_t *dm=d+352;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+174)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+865)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1702)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2370)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4679)),LGF_RVL((const uint8_t *)(cm+7084))))))));
    LGF_RVS((uint8_t *)(dm+384),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+384)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+27)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2340)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3120)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3932)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4639)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+5563)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+6329)),LGF_RVL((const uint8_t *)(cm+6915))))))))));
    LGF_RVS((uint8_t *)(dm+768),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+768)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+25)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+882)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2421)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3182)),LGF_RVL((const uint8_t *)(cm+6258))))))));
    LGF_RVS((uint8_t *)(dm+1152),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*768,d+k*384,384); memcpy(kd+k*768+384,d+k*384,384); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<12; i2++) {
     const uint8_t *cb=c+32*i2, *kb=kd+32*i2;
     LGF_RVS((uint8_t *)(d+1536+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+797)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+72)),LGF_RVL((const uint8_t *)(cb+842)))));   //row 4 [P1<<29 + 2 sys]
     LGF_RVS((uint8_t *)(d+1920+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+816)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+10)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+812)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3961)),LGF_RVL((const uint8_t *)(cb+5456)))))));   //row 5 [P1<<48 + 4 sys]
     LGF_RVS((uint8_t *)(d+2304+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+952)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+129)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3932)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5476)),LGF_RVL((const uint8_t *)(cb+6961)))))));   //row 6 [P1<<184 + 4 sys]
     LGF_RVS((uint8_t *)(d+2688+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+870)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2447)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+848)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4026)),LGF_RVL((const uint8_t *)(cb+5392)))))));   //row 7 [P1<<102+P3<<143 + 3 sys]
     LGF_RVS((uint8_t *)(d+3072+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1688)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+118)),LGF_RVL((const uint8_t *)(cb+838)))));   //row 8 [P2<<152 + 2 sys]
     LGF_RVS((uint8_t *)(d+3456+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+185)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+946)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+796)),LGF_RVL((const uint8_t *)(cb+6276))))));   //row 9 [P0<<185+P1<<178 + 2 sys]
     LGF_RVS((uint8_t *)(d+4224+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2458)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+32)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5468)),LGF_RVL((const uint8_t *)(cb+7086))))));   //row 11 [P3<<154 + 3 sys]
     LGF_RVS((uint8_t *)(d+4608+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+779)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+807)),LGF_RVL((const uint8_t *)(cb+2397)))));   //row 12 [P1<<11 + 2 sys]
     LGF_RVS((uint8_t *)(d+4992+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2470)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+49)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+893)),LGF_RVL((const uint8_t *)(cb+6179))))));   //row 13 [P3<<166 + 3 sys]
     LGF_RVS((uint8_t *)(d+5376+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+789)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2467)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+787)),LGF_RVL((const uint8_t *)(cb+4726))))));   //row 14 [P1<<21+P3<<163 + 2 sys]
     LGF_RVS((uint8_t *)(d+5760+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+63)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+849)),LGF_RVL((const uint8_t *)(cb+68)))));   //row 15 [P0<<63+P1<<81 + 1 sys]
     LGF_RVS((uint8_t *)(d+6144+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+903)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1600)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+855)),LGF_RVL((const uint8_t *)(cb+7089))))));   //row 16 [P1<<135+P2<<64 + 2 sys]
     LGF_RVS((uint8_t *)(d+6528+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+777)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1542)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+926)),LGF_RVL((const uint8_t *)(cb+3863))))));   //row 17 [P1<<9+P2<<6 + 2 sys]
     LGF_RVS((uint8_t *)(d+7296+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+156)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+58)),LGF_RVL((const uint8_t *)(cb+810)))));   //row 19 [P0<<156 + 2 sys]
     LGF_RVS((uint8_t *)(d+7680+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+921)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+844)),LGF_RVL((const uint8_t *)(cb+3133)))));   //row 20 [P1<<153 + 2 sys]
     LGF_RVS((uint8_t *)(d+8064+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2371)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+157)),LGF_RVL((const uint8_t *)(cb+6319)))));   //row 21 [P3<<67 + 2 sys]
     LGF_RVS((uint8_t *)(d+9984+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1591)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2389)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1603)),LGF_RVL((const uint8_t *)(cb+5513))))));   //row 26 [P2<<55+P3<<85 + 2 sys]
     LGF_RVS((uint8_t *)(d+11904+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2474)),LGF_RVL((const uint8_t *)(cb+771))));   //row 31 [P3<<170 + 1 sys]
     LGF_RVS((uint8_t *)(d+12288+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1553)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+84)),LGF_RVL((const uint8_t *)(cb+3848)))));   //row 32 [P2<<17 + 2 sys]
     LGF_RVS((uint8_t *)(d+12672+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+124)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1701)),LGF_RVL((const uint8_t *)(cb+5555)))));   //row 33 [P0<<124 + 2 sys]
     LGF_RVS((uint8_t *)(d+13056+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1713)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2316)),LGF_RVL((const uint8_t *)(cb+173)))));   //row 34 [P2<<177+P3<<12 + 1 sys]
     LGF_RVS((uint8_t *)(d+13440+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+786)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+845)),LGF_RVL((const uint8_t *)(cb+4024)))));   //row 35 [P1<<18 + 2 sys]
     LGF_RVS((uint8_t *)(d+14208+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+37)),LGF_RVL((const uint8_t *)(kb+2335))));   //row 37 [P0<<37+P3<<31 + 0 sys]
     LGF_RVS((uint8_t *)(d+14592+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+958)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+852)),LGF_RVL((const uint8_t *)(cb+3991)))));   //row 38 [P1<<190 + 2 sys]
     LGF_RVS((uint8_t *)(d+14976+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1593)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+93)),LGF_RVL((const uint8_t *)(cb+5508)))));   //row 39 [P2<<57 + 2 sys]
     LGF_RVS((uint8_t *)(d+15360+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+107)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2467)),LGF_RVL((const uint8_t *)(cb+1639)))));   //row 40 [P0<<107+P3<<163 + 1 sys]
     LGF_RVS((uint8_t *)(d+15744+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+828)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+915)),LGF_RVL((const uint8_t *)(cb+3847)))));   //row 41 [P1<<60 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
