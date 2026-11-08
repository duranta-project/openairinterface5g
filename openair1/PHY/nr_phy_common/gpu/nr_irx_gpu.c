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
  c16_t *rx, *ch; // staging of NR_IRX_MAX_SYM symbols, host side
  int16_t *scr, *out;
  c16_t *rx_d, *ch_d; // device aliases
  int16_t *scr_d, *out_d;
  nr_gpu_det_ws_t *ws;
  bool busy;
};

static struct {
  nr_irx_ctx_t *ctx;
  int n;
  int max_nb_rx;
  uint32_t stride; // REs per staging row
  size_t rx_sz, ch_sz, llr_sz; // elements per symbol
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
    void *h[4] = {x->rx, x->ch, x->scr, x->out};
    for (int k = 0; k < 4; k++)
      if (h[k])
        (void)gpuFreeHost(h[k]);
    nr_gpu_det_ws_free(x->ws);
    if (x->st)
      (void)gpuStreamDestroy(x->st);
  }
  free(g.ctx);
  g.ctx = NULL;
  g.n = 0;
  return 0;
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
  g.ctx = calloc(nb_ctx, sizeof(*g.ctx));
  if (!g.ctx)
    return -1;
  g.n = nb_ctx;
  for (int c = 0; c < nb_ctx; c++) {
    nr_irx_ctx_t *x = &g.ctx[c];
    x->rx = mapped_alloc(NR_IRX_MAX_SYM * g.rx_sz * sizeof(c16_t), (void **)&x->rx_d);
    x->ch = mapped_alloc(NR_IRX_MAX_SYM * g.ch_sz * sizeof(c16_t), (void **)&x->ch_d);
    x->scr = mapped_alloc(NR_IRX_MAX_SYM * g.llr_sz * sizeof(int16_t), (void **)&x->scr_d);
    x->out = mapped_alloc(NR_IRX_MAX_SYM * g.llr_sz * sizeof(int16_t), (void **)&x->out_d);
    x->ws = nr_gpu_det_ws_alloc(2 * NR_IRX_MAX_SYM * g.stride);
    if (!x->rx || !x->ch || !x->scr || !x->out || !x->ws || gpuStreamCreateWithFlags(&x->st, gpuStreamNonBlocking) != gpuSuccess) {
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
  if (k < 0 || k >= NR_IRX_MAX_SYM)
    return NULL;
  switch (which) {
    case NR_IRX_STAGING_RX:
      return ctx->rx + k * g.rx_sz;
    case NR_IRX_STAGING_CH:
      return ctx->ch + k * g.ch_sz;
    case NR_IRX_STAGING_SCRAMBLE:
      return ctx->scr + k * g.llr_sz;
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
    if (s->stride > g.stride || s->length > s->stride)
      return -1;
    d[k] = *s;
    d[k].rxF = dev_alias(s->rxF, ctx->rx, ctx->rx_d, NR_IRX_MAX_SYM * g.rx_sz * sizeof(c16_t));
    d[k].chF = dev_alias(s->chF, ctx->ch, ctx->ch_d, NR_IRX_MAX_SYM * g.ch_sz * sizeof(c16_t));
    d[k].scramble = s->scramble ? dev_alias(s->scramble, ctx->scr, ctx->scr_d, NR_IRX_MAX_SYM * g.llr_sz * sizeof(int16_t)) : NULL;
    if (!d[k].rxF || !d[k].chF || (s->scramble && !d[k].scramble))
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
