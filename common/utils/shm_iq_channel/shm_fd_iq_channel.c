/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "shm_fd_iq_channel.h"
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "assertions.h"

#define SHM_FD_MAGIC_NUMBER 0x46444951 // "FDIQ"
#define ALIGN64(x) (((x) + 63) & ~(uint64_t)63)
#define RE_ALIGN 16

typedef struct {
  uint64_t tag; ///< symbol + 1, 0 while free or being filled
  uint64_t len;
  uint8_t pad[48];
} ShmFDIQMsgHdr;

typedef struct {
  uint32_t magic; ///< set last by the server
  uint32_t stopped;
  uint64_t size;
  ShmFDIQGeometry geo;
  uint32_t stride_dl;
  uint32_t stride_ul;
  uint64_t dl_off; ///< [ring_symbols][nb_ant_dl][stride_dl]
  uint64_t ul_off; ///< [ring_symbols][nb_ant_ul][stride_ul]
  uint64_t msg_off; ///< [nb_msg] (ShmFDIQMsgHdr + msg_max_size)
  uint64_t msg_entry_size;
  uint64_t current_symbol;
  pthread_mutex_t mutex; ///< current_symbol changes, stopped, message claims
  pthread_cond_t cond;
} ShmFDIQChannelData;

struct ShmFDIQChannel_s {
  IQChannelType type;
  ShmFDIQChannelData *data;
  size_t size;
  char name[256];
  ino_t ino; ///< server: unlink the name only while it is still this object's
  c16_t *tx_data;
  c16_t *rx_data;
  int nb_tx;
  int nb_rx;
  int n_re_tx;
  int n_re_rx;
  int stride_tx;
  int stride_rx;
  bool abort;
};

static void set_views(ShmFDIQChannel *ch)
{
  ShmFDIQChannelData *d = ch->data;
  c16_t *dl = (c16_t *)((uint8_t *)d + d->dl_off), *ul = (c16_t *)((uint8_t *)d + d->ul_off);
  const bool server = ch->type == IQ_CHANNEL_TYPE_SERVER;
  ch->tx_data = server ? dl : ul;
  ch->rx_data = server ? ul : dl;
  ch->nb_tx = server ? d->geo.nb_ant_dl : d->geo.nb_ant_ul;
  ch->nb_rx = server ? d->geo.nb_ant_ul : d->geo.nb_ant_dl;
  ch->n_re_tx = server ? d->geo.n_re_dl : d->geo.n_re_ul;
  ch->n_re_rx = server ? d->geo.n_re_ul : d->geo.n_re_dl;
  ch->stride_tx = server ? d->stride_dl : d->stride_ul;
  ch->stride_rx = server ? d->stride_ul : d->stride_dl;
}

static uint64_t current_symbol(const ShmFDIQChannelData *d)
{
  return __atomic_load_n(&d->current_symbol, __ATOMIC_ACQUIRE);
}

static IQChannelErrorType tx_window(const ShmFDIQChannel *ch, uint64_t symbol)
{
  const uint64_t now = current_symbol(ch->data);
  if (symbol < now)
    return CHANNEL_ERROR_TOO_LATE;
  if (symbol >= now + ch->data->geo.ring_symbols / 2)
    return CHANNEL_ERROR_TOO_EARLY;
  return CHANNEL_NO_ERROR;
}

static IQChannelErrorType rx_window(const ShmFDIQChannel *ch, uint64_t symbol)
{
  const uint64_t now = current_symbol(ch->data);
  if (symbol >= now)
    return CHANNEL_ERROR_TOO_EARLY;
  if (symbol + ch->data->geo.ring_symbols / 2 < now)
    return CHANNEL_ERROR_TOO_LATE;
  return CHANNEL_NO_ERROR;
}

static ShmFDIQMsgHdr *msg_entry(const ShmFDIQChannel *ch, int i)
{
  const ShmFDIQChannelData *d = ch->data;
  return (ShmFDIQMsgHdr *)((uint8_t *)d + d->msg_off + (uint64_t)i * d->msg_entry_size);
}

static void init_pshared(ShmFDIQChannelData *d)
{
  pthread_mutexattr_t mattr;
  pthread_condattr_t cattr;
  AssertFatal(pthread_mutexattr_init(&mattr) == 0, "pthread_mutexattr_init() failed\n");
  AssertFatal(pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED) == 0, "pthread_mutexattr_setpshared() failed\n");
  AssertFatal(pthread_mutex_init(&d->mutex, &mattr) == 0, "pthread_mutex_init() failed\n");
  pthread_mutexattr_destroy(&mattr);
  AssertFatal(pthread_condattr_init(&cattr) == 0, "pthread_condattr_init() failed\n");
  AssertFatal(pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED) == 0, "pthread_condattr_setpshared() failed\n");
  AssertFatal(pthread_cond_init(&d->cond, &cattr) == 0, "pthread_cond_init() failed\n");
  pthread_condattr_destroy(&cattr);
}

