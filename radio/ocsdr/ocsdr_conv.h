/*
 * Sample format conversion between OAI's per-antenna buffers (complex int16, one array per antenna) and the OC1
 * DMA blocks (complex int16 words, the hardware channels interleaved per sample time).
 * Each function has a scalar reference and an AVX2 path with the same output, bit for bit
 * (radio/ocsdr/tests/test_ocsdr_conv.cpp checks it).
 */
#ifndef OCSDR_CONV_H
#define OCSDR_CONV_H

#include <stdint.h>
#include <string.h>
#if defined(__AVX2__)
#include <immintrin.h>
#endif

/* x << sh, clamped to [-32767, 32767] */
static inline int16_t ocsdr_tx_sample(int16_t x, unsigned sh)
{
  int32_t v = (int32_t)x * (1 << sh);
  return (int16_t)(v > 32767 ? 32767 : v < -32767 ? -32767 : v);
}

/* TX: n sample times from in[0..nch-1] (each 2 * n int16) into dst, hw channels interleaved per sample time,
 * shifted left by sh with saturation to +-32767; channels nch..hw-1 are zero. Scalar reference. */
static inline void ocsdr_tx_pack_ref(int16_t *dst, const int16_t *const *in, unsigned nch, unsigned hw, unsigned n, unsigned sh)
{
  for (unsigned c = 0; c < hw; c++) {
    int16_t *d = dst + 2 * c;
    for (unsigned i = 0; i < n; i++, d += 2 * hw) {
      d[0] = c < nch ? ocsdr_tx_sample(in[c][2 * i], sh) : 0;
      d[1] = c < nch ? ocsdr_tx_sample(in[c][2 * i + 1], sh) : 0;
    }
  }
}

#if defined(__AVX2__)
static inline __m256i ocsdr_shl_sat(__m256i v, unsigned sh)
{
  // doubling with signed saturation sh times equals clamp(x << sh) to [-32768, 32767]; then raise -32768 to -32767
  for (unsigned k = 0; k < sh; k++)
    v = _mm256_adds_epi16(v, v);
  return _mm256_max_epi16(v, _mm256_set1_epi16(-32767));
}
#endif

static inline void ocsdr_tx_pack(int16_t *dst, const int16_t *const *in, unsigned nch, unsigned hw, unsigned n, unsigned sh)
{
#if defined(__AVX2__)
  unsigned i = 0;
  if (hw == 2) {
    const int16_t *i0 = in[0], *i1 = nch > 1 ? in[1] : nullptr;
    for (; i + 8 <= n; i += 8) {
      __m256i a  = ocsdr_shl_sat(_mm256_loadu_si256((const __m256i *)(i0 + 2 * i)), sh);
      __m256i b  = i1 ? ocsdr_shl_sat(_mm256_loadu_si256((const __m256i *)(i1 + 2 * i)), sh) : _mm256_setzero_si256();
      __m256i lo = _mm256_unpacklo_epi32(a, b); // a0 b0 a1 b1 | a4 b4 a5 b5
      __m256i hi = _mm256_unpackhi_epi32(a, b); // a2 b2 a3 b3 | a6 b6 a7 b7
      _mm256_storeu_si256((__m256i *)(dst + 4 * i), _mm256_permute2x128_si256(lo, hi, 0x20));
      _mm256_storeu_si256((__m256i *)(dst + 4 * i + 16), _mm256_permute2x128_si256(lo, hi, 0x31));
    }
  } else if (hw == 1) {
    for (; i + 8 <= n; i += 8)
      _mm256_storeu_si256((__m256i *)(dst + 2 * i), ocsdr_shl_sat(_mm256_loadu_si256((const __m256i *)(in[0] + 2 * i)), sh));
  }
  if (i < n) {
    const int16_t *tail[2] = {in[0] + 2 * i, nch > 1 ? in[1] + 2 * i : nullptr};
    ocsdr_tx_pack_ref(dst + 2 * hw * i, tail, nch, hw, n - i, sh);
  }
#else
  ocsdr_tx_pack_ref(dst, in, nch, hw, n, sh);
#endif
}

/* RX: n sample times from the block words w (hw channels interleaved) into out[0..nout-1] (nout <= hw). Scalar
 * reference. */
static inline void ocsdr_rx_unpack_ref(uint32_t *const *out, unsigned nout, const uint32_t *w, unsigned hw, unsigned n)
{
  for (unsigned a = 0; a < nout; a++) {
    if (hw == 1) {
      memcpy(out[a], w, n * sizeof(uint32_t));
      continue;
    }
    const uint32_t *p = w + a;
    for (unsigned j = 0; j < n; j++, p += hw)
      out[a][j] = *p;
  }
}

static inline void ocsdr_rx_unpack(uint32_t *const *out, unsigned nout, const uint32_t *w, unsigned hw, unsigned n)
{
#if defined(__AVX2__)
  if (hw == 2) {
    const __m256i idx = _mm256_setr_epi32(0, 2, 4, 6, 1, 3, 5, 7);
    unsigned j = 0;
    for (; j + 8 <= n; j += 8) {
      __m256i p0 = _mm256_permutevar8x32_epi32(_mm256_loadu_si256((const __m256i *)(w + 2 * j)), idx);     // s0..3 c0 | s0..3 c1
      __m256i p1 = _mm256_permutevar8x32_epi32(_mm256_loadu_si256((const __m256i *)(w + 2 * j + 8)), idx); // s4..7 c0 | s4..7 c1
      _mm256_storeu_si256((__m256i *)(out[0] + j), _mm256_permute2x128_si256(p0, p1, 0x20));
      if (nout > 1)
        _mm256_storeu_si256((__m256i *)(out[1] + j), _mm256_permute2x128_si256(p0, p1, 0x31));
    }
    if (j < n) {
      uint32_t *tail[2] = {out[0] + j, nout > 1 ? out[1] + j : nullptr};
      ocsdr_rx_unpack_ref(tail, nout, w + 2 * j, hw, n - j);
    }
    return;
  }
#endif
  ocsdr_rx_unpack_ref(out, nout, w, hw, n);
}

#endif
