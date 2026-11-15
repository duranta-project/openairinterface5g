/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Inner-RX offload module for OpenCL GPUs (libinner_rx_cl.so, nr_inner_rx_offload.h): the FP16 2-layer receiver of
// nr_lbest_f16.c (fused MRC + Gram + L-best / full ML, layer demap, descrambling) for 64 and 256QAM, and the integer
// single-layer receiver (MRC and nr_compute_llr, bit-exact with the CPU) for QPSK to 256QAM; the rest stays on the CPU. Written for
// the integrated GPUs of ARM SoCs (Mali, Adreno). L-best runs in two passes: the 3x3 window of every RE and layer, then the full
// grid on the wide ones only (nrh_wide), so that a few wide REs do not hold up whole waves. Build options per GPU and kernel
// (NR_IRX_CL_OPTS overrides them): Mali runs the exact FP16 operations (the LLRs of the ARMv8.2 CPU receiver built without
// contraction); Adreno has no correctly rounded fma and no FP16 subnormals and runs mad() (close LLRs, same decisions).
//
// Memory: the staging is host-allocated buffer memory the GPU reads in place, mapped for the host between runs. On a
// GPU coherent with the CPU caches (Adreno: a buffer mapped once stays consistent both ways; Mali: it does not, its
// map / unmap do the cache maintenance) alloc_host hands out such buffers mapped once, for rxdataF and the codeword
// LLRs (ulsch_llrs): the kernels then read the RX in place and write the LLRs into the codeword buffer, no staging or
// copies. NR_IRX_CL_COHERENT=0 / 1 overrides the per-vendor choice.

#define CL_TARGET_OPENCL_VERSION 300
#include <CL/cl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "nr_inner_rx_offload.h"

// program sources embedded at build time: the 2-layer receiver (nr_lbest_f16_layer.cl, nr_lbest_f16_core.h,
// nr_lbest_f16_kernels.cl) and the single-layer one (nr_irx_1layer.cl)
extern const char nr_irx_cl_src[];
extern const char nr_irx_cl_src_1l[];

// struct nrh_sym_t of nr_lbest_f16_kernels.cl
typedef struct {
  cl_uint rx[NR_IRX_MAX_RX], ch, map, out, length, ch_stride, scr;
  cl_short2 cpe;
  cl_uint direct;
} nr_irx_cl_sym_t;
#define NRH_NO_MAP 0xffffffffu
#define NRH_NO_SCR 0xffffffffu
// kernel arguments
enum { A_SYM = 0, A_RX, A_CH, A_MAP, A_SCR, A_OUT, A_NB_RX, A_LAMBDA, A_SHIFT, A_WIDE, A_NWIDE };

// programs: per modulation (nb 3, 4), the full grid (ML) and the two passes of L-best
enum { K_ML = 0, K_P1, K_P2, K_N };
static const char *kname[K_N] = {"nr_inner_rx_2layer_f16", "nr_inner_rx_2layer_f16_p1", "nr_inner_rx_2layer_f16_p2"};

struct nr_irx_ctx {
  cl_command_queue q;
  cl_kernel k[2][K_N];
  cl_kernel k1[4]; // single layer, QPSK to 256QAM
  cl_mem rx, ch, map, scr, out, sym, wide, nwide;
  // host mappings of the staging while the context is acquired
  c16_t *rx_h, *ch_h;
  uint16_t *map_h;
  uint32_t *scr_h;
  bool busy;
};

#define NRC_MAX_REGIONS 32
#define NRC_ARENA (64 << 20) // minimum region size: the RX of all antennas in one buffer (one kernel argument)

static struct {
  cl_context c;
  cl_device_id d;
  char name[128], vendor[128];
  bool coherent; // host mappings stay consistent with the GPU (alloc_host regions)
  bool exact; // the exact FP16 operations of the CPU receiver (no mad)
  struct {
    cl_mem b;
    char *h;
    size_t bytes, used; // allocated in order, never reused
    int live; // allocations not freed
  } region[NRC_MAX_REGIONS]; // alloc_host memory: rxdataF, ulsch_llrs
  int nb_region;
  cl_program prog[2][K_N];
  cl_program prog1[4];
  int split[2][K_N]; // work-items per RE and layer (NRH_CL_LAYER_SPLIT)
  nr_irx_ctx_t *ctx;
  int n;
  int max_nb_rx;
  uint32_t stride; // REs per staging row
  size_t rx_sz, ch_sz, llr_sz, scr_sz; // elements per symbol (RX, LLRs), per estimate (CH), per batch (SCRAMBLE words)
  pthread_mutex_t lock;
} g = {.lock = PTHREAD_MUTEX_INITIALIZER};