ShmFDIQChannel *shm_fd_iq_channel_create(const char *name, const ShmFDIQGeometry *geo, uint64_t first_symbol)
{
  AssertFatal(geo->ring_symbols >= 2 && geo->ring_symbols % 2 == 0, "ring of %u symbols: even, at least 2\n", geo->ring_symbols);
  AssertFatal(geo->n_re_dl > 0 && geo->n_re_ul > 0 && geo->nb_ant_dl > 0 && geo->nb_ant_ul > 0,
              "geometry %u/%u REs, %u/%u antennas\n",
              geo->n_re_dl,
              geo->n_re_ul,
              geo->nb_ant_dl,
              geo->nb_ant_ul);
  const uint32_t stride_dl = (geo->n_re_dl + RE_ALIGN - 1) / RE_ALIGN * RE_ALIGN;
  const uint32_t stride_ul = (geo->n_re_ul + RE_ALIGN - 1) / RE_ALIGN * RE_ALIGN;
  const uint64_t ring = geo->ring_symbols;
  uint64_t off = ALIGN64(sizeof(ShmFDIQChannelData));
  const uint64_t dl_off = off;
  off = ALIGN64(off + ring * geo->nb_ant_dl * stride_dl * sizeof(c16_t));
  const uint64_t ul_off = off;
  off = ALIGN64(off + ring * geo->nb_ant_ul * stride_ul * sizeof(c16_t));
  const uint64_t msg_off = off;
  const uint64_t msg_entry_size = ALIGN64(sizeof(ShmFDIQMsgHdr) + geo->msg_max_size);
  const uint64_t size = off + geo->nb_msg * msg_entry_size;

  shm_unlink(name);
  int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, S_IRUSR | S_IWUSR);
  AssertFatal(fd != -1, "shm_open(%s) failed: %s\n", name, strerror(errno));
  AssertFatal(ftruncate(fd, size) == 0, "ftruncate failed: %s\n", strerror(errno));
  ShmFDIQChannelData *d = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  AssertFatal(d != MAP_FAILED, "mmap failed: %s\n", strerror(errno));
  struct stat st;
  AssertFatal(fstat(fd, &st) == 0, "fstat failed: %s\n", strerror(errno));
  close(fd);

  memset(d, 0, size);
  d->size = size;
  d->geo = *geo;
  d->stride_dl = stride_dl;
  d->stride_ul = stride_ul;
  d->dl_off = dl_off;
  d->ul_off = ul_off;
  d->msg_off = msg_off;
  d->msg_entry_size = msg_entry_size;
  d->current_symbol = first_symbol;
  init_pshared(d);

  ShmFDIQChannel *ch = calloc(1, sizeof(*ch));
  AssertFatal(ch, "OOM\n");
  ch->type = IQ_CHANNEL_TYPE_SERVER;
  ch->data = d;
  ch->size = size;
  ch->ino = st.st_ino;
  strncpy(ch->name, name, sizeof(ch->name) - 1);
  set_views(ch);
  __atomic_store_n(&d->magic, SHM_FD_MAGIC_NUMBER, __ATOMIC_RELEASE);
  return ch;
}

