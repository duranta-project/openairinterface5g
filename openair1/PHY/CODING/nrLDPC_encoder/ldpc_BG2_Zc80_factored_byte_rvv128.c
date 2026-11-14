/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 16 bytes: needs VLEN >= 128
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 16)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 16)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 16)
// generated: BG2 Zc=80, rvv128-bit, factored
// 173 terms vs 1471 expanded (8.50x); 5 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc80_byte_rvv128(uint8_t *c,uint8_t *d) {
  uint8_t kd[640] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<5; i2++) {
     const uint8_t *cb=c+16*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+19)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+56)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+79)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+173)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+197)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+239)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+334)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+399)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+498)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+523)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+559)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+648)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+661)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+719)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+865)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+879)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+967)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+971)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1039)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1176)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1199)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1307)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1332)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1359)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1485)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1494)),LGF_RVL((const uint8_t *)(cb+1519))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+16*i2),p0);
     LGF_RVS((uint8_t *)(d+800+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+177)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1036)),LGF_RVL((const uint8_t *)(cb+1140))))));   //row 10
     LGF_RVS((uint8_t *)(d+1440+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+978)),LGF_RVL((const uint8_t *)(cb+1126)))));   //row 18
     LGF_RVS((uint8_t *)(d+1760+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVL((const uint8_t *)(cb+324))));   //row 22
     LGF_RVS((uint8_t *)(d+1840+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+555)),LGF_RVL((const uint8_t *)(cb+878)))));   //row 23
     LGF_RVS((uint8_t *)(d+1920+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+389)),LGF_RVL((const uint8_t *)(cb+1447)))));   //row 24
     LGF_RVS((uint8_t *)(d+2000+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+865))));   //row 25
     LGF_RVS((uint8_t *)(d+2160+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+992))));   //row 27
     LGF_RVS((uint8_t *)(d+2240+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+366)),LGF_RVL((const uint8_t *)(cb+830)))));   //row 28
     LGF_RVS((uint8_t *)(d+2320+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+714))));   //row 29
     LGF_RVS((uint8_t *)(d+2400+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+835)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1171)),LGF_RVL((const uint8_t *)(cb+1494))))));   //row 30
     LGF_RVS((uint8_t *)(d+2880+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+383)),LGF_RVL((const uint8_t *)(cb+1122)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+16*(i2-1);
       uint8_t *dm=d+16*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+160)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+320)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+480)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+960)),LGF_RVL((const uint8_t *)(cm+1440))))))));
       LGF_RVS((uint8_t *)(dm+80),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+80)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+57)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+524)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+640)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+800)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+968)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1120)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1280)),LGF_RVL((const uint8_t *)(cm+1495))))))))));
       LGF_RVS((uint8_t *)(dm+160),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+160)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+20)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+174)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+499)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+649)),LGF_RVL((const uint8_t *)(cm+1308))))))));
       LGF_RVS((uint8_t *)(dm+240),p3);
     }
  }

  memcpy(kd,d,80); memcpy(kd+80,d,80);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+64, *km=kd+64; uint8_t *dm=d+64;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+160)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+320)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+480)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+960)),LGF_RVL((const uint8_t *)(cm+1440))))))));
    LGF_RVS((uint8_t *)(dm+80),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+80)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+57)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+524)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+640)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+800)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+968)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1120)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1280)),LGF_RVL((const uint8_t *)(cm+1495))))))))));
    LGF_RVS((uint8_t *)(dm+160),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+160)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+20)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+174)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+499)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+649)),LGF_RVL((const uint8_t *)(cm+1308))))))));
    LGF_RVS((uint8_t *)(dm+240),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*160,d+k*80,80); memcpy(kd+k*160+80,d+k*80,80); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<5; i2++) {
     const uint8_t *cb=c+16*i2, *kb=kd+16*i2;
     LGF_RVS((uint8_t *)(d+320+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+237)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+216)))));   //row 4 [P1<<77 + 2 sys]
     LGF_RVS((uint8_t *)(d+400+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+224)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+211)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+862)),LGF_RVL((const uint8_t *)(cb+1181)))))));   //row 5 [P1<<64 + 4 sys]
     LGF_RVS((uint8_t *)(d+480+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+228)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+844)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1139)),LGF_RVL((const uint8_t *)(cb+1485)))))));   //row 6 [P1<<68 + 4 sys]
     LGF_RVS((uint8_t *)(d+560+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+176)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+558)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+845)),LGF_RVL((const uint8_t *)(cb+1188)))))));   //row 7 [P1<<16+P3<<78 + 3 sys]
     LGF_RVS((uint8_t *)(d+640+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+327)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+225)))));   //row 8 [P2<<7 + 2 sys]
     LGF_RVS((uint8_t *)(d+720+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+51)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+165)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVL((const uint8_t *)(cb+1297))))));   //row 9 [P0<<51+P1<<5 + 2 sys]
     LGF_RVS((uint8_t *)(d+880+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+482)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1127)),LGF_RVL((const uint8_t *)(cb+1444))))));   //row 11 [P3<<2 + 3 sys]
     LGF_RVS((uint8_t *)(d+960+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+208)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVL((const uint8_t *)(cb+513)))));   //row 12 [P1<<48 + 2 sys]
     LGF_RVS((uint8_t *)(d+1040+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+506)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+192)),LGF_RVL((const uint8_t *)(cb+1302))))));   //row 13 [P3<<26 + 3 sys]
     LGF_RVS((uint8_t *)(d+1120+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+217)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+507)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVL((const uint8_t *)(cb+1018))))));   //row 14 [P1<<57+P3<<27 + 2 sys]
     LGF_RVS((uint8_t *)(d+1200+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+73)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+179)),LGF_RVL((const uint8_t *)(cb+0)))));   //row 15 [P0<<73+P1<<19 + 1 sys]
     LGF_RVS((uint8_t *)(d+1280+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+191)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+383)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVL((const uint8_t *)(cb+1519))))));   //row 16 [P1<<31+P2<<63 + 2 sys]
     LGF_RVS((uint8_t *)(d+1360+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+189)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+338)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVL((const uint8_t *)(cb+824))))));   //row 17 [P1<<29+P2<<18 + 2 sys]
     LGF_RVS((uint8_t *)(d+1520+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+74)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+238)))));   //row 19 [P0<<74 + 2 sys]
     LGF_RVS((uint8_t *)(d+1600+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+184)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVL((const uint8_t *)(cb+708)))));   //row 20 [P1<<24 + 2 sys]
     LGF_RVS((uint8_t *)(d+1680+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+513)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+1297)))));   //row 21 [P3<<33 + 2 sys]
     LGF_RVS((uint8_t *)(d+2080+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+333)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+487)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+1140))))));   //row 26 [P2<<13+P3<<7 + 2 sys]
     LGF_RVS((uint8_t *)(d+2480+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+500)),LGF_RVL((const uint8_t *)(cb+160))));   //row 31 [P3<<20 + 1 sys]
     LGF_RVS((uint8_t *)(d+2560+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+362)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+820)))));   //row 32 [P2<<42 + 2 sys]
     LGF_RVS((uint8_t *)(d+2640+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+13)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+320)),LGF_RVL((const uint8_t *)(cb+1128)))));   //row 33 [P0<<13 + 2 sys]
     LGF_RVS((uint8_t *)(d+2720+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+339)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+558)),LGF_RVL((const uint8_t *)(cb+0)))));   //row 34 [P2<<19+P3<<78 + 1 sys]
     LGF_RVS((uint8_t *)(d+2800+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+166)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVL((const uint8_t *)(cb+877)))));   //row 35 [P1<<6 + 2 sys]
     LGF_RVS((uint8_t *)(d+2960+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+0)),LGF_RVL((const uint8_t *)(kb+544))));   //row 37 [P0<<0+P3<<64 + 0 sys]
     LGF_RVS((uint8_t *)(d+3040+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+179)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVL((const uint8_t *)(cb+813)))));   //row 38 [P1<<19 + 2 sys]
     LGF_RVS((uint8_t *)(d+3120+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+378)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+1144)))));   //row 39 [P2<<58 + 2 sys]
     LGF_RVS((uint8_t *)(d+3200+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+36)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+543)),LGF_RVL((const uint8_t *)(cb+320)))));   //row 40 [P0<<36+P3<<63 + 1 sys]
     LGF_RVS((uint8_t *)(d+3280+16*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+215)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+160)),LGF_RVL((const uint8_t *)(cb+802)))));   //row 41 [P1<<55 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