// build options of program (nb, kind) on this GPU: measured on a full slot (273 PRB, 12 symbols, 2 layers, 2 / 4 Rx).
// Mali-G610: one work-item per RE, no unrolling but the 256QAM window pass (unrolled code in the full grid is ~7x
// slower). Adreno 663: mad(); the 256QAM kernels per RE and layer and unrolled (two copies of the detector or a rolled
// 16x16 grid spill its registers: up to 50-400x slower), the 64QAM ones per RE and rolled.
static void cl_options(int nb, int kind, char *opt, size_t sz, int *split)
{
  const bool adreno = strstr(g.vendor, "QUALCOMM") != NULL;
  const bool unroll = adreno ? nb == 4 : nb == 4 && kind == K_P1;
  *split = adreno && nb == 4;
  snprintf(opt,
           sz,
           "-cl-std=CL3.0 -DNRH_NB=%d -DNRH_FULL=%d%s%s%s",
           nb,
           kind == K_ML,
           adreno ? " -DNRH_CL_MAD" : "",
           unroll ? " -DNRH_CL_UNROLL" : "",
           *split ? " -DNRH_CL_LAYER_SPLIT" : "");
  const char *x = getenv("NR_IRX_CL_OPTS"); // replaces the GPU defaults (but NRH_NB / NRH_FULL)
  if (x) {
    snprintf(opt, sz, "-cl-std=CL3.0 -DNRH_NB=%d -DNRH_FULL=%d %s", nb, kind == K_ML, x);
    *split = strstr(x, "NRH_CL_LAYER_SPLIT") != NULL;
  }
  if (strstr(opt, "NRH_CL_MAD"))
    g.exact = false;
}

// the LLRs of the ARMv8.2 CPU receiver built without contraction (no mad in any program); for the module test
bool nr_irx_cl_exact(void)
{
  return g.exact;
}

// the GPU and its context, at the first alloc_host or init (the RU allocates rxdataF before the gNB init); g.lock held
static int cl_setup(void)
{
  if (g.c)
    return 0;
  cl_platform_id pl[8];
  cl_uint npl = 0;
  if (clGetPlatformIDs(8, pl, &npl) != CL_SUCCESS)
    return -1;
  cl_int e = CL_DEVICE_NOT_FOUND;
  for (cl_uint i = 0; i < npl && e != CL_SUCCESS; i++) // the first platform with a GPU (Adreno: before rusticl)
    e = clGetDeviceIDs(pl[i], CL_DEVICE_TYPE_GPU, 1, &g.d, NULL);
  if (e != CL_SUCCESS)
    return -1;
  clGetDeviceInfo(g.d, CL_DEVICE_NAME, sizeof(g.name), g.name, NULL);
  clGetDeviceInfo(g.d, CL_DEVICE_VENDOR, sizeof(g.vendor), g.vendor, NULL);
  g.coherent = strstr(g.vendor, "QUALCOMM") != NULL;
  const char *x = getenv("NR_IRX_CL_COHERENT");
  if (x)
    g.coherent = atoi(x) != 0;
  g.c = clCreateContext(NULL, 1, &g.d, NULL, NULL, &e);
  if (e != CL_SUCCESS) {
    g.c = NULL;
    return -1;
  }
  return 0;
}

static cl_mem host_buffer(size_t bytes)
{
  cl_int e;
  cl_mem b = clCreateBuffer(g.c, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, bytes, NULL, &e);
  return e == CL_SUCCESS ? b : NULL;
}

