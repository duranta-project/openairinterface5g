/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Inner-RX offload module for CUDA/HIP GPUs (libinner_rx_gpu.so, nr_inner_rx_offload.h) around
// nr_gpu_inner_rx_2layer. Staging is mapped pinned host memory (coarse-grained on HIP): zero-copy on integrated and
// coherent GPUs (Ryzen AI, GH200, GB10), read over the bus elsewhere. Device buffers with copies were ~5x slower on the
// Radeon 8060S (its copy engines), managed memory ~2x. The codeword LLRs go straight to the caller's buffer when it is
// mapped pinned memory (ulsch_llrs of an LDPC_CUDA build), else through staging and a copy.

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "PHY/gpu_compat.h"
#include "nr_inner_rx_offload.h"
#include "nr_detector_gpu.h"

struct nr_irx_ctx {
  gpuStream_t st;
  c16_t *rx, *ch; // staging: RX of NR_IRX_MAX_SYM symbols, NR_IRX_MAX_CH channel estimates (host side)
  uint16_t *map; // RE maps of NR_IRX_MAX_SYM symbols
  uint32_t *scr; // packed descrambling bits of the batch
  int16_t *out; // LLR output staging of NR_IRX_MAX_SYM symbols
  c16_t *rx_d, *ch_d; // device aliases
  uint16_t *map_d;
  uint32_t *scr_d;
  int16_t *out_d;
  nr_gpu_det_ws_t *ws;
  bool busy;
};

#define NRG_MAX_REGIONS 32

static struct {
  struct {
    const char *h;
    size_t bytes;
    char *d;
  } region[NRG_MAX_REGIONS]; // host memory registered with nr_irx_register_host (rxdataF) and its device alias
  int nb_region;
  nr_irx_ctx_t *ctx;
  int n;
  int max_nb_rx;
  uint32_t stride; // REs per staging row
  size_t rx_sz, ch_sz, llr_sz, scr_sz; // elements per symbol (RX, LLRs), per estimate (CH), per batch (SCRAMBLE words)
  pthread_mutex_t lock;
} g = {.lock = PTHREAD_MUTEX_INITIALIZER};

static void *mapped_alloc(size_t bytes, void **dev)
{
  void *h = NULL;
#if defined(GPU_USE_HIP)
  // coarse-grained: the GPU caches it, coherent at kernel boundaries (the staging is written before the launch and
  // the output read after the synchronization). On the Radeon 8060S the fine-grained default makes the inner-RX
  // kernel ~1.5x slower and its launch ~3x.
  if (hipHostMalloc(&h, bytes, hipHostMallocMapped | hipHostMallocNonCoherent) != hipSuccess)
    return NULL;
#else
  if (gpuHostAlloc(&h, bytes, gpuHostAllocMapped) != gpuSuccess)
    return NULL;
#endif
  if (gpuHostGetDevicePointer(dev, h, 0) != gpuSuccess) {
    (void)gpuFreeHost(h);
    return NULL;
  }
  memset(h, 0, bytes);
  return h;
}

int nr_irx_shutdown(void)
{
  for (int c = 0; c < g.n; c++) {
    nr_irx_ctx_t *x = &g.ctx[c];
    void *h[5] = {x->rx, x->ch, x->map, x->scr, x->out};
    for (int k = 0; k < 5; k++)
      if (h[k])
        (void)gpuFreeHost(h[k]);
    nr_gpu_det_ws_free(x->ws);
    if (x->st)
      (void)gpuStreamDestroy(x->st);
  }
  free(g.ctx);
  g.ctx = NULL;
  g.n = 0;
  for (int r = 0; r < g.nb_region; r++)
    (void)gpuHostUnregister((void *)g.region[r].h);
  g.nb_region = 0;
  return 0;
}

