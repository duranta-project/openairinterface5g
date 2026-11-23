/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Internal part of oai_dfts.c: NR shifted frequency layout for the pruned API
 * (dft_pruned_implementation / idft_pruned_implementation). Included only by oai_dfts.c, after
 * oai_dfts_common.h; not a public header.
 *
 * NR layout of an N-bin symbol with num used bins (num even): index j holds bin (j - num / 2) mod N, i.e. the
 * band at [0, num), the guard bins at [num, N) (what fftshift_inplace() produces). With the two halves of the
 * transform swapped (bin b at index R = (b + N / 2) mod N), this is the swapped order read from R = d, with
 * d = (N - num) / 2: index j = R - d, wrapping only for R < d (guard bins).
 *
 * The kernels that support it store (DFT) or load (IDFT) through the helpers below instead of plain vector
 * accesses, so the transform arithmetic is unchanged and the results are bit-identical to the full transform.
 * The swap is a compile-time index change (the leaf index k of a vector becomes k ^ K/2, K leaves); only the
 * accesses with R < N / 2 test the run-time d. The one vector holding the band edge (R < d < R + 8, when
 * d % 8 != 0) goes through a scratch vector. */

#ifndef OAI_DFTS_INTERNAL
#error "oai_dfts_prune.h is included only by oai_dfts.c"
#endif

typedef struct {
  const c16_t *in; /* IDFT: NR-layout input */
  c16_t *out; /* DFT: NR-layout output */
  int d; /* (N - num) / 2 */
  c16_t *edge_buf; /* scratch for the vector holding the band edge (R < d < R + 8) */
} dft_band_t;

/* Band of a kernel call, built inside the kernel from its scalar arguments (kept in registers). */
static __attribute__((always_inline)) inline dft_band_t dft_band_make(const c16_t *in, c16_t *out, int d, c16_t *edge_buf)
{
  return (dft_band_t){.in = in, .out = out, .d = d, .edge_buf = edge_buf};
}

/* IDFT: the edge vector, band lanes from the start of the input, zero guard lanes. */
static inline void dft_band_edge_load(const c16_t *in, int d, c16_t *edge_buf)
{
  const int g = d & 7;
  if (!g)
    return;
  memset(edge_buf, 0, sizeof(c16_t) * g);
  memcpy(edge_buf + g, in, sizeof(c16_t) * (8 - g));
}

/* The kernels pass the swapped index R of a vector (R % 8 == 0) and upper = 1 when R >= N / 2 is a compile-time
 * fact (unrolled leaf index); with upper = 0 the run-time tests below also hold for the upper half. */

/* DFT output vector of swapped indices R .. R+7. */
static __attribute__((always_inline)) inline void dft_band_store8(const dft_band_t *p, int N, int R, int upper, simde__m256i v)
{
  c16_t *a;
  if (upper)
    a = p->out + R - p->d;
  else if (R + 8 <= p->d)
    a = p->out + N + R - p->d;
  else
    a = R < p->d ? p->edge_buf : p->out + R - p->d;
  simde_mm256_storeu_si256((simde__m256i *)a, v);
}

/* DFT: scatter the edge vector after all stores, guard lanes to the end of the buffer, band lanes to its start. */
static inline void dft_band_finish(c16_t *out, int N, int d, const c16_t *edge_buf)
{
  const int g = d & 7;
  if (!g)
    return;
  memcpy(out + N - g, edge_buf, sizeof(c16_t) * g);
  memcpy(out, edge_buf + g, sizeof(c16_t) * (8 - g));
}

/* IDFT input vector of swapped indices R .. R+7. The guard bins of the input are zero: a guard vector
 * (R + 8 <= d) is read from the guard bins at the end of the input, as the rotation gives. */
static __attribute__((always_inline)) inline simde__m256i dft_band_load8(const dft_band_t *p, int N, int R, int upper)
{
  const c16_t *a = p->in + R - p->d;
  if (!upper) {
    a = R + 8 <= p->d ? a + N : a;
    a = R < p->d && R + 8 > p->d ? p->edge_buf : a;
  }
  return simde_mm256_loadu_si256((const simde__m256i *)a);
}