int nr_irx_shutdown(void)
{
  for (int c = 0; c < g.n; c++) {
    nr_irx_ctx_t *x = &g.ctx[c];
    if (x->q && x->rx_h) { // the staging of a coherent GPU, mapped since init
      void *h[4] = {x->rx_h, x->ch_h, x->map_h, x->scr_h};
      cl_mem m[4] = {x->rx, x->ch, x->map, x->scr};
      for (int i = 0; i < 4; i++)
        if (h[i])
          clEnqueueUnmapMemObject(x->q, m[i], h[i], 0, NULL, NULL);
    }
    if (x->q)
      clFinish(x->q);
    for (int nb = 0; nb < 2; nb++)
      for (int k = 0; k < K_N; k++)
        if (x->k[nb][k])
          clReleaseKernel(x->k[nb][k]);
    for (int m = 0; m < 4; m++)
      if (x->k1[m])
        clReleaseKernel(x->k1[m]);
    cl_mem m[8] = {x->rx, x->ch, x->map, x->scr, x->out, x->sym, x->wide, x->nwide};
    for (int i = 0; i < 8; i++)
      if (m[i])
        clReleaseMemObject(m[i]);
    if (x->q)
      clReleaseCommandQueue(x->q);
  }
  free(g.ctx);
  g.ctx = NULL;
  g.n = 0;
  for (int nb = 0; nb < 2; nb++)
    for (int k = 0; k < K_N; k++)
      if (g.prog[nb][k]) {
        clReleaseProgram(g.prog[nb][k]);
        g.prog[nb][k] = NULL;
      }
  for (int m = 0; m < 4; m++)
    if (g.prog1[m]) {
      clReleaseProgram(g.prog1[m]);
      g.prog1[m] = NULL;
    }
  // the context stays for the allocations of alloc_host, valid until free_host
  return 0;
}

// alloc_host memory: regions of at least NRC_ARENA, allocated from in order (the RU allocates the rxdataF of its antennas
// one after the other: they share a region, so the kernels take their RX from one buffer)
void *nr_irx_alloc_host(size_t bytes)
{
  void *h = NULL;
  bytes = (bytes + 4095) & ~(size_t)4095;
  pthread_mutex_lock(&g.lock);
  if (cl_setup() == 0 && g.coherent) {
    int r = 0;
    while (r < g.nb_region && g.region[r].bytes - g.region[r].used < bytes)
      r++;
    if (r == g.nb_region && r < NRC_MAX_REGIONS) {
      const size_t sz = bytes > NRC_ARENA ? bytes : NRC_ARENA;
      cl_mem b = host_buffer(sz);
      cl_command_queue q = b ? clCreateCommandQueueWithProperties(g.c, g.d, NULL, NULL) : NULL;
      cl_int e = CL_INVALID_VALUE;
      char *m = q ? clEnqueueMapBuffer(q, b, CL_TRUE, CL_MAP_READ | CL_MAP_WRITE, 0, sz, 0, NULL, NULL, &e) : NULL;
      if (q)
        clReleaseCommandQueue(q);
      if (m && e == CL_SUCCESS) {
        memset(m, 0, sz);
        g.region[g.nb_region++] = (typeof(g.region[0])){.b = b, .h = m, .bytes = sz};
      } else if (b) {
        clReleaseMemObject(b);
      }
    }
    if (r < g.nb_region) {
      h = g.region[r].h + g.region[r].used;
      g.region[r].used += bytes;
      g.region[r].live++;
    }
  }
  pthread_mutex_unlock(&g.lock);
  return h;
}

int nr_irx_free_host(void *ptr)
{
  int ret = -1;
  pthread_mutex_lock(&g.lock);
  for (int r = 0; r < g.nb_region; r++)
    if ((char *)ptr >= g.region[r].h && (char *)ptr < g.region[r].h + g.region[r].used) {
      if (--g.region[r].live == 0) {
        clReleaseMemObject(g.region[r].b); // the mapping goes with it
        g.region[r] = g.region[--g.nb_region];
      }
      ret = 0;
      break;
    }
  pthread_mutex_unlock(&g.lock);
  return ret;
}

// region holding [p, p + bytes), -1 if none
static int region_of(const void *p, size_t bytes)
{
  int r = -1;
  pthread_mutex_lock(&g.lock);
  for (int i = 0; i < g.nb_region && r < 0; i++)
    if ((const char *)p >= g.region[i].h && (const char *)p + bytes <= g.region[i].h + g.region[i].bytes)
      r = i;
  pthread_mutex_unlock(&g.lock);
  return r;
}

// rxdataF in an alloc_host region is read in place; other memory goes through the RX staging
int nr_irx_register_host(const void *ptr, size_t bytes)
{
  return region_of(ptr, bytes) >= 0 ? 0 : 1;
}

