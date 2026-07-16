/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief common APIs for different RF frontend device
 */
#include <pthread.h>
#include <stdio.h>
#include <strings.h>
#include <dlfcn.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include "common_lib.h"
#include "assertions.h"
#include "common/utils/load_module_shlib.h"
#include "common/utils/LOG/log.h"
#include "executables/softmodem-common.h"
#include "common/config/config_paramdesc.h"
#include "common/config/config_userapi.h"
#include "common/cmake_defs.h"
#include "openair1/PHY/TOOLS/tools_defs.h"

#define MAX_GAP 100ULL
const char *const devtype_names[MAX_RF_DEV_TYPE] =
  {"", "USRP B200", "USRP X300", "USRP N300", "USRP X400", "BLADERF", "LMSSDR", "IRIS", "No HW", "UEDv2", "RFSIMULATOR"};

const char *get_devname(int devtype) {
  if (devtype < MAX_RF_DEV_TYPE && devtype !=MIN_RF_DEV_TYPE )
    return devtype_names[devtype];
  return "none";
}

static int set_device(openair0_device_t *device)
{
  char *dev_type = device->host_type == RAU_HOST ? "RAU" : "RRU";
  const char *devname = get_devname(device->type);
  if (strcmp(devname, "none") != 0) {
    LOG_I(HW, "[%s] has loaded %s device.\n", dev_type, devname);
    return 0;
  }
  LOG_E(HW, "[%s] invalid HW device.\n", dev_type);
  return -1;
}

static int set_transport(openair0_device_t *device)
{
  char *dev_type = device->host_type == RAU_HOST ? "RAU" : "RRU";
  switch (device->transp_type) {
  case ETHERNET_TP:
    LOG_I(HW, "[%s] has loaded ETHERNET trasport protocol.\n", dev_type);
    return 0;
    break;

  case NONE_TP:
    LOG_I(HW, "[%s] has not loaded a transport protocol.\n", dev_type);
    return 0;
    break;

  default:
    LOG_E(HW, "[%s] invalid transport protocol.\n", dev_type);
    return -1;
    break;
  }
}

typedef int (*devfunc_t)(openair0_device_t *, openair0_config_t *);

#define  DEVICE_SECTION   "device"
#define CONFIG_HLP_DEVICE "Identifies the oai device (the interface to RF) to use, the shared lib \"lib_<name>.so\" will be loaded"
/* look for the interface library and load it */
int load_lib(openair0_device_t *device, openair0_config_t *openair0_cfg, rau_type_t rau_type)
{
  openair0_cfg->command_line_sample_advance = get_softmodem_params()->command_line_sample_advance;
  openair0_cfg->recplay_mode = read_recplayconfig(&openair0_cfg->recplay_conf, &device->recplay_state);
  // softmodem has to know we use the iqrecorder to workaround randomized algorithms
  if (openair0_cfg->recplay_mode == RECPLAY_RECORDMODE) {
    IS_SOFTMODEM_IQRECORDER = true; // softmodem has to know we use the iqrecorder to workaround randomized algorithms
  }
  char *deflibname = OAI_RF_LIBNAME;
  loader_shlibfunc_t shlib_fdesc = {.fname = "device_init"};
  if (openair0_cfg->recplay_mode == RECPLAY_REPLAYMODE) {
    deflibname = OAI_IQPLAYER_LIBNAME;
    IS_SOFTMODEM_IQPLAYER = true; // softmodem has to know we use the iqplayer to workaround randomized algorithms
  } else {
    switch (rau_type) {
    case RAU_LOCAL_RADIO_HEAD:
      if (IS_SOFTMODEM_RFSIM)
        deflibname = OAI_RFSIM_LIBNAME;
      break;
    case RAU_REMOTE_THIRDPARTY_RADIO_HEAD:
      deflibname = OAI_THIRDPARTY_TP_LIBNAME;
      shlib_fdesc.fname = "transport_init";
      break;
    case RAU_REMOTE_RADIO_HEAD:
      deflibname = OAI_TP_LIBNAME;
      shlib_fdesc.fname = "transport_init";
      break;
    default:
      AssertFatal(false, "impossible radio head\n");
    }
  }

  char *devname = NULL;
  paramdef_t device_params = {"name", CONFIG_HLP_DEVICE, 0, .strptr = &devname, .defstrval = deflibname, TYPE_STRING, 0};
  config_get(config_get_if(), &device_params, 1, DEVICE_SECTION);

  int ret = load_module_shlib(devname, &shlib_fdesc, 1, NULL);
  AssertFatal(ret >= 0, "Library %s couldn't be loaded\n", devname);
  return ((devfunc_t)shlib_fdesc.fptr)(device, openair0_cfg);
}

