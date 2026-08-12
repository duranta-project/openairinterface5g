/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Generates the factored LDPC encoder (128-bit alignr path).
 *
 * The stock generator expands every parity row into a XOR of shifted systematic
 * terms. That discards the base-graph structure: the four core parity groups
 * P0..P3 have been pre-inverted into every row that references them.
 *
 * This generator recovers that structure symbolically over GF(2):
 *
 *   P0            seed, sum of the four core rows' systematic parts
 *   P1,P2,P3      back-substituted from already-emitted kernel rows
 *   rows 4..n-1   rot(Pk,s) references plus a sparse systematic residual
 *
 * BG1 Zc=384: 2109 -> 319 terms. BG2 Zc=384: 1473 -> 175 terms.
 *
 * Emission uses two stages with a single barrier:
 *
 *   stage 1   P0, the extension rows that need no kernel, and the kernel rows
 *             software-pipelined LAG chunks behind P0 (so no loop split)
 *   stage 2   the extension rows that do reference the kernel, reading it from
 *             a doubled scratch buffer so any rotation is one unaligned load
 *
 * The pipelining in stage 1 matters: splitting it into two loops instead costs
 * more than the arithmetic saved.
 */

#ifndef LDPC_GENERATE_FACTORED_C
#define LDPC_GENERATE_FACTORED_C

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>

#define LGF_MAXNC 22
#define LGF_MAXZ 384
#define LGF_MAXROWS 46
#define LGF_NKERN 4
#define LGF_MAXREF 3

/* ------------------------------------------------------------------ */
/* symbolic rows: m[col*Z + shift] is a GF(2) coefficient             */
/* ------------------------------------------------------------------ */

typedef struct {
  int nref;
  int k[LGF_MAXREF];
  int s[LGF_MAXREF];
  int cost; /* residual terms + nref */
} lgf_fac;

static uint8_t lgf_G[LGF_MAXROWS][LGF_MAXNC * LGF_MAXZ];
static uint8_t lgf_res[LGF_MAXROWS][LGF_MAXNC * LGF_MAXZ];
static uint8_t lgf_t0[LGF_MAXNC * LGF_MAXZ], lgf_t1[LGF_MAXNC * LGF_MAXZ];
static int lgf_hist_buf[LGF_MAXZ];

static int lgf_pop(const uint8_t *m, int n)
{
  int s = 0;
  for (int i = 0; i < n; i++)
    s += m[i];
  return s;
}

static void lgf_rot(uint8_t *dst, const uint8_t *src, int nc, int Z, int s)
{
  memset(dst, 0, nc * Z);
  for (int c = 0; c < nc; c++)
    for (int g = 0; g < Z; g++)
      if (src[c * Z + g])
        dst[c * Z + (g + s) % Z] = 1;
}

static void lgf_xor(uint8_t *a, const uint8_t *b, int n)
{
  for (int i = 0; i < n; i++)
    a[i] ^= b[i];
}

/* hist[s] = |A intersect rot(B,s)| -- one pass, used to rank candidate shifts */
static void lgf_hist(int *hist, const uint8_t *A, const uint8_t *B, int nc, int Z)
{
  int bl[LGF_MAXZ], nb;
  memset(hist, 0, Z * sizeof(int));
  for (int c = 0; c < nc; c++) {
    nb = 0;
    for (int h = 0; h < Z; h++)
      if (B[c * Z + h])
        bl[nb++] = h;
    if (!nb)
      continue;
    for (int g = 0; g < Z; g++)
      if (A[c * Z + g])
        for (int j = 0; j < nb; j++) {
          int s = g - bl[j];
          if (s < 0)
            s += Z;
          hist[s]++;
        }
  }
}

/* Search a representation of Gi using shifted rows from pool[0..npool-1].
 *
 * Candidate shifts are ranked by overlap. Ties at the top matter: several
 * shifts can give the same residual size while only one of them admits a good
 * second reference, so all shifts within LGF_SLACK of the best are tried
 * rather than an arbitrary one. Picking a single locally-best first shift
 * silently produces non-minimal results on the two-reference rows. */
#define LGF_SLACK 1
#define LGF_MAXCAND 96