// the context's staging mapped for the host (between acquire and run; on a coherent GPU from init to shutdown)
static int map_staging(nr_irx_ctx_t *x)
{
  if (g.coherent && x->rx_h)
    return 0;
  cl_int e[4];
  const cl_map_flags w = CL_MAP_READ | CL_MAP_WRITE; // kept between runs: a caller may run a batch twice
  x->rx_h = clEnqueueMapBuffer(x->q, x->rx, CL_FALSE, w, 0, NR_IRX_MAX_SYM * g.rx_sz * sizeof(c16_t), 0, NULL, NULL, &e[0]);
  x->ch_h = clEnqueueMapBuffer(x->q, x->ch, CL_FALSE, w, 0, NR_IRX_MAX_CH * g.ch_sz * sizeof(c16_t), 0, NULL, NULL, &e[1]);
  x->map_h = clEnqueueMapBuffer(x->q, x->map, CL_FALSE, w, 0, NR_IRX_MAX_SYM * g.stride * sizeof(uint16_t), 0, NULL, NULL, &e[2]);
  x->scr_h = clEnqueueMapBuffer(x->q, x->scr, CL_FALSE, w, 0, g.scr_sz * sizeof(uint32_t), 0, NULL, NULL, &e[3]);
  if (clFinish(x->q) != CL_SUCCESS)
    return -1;
  for (int i = 0; i < 4; i++)
    if (e[i] != CL_SUCCESS)
      return -1;
  return 0;
}

static void unmap_staging(nr_irx_ctx_t *x)
{
  if (g.coherent && g.ctx) // mapped from init to shutdown
    return;
  void *h[4] = {x->rx_h, x->ch_h, x->map_h, x->scr_h};
  cl_mem m[4] = {x->rx, x->ch, x->map, x->scr};
  for (int i = 0; i < 4; i++)
    if (h[i])
      clEnqueueUnmapMemObject(x->q, m[i], h[i], 0, NULL, NULL);
  x->rx_h = x->ch_h = NULL;
  x->map_h = NULL;
  x->scr_h = NULL;
}

// a program of src built with opt, its log printed on failure
static cl_program build(const char *src, const char *opt, const char *what)
{
  cl_int e;
  cl_program pr = clCreateProgramWithSource(g.c, 1, &src, NULL, &e);
  if (e == CL_SUCCESS && clBuildProgram(pr, 1, &g.d, opt, NULL, NULL) == CL_SUCCESS)
    return pr;
  static char log[1 << 14];
  log[0] = 0;
  if (pr) {
    clGetProgramBuildInfo(pr, g.d, CL_PROGRAM_BUILD_LOG, sizeof(log), log, NULL);
    clReleaseProgram(pr);
  }
  fprintf(stderr, "[inner_rx_cl] %s: build of %s (%s) failed:\n%s\n", g.name, what, opt, log);
  return NULL;
}

