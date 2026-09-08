/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "setup_msg_store.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static pthread_mutex_t store_lock = PTHREAD_MUTEX_INITIALIZER;
static byte_array_t setup_req[E2AP_SETUP_MSG_IFACE_END];
static byte_array_t setup_resp[E2AP_SETUP_MSG_IFACE_END];

static void store_one(byte_array_t *slot, const uint8_t *buf, uint32_t len)
{
  pthread_mutex_lock(&store_lock);
  free(slot->buf);
  slot->buf = NULL;
  slot->len = 0;
  if (len > 0 && buf != NULL) {
    slot->buf = malloc(len);
    memcpy(slot->buf, buf, len);
    slot->len = len;
  }
  pthread_mutex_unlock(&store_lock);
}

static byte_array_t get_one(byte_array_t *slot)
{
  byte_array_t out = {0};
  pthread_mutex_lock(&store_lock);
  if (slot->len > 0) {
    out.buf = malloc(slot->len);
    memcpy(out.buf, slot->buf, slot->len);
    out.len = slot->len;
  }
  pthread_mutex_unlock(&store_lock);
  return out;
}

void e2ap_store_setup_req(e2ap_setup_msg_iface_t iface, const uint8_t *buf, uint32_t len)
{
  if (iface >= E2AP_SETUP_MSG_IFACE_END)
    return;
  store_one(&setup_req[iface], buf, len);
}

void e2ap_store_setup_resp(e2ap_setup_msg_iface_t iface, const uint8_t *buf, uint32_t len)
{
  if (iface >= E2AP_SETUP_MSG_IFACE_END)
    return;
  store_one(&setup_resp[iface], buf, len);
}

byte_array_t e2ap_get_setup_req(e2ap_setup_msg_iface_t iface)
{
  if (iface >= E2AP_SETUP_MSG_IFACE_END)
    return (byte_array_t){0};
  return get_one(&setup_req[iface]);
}

byte_array_t e2ap_get_setup_resp(e2ap_setup_msg_iface_t iface)
{
  if (iface >= E2AP_SETUP_MSG_IFACE_END)
    return (byte_array_t){0};
  return get_one(&setup_resp[iface]);
}