static uint64_t now_us(void)
{
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

// The initialized object behind name, NULL if none (yet).
static ShmFDIQChannelData *map_initialized(const char *name, size_t *size)
{
  int fd = shm_open(name, O_RDWR, 0);
  if (fd == -1)
    return NULL;
  struct stat st;
  ShmFDIQChannelData *d = NULL;
  if (fstat(fd, &st) == 0 && (size_t)st.st_size >= sizeof(ShmFDIQChannelData)) {
    d = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (d == MAP_FAILED) {
      d = NULL;
    } else if (__atomic_load_n(&d->magic, __ATOMIC_ACQUIRE) != SHM_FD_MAGIC_NUMBER || d->size != (uint64_t)st.st_size) {
      munmap(d, st.st_size);
      d = NULL;
    } else {
      *size = st.st_size;
    }
  }
  close(fd);
  return d;
}

ShmFDIQChannel *shm_fd_iq_channel_connect(const char *name, int timeout_ms)
{
  const uint64_t deadline = now_us() + (uint64_t)timeout_ms * 1000;
  while (now_us() < deadline) {
    size_t size;
    ShmFDIQChannelData *d = map_initialized(name, &size);
    if (!d) {
      usleep(10000);
      continue;
    }
    // A crashed server leaves its object initialized: only a moving current symbol is live.
    const uint64_t first = current_symbol(d);
    const uint64_t live_deadline = now_us() + 1000000;
    bool live = false;
    while (!live && !__atomic_load_n(&d->stopped, __ATOMIC_ACQUIRE) && now_us() < live_deadline && now_us() < deadline) {
      live = current_symbol(d) != first;
      if (!live)
        usleep(1000);
    }
    if (!live) {
      munmap(d, size);
      continue;
    }
    ShmFDIQChannel *ch = calloc(1, sizeof(*ch));
    AssertFatal(ch, "OOM\n");
    ch->type = IQ_CHANNEL_TYPE_CLIENT;
    ch->data = d;
    ch->size = size;
    strncpy(ch->name, name, sizeof(ch->name) - 1);
    set_views(ch);
    return ch;
  }
  return NULL;
}

void shm_fd_iq_channel_destroy(ShmFDIQChannel *ch)
{
  if (!ch)
    return;
  ShmFDIQChannelData *d = ch->data;
  if (ch->type == IQ_CHANNEL_TYPE_SERVER) {
    pthread_mutex_lock(&d->mutex);
    __atomic_store_n(&d->stopped, 1, __ATOMIC_RELEASE);
    pthread_cond_broadcast(&d->cond);
    pthread_mutex_unlock(&d->mutex);
    __atomic_store_n(&d->magic, 0, __ATOMIC_RELEASE);
    int fd = shm_open(ch->name, O_RDONLY, 0);
    if (fd != -1) {
      struct stat st;
      const bool own = fstat(fd, &st) == 0 && st.st_ino == ch->ino;
      close(fd);
      if (own)
        shm_unlink(ch->name);
    }
  }
  munmap(d, ch->size);
  free(ch);
}

const ShmFDIQGeometry *shm_fd_iq_channel_geometry(const ShmFDIQChannel *ch)
{
  return &ch->data->geo;
}

static c16_t *stream(c16_t *ring, int nb_ant, int stride, uint32_t ring_symbols, uint64_t symbol, int ant)
{
  return ring + ((symbol % ring_symbols) * nb_ant + ant) * stride;
}

IQChannelErrorType shm_fd_iq_channel_tx(ShmFDIQChannel *ch, uint64_t symbol, int first_ant, int nb_ant, const c16_t *const *data)
{
  AssertFatal(first_ant >= 0 && first_ant + nb_ant <= ch->nb_tx, "TX streams %d..%d of %d\n", first_ant, first_ant + nb_ant - 1, ch->nb_tx);
  const IQChannelErrorType ret = tx_window(ch, symbol);
  if (ret != CHANNEL_NO_ERROR)
    return ret;
  for (int a = 0; a < nb_ant; a++)
    memcpy(stream(ch->tx_data, ch->nb_tx, ch->stride_tx, ch->data->geo.ring_symbols, symbol, first_ant + a),
           data[a],
           ch->n_re_tx * sizeof(c16_t));
  return CHANNEL_NO_ERROR;
}

IQChannelErrorType shm_fd_iq_channel_rx(ShmFDIQChannel *ch, uint64_t symbol, int first_ant, int nb_ant, c16_t *const *dst)
{
  AssertFatal(first_ant >= 0 && first_ant + nb_ant <= ch->nb_rx, "RX streams %d..%d of %d\n", first_ant, first_ant + nb_ant - 1, ch->nb_rx);
  const IQChannelErrorType ret = rx_window(ch, symbol);
  if (ret != CHANNEL_NO_ERROR)
    return ret;
  for (int a = 0; a < nb_ant; a++) {
    c16_t *src = stream(ch->rx_data, ch->nb_rx, ch->stride_rx, ch->data->geo.ring_symbols, symbol, first_ant + a);
    memcpy(dst[a], src, ch->n_re_rx * sizeof(c16_t));
    memset(src, 0, ch->n_re_rx * sizeof(c16_t));
  }
  return CHANNEL_NO_ERROR;
}

IQChannelErrorType shm_fd_iq_channel_tx_msg(ShmFDIQChannel *ch, uint64_t symbol, const void *msg, size_t len)
{
  AssertFatal(ch->type == IQ_CHANNEL_TYPE_CLIENT, "only clients post messages\n");
  ShmFDIQChannelData *d = ch->data;
  AssertFatal(len <= d->geo.msg_max_size, "message of %zu bytes, at most %u\n", len, d->geo.msg_max_size);
  const IQChannelErrorType ret = tx_window(ch, symbol);
  if (ret != CHANNEL_NO_ERROR)
    return ret;
  // An entry older than the server's rx window is free
  const uint64_t now = current_symbol(d);
  const uint64_t half = d->geo.ring_symbols / 2;
  pthread_mutex_lock(&d->mutex);
  ShmFDIQMsgHdr *e = NULL;
  for (int i = 0; i < (int)d->geo.nb_msg && !e; i++) {
    ShmFDIQMsgHdr *c = msg_entry(ch, i);
    const uint64_t tag = __atomic_load_n(&c->tag, __ATOMIC_RELAXED);
    if (tag == 0 || tag - 1 + half < now)
      e = c;
  }
  if (e) {
    __atomic_store_n(&e->tag, 0, __ATOMIC_RELAXED);
    e->len = len;
    memcpy(e + 1, msg, len);
    __atomic_store_n(&e->tag, symbol + 1, __ATOMIC_RELEASE);
  }
  pthread_mutex_unlock(&d->mutex);
  return e ? CHANNEL_NO_ERROR : CHANNEL_ERROR_NO_DATA;
}

IQChannelErrorType shm_fd_iq_channel_rx_msg(const ShmFDIQChannel *ch, uint64_t symbol, int entry, void *msg, size_t max_len, size_t *len)
{
  AssertFatal(ch->type == IQ_CHANNEL_TYPE_SERVER, "only the server reads messages\n");
  AssertFatal(entry >= 0 && entry < (int)ch->data->geo.nb_msg, "message entry %d of %u\n", entry, ch->data->geo.nb_msg);
  const IQChannelErrorType ret = rx_window(ch, symbol);
  if (ret != CHANNEL_NO_ERROR)
    return ret;
  const ShmFDIQMsgHdr *e = msg_entry(ch, entry);
  if (__atomic_load_n(&e->tag, __ATOMIC_ACQUIRE) != symbol + 1 || e->len > max_len)
    return CHANNEL_ERROR_NO_DATA;
  *len = e->len;
  memcpy(msg, e + 1, e->len);
  return CHANNEL_NO_ERROR;
}

void shm_fd_iq_channel_produce(ShmFDIQChannel *ch, uint64_t num_symbols)
{
  ShmFDIQChannelData *d = ch->data;
  pthread_mutex_lock(&d->mutex);
  __atomic_store_n(&d->current_symbol, d->current_symbol + num_symbols, __ATOMIC_RELEASE);
  pthread_cond_broadcast(&d->cond);
  pthread_mutex_unlock(&d->mutex);
}

uint64_t shm_fd_iq_channel_get_current_symbol(const ShmFDIQChannel *ch)
{
  return current_symbol(ch->data);
}

bool shm_fd_iq_channel_is_stopped(const ShmFDIQChannel *ch)
{
  return __atomic_load_n(&ch->abort, __ATOMIC_ACQUIRE) || __atomic_load_n(&ch->data->stopped, __ATOMIC_ACQUIRE);
}

int shm_fd_iq_channel_wait(ShmFDIQChannel *ch, uint64_t symbol, uint64_t timeout_us)
{
  ShmFDIQChannelData *d = ch->data;
  if (current_symbol(d) >= symbol)
    return 0;
  struct timespec ts;
  if (timeout_us) {
    clock_gettime(CLOCK_REALTIME, &ts);
    const uint64_t ns = ts.tv_nsec + (timeout_us % 1000000) * 1000;
    ts.tv_sec += timeout_us / 1000000 + ns / 1000000000;
    ts.tv_nsec = ns % 1000000000;
  }
  int ret = 0;
  pthread_mutex_lock(&d->mutex);
  while (d->current_symbol < symbol) {
    if (shm_fd_iq_channel_is_stopped(ch)) {
      ret = -1;
      break;
    }
    if (!timeout_us) {
      pthread_cond_wait(&d->cond, &d->mutex);
    } else if (pthread_cond_timedwait(&d->cond, &d->mutex, &ts) == ETIMEDOUT) {
      ret = d->current_symbol < symbol ? 1 : 0;
      break;
    }
  }
  pthread_mutex_unlock(&d->mutex);
  return ret;
}

void shm_fd_iq_channel_abort(ShmFDIQChannel *ch)
{
  ShmFDIQChannelData *d = ch->data;
  pthread_mutex_lock(&d->mutex);
  __atomic_store_n(&ch->abort, true, __ATOMIC_RELEASE);
  pthread_cond_broadcast(&d->cond);
  pthread_mutex_unlock(&d->mutex);
}
