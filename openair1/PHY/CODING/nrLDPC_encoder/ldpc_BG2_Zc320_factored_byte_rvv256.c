/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <string.h>
#include <riscv_vector.h>
// one chunk = one LMUL=1 register at vl = 32 bytes: needs VLEN >= 256
#define LGF_RVL(p) __riscv_vle8_v_u8m1((p), 32)
#define LGF_RVX(a, b) __riscv_vxor_vv_u8m1((a), (b), 32)
#define LGF_RVS(p, v) __riscv_vse8_v_u8m1((p), (v), 32)
// generated: BG2 Zc=320, rvv256-bit, factored
// 173 terms vs 1483 expanded (8.57x); 10 chunks/group; pipeline lag 1
// every read is one unaligned load at a constant offset
static inline void ldpc_BG2_Zc320_byte_rvv256(uint8_t *c,uint8_t *d) {
  uint8_t kd[2560] __attribute__((aligned(64)));  // P0..P3, each stored twice
  int i2;

  // stage 1: P0 and the kernel-free extension rows, with the kernel rows
  // pipelined 1 chunk(s) behind so the loop need not be split
  for (i2=0; i2<10; i2++) {
     const uint8_t *cb=c+32*i2;
     vuint8m1_t p0=LGF_RVX(LGF_RVL((const uint8_t *)(cb+19)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+136)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+319)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+677)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+733)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+959)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1294)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1599)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2018)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2043)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2239)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2568)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2661)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+2879)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3345)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3519)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3851)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3927)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4159)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4536)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4799)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5172)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5227)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5439)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5805)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+5814)),LGF_RVL((const uint8_t *)(cb+6079))))))))))))))))))))))))))));
     LGF_RVS((uint8_t *)(d+32*i2),p0);
     LGF_RVS((uint8_t *)(d+3200+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+657)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3996)),LGF_RVL((const uint8_t *)(cb+4500))))));   //row 10
     LGF_RVS((uint8_t *)(d+5760+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3858)),LGF_RVL((const uint8_t *)(cb+4566)))));   //row 18
     LGF_RVS((uint8_t *)(d+7040+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+1284))));   //row 22
     LGF_RVS((uint8_t *)(d+7360+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1995)),LGF_RVL((const uint8_t *)(cb+3358)))));   //row 23
     LGF_RVS((uint8_t *)(d+7680+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1349)),LGF_RVL((const uint8_t *)(cb+5847)))));   //row 24
     LGF_RVS((uint8_t *)(d+8000+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+3265))));   //row 25
     LGF_RVS((uint8_t *)(d+8640+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+3872))));   //row 27
     LGF_RVS((uint8_t *)(d+8960+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1406)),LGF_RVL((const uint8_t *)(cb+3310)))));   //row 28
     LGF_RVS((uint8_t *)(d+9280+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+2714))));   //row 29
     LGF_RVS((uint8_t *)(d+9600+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1280)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3235)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4531)),LGF_RVL((const uint8_t *)(cb+5894))))));   //row 30
     LGF_RVS((uint8_t *)(d+11520+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1343)),LGF_RVL((const uint8_t *)(cb+4562)))));   //row 36
     if (i2>=1) {
       const uint8_t *cm=c+32*(i2-1);
       uint8_t *dm=d+32*(i2-1);
       vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(dm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+640)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1280)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1920)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3840)),LGF_RVL((const uint8_t *)(cm+5760))))))));
       LGF_RVS((uint8_t *)(dm+320),p1);
       vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+320)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+137)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2044)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2560)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3200)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3928)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4480)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+5120)),LGF_RVL((const uint8_t *)(cm+5815))))))))));
       LGF_RVS((uint8_t *)(dm+640),p2);
       vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+640)),LGF_RVX(LGF_RVL((const uint8_t *)(dm+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+20)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+734)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2019)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2569)),LGF_RVL((const uint8_t *)(cm+5228))))))));
       LGF_RVS((uint8_t *)(dm+960),p3);
     }
  }

  memcpy(kd,d,320); memcpy(kd+320,d,320);   // P0 doubled: the epilogue rotations wrap
  // epilogue: kernel rows for the final 1 chunk(s)
  { const uint8_t *cm=c+288, *km=kd+288; uint8_t *dm=d+288;
    vuint8m1_t p1=LGF_RVX(LGF_RVL((const uint8_t *)(km+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+640)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1280)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+1920)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3840)),LGF_RVL((const uint8_t *)(cm+5760))))))));
    LGF_RVS((uint8_t *)(dm+320),p1);
    vuint8m1_t p2=LGF_RVX(LGF_RVL((const uint8_t *)(dm+320)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+137)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2044)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2560)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3200)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+3928)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+4480)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+5120)),LGF_RVL((const uint8_t *)(cm+5815))))))))));
    LGF_RVS((uint8_t *)(dm+640),p2);
    vuint8m1_t p3=LGF_RVX(LGF_RVL((const uint8_t *)(dm+640)),LGF_RVX(LGF_RVL((const uint8_t *)(km+1)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+20)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+734)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2019)),LGF_RVX(LGF_RVL((const uint8_t *)(cm+2569)),LGF_RVL((const uint8_t *)(cm+5228))))))));
    LGF_RVS((uint8_t *)(dm+960),p3);
  }
  for (int k=1;k<4;k++) { memcpy(kd+k*640,d+k*320,320); memcpy(kd+k*640+320,d+k*320,320); }

  // stage 2: extension rows that reference the kernel
  for (i2=0; i2<10; i2++) {
     const uint8_t *cb=c+32*i2, *kb=kd+32*i2;
     LGF_RVS((uint8_t *)(d+1280+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+797)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+776)))));   //row 4 [P1<<157 + 2 sys]
     LGF_RVS((uint8_t *)(d+1600+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+704)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+771)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3342)),LGF_RVL((const uint8_t *)(cb+4621)))))));   //row 5 [P1<<64 + 4 sys]
     LGF_RVS((uint8_t *)(d+1920+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+788)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3324)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4579)),LGF_RVL((const uint8_t *)(cb+5805)))))));   //row 6 [P1<<148 + 4 sys]
     LGF_RVS((uint8_t *)(d+2240+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+736)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1998)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+3245)),LGF_RVL((const uint8_t *)(cb+4628)))))));   //row 7 [P1<<96+P3<<78 + 3 sys]
     LGF_RVS((uint8_t *)(d+2560+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1367)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+705)))));   //row 8 [P2<<87 + 2 sys]
     LGF_RVS((uint8_t *)(d+2880+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+51)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+725)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+5217))))));   //row 9 [P0<<51+P1<<85 + 2 sys]
     LGF_RVS((uint8_t *)(d+3520+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1922)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+4487)),LGF_RVL((const uint8_t *)(cb+5764))))));   //row 11 [P3<<2 + 3 sys]
     LGF_RVS((uint8_t *)(d+3840+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+688)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+2033)))));   //row 12 [P1<<48 + 2 sys]
     LGF_RVS((uint8_t *)(d+4160+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1946)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+752)),LGF_RVL((const uint8_t *)(cb+5222))))));   //row 13 [P3<<26 + 3 sys]
     LGF_RVS((uint8_t *)(d+4480+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+697)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1947)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+3978))))));   //row 14 [P1<<57+P3<<27 + 2 sys]
     LGF_RVS((uint8_t *)(d+4800+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+73)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+739)),LGF_RVL((const uint8_t *)(cb+0)))));   //row 15 [P0<<73+P1<<99 + 1 sys]
     LGF_RVS((uint8_t *)(d+5120+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+751)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1423)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+5839))))));   //row 16 [P1<<111+P2<<143 + 2 sys]
     LGF_RVS((uint8_t *)(d+5440+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+749)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1298)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+3224))))));   //row 17 [P1<<109+P2<<18 + 2 sys]
     LGF_RVS((uint8_t *)(d+6080+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+154)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+798)))));   //row 19 [P0<<154 + 2 sys]
     LGF_RVS((uint8_t *)(d+6400+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+744)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+2708)))));   //row 20 [P1<<104 + 2 sys]
     LGF_RVS((uint8_t *)(d+6720+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1953)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+5137)))));   //row 21 [P3<<33 + 2 sys]
     LGF_RVS((uint8_t *)(d+8320+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1293)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1927)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1280)),LGF_RVL((const uint8_t *)(cb+4580))))));   //row 26 [P2<<13+P3<<7 + 2 sys]
     LGF_RVS((uint8_t *)(d+9920+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1940)),LGF_RVL((const uint8_t *)(cb+640))));   //row 31 [P3<<20 + 1 sys]
     LGF_RVS((uint8_t *)(d+10240+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1402)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+3220)))));   //row 32 [P2<<122 + 2 sys]
     LGF_RVS((uint8_t *)(d+10560+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+13)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+1280)),LGF_RVL((const uint8_t *)(cb+4568)))));   //row 33 [P0<<13 + 2 sys]
     LGF_RVS((uint8_t *)(d+10880+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1299)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1998)),LGF_RVL((const uint8_t *)(cb+0)))));   //row 34 [P2<<19+P3<<78 + 1 sys]
     LGF_RVS((uint8_t *)(d+11200+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+646)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+3357)))));   //row 35 [P1<<6 + 2 sys]
     LGF_RVS((uint8_t *)(d+11840+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+0)),LGF_RVL((const uint8_t *)(kb+2064))));   //row 37 [P0<<0+P3<<144 + 0 sys]
     LGF_RVS((uint8_t *)(d+12160+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+659)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+3293)))));   //row 38 [P1<<19 + 2 sys]
     LGF_RVS((uint8_t *)(d+12480+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+1418)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+0)),LGF_RVL((const uint8_t *)(cb+4504)))));   //row 39 [P2<<138 + 2 sys]
     LGF_RVS((uint8_t *)(d+12800+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+36)),LGF_RVX(LGF_RVL((const uint8_t *)(kb+2063)),LGF_RVL((const uint8_t *)(cb+1280)))));   //row 40 [P0<<36+P3<<143 + 1 sys]
     LGF_RVS((uint8_t *)(d+13120+32*i2),LGF_RVX(LGF_RVL((const uint8_t *)(kb+695)),LGF_RVX(LGF_RVL((const uint8_t *)(cb+640)),LGF_RVL((const uint8_t *)(cb+3202)))));   //row 41 [P1<<55 + 2 sys]
  }
}
#undef LGF_RVL
#undef LGF_RVX
#undef LGF_RVS