static void lgf_factor(const uint8_t *Gi, const uint8_t pool[][LGF_MAXNC * LGF_MAXZ], const int *poolid, int npool, int nc, int Z, lgf_fac *best)
{
  const int n = nc * Z;
  best->nref = 0;
  best->cost = lgf_pop(Gi, n);

  for (int a = 0; a < npool; a++) {
    lgf_hist(lgf_hist_buf, Gi, pool[a], nc, Z);
    int top = 0;
    for (int s = 0; s < Z; s++)
      if (lgf_hist_buf[s] > top)
        top = lgf_hist_buf[s];
    if (!top)
      continue;
    int tried = 0;
    for (int s1 = 0; s1 < Z && tried < LGF_MAXCAND; s1++) {
      if (lgf_hist_buf[s1] < top - LGF_SLACK)
        continue;
      tried++;
      lgf_rot(lgf_t0, pool[a], nc, Z, s1);
      lgf_xor(lgf_t0, Gi, n); /* t0 = residual after one reference */
      int c1 = lgf_pop(lgf_t0, n) + 1;
      if (c1 < best->cost) {
        best->cost = c1;
        best->nref = 1;
        best->k[0] = poolid[a];
        best->s[0] = s1;
      }
      /* second reference on top of this one */
      for (int b = 0; b < npool; b++) {
        if (b == a)
          continue;
        int h2[LGF_MAXZ], top2 = 0;
        lgf_hist(h2, lgf_t0, pool[b], nc, Z);
        for (int s = 0; s < Z; s++)
          if (h2[s] > top2)
            top2 = h2[s];
        if (!top2)
          continue;
        for (int s2 = 0; s2 < Z; s2++) {
          if (h2[s2] < top2)
            continue;
          lgf_rot(lgf_t1, pool[b], nc, Z, s2);
          lgf_xor(lgf_t1, lgf_t0, n);
          int c2 = lgf_pop(lgf_t1, n) + 2;
          if (c2 < best->cost) {
            best->cost = c2;
            best->nref = 2;
            best->k[0] = poolid[a];
            best->s[0] = s1;
            best->k[1] = poolid[b];
            best->s[1] = s2;
          }
          break; /* only the top-overlap s2 is ever useful */
        }
      }
    }
  }
}

/* ------------------------------------------------------------------ */
/* emission                                                           */
/* ------------------------------------------------------------------ */

#define LGF_MAXTERM 512

typedef struct {
  char *s[LGF_MAXTERM];
  int n;
} lgf_expr;

static void lgf_push(lgf_expr *e, const char *fmt, ...)
{
  va_list ap;
  char buf[256];
  va_start(ap, fmt);
  vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  e->s[e->n++] = strdup(buf);
}

/* emit nested XORs, then release the term strings */
static void lgf_flush(FILE *fd, lgf_expr *e, const char *xorc)
{
  for (int i = 0; i < e->n - 1; i++)
    fprintf(fd, "%s(%s,", xorc, e->s[i]);
  fprintf(fd, "%s", e->s[e->n - 1]);
  for (int i = 0; i < e->n - 1; i++)
    fprintf(fd, ")");
  for (int i = 0; i < e->n; i++)
    free(e->s[i]);
  e->n = 0;
}

/* one systematic term, in the stock generator's doubled-column layout */
static void lgf_sys(lgf_expr *e, int col, int g, int Zc, int shift, int mask, const char *base)
{
  int index = col * 2 * Zc + (g + 1) % Zc;
  int v = index >> shift, b = index & mask;
  if (b == 0)
    lgf_push(e, "%s[%d]", base, v);
  else
    lgf_push(e, "simde_mm_alignr_epi8(%s[%d],%s[%d],%d)", base, v + 1, base, v, b);
}

static void lgf_terms(lgf_expr *e, const uint8_t *R, int nc, int Zc, int shift, int mask, const char *base)
{
  for (int c = 0; c < nc; c++)
    for (int g = 0; g < Zc; g++)
      if (R[c * Zc + g])
        lgf_sys(e, c, g, Zc, shift, mask, base);
}

/* rot(Pk,s) read out of the parity output during stage 1.
 * jrel is the chunk being written, relative to the dm/dsimd base. When wrap is
 * set, absolute indices with an explicit modulo are emitted (epilogue). */
