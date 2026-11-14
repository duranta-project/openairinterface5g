/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 128 bytes: needs VLEN >= 1024
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 128)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 128)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 128)
// generated: BG2 Zc=256, rvv1024-bit, factored
// 173 terms vs 1477 expanded (8.54x); 2 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc256_byte_rvv1024(uint8_t *c,uint8_t *d) {
  uint8_t kd[2048] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<2; i2++) {
     const uint8_t *cb=c+128*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+8)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+80)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+166)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+519)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+625)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+628)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1081)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1227)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1561)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1579)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1701)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2099)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2205)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2300)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2663)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2684)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3260)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3280)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3297)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3637)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3739)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4113)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4319)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4335)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4735)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4812)),LGF_RVL((const uint8_t *)(cb+4859))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+128*i2),p0);
     LGF_RVS((uint8_t *)(d+2560+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+11)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+697)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3072)),LGF_RVL((const uint8_t *)(cb+3701))))));   //row 10
     LGF_RVS((uint8_t *)(d+4608+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+220)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3266)),LGF_RVL((const uint8_t *)(cb+3634)))));   //row 18
     LGF_RVS((uint8_t *)(d+5632+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+734)),LGF_RVL((const uint8_t *)(cb+1087))));   //row 22
     LGF_RVS((uint8_t *)(d+5888+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+23)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1771)),LGF_RVL((const uint8_t *)(cb+2798)))));   //row 23
     LGF_RVS((uint8_t *)(d+6144+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+558)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1163)),LGF_RVL((const uint8_t *)(cb+4616)))));   //row 24
     LGF_RVS((uint8_t *)(d+6400+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+228)),LGF_RVL((const uint8_t *)(cb+2716))));   //row 25
     LGF_RVS((uint8_t *)(d+6912+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+8)),LGF_RVL((const uint8_t *)(cb+3223))));   //row 27
     LGF_RVS((uint8_t *)(d+7168+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+610)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1125)),LGF_RVL((const uint8_t *)(cb+2695)))));   //row 28
     LGF_RVS((uint8_t *)(d+7424+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+18)),LGF_RVL((const uint8_t *)(cb+2076))));   //row 29
     LGF_RVS((uint8_t *)(d+7680+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1095)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2800)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3593)),LGF_RVL((const uint8_t *)(cb+4692))))));   //row 30
     LGF_RVS((uint8_t *)(d+9216+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+140)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1062)),LGF_RVL((const uint8_t *)(cb+3738)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+128*(i2-1);
       uint8_t *dm=d+128*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+9)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+629)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1228)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1562)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3261)),LGF_RVL((const uint8_t *)(cm+4813))))))));
       LGF_RVS((uint8_t *)(dm+256),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+256)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+167)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1702)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2301)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2685)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3298)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3740)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4320)),LGF_RVL((const uint8_t *)(cm+4860))))))))));
       LGF_RVS((uint8_t *)(dm+512),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+512)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+81)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+626)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1580)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2100)),LGF_RVL((const uint8_t *)(cm+4336))))))));
       LGF_RVS((uint8_t *)(dm+768),p3);
     }
  }

  memcpy(kd,d,256); memcpy(kd+256,d,256);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+128, *km=kd+128; uint8_t *dm=d+128;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+9)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+629)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1228)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1562)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3261)),LGF_RVL((const uint8_t *)(cm+4813))))))));
    LGF_RVS((uint8_t *)(dm+256),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+256)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+167)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1702)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2301)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2685)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3298)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3740)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4320)),LGF_RVL((const uint8_t *)(cm+4860))))))))));
    LGF_RVS((uint8_t *)(dm+512),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+512)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+81)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+626)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1580)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2100)),LGF_RVL((const uint8_t *)(cm+4336))))))));
    LGF_RVS((uint8_t *)(dm+768),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*512,d+k*256,256); memcpy(kd+k*512+256,d+k*256,256); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<2; i2++) {
     const uint8_t *cb=c+128*i2, *kb=kd+128*i2;
     LGF_RVS((uint8_t *)(d+1024+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+583)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+179)),LGF_RVL((const uint8_t *)(cb+726)))));   //row 4 [P1<<71 + 2 sys]
     LGF_RVS((uint8_t *)(d+1280+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+615)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+231)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+553)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2754)),LGF_RVL((const uint8_t *)(cb+3743)))))));   //row 5 [P1<<103 + 4 sys]
     LGF_RVS((uint8_t *)(d+1536+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+670)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+155)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2788)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3629)),LGF_RVL((const uint8_t *)(cb+4636)))))));   //row 6 [P1<<158 + 4 sys]
     LGF_RVS((uint8_t *)(d+1792+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+515)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1652)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+641)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2707)),LGF_RVL((const uint8_t *)(cb+3724)))))));   //row 7 [P1<<3+P3<<116 + 3 sys]
     LGF_RVS((uint8_t *)(d+2048+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1254)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+142)),LGF_RVL((const uint8_t *)(cb+606)))));   //row 8 [P2<<230 + 2 sys]
     LGF_RVS((uint8_t *)(d+2304+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+61)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+759)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+715)),LGF_RVL((const uint8_t *)(cb+4301))))));   //row 9 [P0<<61+P1<<247 + 2 sys]
     LGF_RVS((uint8_t *)(d+2816+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1592)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+11)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3820)),LGF_RVL((const uint8_t *)(cb+4818))))));   //row 11 [P3<<56 + 3 sys]
     LGF_RVS((uint8_t *)(d+3072+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+526)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+575)),LGF_RVL((const uint8_t *)(cb+1647)))));   //row 12 [P1<<14 + 2 sys]
     LGF_RVS((uint8_t *)(d+3328+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1758)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+83)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+514)),LGF_RVL((const uint8_t *)(cb+4134))))));   //row 13 [P3<<222 + 3 sys]
     LGF_RVS((uint8_t *)(d+3584+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+515)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1768)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+627)),LGF_RVL((const uint8_t *)(cb+3217))))));   //row 14 [P1<<3+P3<<232 + 2 sys]
     LGF_RVS((uint8_t *)(d+3840+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+175)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+725)),LGF_RVL((const uint8_t *)(cb+51)))));   //row 15 [P0<<175+P1<<213 + 1 sys]
     LGF_RVS((uint8_t *)(d+4096+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+520)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1266)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+715)),LGF_RVL((const uint8_t *)(cb+4750))))));   //row 16 [P1<<8+P2<<242 + 2 sys]
     LGF_RVS((uint8_t *)(d+4352+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+626)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1088)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+766)),LGF_RVL((const uint8_t *)(cb+2684))))));   //row 17 [P1<<114+P2<<64 + 2 sys]
     LGF_RVS((uint8_t *)(d+4864+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+185)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+87)),LGF_RVL((const uint8_t *)(cb+532)))));   //row 19 [P0<<185 + 2 sys]
     LGF_RVS((uint8_t *)(d+5120+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+541)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+538)),LGF_RVL((const uint8_t *)(cb+2153)))));   //row 20 [P1<<29 + 2 sys]
     LGF_RVS((uint8_t *)(d+5376+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1746)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+76)),LGF_RVL((const uint8_t *)(cb+4138)))));   //row 21 [P3<<210 + 2 sys]
     LGF_RVS((uint8_t *)(d+6656+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1184)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1658)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1053)),LGF_RVL((const uint8_t *)(cb+3727))))));   //row 26 [P2<<160+P3<<122 + 2 sys]
     LGF_RVS((uint8_t *)(d+7936+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1537)),LGF_RVL((const uint8_t *)(cb+618))));   //row 31 [P3<<1 + 1 sys]
     LGF_RVS((uint8_t *)(d+8192+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1190)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+242)),LGF_RVL((const uint8_t *)(cb+2604)))));   //row 32 [P2<<166 + 2 sys]
     LGF_RVS((uint8_t *)(d+8448+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+235)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1156)),LGF_RVL((const uint8_t *)(cb+3748)))));   //row 33 [P0<<235 + 2 sys]
     LGF_RVS((uint8_t *)(d+8704+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1109)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1572)),LGF_RVL((const uint8_t *)(cb+147)))));   //row 34 [P2<<85+P3<<36 + 1 sys]
     LGF_RVS((uint8_t *)(d+8960+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+575)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+569)),LGF_RVL((const uint8_t *)(cb+2600)))));   //row 35 [P1<<63 + 2 sys]
     LGF_RVS((uint8_t *)(d+9472+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+219)),LGF_RVL((const uint8_t *)(kb+1687))));   //row 37 [P0<<219+P3<<151 + 0 sys]
     LGF_RVS((uint8_t *)(d+9728+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+550)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+543)),LGF_RVL((const uint8_t *)(cb+2626)))));   //row 38 [P1<<38 + 2 sys]
     LGF_RVS((uint8_t *)(d+9984+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1058)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+239)),LGF_RVL((const uint8_t *)(cb+3756)))));   //row 39 [P2<<34 + 2 sys]
     LGF_RVS((uint8_t *)(d+10240+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+75)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1656)),LGF_RVL((const uint8_t *)(cb+1024)))));   //row 40 [P0<<75+P3<<120 + 1 sys]
     LGF_RVS((uint8_t *)(d+10496+128*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+630)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+641)),LGF_RVL((const uint8_t *)(cb+2789)))));   //row 41 [P1<<118 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