void *create_ring(int sz_bytes)
{
  AssertFatal(sz_bytes % PAGE_SIZE == 0, "must be a number of pages %d", sz_bytes);
  // get a temporary file fd
  const int fd = fileno(tmpfile());
  // set it's size appropriately. We need exactly `sz` bytes as underlying memory
  if (ftruncate(fd, sz_bytes))
    AssertFatal(false, "errno: %s\n", strerror(errno));
  void *ret = mmap(NULL, 2 * sz_bytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  mmap(ret, sz_bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
  mmap(ret + sz_bytes, sz_bytes, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
  close(fd);
  return ret;
}

static void init_reorder(re_order_t *r, openair0_config_t *openair0_cfg)
{
  pthread_mutex_init(&r->mutex_store, NULL);
  pthread_mutex_init(&r->mutex_write, NULL);

  r->sz = ceil_mod(openair0_cfg->sample_rate / 100, PAGE_SIZE / sizeof(int)); // 10 ms storage
  r->grain = 2048; // arbitrary read size, chosen to fit in a ethernet jumbo frame
  r->nb_writers = create_ring(r->sz * sizeof(*r->nb_writers));

  r->ring = calloc(openair0_cfg->tx_num_channels, sizeof(*r->ring));
  for (int i = 0; i < openair0_cfg->tx_num_channels; i++)
    r->ring[i] = create_ring(r->sz * sizeof(c16_t));
}

int openair0_device_load(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  int rc=0;
  rc=load_lib(device, openair0_cfg, RAU_LOCAL_RADIO_HEAD);

  if ( rc >= 0) {
    if ( set_device(device) < 0) {
      LOG_E(HW, "%s %d:Unsupported radio head\n", __FILE__, __LINE__);
      return -1;
    }
  } else {
    AssertFatal(false, "can't open the radio device: %s\n", get_devname(device->type));
  }
  init_reorder(&device->reOrder, openair0_cfg);
  return rc;
}

int openair0_transport_load(openair0_device_t *device, openair0_config_t *openair0_cfg)
{
  int rc = load_lib(device, openair0_cfg, RAU_REMOTE_RADIO_HEAD);

  if ( rc >= 0) {
    if ( set_transport(device) < 0) {
      LOG_E(HW, "%s %d:Unsupported transport protocol\n", __FILE__, __LINE__);
      return -1;
    }
  }

  return rc;
}

int openair0_load(openair0_device_t *device, char *name, openair0_config_t *openair0_cfg, eth_params_t *eth_params)
{
  loader_shlibfunc_t shlib_fdesc[1];
  int ret = 0;

  shlib_fdesc[0].fname = eth_params == NULL ? "device_init" : "transport_init";

  ret = load_module_shlib(name, shlib_fdesc, 1, NULL);
  AssertFatal((ret >= 0), "Library %s couldn't be loaded\n", name);
  return ((devfunc_t)shlib_fdesc[0].fptr)(device, openair0_cfg);
}

// mutex (or atomic flags) will be mandatory because this out order system root cause is there are several writer threads
int openair0_write_reorder_common(nrue_ru_write_t nrue_ru_write,
                                  PHY_VARS_NR_UE *UE,
                                  openair0_device_t *device,
                                  openair0_timestamp_t timestamp,
                                  void **txp,
                                  int nsamps,
                                  int nb_writers,
                                  int writer_id,
                                  int nbAnt,
                                  int flags)
{
  re_order_t *ctx = &device->reOrder;
  LOG_D(HW,
        "received write order ts: %lu + %lu, nb samples %d, next ts %lu flags %d\n",
        timestamp,
        device->firstTS,
        nsamps,
        timestamp + nsamps,
        flags);

  // Add data in the ring buffer

  int ret=pthread_mutex_lock(&ctx->mutex_store);
  AssertFatal(ret==0,"mutex_store: %s\n",strerror(ret));
  if (!ctx->initDone) {
    ctx->nextTS = timestamp;
    ctx->initDone = true;
  }
  // We have the write exclusivity
  AssertFatal(nb_writers, "no UE writers, the minimum is 1");
  AssertFatal(nbAnt, "no tx antennas, the minimum is 1");
  int write_buff_index = timestamp % ctx->sz;
  if (flags & TX_BURST_FILL) {
    if (ctx->lastTS[writer_id] < timestamp) {
      int write_buff_index = ctx->lastTS[writer_id] % ctx->sz;
      const int *endl = ctx->nb_writers + (timestamp % ctx->sz);
      for (int *i = ctx->nb_writers + write_buff_index; i < endl; i++)
        *i = *i + 1;
    }
  }
  flags &= ~TX_BURST_FILL;
  for (int a = 0; a < nbAnt; a++) {
    c16_t *out = ((c16_t *)ctx->ring[a]) + write_buff_index;
    c16adds(txp[a], out, out, nsamps);
  }
  const int *endl = ctx->nb_writers + write_buff_index + nsamps;
  for (int *i = ctx->nb_writers + write_buff_index; i < endl; i++)
    *i = *i + 1;

  ctx->lastTS[writer_id] = timestamp + nsamps;

  // check it we have ready output now
  int consume_buff_index = ctx->nextTS % ctx->sz;
  int end = consume_buff_index;
  const int grain = ctx->grain;
  while (ctx->nb_writers[end] >= nb_writers)
    end++;

  if (end - consume_buff_index > grain) {
    while (consume_buff_index + grain <= end) {
      LOG_D(HW, "sending to RF: %ld\n", timestamp);
      if (flags || IS_SOFTMODEM_RFSIM) {
        void *ptr[nbAnt];
        for (int a = 0; a < nbAnt; a++)
          ptr[a] = ((c16_t *)ctx->ring[a]) + consume_buff_index;
        int wroteSamples;
        if (nrue_ru_write)
          wroteSamples = nrue_ru_write(UE, ctx->nextTS, ptr, grain, nbAnt, flags);
        else
          wroteSamples = device->trx_write_func(device, ctx->nextTS, ptr, grain, nbAnt, flags);
        if (wroteSamples != grain)
          LOG_W(HW, "Failed to write to RF: wrote %d out of %d samples\n", wroteSamples, grain);
      }
      for (int a = 0; a < nbAnt; a++)
        memset(((c16_t *)ctx->ring[a]) + consume_buff_index, 0, grain * sizeof(c16_t));
      memset(ctx->nb_writers + consume_buff_index, 0, grain * sizeof(*ctx->nb_writers));
      consume_buff_index += grain;
      ctx->nextTS += grain;
      timestamp += grain;
    }
  }
  pthread_mutex_unlock(&ctx->mutex_store);
  return nsamps;
}

int openair0_write_reorder(openair0_device_t *device, openair0_timestamp_t timestamp, void **txp, int nsamps, int nbAnt, int flags)
{
  return openair0_write_reorder_common(NULL, NULL, device, timestamp, txp, nsamps, 1, 0, nbAnt, flags);
}

  void openair0_write_reorder_clear_context(openair0_device_t *device)
  {
  LOG_I(HW, "received write reorder clear context\n");
    re_order_t *ctx = &device->reOrder;
    if (!ctx->initDone)
      return;
    if (pthread_mutex_trylock(&ctx->mutex_write) != 0)
      LOG_E(HW, "write_reorder_clear_context call while still writing on the device\n");
    else
      pthread_mutex_unlock(&ctx->mutex_write);
    pthread_mutex_lock(&ctx->mutex_store);
    ctx->initDone = false;
    pthread_mutex_unlock(&ctx->mutex_store);
  }