static void lgf_kref_stage1(lgf_expr *e, int k, int s, int NCH, int shift, int mask, int jabs, int wrap)
{
  int q = s >> shift, b = s & mask;
  int lo = jabs + q, hi = lo + 1;
  if (wrap) {
    if (b == 0)
      lgf_push(e, "dsimd[%d]", k * NCH + lo % NCH);
    else
      lgf_push(e, "simde_mm_alignr_epi8(dsimd[%d],dsimd[%d],%d)", k * NCH + hi % NCH, k * NCH + lo % NCH, b);
  } else {
    if (b == 0)
      lgf_push(e, "dm[%d]", k * NCH + q);
    else
      lgf_push(e, "simde_mm_alignr_epi8(dm[%d],dm[%d],%d)", k * NCH + q + 1, k * NCH + q, b);
  }
}

/* rot(Pk,s) read out of the doubled scratch during stage 2: one unaligned load */
static void lgf_kref_stage2(lgf_expr *e, int k, int s, int Zc)
{
  lgf_push(e, "simde_mm_loadu_si128((const simde__m128i *)(kb+%d))", k * 2 * Zc + s);
}

/* Build the residual of a row given its factorisation. */
static void lgf_residual(uint8_t *out, const uint8_t *Gi, const lgf_fac *f, int nc, int Z)
{
  memcpy(out, Gi, nc * Z);
  for (int i = 0; i < f->nref; i++) {
    lgf_rot(lgf_t0, lgf_G[f->k[i]], nc, Z, f->s[i]);
    lgf_xor(out, lgf_t0, nc * Z);
  }
}

/* ------------------------------------------------------------------ */
/* per-width emission                                                 */
/* ------------------------------------------------------------------ */

/* Wide (256/512-bit) emission.
 *
 * AVX2 and AVX512 `alignr` operate within 128-bit lanes and so cannot express a
 * full-width byte rotation at all. Instead every value read -- systematic term or
 * kernel rotation -- is one unaligned load at a constant byte offset:
 *
 *   systematic term at chunk j : loadu(c  + W*j + col*2*Zc + (g+1)%Zc)
 *   rot(Pk,s)       at chunk j : loadu(kd + W*j + k*2*Zc + s)
 *
 * This is the only mechanism the generated code uses, which is what makes the
 * same emitter work for NEON, AVX2, AVX512 and RVV. It matters most for AVX2 and
 * AVX512, whose `alignr` operates within 128-bit lanes and so cannot express a
 * full-width byte rotation at all; the stock generator works around that by
 * pre-rotating the entire systematic input simd_size times (~1 MB per code block
 * at BG1 Zc=384). Nothing here needs that.
 *
 * At 128 bit this form is NOT used: isolated microbenchmarks make it look
 * marginally faster, but in ldpctest it is consistently slower than the alignr
 * emission below (BG1 2.196 vs 2.100 us, BG2 1.390 vs 1.250 us parity). alignr
 * keeps the systematic operands in registers across the many rows that share
 * them, which the load form gives up. So: alignr at 128 bit, this at 256/512.
 *
 * Bounds. Systematic: W*(NCH-1) + (nc-1)*2*Zc + Zc-1 + W == nc*2*Zc exactly, so
 * the doubled input needs no padding. Kernel: the scratch holds each group twice
 * (4 * 2 * Zc), so W*j + s + W < 2*Zc always. Within stage 1 the second copy is
 * not yet written, but LAG is chosen so that W*j + s + W <= Zc there. */

typedef struct {
  int W, shift;
  const char *type, *xorf, *loaduf, *storeuf, *lcast, *scast, *tag;
} lgf_isa;

static const lgf_isa LGF_ISAS[] = {
    {16, 4, "simde__m128i", "simde_mm_xor_si128", "simde_mm_loadu_si128", "simde_mm_storeu_si128", "(const simde__m128i *)", "(simde__m128i *)", "128"},
    {32, 5, "simde__m256i", "simde_mm256_xor_si256", "simde_mm256_loadu_si256", "simde_mm256_storeu_si256", "(const simde__m256i *)", "(simde__m256i *)", "256"},
    {64, 6, "__m512i", "_mm512_xor_si512", "_mm512_loadu_si512", "_mm512_storeu_si512", "(const void *)", "(void *)", "512"},
};
#define LGF_NISA (int)(sizeof(LGF_ISAS) / sizeof(LGF_ISAS[0]))