int nr_irx_register_host(const void *ptr, size_t bytes)
{
#if defined(GPU_USE_HIP)
  // On the Radeon 8060S the kernel reads registered pageable memory ~10x slower than the hipHostMalloc staging, whatever
  // the registration flags: the RX goes through the staging
  (void)ptr;
  (void)bytes;
  return 1;
#endif
  int ret = 0;
  pthread_mutex_lock(&g.lock);
  int r = 0;
  while (r < g.nb_region && g.region[r].h != ptr)
    r++;
  if (r == g.nb_region) {
    void *d = NULL;
    if (r == NRG_MAX_REGIONS || gpuHostRegister((void *)ptr, bytes, gpuHostRegisterMapped) != gpuSuccess)
      ret = -1;
    else if (gpuHostGetDevicePointer(&d, (void *)ptr, 0) != gpuSuccess) {
      (void)gpuHostUnregister((void *)ptr);
      ret = -1;
    } else {
      g.region[r].h = ptr;
      g.region[r].bytes = bytes;
      g.region[r].d = d;
      g.nb_region++;
    }
  } else if (g.region[r].bytes < bytes) {
    ret = -1; // registered smaller
  }
  pthread_mutex_unlock(&g.lock);
  return ret;
}

// device alias of a pointer into registered host memory, NULL if outside
static const void *region_alias(const void *h)
{
  const void *d = NULL;
  pthread_mutex_lock(&g.lock);
  for (int r = 0; r < g.nb_region && !d; r++)
    if ((const char *)h >= g.region[r].h && (const char *)h < g.region[r].h + g.region[r].bytes)
      d = g.region[r].d + ((const char *)h - g.region[r].h);
  pthread_mutex_unlock(&g.lock);
  return d;
}

int nr_irx_init(uint32_t max_re_per_symbol, int max_nb_rx, int nb_ctx)
{
  if (g.ctx || nb_ctx < 1 || max_nb_rx < 1 || max_re_per_symbol == 0 || max_re_per_symbol >= 65536)
    return -1;
  g.max_nb_rx = max_nb_rx;
  g.stride = (max_re_per_symbol + 15) & ~15u;
  g.rx_sz = (size_t)max_nb_rx * g.stride;
  g.ch_sz = 2 * g.rx_sz;
  g.llr_sz = (size_t)g.stride * 2 * 8;
  g.scr_sz = NR_IRX_MAX_SYM * g.llr_sz / 32 + 1;
  g.ctx = calloc(nb_ctx, sizeof(*g.ctx));
  if (!g.ctx)
    return -1;
  g.n = nb_ctx;
  for (int c = 0; c < nb_ctx; c++) {
    nr_irx_ctx_t *x = &g.ctx[c];
    x->rx = mapped_alloc(NR_IRX_MAX_SYM * g.rx_sz * sizeof(c16_t), (void **)&x->rx_d);
    x->ch = mapped_alloc(NR_IRX_MAX_CH * g.ch_sz * sizeof(c16_t), (void **)&x->ch_d);
    x->map = mapped_alloc(NR_IRX_MAX_SYM * g.stride * sizeof(uint16_t), (void **)&x->map_d);
    x->scr = mapped_alloc(g.scr_sz * sizeof(uint32_t), (void **)&x->scr_d);
    x->out = mapped_alloc(NR_IRX_MAX_SYM * g.llr_sz * sizeof(int16_t), (void **)&x->out_d);
    x->ws = nr_gpu_det_ws_alloc(2 * NR_IRX_MAX_SYM * g.stride);
    if (!x->rx || !x->ch || !x->map || !x->scr || !x->out || !x->ws || gpuStreamCreateWithFlags(&x->st, gpuStreamNonBlocking) != gpuSuccess) {
      nr_irx_shutdown();
      return -1;
    }
  }
  return 0;
}

bool nr_irx_supported(int nb_layers, int nb_rx, int mod_order, int detector)
{
  return nb_layers == 2 && nb_rx >= 1 && nb_rx <= g.max_nb_rx
         && (mod_order == 2 || mod_order == 4 || mod_order == 6 || mod_order == 8)
         && (detector == NR_IRX_DET_MMSE || detector == NR_IRX_DET_LBEST || detector == NR_IRX_DET_ML);
}

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
  return x;
}

void nr_irx_release(nr_irx_ctx_t *ctx)
{
  pthread_mutex_lock(&g.lock);
  ctx->busy = false;
  pthread_mutex_unlock(&g.lock);
}

