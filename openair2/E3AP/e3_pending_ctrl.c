/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "e3_pending_ctrl.h"

#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>

/* Must stay under FlexRIC's PENDING_CTRL_TIMEOUT_NS so this side gives up first. */
#define E3_PENDING_CTRL_TIMEOUT_NS (150 * 1000 * 1000)

typedef enum {
  SLOT_FREE = 0,
  SLOT_OPEN, /* forwarded to the dApp, nothing back yet */
  SLOT_INSTALLED, /* mask in the MAC, waiting for a tick */
  SLOT_DONE, /* on air; the relay may take it */
} slot_state_e;

typedef struct {
  _Atomic int state;
  uint32_t sequence_id;
  int64_t opened_ts_ns;
  int64_t installed_ts_ns;
  _Atomic int64_t on_air_ts_ns;
  _Atomic uint32_t sfn_slot; /* sfn << 16 | slot, published with on_air_ts_ns */
  bool ok;
} slot_t;

static slot_t g_slot[E3_PENDING_CTRL_MAX];
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;

static int64_t now_realtime_ns(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  return (int64_t)ts.tv_sec * 1000000000 + ts.tv_nsec;
}

void e3_pending_ctrl_init(void)
{
  pthread_mutex_lock(&g_mtx);
  memset(g_slot, 0, sizeof(g_slot));
  pthread_mutex_unlock(&g_mtx);
}

bool e3_pending_ctrl_open(uint32_t sequence_id)
{
  if (sequence_id == 0)
    return false;

  bool opened = false;
  pthread_mutex_lock(&g_mtx);
  for (size_t i = 0; i < E3_PENDING_CTRL_MAX; ++i) {
    if (atomic_load_explicit(&g_slot[i].state, memory_order_relaxed) != SLOT_FREE)
      continue;
    g_slot[i].sequence_id = sequence_id;
    g_slot[i].opened_ts_ns = now_realtime_ns();
    g_slot[i].installed_ts_ns = 0;
    atomic_store_explicit(&g_slot[i].on_air_ts_ns, 0, memory_order_relaxed);
    atomic_store_explicit(&g_slot[i].sfn_slot, 0, memory_order_relaxed);
    g_slot[i].ok = false;
    atomic_store_explicit(&g_slot[i].state, SLOT_OPEN, memory_order_release);
    opened = true;
    break;
  }
  pthread_mutex_unlock(&g_mtx);
  return opened;
}

void e3_pending_ctrl_installed(uint32_t sequence_id, int64_t installed_ts_ns, bool ok)
{
  if (sequence_id == 0)
    return;

  pthread_mutex_lock(&g_mtx);
  for (size_t i = 0; i < E3_PENDING_CTRL_MAX; ++i) {
    if (atomic_load_explicit(&g_slot[i].state, memory_order_relaxed) != SLOT_OPEN)
      continue;
    if (g_slot[i].sequence_id != sequence_id)
      continue;
    g_slot[i].installed_ts_ns = installed_ts_ns;
    g_slot[i].ok = ok;
    /* A failed install never reaches the air: finish it here. */
    atomic_store_explicit(&g_slot[i].state, ok ? SLOT_INSTALLED : SLOT_DONE, memory_order_release);
    break;
  }
  pthread_mutex_unlock(&g_mtx);
}

void e3_pending_ctrl_on_air(uint32_t sequence_id, int64_t on_air_ts_ns, uint16_t sfn, uint16_t slot)
{
  if (sequence_id == 0)
    return;

  /* MAC thread, slot deadline: lock-free. The relay freeing a slot cannot resurrect this id. */
  for (size_t i = 0; i < E3_PENDING_CTRL_MAX; ++i) {
    if (atomic_load_explicit(&g_slot[i].state, memory_order_acquire) != SLOT_INSTALLED)
      continue;
    if (g_slot[i].sequence_id != sequence_id)
      continue;
    atomic_store_explicit(&g_slot[i].sfn_slot, ((uint32_t)sfn << 16) | slot, memory_order_relaxed);
    atomic_store_explicit(&g_slot[i].on_air_ts_ns, on_air_ts_ns, memory_order_relaxed);
    /* Release last: the relay reads state first. */
    atomic_store_explicit(&g_slot[i].state, SLOT_DONE, memory_order_release);
    return;
  }
}

void e3_pending_ctrl_superseded(uint32_t sequence_id)
{
  if (sequence_id == 0)
    return;

  /* MAC thread, lock-free like e3_pending_ctrl_on_air(). Only an installed control can be superseded. */
  for (size_t i = 0; i < E3_PENDING_CTRL_MAX; ++i) {
    if (atomic_load_explicit(&g_slot[i].state, memory_order_acquire) != SLOT_INSTALLED)
      continue;
    if (g_slot[i].sequence_id != sequence_id)
      continue;
    /* on_air_ts_ns stays 0: this control never reached the air. */
    atomic_store_explicit(&g_slot[i].state, SLOT_DONE, memory_order_release);
    return;
  }
}

static void fill_outcome(e3_ctrl_outcome_t* dst, const slot_t* src)
{
  const uint32_t sfn_slot = atomic_load_explicit(&src->sfn_slot, memory_order_relaxed);
  dst->sequence_id = src->sequence_id;
  dst->installed_ts_ns = src->installed_ts_ns;
  dst->on_air_ts_ns = atomic_load_explicit(&src->on_air_ts_ns, memory_order_relaxed);
  dst->sfn = (uint16_t)(sfn_slot >> 16);
  dst->slot = (uint16_t)(sfn_slot & 0xFFFF);
  dst->ok = src->ok;
}

size_t e3_pending_ctrl_drain(e3_ctrl_outcome_t* out, size_t max)
{
  size_t n = 0;
  pthread_mutex_lock(&g_mtx);
  for (size_t i = 0; i < E3_PENDING_CTRL_MAX && n < max; ++i) {
    if (atomic_load_explicit(&g_slot[i].state, memory_order_acquire) != SLOT_DONE)
      continue;
    fill_outcome(&out[n++], &g_slot[i]);
    atomic_store_explicit(&g_slot[i].state, SLOT_FREE, memory_order_release);
  }
  pthread_mutex_unlock(&g_mtx);
  return n;
}

size_t e3_pending_ctrl_sweep(e3_ctrl_outcome_t* out, size_t max)
{
  const int64_t now = now_realtime_ns();
  size_t n = 0;

  pthread_mutex_lock(&g_mtx);
  for (size_t i = 0; i < E3_PENDING_CTRL_MAX && n < max; ++i) {
    const int st = atomic_load_explicit(&g_slot[i].state, memory_order_relaxed);
    if (st != SLOT_OPEN && st != SLOT_INSTALLED)
      continue;
    if (now - g_slot[i].opened_ts_ns < E3_PENDING_CTRL_TIMEOUT_NS)
      continue;
    /* Timestamps already recorded are kept. */
    fill_outcome(&out[n], &g_slot[i]);
    out[n].ok = false;
    n++;
    atomic_store_explicit(&g_slot[i].state, SLOT_FREE, memory_order_release);
  }
  pthread_mutex_unlock(&g_mtx);
  return n;
}

/* No-op for targets linking e3ap without e3ap_ctrl_ack (the CU-UP); nr-softmodem's definition wins. */
__attribute__((weak)) void start_dapp_ctrl_ack_relay(void)
{
}
