/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_transport_common_proto.h"
#include "PHY/NR_REFSIG/nr_refsig.h"
#if defined(__riscv) && defined(__riscv_vector)
#include <riscv_vector.h>
#endif
#define DEBUG_SCRAMBLING(a)
//#define DEBUG_SCRAMBLING(a) a
void nr_codeword_scrambling(uint8_t *in,
                            uint32_t size,
                            uint8_t q,
                            uint32_t Nid,
                            uint32_t n_RNTI,
                            uint32_t* out)
{
  const int roundedSz = (size + 31) / 32;
  uint32_t *seq = gold_cache((n_RNTI << 15) + (q << 14) + Nid, roundedSz);
  unsigned int i_32 = 0;
#if defined(__AVX512F__) && defined(__AVX512VL__)
  for (; i_32 < ((roundedSz >> 3) << 3); i_32 += 8) {
    __m256i in_256 = _mm256_load_epi32(&((uint32_t *)in)[i_32]);
    __m256i seq_256 = _mm256_load_epi32(&seq[i_32]);
    _mm256_storeu_epi32(&out[i_32], _mm256_xor_si256(in_256, seq_256));
  }
#endif
#if defined(__aarch64__)
  for (; i_32 < ((roundedSz >> 2) << 2); i_32 += 4) {
    unsigned int i = i_32 >> 2;
    uint32x4_t in_32x4 = vld1q_u32(&((uint32_t *)in)[i_32]);
    uint32x4_t seq_32x4 = vld1q_u32(&seq[i_32]);
    ((uint32x4_t *)out)[i] = veorq_u32(in_32x4, seq_32x4);
  }
#endif
  for (; i_32 < roundedSz; i_32++) { 
    unsigned int i = i_32;
    out[i] = ((uint32_t *)in)[i] ^ seq[i];
    DEBUG_SCRAMBLING(LOG_D(PHY, "in[%d] %x => %x\n", i, ((uint32_t*)in)[i], out[i]));
  }
}

void nr_codeword_unscrambling(int16_t* llr, uint32_t size, uint8_t q, uint32_t Nid, uint32_t n_RNTI)
{
  const int roundedSz = (size + 31) / 32;
  uint32_t *seq = gold_cache((n_RNTI << 15) + (q << 14) + Nid, roundedSz);
#if defined(__riscv) && defined(__riscv_vector)
  /* negate llr[i] where scramble bit i is set: the gold sequence is a packed
   * bit array -> load it straight into a mask register (vlm) and masked-vneg.
   * VLMAX (VLEN/16) is a multiple of 8 so bits+i/8 stays byte-aligned. */
  const uint8_t *bits = (const uint8_t *)seq;
  for (uint32_t i = 0; i < size;) {
    size_t vl = __riscv_vsetvl_e16m1(size - i);
    vbool16_t m = __riscv_vlm_v_b16(bits + i / 8, vl);
    vint16m1_t v = __riscv_vle16_v_i16m1(llr + i, vl);
    v = __riscv_vneg_v_i16m1_mu(m, v, v, vl);
    __riscv_vse16_v_i16m1(llr + i, v, vl);
    i += (uint32_t)vl;
  }
#elif defined(__x86_64__) || defined(__i386__) || defined(__arm__) || defined(__aarch64__)
  simde__m128i *llr128 = (simde__m128i*)llr;
  for (int i = 0, j = 0; i < roundedSz; i++, j += 4) {
    uint8_t *s8 = (uint8_t *)(seq + i);
    llr128[j]   = simde_mm_mullo_epi16(llr128[j],byte2m128i[s8[0]]);
    llr128[j+1] = simde_mm_mullo_epi16(llr128[j+1],byte2m128i[s8[1]]);
    llr128[j+2] = simde_mm_mullo_epi16(llr128[j+2],byte2m128i[s8[2]]);
    llr128[j + 3] = simde_mm_mullo_epi16(llr128[j + 3], byte2m128i[s8[3]]);
  }
#else
  for (uint32_t i=0; i<size; i++) {
    if (seq[i / 32] & (1U << (i % 32)))
      llr[i] = -llr[i];
  }
#endif
}

void nr_codeword_unscrambling_init(int16_t *s2, uint32_t size, uint8_t q, uint32_t Nid, uint32_t n_RNTI)
{
  const int roundedSz = (size + 31) / 32;
  uint32_t *seq = gold_cache((n_RNTI << 15) + (q << 14) + Nid, roundedSz);
#if defined(__riscv) && defined(__riscv_vector)
  /* expand the packed gold bits to a +/-1 int16 sequence: s2[i] = bit i ? -1 : 1
   * (so a later llr*s2 multiply descrambles). vlm mask + masked vneg of a ones
   * vector. Fills [0,size); the consumer reads at most `size` entries. */
  const uint8_t *bits = (const uint8_t *)seq;
  for (uint32_t i = 0; i < size;) {
    size_t vl = __riscv_vsetvl_e16m1(size - i);
    vbool16_t m = __riscv_vlm_v_b16(bits + i / 8, vl);
    vint16m1_t ones = __riscv_vmv_v_x_i16m1(1, vl);
    vint16m1_t pm1 = __riscv_vneg_v_i16m1_mu(m, ones, ones, vl);
    __riscv_vse16_v_i16m1(s2 + i, pm1, vl);
    i += (uint32_t)vl;
  }
#else
  simde__m128i *s128=(simde__m128i *)s2;
  for (int i = 0; i < roundedSz; i++) {
    uint8_t *s8 = (uint8_t *)(seq + i);
    *s128++ = byte2m128i[s8[0]];
    *s128++ = byte2m128i[s8[1]];
    *s128++ = byte2m128i[s8[2]];
    *s128++ = byte2m128i[s8[3]];
  }
#endif
}