void *nr_irx_staging(nr_irx_ctx_t *ctx, int k, int which)
{
  switch (which) {
    case NR_IRX_STAGING_RX:
      return k >= 0 && k < NR_IRX_MAX_SYM ? ctx->rx + k * g.rx_sz : NULL;
    case NR_IRX_STAGING_CH:
      return k >= 0 && k < NR_IRX_MAX_CH ? ctx->ch + k * g.ch_sz : NULL;
    case NR_IRX_STAGING_REMAP:
      return k >= 0 && k < NR_IRX_MAX_SYM ? ctx->map + k * g.stride : NULL;
    case NR_IRX_STAGING_SCRAMBLE:
      return k == 0 ? ctx->scr : NULL;
    default:
      return NULL;
  }
}

// device alias of a pointer into one of the context's staging buffers
static const void *dev_alias(const void *h, const void *base_h, void *base_d, size_t bytes)
{
  const char *p = h, *b = base_h;
  return h && p >= b && p < b + bytes ? (const char *)base_d + (p - b) : NULL;
}

int nr_irx_run(nr_irx_ctx_t *ctx, const nr_irx_sym_t *sym, int nsym, const nr_irx_params_t *p)
{
  if (nsym < 1 || nsym > NR_IRX_MAX_SYM || !nr_irx_supported(p->nb_layers, p->nb_rx, p->mod_order, p->detector))
    return -1;
  nr_gpu_rx_sym_t d[NR_IRX_MAX_SYM];
  bool staged[NR_IRX_MAX_SYM];
  for (int k = 0; k < nsym; k++) {
    const nr_irx_sym_t *s = &sym[k];
    if (s->stride > g.stride || s->ch_stride > g.stride || s->length > s->stride
        || (s->scramble && s->scramble_offset + s->length * 2 * p->mod_order > g.scr_sz * 32))
      return -1;
    d[k] = *s;
    if (s->rx_ant[0]) { // RX gathered on the device from registered rxdataF
      if (p->nb_rx > NR_IRX_MAX_RX)
        return -1;
      for (int a = 0; a < p->nb_rx; a++)
        if (!(d[k].rx_ant[a] = region_alias(s->rx_ant[a])))
          return -1;
      d[k].rxF = NULL;
    } else if (!(d[k].rxF = dev_alias(s->rxF, ctx->rx, ctx->rx_d, NR_IRX_MAX_SYM * g.rx_sz * sizeof(c16_t)))) {
      return -1;
    }
    d[k].chF = dev_alias(s->chF, ctx->ch, ctx->ch_d, NR_IRX_MAX_CH * g.ch_sz * sizeof(c16_t));
    d[k].re_map = s->re_map ? dev_alias(s->re_map, ctx->map, ctx->map_d, NR_IRX_MAX_SYM * g.stride * sizeof(uint16_t)) : NULL;
    d[k].scramble = s->scramble ? dev_alias(s->scramble, ctx->scr, ctx->scr_d, g.scr_sz * sizeof(uint32_t)) : NULL;
    if (!d[k].chF || (s->re_map && !d[k].re_map) || (s->scramble && !d[k].scramble))
      return -1; // inputs must be in this context's staging
    // the codeword slice directly if it is mapped pinned memory, else this symbol's output staging
    void *cw = NULL;
    staged[k] = gpuHostGetDevicePointer(&cw, s->llr_cw, 0) != gpuSuccess || !cw;
    if (staged[k]) {
      (void)gpuGetLastError(); // clear the failed lookup, which the launch check would report
      cw = ctx->out_d + k * g.llr_sz;
    }
    d[k].llr_cw = cw;
  }
  int ret = nr_gpu_inner_rx_2layer(d, nsym, p->nb_rx, p->mod_order, p->detector, p->lambda, p->output_shift, ctx->ws, ctx->st);
  if (gpuStreamSynchronize(ctx->st) != gpuSuccess)
    ret = -1;
  for (int k = 0; k < nsym && ret == 0; k++)
    if (staged[k])
      memcpy(sym[k].llr_cw, ctx->out + k * g.llr_sz, (size_t)sym[k].length * 2 * p->mod_order * sizeof(int16_t));
  return ret;
}