/// 0, -1 without an OpenCL GPU, -2 if a program does not build
int nr_irx_init(uint32_t max_re_per_symbol, int max_nb_rx, int nb_ctx)
{
  if (g.ctx || nb_ctx < 1 || max_nb_rx < 1 || max_nb_rx > NR_IRX_MAX_RX || max_re_per_symbol == 0 || max_re_per_symbol >= 65536)
    return -1;
  pthread_mutex_lock(&g.lock);
  const int s = cl_setup();
  pthread_mutex_unlock(&g.lock);
  if (s != 0)
    return -1;
  cl_int e;
  // all programs now: the unrolled ones take seconds to build (Adreno)
  g.exact = true;
  for (int nb = 0; nb < 2; nb++)
    for (int k = 0; k < K_N; k++) {
      char opt[256];
      cl_options(nb + 3, k, opt, sizeof(opt), &g.split[nb][k]);
      if (!(g.prog[nb][k] = build(nr_irx_cl_src, opt, kname[k]))) {
        nr_irx_shutdown();
        return -2;
      }
    }
  for (int m = 0; m < 4; m++) { // single layer: integer, the same options everywhere
    char opt[64];
    snprintf(opt, sizeof(opt), "-cl-std=CL3.0 -DNRH_NB=%d", m + 1);
    if (!(g.prog1[m] = build(nr_irx_cl_src_1l, opt, "nr_inner_rx_1layer"))) {
      nr_irx_shutdown();
      return -2;
    }
  }
  fprintf(stderr,
          "[inner_rx_cl] %s (%s): single layer, 2-layer 64/256QAM L-best and ML, %s\n",
          g.name,
          g.vendor,
          g.coherent ? "RX and LLRs in place in alloc_host memory" : "RX and LLRs through the staging");
  g.max_nb_rx = max_nb_rx;
  g.stride = (max_re_per_symbol + 15) & ~15u;
  g.rx_sz = (size_t)max_nb_rx * g.stride;
  g.ch_sz = 2 * g.rx_sz;
  g.llr_sz = (size_t)g.stride * 2 * 8;
  g.scr_sz = NR_IRX_MAX_SYM * g.llr_sz / 32 + 1;
  g.ctx = calloc(nb_ctx, sizeof(*g.ctx));
  if (!g.ctx) {
    nr_irx_shutdown();
    return -1;
  }
  g.n = nb_ctx;
  for (int c = 0; c < nb_ctx; c++) {
    nr_irx_ctx_t *x = &g.ctx[c];
    x->q = clCreateCommandQueueWithProperties(g.c, g.d, NULL, &e);
    x->rx = host_buffer(NR_IRX_MAX_SYM * g.rx_sz * sizeof(c16_t));
    x->ch = host_buffer(NR_IRX_MAX_CH * g.ch_sz * sizeof(c16_t));
    x->map = host_buffer(NR_IRX_MAX_SYM * g.stride * sizeof(uint16_t));
    x->scr = host_buffer(g.scr_sz * sizeof(uint32_t));
    x->out = host_buffer(NR_IRX_MAX_SYM * g.llr_sz * sizeof(int16_t));
    x->sym = host_buffer(NR_IRX_MAX_SYM * sizeof(nr_irx_cl_sym_t));
    x->wide = host_buffer(2 * NR_IRX_MAX_SYM * (size_t)g.stride * sizeof(cl_uint));
    x->nwide = host_buffer(sizeof(cl_uint));
    bool ok = e == CL_SUCCESS && x->rx && x->ch && x->map && x->scr && x->out && x->sym && x->wide && x->nwide;
    for (int nb = 0; nb < 2 && ok; nb++)
      for (int k = 0; k < K_N && ok; k++) {
        x->k[nb][k] = clCreateKernel(g.prog[nb][k], kname[k], &e);
        ok = e == CL_SUCCESS;
      }
    for (int m = 0; m < 4 && ok; m++) {
      x->k1[m] = clCreateKernel(g.prog1[m], "nr_inner_rx_1layer", &e);
      ok = e == CL_SUCCESS;
    }
    if (!ok || (g.coherent && map_staging(x) != 0)) {
      nr_irx_shutdown();
      return -1;
    }
  }
  return 0;
}

bool nr_irx_supported(int nb_layers, int nb_rx, int mod_order, int detector)
{
  if (nb_rx < 1 || nb_rx > g.max_nb_rx)
    return false;
  if (nb_layers == 1) // the detector is not used
    return mod_order == 2 || mod_order == 4 || mod_order == 6 || mod_order == 8;
  return nb_layers == 2 && (mod_order == 6 || mod_order == 8) && (detector == NR_IRX_DET_LBEST || detector == NR_IRX_DET_ML);
}

void nr_irx_release(nr_irx_ctx_t *ctx);

nr_irx_ctx_t *nr_irx_acquire(void)
{
  nr_irx_ctx_t *x = NULL;
  pthread_mutex_lock(&g.lock);
  for (int c = 0; c < g.n && !x; c++)
    if (!g.ctx[c].busy) {
      x = &g.ctx[c];
      x->busy = true;
    }
  pthread_mutex_unlock(&g.lock);
  if (x && map_staging(x) != 0) {
    nr_irx_release(x);
    x = NULL;
  }
  return x;
}

void nr_irx_release(nr_irx_ctx_t *ctx)
{
  unmap_staging(ctx);
  clFinish(ctx->q);
  pthread_mutex_lock(&g.lock);
  ctx->busy = false;
  pthread_mutex_unlock(&g.lock);
}

void *nr_irx_staging(nr_irx_ctx_t *ctx, int k, int which)
{
  switch (which) {
    case NR_IRX_STAGING_RX:
      return ctx->rx_h && k >= 0 && k < NR_IRX_MAX_SYM ? ctx->rx_h + k * g.rx_sz : NULL;
    case NR_IRX_STAGING_CH:
      return ctx->ch_h && k >= 0 && k < NR_IRX_MAX_CH ? ctx->ch_h + k * g.ch_sz : NULL;
    case NR_IRX_STAGING_REMAP:
      return ctx->map_h && k >= 0 && k < NR_IRX_MAX_SYM ? ctx->map_h + k * g.stride : NULL;
    case NR_IRX_STAGING_SCRAMBLE:
      return k == 0 ? ctx->scr_h : NULL;
    default:
      return NULL;
  }
}

