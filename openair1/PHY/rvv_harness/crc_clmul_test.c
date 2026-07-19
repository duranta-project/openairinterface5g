/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * RVV port harness -- kernel #8: CRC via RISC-V Zbc carry-less multiply.
 *
 * NR CRC (crc24a/b/c) is MSB-first (non-reflected), the poly top-aligned in a
 * 32-bit word (e.g. crc24a poly = 0x864cfb00). Today on RISC-V it runs the
 * byte-wise LUT (~1 byte/iter -> ~170 us/slot). This ports OAI's PCLMULQDQ
 * folding CRC (crc.h) to native RISC-V clmul/clmulh (+ rev8), reusing the
 * proven fold constants (k1,k2,k3,q,p), validated byte-for-byte against the
 * bit-by-bit reference crcbit().
 *
 * 128-bit values are held as {lo,hi} u64 pairs with the simde__m128i memory
 * layout (lo = bytes 0..7, hi = bytes 8..15).
 */

#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#if defined(__riscv) && (defined(__riscv_zbc) || defined(__riscv_zbkc))
#include <riscv_bitmanip.h>
#define HAVE_CLMUL 1
#endif

/* ---- reference: bit-by-bit MSB-first CRC (from crc_byte.c) ------------------ */
static uint32_t crcbit(const uint8_t *inputptr, int octetlen, uint32_t poly)
{
  uint32_t crc = 0, c;
  while (octetlen-- > 0) {
    c = ((uint32_t)(*inputptr++)) << 24;
    for (int i = 8; i != 0; i--) {
      if ((1U << 31) & (c ^ crc))
        crc = (crc << 1) ^ poly;
      else
        crc <<= 1;
      c <<= 1;
    }
  }
  return crc;
}

#ifdef HAVE_CLMUL
/* ---- 128-bit helpers on {lo,hi} ------------------------------------------- */
typedef struct {
  uint64_t lo, hi;
} u128;

static inline u128 xor128(u128 a, u128 b)
{
  return (u128){a.lo ^ b.lo, a.hi ^ b.hi};
}
static inline u128 load128(const uint8_t *p)
{
  u128 r;
  memcpy(&r.lo, p, 8);
  memcpy(&r.hi, p + 8, 8);
  return r;
}
/* byte-reverse the whole 16-byte value (be<->le swap) */
static inline u128 bswap128(u128 a)
{
  return (u128){__riscv_rev8_64(a.hi), __riscv_rev8_64(a.lo)};
}
/* logical shift right by nbytes (0..16), zero fill (== _mm_srli_si128) */
static inline u128 srl128(u128 a, unsigned nbytes)
{
  if (nbytes == 0)
    return a;
  if (nbytes >= 16)
    return (u128){0, 0};
  if (nbytes >= 8) {
    unsigned s = (nbytes - 8) * 8;
    return (u128){s ? (a.hi >> s) : a.hi, 0};
  }
  unsigned s = nbytes * 8;
  return (u128){(a.lo >> s) | (a.hi << (64 - s)), a.hi >> s};
}
/* logical shift left by nbytes (0..16), zero fill (== _mm_slli_si128) */
static inline u128 sll128(u128 a, unsigned nbytes)
{
  if (nbytes == 0)
    return a;
  if (nbytes >= 16)
    return (u128){0, 0};
  if (nbytes >= 8) {
    unsigned s = (nbytes - 8) * 8;
    return (u128){0, s ? (a.lo << s) : a.lo};
  }
  unsigned s = nbytes * 8;
  return (u128){a.lo << s, (a.hi << s) | (a.lo >> (64 - s))};
}
/* carry-less multiply of selected 64-bit halves -> 128-bit product.
 * sel: 0x00=lo*lo, 0x11=hi*hi, 0x10=lo*hi, 0x01=hi*lo (matches _mm_clmulepi64) */
static inline u128 clmul128(u128 a, u128 b, int sel)
{
  uint64_t x = (sel & 0x01) ? a.hi : a.lo;
  uint64_t y = (sel & 0x10) ? b.hi : b.lo;
  return (u128){__riscv_clmul_64(x, y), __riscv_clmulh_64(x, y)};
}

/* ---- folding + reduction (ports of crc.h) --------------------------------- */
static inline u128 folding_round(u128 data_block, u128 k1_k2, u128 fold)
{
  u128 tmp = clmul128(fold, k1_k2, 0x11);
  return xor128(clmul128(fold, k1_k2, 0x00), xor128(data_block, tmp));
}
static inline u128 reduce_128_to_64(u128 data128, u128 k3_q)
{
  u128 tmp = xor128(clmul128(data128, k3_q, 0x01), data128);
  data128 = xor128(clmul128(tmp, k3_q, 0x01), data128);
  return srl128(sll128(data128, 8), 8);
}
static inline uint32_t reduce_64_to_32(u128 fold, u128 k3_q, u128 p_res)
{
  u128 t = clmul128(srl128(fold, 4), k3_q, 0x10);
  t = srl128(xor128(t, fold), 4);
  t = clmul128(t, p_res, 0x00);
  return (uint32_t)(xor128(t, fold).lo & 0xFFFFFFFFu);
}