static void lgf_load(lgf_expr *e, const lgf_isa *I, const char *base, int off)
{
  lgf_push(e, "%s(%s(%s+%d))", I->loaduf, I->lcast, base, off);
}

/* all systematic residual terms of a row, read relative to byte base `cbase` */
static void lgf_sys_terms(lgf_expr *e, const lgf_isa *I, const uint8_t *R, int nc, int Zc, const char *cbase)
{
  for (int c = 0; c < nc; c++)
    for (int g = 0; g < Zc; g++)
      if (R[c * Zc + g])
        lgf_load(e, I, cbase, c * 2 * Zc + (g + 1) % Zc);
}

/* rot(Pk,s), read relative to byte base `kbase` (which already carries W*j) */
static void lgf_kern_ref(lgf_expr *e, const lgf_isa *I, const char *kbase, int k, int s, int Zc)
{
  lgf_load(e, I, kbase, k * 2 * Zc + s);
}

/* store one parity chunk to the output, and to the kernel scratch when needed */
static void lgf_store(FILE *fd, const lgf_isa *I, const char *dst, int off, const char *val)
{
  fprintf(fd, "     %s(%s(%s+%d),%s);\n", I->storeuf, I->scast, dst, off, val);
}

/* Emit the factored encoder for one (BG, Zc, width).
 * Returns 0 on success, -1 if the structure is not emittable at this width. */