// element offset of a pointer into a mapped staging buffer, -1 if outside
static long staging_off(const void *p, const void *base, size_t bytes, size_t elem)
{
  const char *c = p, *b = base;
  return p && base && c >= b && c < b + bytes ? (long)((c - b) / elem) : -1;
}

int nr_irx_run(nr_irx_ctx_t *ctx, const nr_irx_sym_t *sym, int nsym, const nr_irx_params_t *p)
{
  if (nsym < 1 || nsym > NR_IRX_MAX_SYM || !nr_irx_supported(p->nb_layers, p->nb_rx, p->mod_order, p->detector) || !ctx->rx_h)
    return -1;
  const int qm = p->mod_order, nl = p->nb_layers, nb = qm / 2 - 3, nb_rx = p->nb_rx;
  const bool direct = sym[0].rx_ant[0] != NULL;
  cl_mem rxb = ctx->rx; // the RX staging, or the region of the antennas' rxdataF
  int out_r = -1; // the region of all the codeword slices when the kernels write them in place
  nr_irx_cl_sym_t d[NR_IRX_MAX_SYM];
  uint32_t maxlen = 0, total = 0;
  for (int k = 0; k < nsym; k++) {
    const nr_irx_sym_t *s = &sym[k];
    const long ch = staging_off(s->chF, ctx->ch_h, NR_IRX_MAX_CH * g.ch_sz * sizeof(c16_t), sizeof(c16_t));
    const long map =
        s->re_map ? staging_off(s->re_map, ctx->map_h, NR_IRX_MAX_SYM * g.stride * sizeof(uint16_t), sizeof(uint16_t)) : 0;
    if ((s->rx_ant[0] != NULL) != direct || ch < 0 || map < 0 || s->ch_stride > g.stride || s->length > s->stride
        || s->stride > g.stride
        || (s->scramble && (s->scramble != ctx->scr_h || s->scramble_offset + s->length * nl * qm > g.scr_sz * 32)))
      return -1;
    d[k] = (nr_irx_cl_sym_t){.ch = ch,
                             .map = s->re_map ? (cl_uint)map : NRH_NO_MAP,
                             .length = s->length,
                             .ch_stride = s->ch_stride,
                             .scr = s->scramble ? s->scramble_offset : NRH_NO_SCR,
                             .cpe = {{s->cpe.r, s->cpe.i}},
                             .direct = direct};
    if (direct) { // each antenna's rxdataF, an alloc_host region (register_host returned 0)
      const size_t span = (s->re_map ? (size_t)s->re_map[s->length - 1] + 1 : s->length) * sizeof(c16_t);
      for (int a = 0; a < nb_rx; a++) {
        const int r = region_of(s->rx_ant[a], span);
        if (r < 0 || ((k > 0 || a > 0) && g.region[r].b != rxb))
          return -1;
        rxb = g.region[r].b;
        d[k].rx[a] = ((const char *)s->rx_ant[a] - g.region[r].h) / sizeof(c16_t);
      }
    } else {
      const long rx = staging_off(s->rxF, ctx->rx_h, NR_IRX_MAX_SYM * g.rx_sz * sizeof(c16_t), sizeof(c16_t));
      if (rx < 0)
        return -1;
      for (int a = 0; a < nb_rx; a++)
        d[k].rx[a] = rx + a * s->stride;
    }
    // the codeword slice in place when every slice is in the same region of a coherent GPU
    const int r = g.coherent ? region_of(s->llr_cw, (size_t)s->length * nl * qm * sizeof(int16_t)) : -1;
    out_r = k == 0 ? r : out_r == r ? r : -1;
    maxlen = s->length > maxlen ? s->length : maxlen;
    total += s->length;
  }
  for (int k = 0; k < nsym; k++)
    d[k].out = out_r >= 0 ? ((const char *)sym[k].llr_cw - g.region[out_r].h) / sizeof(int16_t) : k * g.llr_sz;
  const cl_mem out = out_r >= 0 ? g.region[out_r].b : ctx->out;
  unmap_staging(ctx);
  cl_command_queue q = ctx->q;
  int ret = clEnqueueWriteBuffer(q, ctx->sym, CL_FALSE, 0, nsym * sizeof(*d), d, 0, NULL, NULL) == CL_SUCCESS ? 0 : -1;
  if (nl == 1) {
    cl_kernel k = ctx->k1[qm / 2 - 1];
    const cl_mem a[6] = {ctx->sym, rxb, ctx->ch, ctx->map, ctx->scr, out};
    for (int i = 0; i < 6; i++)
      ret |= clSetKernelArg(k, i, sizeof(cl_mem), &a[i]);
    ret |= clSetKernelArg(k, 6, sizeof(int), &nb_rx);
    ret |= clSetKernelArg(k, 7, sizeof(int), &p->output_shift);
    const size_t lws[2] = {64, 1}, gws[2] = {(maxlen + 63) / 64 * 64, (size_t)nsym};
    ret |= clEnqueueNDRangeKernel(q, k, 2, NULL, gws, lws, 0, NULL, NULL);
  }
  const int two_pass = nl == 2 && p->detector == NR_IRX_DET_LBEST;
  for (int pass = 0; pass < (nl == 2) + two_pass; pass++) {
    const int kind = pass ? K_P2 : two_pass ? K_P1 : K_ML;
    cl_kernel k = ctx->k[nb][kind];
    ret |= clSetKernelArg(k, A_SYM, sizeof(cl_mem), &ctx->sym);
    ret |= clSetKernelArg(k, A_RX, sizeof(cl_mem), &rxb);
    ret |= clSetKernelArg(k, A_CH, sizeof(cl_mem), &ctx->ch);
    ret |= clSetKernelArg(k, A_MAP, sizeof(cl_mem), &ctx->map);
    ret |= clSetKernelArg(k, A_SCR, sizeof(cl_mem), &ctx->scr);
    ret |= clSetKernelArg(k, A_OUT, sizeof(cl_mem), &out);
    ret |= clSetKernelArg(k, A_NB_RX, sizeof(int), &nb_rx);
    ret |= clSetKernelArg(k, A_LAMBDA, sizeof(float), &p->lambda);
    ret |= clSetKernelArg(k, A_SHIFT, sizeof(int), &p->output_shift);
    if (two_pass) {
      ret |= clSetKernelArg(k, A_WIDE, sizeof(cl_mem), &ctx->wide);
      ret |= clSetKernelArg(k, A_NWIDE, sizeof(cl_mem), &ctx->nwide);
    }
    if (kind == K_P1) {
      const cl_uint zero = 0;
      ret |= clEnqueueFillBuffer(q, ctx->nwide, &zero, sizeof(zero), 0, sizeof(zero), 0, NULL, NULL);
    }
    if (kind == K_P2) { // the maximum count: no host round trip
      const size_t g2 = (2 * (size_t)total + 63) / 64 * 64, l2 = 64;
      ret |= clEnqueueNDRangeKernel(q, k, 1, NULL, &g2, &l2, 0, NULL, NULL);
    } else {
      const size_t lws[2] = {64, 1}, gws[2] = {(maxlen + 63) / 64 * 64, (size_t)nsym * (g.split[nb][kind] ? 2 : 1)};
      ret |= clEnqueueNDRangeKernel(q, k, 2, NULL, gws, lws, 0, NULL, NULL);
    }
  }
  if (out_r >= 0) { // the LLRs are in the codeword buffer once the kernels are done
    ret |= clFinish(q);
  } else {
    cl_int e;
    const int16_t *o =
        clEnqueueMapBuffer(q, ctx->out, CL_TRUE, CL_MAP_READ, 0, nsym * g.llr_sz * sizeof(int16_t), 0, NULL, NULL, &e);
    if (e != CL_SUCCESS) {
      map_staging(ctx);
      return -1;
    }
    for (int s = 0; s < nsym && ret == 0; s++)
      memcpy(sym[s].llr_cw, o + s * g.llr_sz, (size_t)sym[s].length * nl * qm * sizeof(int16_t));
    clEnqueueUnmapMemObject(q, ctx->out, (void *)o, 0, NULL, NULL);
  }
  // the staging mapped again: the caller may run another batch before release
  return map_staging(ctx) != 0 || ret != 0 ? -1 : 0;
}