/* full MSB-first folding CRC, ported from crc32_calc_pclmulqdq */
static uint32_t crc_clmul(const uint8_t *data, uint32_t data_len, const uint64_t ctx[6])
{
  const u128 k1_k2 = {ctx[0], ctx[1]};
  const u128 k3_q = {ctx[2], ctx[3]};
  const u128 p_res = {ctx[4], ctx[5]};
  u128 fold, next_data, newd;
  uint32_t n, crc = 0;

  data_len += 4;
  fold = load128(data);

  if (data_len <= 16) {
    fold = bswap128(fold);
    fold = sll128(srl128(fold, 20 - data_len), 4);
    u128 temp = (u128){__builtin_bswap32(crc), 0};
    temp = sll128(temp, data_len - 4);
    fold = xor128(fold, temp);
  } else {
    n = ((~data_len) + 1) & 15;
    fold = xor128(fold, (u128){crc, 0});
    fold = bswap128(fold);
    next_data = bswap128(load128(&data[16]));
    next_data = xor128(srl128(next_data, n), sll128(fold, 16 - n));
    fold = srl128(fold, n);
    if (data_len <= 32)
      next_data = sll128(srl128(next_data, 4), 4);
    fold = folding_round(next_data, k1_k2, fold);
    if (data_len > 32) {
      for (n = 16 + 16 - n; n < (data_len - 16); n += 16) {
        newd = bswap128(load128(&data[n]));
        fold = folding_round(newd, k1_k2, fold);
      }
      newd = bswap128(load128(&data[n - 4]));
      newd = sll128(newd, 4);
      fold = folding_round(newd, k1_k2, fold);
    }
  }
  fold = reduce_128_to_64(fold, k3_q);
  return reduce_64_to_32(fold, k3_q, p_res);
}
#endif /* HAVE_CLMUL */

/* ---- driver ---------------------------------------------------------------- */
static int pin_to_cpu(int cpu)
{
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(cpu, &set);
  return sched_setaffinity(0, sizeof(set), &set);
}
static uint64_t rng = 0xc3a5c85c97cb3127ULL;
static uint8_t rnd8(void)
{
  rng = rng * 6364136223846793005ULL + 1442695040888963407ULL;
  return (uint8_t)(rng >> 33);
}
static double now_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

/* crc24a: poly 0x864cfb00; constants from crc_byte.c lte_crc24a_pclmulqdq */
static const uint32_t poly24a = 0x864cfb00;
static const uint64_t ctx24a[6] = {0x64e4d700, 0x2c8c9d00, 0xd9fe8c00, 0xf845fe24, 0x864cfb00, 0};

int main(int argc, char **argv)
{
  static uint8_t buf[32768];
  if (argc > 1 && pin_to_cpu(atoi(argv[1])) == 0)
    printf("pinned to CPU %d\n", atoi(argv[1]));
#ifdef HAVE_CLMUL
  printf("Zbc clmul available\n");
#else
  printf("built WITHOUT Zbc (add zbc to -march)\n");
  return 0;
#endif

#ifdef HAVE_CLMUL
  int fails = 0;
  /* byte-exact vs crcbit over many lengths */
  for (int len = 1; len <= 4096; len++) {
    for (int k = 0; k < len; k++)
      buf[k] = rnd8();
    uint32_t ref = crcbit(buf, len, poly24a);
    uint32_t got = crc_clmul(buf, len, ctx24a);
    if (ref != got) {
      if (fails < 5)
        fprintf(stderr, "  MISMATCH len=%d: ref=%08x got=%08x\n", len, ref, got);
      fails++;
    }
  }
  printf("crc24a: %s (%d/4096 lengths failed)\n", fails ? "FAIL" : "byte-exact OK", fails);

  /* benchmark on a representative code-block size (~2400 bytes) */
  const int blen = 2400, iters = 20000;
  for (int k = 0; k < blen; k++)
    buf[k] = rnd8();
  double t0 = now_ns();
  volatile uint32_t sink = 0;
  for (int it = 0; it < iters; it++)
    sink ^= crcbit(buf, blen, poly24a);
  double tb = now_ns() - t0;
  t0 = now_ns();
  for (int it = 0; it < iters; it++)
    sink ^= crc_clmul(buf, blen, ctx24a);
  double tc = now_ns() - t0;
  printf("\nbitwise ref : %.2f ns/byte\n", tb / ((double)iters * blen));
  printf("clmul       : %.2f ns/byte  (%.1fx vs bitwise)\n", tc / ((double)iters * blen), tb / tc);

  printf("\nRESULT: %s\n", fails ? "FAIL" : "PASS");
  return fails ? 1 : 0;
#endif
}
