/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2017 Cisco Systems, Inc.
 */

#include "pnf_p7.h"
#include <stdlib.h>
#include <string.h>

nfapi_pnf_p7_config_t* nfapi_pnf_p7_config_create()
{
  pnf_p7_t* _this = NULL;
  int rc = posix_memalign((void**)&_this, 32, sizeof(pnf_p7_t));

  if (_this == NULL || rc != 0)
    return 0;

  // set the default parameters
  _this->_public.segment_size = 65000; // UDP max packet size is 65535
  _this->max_num_segments = 8;

  _this->_public.subframe_buffer_size = 8; // TODO: Initialize the slot_buffer size
  _this->_public.timing_info_mode_periodic = 1;
  _this->_public.timing_info_period = 32;
  _this->_public.timing_info_mode_aperiodic = 1;

  _this->_public.checksum_enabled = 1;

  _this->_public.malloc = &malloc;
  _this->_public.free = &free;

  _this->_public.codec_config.allocate = &malloc;
  _this->_public.codec_config.deallocate = &free;

  // cppcheck-suppress memleak
  return &(_this->_public);
}

void nfapi_pnf_p7_config_destory(nfapi_pnf_p7_config_t* config)
{
  if (config == 0)
    return;

  free(config);
}

int nfapi_pnf_p7_stop(nfapi_pnf_p7_config_t* config)
{
  // Verify that config is not null
  if (config == 0)
    return -1;

  pnf_p7_t* _this = (pnf_p7_t*)(config);
  _this->terminate = 1;

  return 0;
}

nfapi_nr_pnf_p7_config_t* nfapi_nr_pnf_p7_config_create(void)
{
  pnf_p7_t* _this = NULL;
  int rc = posix_memalign((void**)&_this, 32, sizeof(pnf_p7_t));
  if (_this == NULL || rc != 0)
    return NULL;

  memset(_this, 0, sizeof(pnf_p7_t));

  _this->_nr_public.segment_size = 65000;
  _this->max_num_segments = 8;
  _this->_nr_public.slot_buffer_size = 8;
  _this->_nr_public.timing_info_mode_periodic = 1;
  _this->_nr_public.timing_info_period = 32;
  _this->_nr_public.timing_info_mode_aperiodic = 1;
  _this->_nr_public.checksum_enabled = 1;
  _this->_nr_public.malloc = &malloc;
  _this->_nr_public.free = &free;
  /* pnf_p7_common.c uses _public.malloc/free for reassembly queue */
  _this->_public.malloc = &malloc;
  _this->_public.free = &free;

  return &_this->_nr_public;
}

void nfapi_nr_pnf_p7_config_destroy(nfapi_nr_pnf_p7_config_t* config)
{
  if (config)
    free(PNF_P7_FROM_NR_CONFIG(config));
}

int nfapi_nr_pnf_p7_stop(nfapi_nr_pnf_p7_config_t* config)
{
  if (!config)
    return -1;
  PNF_P7_FROM_NR_CONFIG(config)->terminate = 1;
  return 0;
}