static int lgf_emit_wide(int BG, int Zc, int nrows, int ncols, const lgf_isa *I, const lgf_fac *fac, int LAG, int direct, int factored)
{
  const int W = I->W, NCH = Zc / W, KSL = 2 * Zc; /* kernel slot stride */
  char fname[160], fn[96];

  if (BG == 1)
    snprintf(fn, sizeof fn, "ldpc%d_byte_%s", Zc, I->tag);
  else
    snprintf(fn, sizeof fn, "ldpc_BG2_Zc%d_byte_%s", Zc, I->tag);
  if (BG == 1)
    snprintf(fname, sizeof fname, "ldpc%d_factored_byte_%s.c", Zc, I->tag);
  else
    snprintf(fname, sizeof fname, "ldpc_BG2_Zc%d_factored_byte_%s.c", Zc, I->tag);

  FILE *fd = fopen(fname, "w");
  if (!fd) {
    fprintf(stderr, "cannot open %s\n", fname);
    return -1;
  }
  lgf_expr e = {.n = 0};

  fprintf(fd, "#include <string.h>\n#include \"PHY/sse_intrin.h\"\n");
  fprintf(fd, "// generated: BG%d Zc=%d, %s-bit, factored\n", BG, Zc, I->tag);
  fprintf(fd, "// %d terms vs %d expanded (%.2fx); %d chunks/group; pipeline lag %d\n", factored, direct, (double)direct / factored, NCH, LAG);
  fprintf(fd, "// every read is one unaligned load at a constant offset\n");
  fprintf(fd, "static inline void %s(uint8_t *c,uint8_t *d) {\n", fn);
  fprintf(fd, "  uint8_t kd[%d] __attribute__((aligned(64)));  // P0..P3, each stored twice\n", LGF_NKERN * KSL);
  fprintf(fd, "  int i2;\n\n");

  /* ---------- stage 1: P0, kernel-free extension rows, lagged kernel ---------- */
  fprintf(fd, "  // stage 1: P0 and the kernel-free extension rows, with the kernel rows\n");
  fprintf(fd, "  // pipelined %d chunk(s) behind so the loop need not be split\n", LAG);
  fprintf(fd, "  for (i2=0; i2<%d; i2++) {\n", NCH);
  fprintf(fd, "     const uint8_t *cb=c+%d*i2;\n", W);

  fprintf(fd, "     %s p0=", I->type);
  lgf_sys_terms(&e, I, lgf_res[0], ncols, Zc, "cb");
  lgf_flush(fd, &e, I->xorf);
  fprintf(fd, ";\n");
  fprintf(fd, "     %s(%s(d+%d*i2),p0);\n", I->storeuf, I->scast, W);

  for (int r = LGF_NKERN; r < nrows; r++) {
    if (fac[r].nref)
      continue;
    fprintf(fd, "     %s(%s(d+%d+%d*i2),", I->storeuf, I->scast, Zc * r, W);
    lgf_sys_terms(&e, I, lgf_res[r], ncols, Zc, "cb");
    lgf_flush(fd, &e, I->xorf);
    fprintf(fd, ");   //row %d\n", r);
  }

  fprintf(fd, "     if (i2>=%d) {\n", LAG);
  fprintf(fd, "       const uint8_t *cm=c+%d*(i2-%d);\n", W, LAG);
  fprintf(fd, "       uint8_t *dm=d+%d*(i2-%d);\n", W, LAG);
  for (int r = 1; r < LGF_NKERN; r++) {
    fprintf(fd, "       %s p%d=", I->type, r);
    for (int i = 0; i < fac[r].nref; i++)
      lgf_load(&e, I, "dm", fac[r].k[i] * Zc + fac[r].s[i]);
    lgf_sys_terms(&e, I, lgf_res[r], ncols, Zc, "cm");
    lgf_flush(fd, &e, I->xorf);
    fprintf(fd, ";\n");
    fprintf(fd, "       %s(%s(dm+%d),p%d);\n", I->storeuf, I->scast, Zc * r, r);
  }
  fprintf(fd, "     }\n  }\n\n");

  /* P0 must be doubled before the epilogue, whose rotations wrap past Zc */
  fprintf(fd, "  memcpy(kd,d,%d); memcpy(kd+%d,d,%d);   // P0 doubled: the epilogue rotations wrap\n", Zc, Zc, Zc);

  fprintf(fd, "  // epilogue: kernel rows for the final %d chunk(s)\n", LAG);
  for (int j = NCH - LAG; j < NCH; j++) {
    fprintf(fd, "  { const uint8_t *cm=c+%d, *km=kd+%d; uint8_t *dm=d+%d;\n", W * j, W * j, W * j);
    for (int r = 1; r < LGF_NKERN; r++) {
      fprintf(fd, "    %s p%d=", I->type, r);
      for (int i = 0; i < fac[r].nref; i++)
        if (fac[r].k[i] == 0)
          lgf_kern_ref(&e, I, "km", 0, fac[r].s[i], Zc);   /* wraps: doubled scratch */
        else
          lgf_load(&e, I, "dm", fac[r].k[i] * Zc + fac[r].s[i]); /* chunk-local: from d */
      lgf_sys_terms(&e, I, lgf_res[r], ncols, Zc, "cm");
      lgf_flush(fd, &e, I->xorf);
      fprintf(fd, ";\n");
      fprintf(fd, "    %s(%s(dm+%d),p%d);\n", I->storeuf, I->scast, Zc * r, r);
    }
    fprintf(fd, "  }\n");
  }

  fprintf(fd, "  for (int k=1;k<%d;k++) { memcpy(kd+k*%d,d+k*%d,%d); memcpy(kd+k*%d+%d,d+k*%d,%d); }\n\n", LGF_NKERN, KSL, Zc, Zc, KSL, Zc, Zc, Zc);

  /* ---------- stage 2: extension rows that reference the kernel ---------- */
  fprintf(fd, "  // stage 2: extension rows that reference the kernel\n");
  fprintf(fd, "  for (i2=0; i2<%d; i2++) {\n", NCH);
  fprintf(fd, "     const uint8_t *cb=c+%d*i2, *kb=kd+%d*i2;\n", W, W);
  for (int r = LGF_NKERN; r < nrows; r++) {
    if (!fac[r].nref)
      continue;
    fprintf(fd, "     %s(%s(d+%d+%d*i2),", I->storeuf, I->scast, Zc * r, W);
    for (int i = 0; i < fac[r].nref; i++)
      lgf_kern_ref(&e, I, "kb", fac[r].k[i], fac[r].s[i], Zc);
    lgf_sys_terms(&e, I, lgf_res[r], ncols, Zc, "cb");
    lgf_flush(fd, &e, I->xorf);
    fprintf(fd, ");   //row %d [", r);
    for (int i = 0; i < fac[r].nref; i++)
      fprintf(fd, "%sP%d<<%d", i ? "+" : "", fac[r].k[i], fac[r].s[i]);
    fprintf(fd, " + %d sys]\n", fac[r].cost - fac[r].nref);
  }
  fprintf(fd, "  }\n}\n");
  fclose(fd);
  printf("    %s-bit -> %s (%d chunks, LAG %d)\n", I->tag, fname, NCH, LAG);
  return 0;
}


/*
 * Generate the factored encoder for (BG,Zc).
 * Returns 0 on success, -1 if the structure is not emittable (caller should
 * fall back to the stock expanded generator).
 */
static int generate_factored_encoder(int BG, int Zc, const short *nos, const short *ptr, const short *gsv)
{
  const int nrows = (BG == 1) ? 46 : 42;
  const int ncols = (BG == 1) ? 22 : 10;
  const int shift = 4, mask = 15, W = 16;
  const int NCH = Zc >> shift;
  const int n = ncols * Zc;

  if (Zc & mask) {
    fprintf(stderr, "factored generator: Zc=%d not a multiple of %d\n", Zc, W);
    return -1;
  }

  /* ---- symbolic rows from the shift tables ---- */
  for (int r = 0; r < nrows; r++) {
    memset(lgf_G[r], 0, n);
    for (int c = 0; c < ncols; c++) {
      int t = r * ncols + c;
      for (int i = 0; i < nos[t]; i++) {
        int g = gsv[ptr[t] + i] % Zc;
        lgf_G[r][c * Zc + g] ^= 1; /* GF(2): duplicate monomials cancel */
      }
    }
  }

  /* ---- factor: kernel rows against earlier kernel rows, then the rest ---- */
  lgf_fac fac[LGF_MAXROWS];
  static uint8_t pool[LGF_NKERN][LGF_MAXNC * LGF_MAXZ];
  int poolid[LGF_NKERN];

  fac[0].nref = 0;
  fac[0].cost = lgf_pop(lgf_G[0], n);
  memcpy(lgf_res[0], lgf_G[0], n);

  for (int r = 1; r < LGF_NKERN; r++) {
    for (int a = 0; a < r; a++) {
      memcpy(pool[a], lgf_G[a], n);
      poolid[a] = a;
    }
    lgf_factor(lgf_G[r], (const uint8_t(*)[LGF_MAXNC * LGF_MAXZ])pool, poolid, r, ncols, Zc, &fac[r]);
    lgf_residual(lgf_res[r], lgf_G[r], &fac[r], ncols, Zc);
  }
  for (int a = 0; a < LGF_NKERN; a++) {
    memcpy(pool[a], lgf_G[a], n);
    poolid[a] = a;
  }
  for (int r = LGF_NKERN; r < nrows; r++) {
    lgf_factor(lgf_G[r], (const uint8_t(*)[LGF_MAXNC * LGF_MAXZ])pool, poolid, LGF_NKERN, ncols, Zc, &fac[r]);
    lgf_residual(lgf_res[r], lgf_G[r], &fac[r], ncols, Zc);
  }

  /* ---- pipeline lag: rot(Pk,s) at chunk j reads P0 chunks j+q and j+q+1, so
   *      emitting kernel rows LAG=max(q)+1 chunks behind P0 keeps every read
   *      inside what has already been computed. References between kernel rows
   *      must be chunk-local (q==0), else the scheme does not hold. ---- */
  int LAG = 1;
  for (int r = 1; r < LGF_NKERN; r++)
    for (int i = 0; i < fac[r].nref; i++) {
      int q = fac[r].s[i] >> shift;
      if (fac[r].k[i] != 0 && q != 0) {
        fprintf(stderr, "factored generator: BG%d Zc=%d row %d references P%d with shift %d (not chunk-local)\n", BG, Zc, r, fac[r].k[i], fac[r].s[i]);
        return -1;
      }
      if (q + 1 > LAG)
        LAG = q + 1;
    }
  if (LAG >= NCH) {
    fprintf(stderr, "factored generator: BG%d Zc=%d needs LAG=%d >= NCH=%d\n", BG, Zc, LAG, NCH);
    return -1;
  }

  /* ---- report ---- */
  int direct = 0, factored = 0, nk_rows = 0;
  for (int r = 0; r < nrows; r++) {
    direct += lgf_pop(lgf_G[r], n);
    factored += fac[r].cost;
    if (r >= LGF_NKERN && fac[r].nref)
      nk_rows++;
  }
  printf("BG%d Zc=%3d : direct %5d -> factored %4d terms (%.2fx)  LAG=%d  kernel-referencing ext rows %d/%d\n", BG, Zc, direct, factored, (double)direct / factored, LAG, nk_rows, nrows - LGF_NKERN);

  /* ---- emit ---- */
  char fname[128];
  if (BG == 1)
    snprintf(fname, sizeof fname, "ldpc%d_factored_byte_128.c", Zc);
  else
    snprintf(fname, sizeof fname, "ldpc_BG2_Zc%d_factored_byte_128.c", Zc);
  FILE *fd = fopen(fname, "w");
  if (!fd) {
    fprintf(stderr, "cannot open %s\n", fname);
    return -1;
  }

  const char *xorc = "simde_mm_xor_si128";
  lgf_expr e = {.n = 0};

  fprintf(fd, "#include <string.h>\n#include \"PHY/sse_intrin.h\"\n");
  fprintf(fd, "// generated code for BG%d Zc=%d, byte encoding, factored\n", BG, Zc);
  fprintf(fd, "// %d terms vs %d expanded (%.2fx); pipeline lag %d chunk(s)\n", factored, direct, (double)direct / factored, LAG);
  if (BG == 1)
    fprintf(fd, "static inline void ldpc%d_byte_128(uint8_t *c,uint8_t *d) {\n", Zc);
  else
    fprintf(fd, "static inline void ldpc_BG2_Zc%d_byte_128(uint8_t *c,uint8_t *d) {\n", Zc);
  fprintf(fd, "  simde__m128i *csimd=(simde__m128i *)c,*dsimd=(simde__m128i *)d;\n");
  fprintf(fd, "  simde__m128i *c2,*d2,*cm,*dm;\n");
  fprintf(fd, "  uint8_t kd[%d] __attribute__((aligned(64)));\n", LGF_NKERN * 2 * Zc);
  fprintf(fd, "  int i2;\n\n");

  /* ---------- stage 1 ---------- */
  fprintf(fd, "  // stage 1: P0, the kernel-free extension rows, and the kernel rows\n");
  fprintf(fd, "  // pipelined %d chunk(s) behind P0 so no loop split is needed\n", LAG);
  fprintf(fd, "  for (i2=0; i2<%d; i2++) {\n", NCH);
  fprintf(fd, "     c2=&csimd[i2];\n     d2=&dsimd[i2];\n");

  fprintf(fd, "\n//row: 0\n     d2[0]=");
  lgf_terms(&e, lgf_res[0], ncols, Zc, shift, mask, "c2");
  lgf_flush(fd, &e, xorc);
  fprintf(fd, ";\n");

  for (int r = LGF_NKERN; r < nrows; r++) {
    if (fac[r].nref)
      continue;
    fprintf(fd, "\n//row: %d  [direct, %d terms]\n     d2[%d]=", r, fac[r].cost, (Zc * r) >> shift);
    lgf_terms(&e, lgf_res[r], ncols, Zc, shift, mask, "c2");
    lgf_flush(fd, &e, xorc);
    fprintf(fd, ";\n");
  }

  fprintf(fd, "\n     if (i2>=%d) {\n", LAG);
  fprintf(fd, "       cm=&csimd[i2-%d]; dm=&dsimd[i2-%d];\n", LAG, LAG);
  for (int r = 1; r < LGF_NKERN; r++) {
    fprintf(fd, "       dm[%d]=", (Zc * r) >> shift);
    for (int i = 0; i < fac[r].nref; i++)
      lgf_kref_stage1(&e, fac[r].k[i], fac[r].s[i], NCH, shift, mask, 0, 0);
    lgf_terms(&e, lgf_res[r], ncols, Zc, shift, mask, "cm");
    lgf_flush(fd, &e, xorc);
    fprintf(fd, ";\n");
  }
  fprintf(fd, "     }\n  }\n\n");

  /* epilogue: the last LAG chunks of the kernel rows, wrapping on P0 */
  fprintf(fd, "  // epilogue: kernel rows for the final %d chunk(s), wrapping\n", LAG);
  for (int j = NCH - LAG; j < NCH; j++) {
    fprintf(fd, "  cm=&csimd[%d]; dm=&dsimd[%d];\n", j, j);
    for (int r = 1; r < LGF_NKERN; r++) {
      fprintf(fd, "  dm[%d]=", (Zc * r) >> shift);
      for (int i = 0; i < fac[r].nref; i++)
        lgf_kref_stage1(&e, fac[r].k[i], fac[r].s[i], NCH, shift, mask, j, 1);
      lgf_terms(&e, lgf_res[r], ncols, Zc, shift, mask, "cm");
      lgf_flush(fd, &e, xorc);
      fprintf(fd, ";\n");
    }
  }

  /* ---------- kernel duplication ---------- */
  fprintf(fd, "\n  // double P0..P3 so any rotation becomes one unaligned load\n");
  fprintf(fd, "  for (int k=0;k<%d;k++) {\n", LGF_NKERN);
  fprintf(fd, "    memcpy(kd+k*%d,     d+k*%d, %d);\n", 2 * Zc, Zc, Zc);
  fprintf(fd, "    memcpy(kd+k*%d+%d, d+k*%d, %d);\n", 2 * Zc, Zc, Zc, Zc);
  fprintf(fd, "  }\n\n");

  /* ---------- stage 2 ---------- */
  fprintf(fd, "  // stage 2: extension rows that reference the kernel\n");
  fprintf(fd, "  for (i2=0; i2<%d; i2++) {\n", NCH);
  fprintf(fd, "     c2=&csimd[i2];\n     d2=&dsimd[i2];\n");
  fprintf(fd, "     const uint8_t *kb=kd+%d*i2;\n", W);
  for (int r = LGF_NKERN; r < nrows; r++) {
    if (!fac[r].nref)
      continue;
    fprintf(fd, "\n//row: %d  [", r);
    for (int i = 0; i < fac[r].nref; i++)
      fprintf(fd, "%sP%d<<%d", i ? "+" : "", fac[r].k[i], fac[r].s[i]);
    fprintf(fd, " + %d sys]\n     d2[%d]=", fac[r].cost - fac[r].nref, (Zc * r) >> shift);
    for (int i = 0; i < fac[r].nref; i++)
      lgf_kref_stage2(&e, fac[r].k[i], fac[r].s[i], Zc);
    lgf_terms(&e, lgf_res[r], ncols, Zc, shift, mask, "c2");
    lgf_flush(fd, &e, xorc);
    fprintf(fd, ";\n");
  }
  fprintf(fd, "  }\n}\n");
  fclose(fd);

  /* Also emit 256- and 512-bit encoders where the lifting size divides evenly.
   * Those widths cannot use alignr (it is lane-local), so they take the
   * unaligned-load emission above. LAG is recomputed per width because it is
   * measured in chunks, and a chunk is wider. */
  for (int w = 0; w < LGF_NISA; w++) {
    const lgf_isa *I = &LGF_ISAS[w];
    if (I->W == 16 || Zc % I->W)
      continue;
    const int wNCH = Zc / I->W;
    int wLAG = 1;
    for (int r = 1; r < LGF_NKERN; r++)
      for (int i = 0; i < fac[r].nref; i++)
        if (fac[r].s[i] / I->W + 1 > wLAG)
          wLAG = fac[r].s[i] / I->W + 1;
    if (wLAG >= wNCH) {
      printf("    %s-bit skipped: LAG %d >= %d chunks\n", I->tag, wLAG, wNCH);
      continue;
    }
    lgf_emit_wide(BG, Zc, nrows, ncols, I, fac, wLAG, direct, factored);
  }
  return 0;
}

#endif /* LDPC_GENERATE_FACTORED_C */
