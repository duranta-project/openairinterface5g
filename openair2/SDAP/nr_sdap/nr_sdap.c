/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_sdap.h"
#include "assertions.h"
#include "utils.h"
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "nr_sdap_entity.h"
#include "common/utils/LOG/log.h"
#include "rlc.h"
#include "tuntap_if.h"
#include "system.h"

typedef struct sdap_tun_iface_s {
  ue_id_t ue_id;
  int pdusession_id;
  int sock;
  char *ifname;
  int qfi;
  struct sdap_tun_iface_s *next;
} sdap_tun_iface_t;

static sdap_tun_iface_t *sdap_tun_iface_list = NULL;

static void reblock_tun_socket(int fd)
{
  int f;

  f = fcntl(fd, F_GETFL, 0);
  f &= ~(O_NONBLOCK);
  if (fcntl(fd, F_SETFL, f) == -1) {
    LOG_E(PDCP, "fcntl(F_SETFL) failed on fd %d: errno %d, %s\n", fd, errno, strerror(errno));
  }
}

static void *sdap_tun_read_thread(void *arg);

static sdap_tun_iface_t *sdap_tun_iface_lookup(ue_id_t ue_id, int pdusession_id)
{
  for (sdap_tun_iface_t *it = sdap_tun_iface_list; it != NULL; it = it->next) {
    if (it->ue_id == ue_id && it->pdusession_id == pdusession_id)
      return it;
  }
  return NULL;
}

void nr_sdap_tun_store_qfi(ue_id_t ue_id, int pdusession_id, uint8_t qfi)
{
  DevAssert(qfi < SDAP_MAX_QFI);
  sdap_tun_iface_t *iface = sdap_tun_iface_lookup(ue_id, pdusession_id);
  if (iface == NULL)
    return;

  iface->qfi = qfi;
}

void nr_sdap_tun_attach(nr_sdap_entity_t *entity)
{
  DevAssert(entity);
  if (entity->pdusession_sock >= 0)
    return;

  sdap_tun_iface_t *iface = sdap_tun_iface_lookup(entity->ue_id, entity->pdusession_id);
  if (iface == NULL)
    return;

  if (!entity->is_gnb && iface->qfi >= 0 && iface->qfi < SDAP_MAX_QFI) {
    entity->qfi = iface->qfi;
    LOG_I(SDAP, "UE %ld PDU session %d: cached QFI %d\n", entity->ue_id, entity->pdusession_id, entity->qfi);
  }

  /* For UE, reflect UP suspend/resume to the OS by toggling IFF_UP. */
  if (!entity->is_gnb) {
    LOG_I(SDAP, "UE %ld PDU session %d: bringing TUN %s up\n", entity->ue_id, entity->pdusession_id, iface->ifname);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0) {
      tuntap_set_up(iface->ifname, fd);
      close(fd);
    }
  }

  int d = dup(iface->sock);
  if (d < 0)
    LOG_W(SDAP, "dup(tun sock) failed: errno %d %s\n", errno, strerror(errno));
  else
    reblock_tun_socket(d);
  entity->pdusession_sock = d;
  if (d < 0)
    return;
  entity->stop_thread = false;

  char thread_name[64];
  if (entity->is_gnb) {
    snprintf(thread_name, sizeof(thread_name), "gnb_tun_read_thread");
  } else {
    snprintf(thread_name,
             sizeof(thread_name),
             "ue_tun_read_%ld_p%d",
             entity->ue_id,
             entity->pdusession_id);
  }
  threadCreate(&entity->pdusession_thread, sdap_tun_read_thread, entity, thread_name, -1, OAI_PRIORITY_RT_LOW);
}

static sdap_tun_iface_t *sdap_tun_iface_register(ue_id_t ue_id, int pdusession_id, int sock, const char *ifname)
{
  DevAssert(sdap_tun_iface_lookup(ue_id, pdusession_id) == NULL);
  sdap_tun_iface_t *iface = calloc_or_fail(1, sizeof(*iface));
  iface->ue_id = ue_id;
  iface->pdusession_id = pdusession_id;
  iface->sock = sock;
  iface->qfi = -1;
  iface->ifname = strdup(ifname);
  AssertFatal(iface->ifname != NULL, "strdup(ifname) failed\n");
  nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, pdusession_id);
  if (entity != NULL && !entity->is_gnb && entity->qfi >= 0 && entity->qfi < SDAP_MAX_QFI)
    iface->qfi = entity->qfi;
  iface->next = sdap_tun_iface_list;
  sdap_tun_iface_list = iface;
  return iface;
}

bool sdap_data_req(protocol_ctxt_t *ctxt_p,
                   const ue_id_t ue_id,
                   const srb_flag_t srb_flag,
                   const mui_t mui,
                   const confirm_t confirm,
                   const sdu_size_t sdu_buffer_size,
                   unsigned char *const sdu_buffer,
                   const pdcp_transmission_mode_t pt_mode,
                   const uint32_t *sourceL2Id,
                   const uint32_t *destinationL2Id,
                   const uint8_t qfi,
                   const bool rqi,
                   const int pdusession_id) {
  nr_sdap_entity_t *sdap_entity;
  sdap_entity = nr_sdap_get_entity(ue_id, pdusession_id);

  if(sdap_entity == NULL) {
    LOG_E(SDAP, "%s:%d:%s: Entity not found with ue: 0x%"PRIx64" and pdusession id: %d\n", __FILE__, __LINE__, __FUNCTION__, ue_id, pdusession_id);
    return 0;
  }

  return sdap_entity->tx_entity(sdap_entity,
                                ctxt_p,
                                srb_flag,
                                mui,
                                confirm,
                                sdu_buffer_size,
                                sdu_buffer,
                                pt_mode,
                                sourceL2Id,
                                destinationL2Id,
                                qfi,
                                rqi);
}

void sdap_data_ind(int pdcp_entity, int is_gnb, int pdusession_id, ue_id_t ue_id, char *buf, int size)
{
  nr_sdap_entity_t *sdap_entity;
  sdap_entity = nr_sdap_get_entity(ue_id, pdusession_id);

  if (sdap_entity == NULL) {
    LOG_E(SDAP, "Entity not found for ue rnti/ue_id: %lx and pdusession id: %d\n", ue_id, pdusession_id);
    return;
  }

  sdap_entity->rx_entity(sdap_entity,
                         pdcp_entity,
                         is_gnb,
                         pdusession_id,
                         ue_id,
                         buf,
                         size);
}

static void *sdap_tun_read_thread(void *arg)
{
  DevAssert(arg != NULL);
  nr_sdap_entity_t *entity = arg;

  char rx_buf[NL_MAX_PAYLOAD];
  int len;
  DevAssert(entity->pdusession_sock >= 0);
  reblock_tun_socket(entity->pdusession_sock);

  while (!entity->stop_thread) {
    len = read(entity->pdusession_sock, &rx_buf, NL_MAX_PAYLOAD);
    if (len == -1) {
      if (errno == EINTR)
        continue; // interrupted system call

      if (errno == EBADF || errno == EINVAL) {
        LOG_I(SDAP, "Socket closed, exiting TUN read thread for UE %ld, PDU session %d\n", entity->ue_id, entity->pdusession_id);
        break;
      }

      LOG_E(PDCP, "read() failed: errno %d (%s)\n", errno, strerror(errno));
      break;
    }

    if (len == 0) {
      LOG_W(SDAP, "TUN socket returned EOF - exiting thread\n");
      break;
    }

    LOG_D(SDAP, "read data of size %d\n", len);

    if (!entity->is_gnb && entity->enable_sdap && (entity->qfi < 0 || entity->qfi >= SDAP_MAX_QFI)) {
      LOG_W(SDAP,
            "Dropping UL SDU for UE %ld PDU session %d: no QoS rule QFI available for SDAP header\n",
            entity->ue_id,
            entity->pdusession_id);
      continue;
    }

    protocol_ctxt_t ctxt = {.enb_flag = entity->is_gnb, .rntiMaybeUEid = entity->ue_id};

    bool dc = entity->is_gnb ? false : SDAP_HDR_UL_DATA_PDU;

    entity->tx_entity(entity,
                      &ctxt,
                      SRB_FLAG_NO,
                      RLC_MUI_UNDEFINED,
                      RLC_SDU_CONFIRM_NO,
                      len,
                      (unsigned char *)rx_buf,
                      PDCP_TRANSMISSION_MODE_DATA,
                      NULL,
                      NULL,
                      entity->qfi,
                      dc);
  }

  return NULL;
}

void nr_sdap_tun_detach(nr_sdap_entity_t *entity)
{
  DevAssert(entity != NULL);
  sdap_tun_iface_t *iface = NULL;
  if (!entity->is_gnb) {
    iface = sdap_tun_iface_lookup(entity->ue_id, entity->pdusession_id);
    if (iface != NULL && entity->qfi >= 0 && entity->qfi < SDAP_MAX_QFI)
      iface->qfi = entity->qfi; // store the QFI for the next attach
  }
  if (entity->pdusession_sock < 0)
    return;

  entity->stop_thread = true;
  close(entity->pdusession_sock);
  entity->pdusession_sock = -1;

  /* For UE, bring interface down so the OS reflects UP suspension. */
  if (!entity->is_gnb && iface != NULL) {
    LOG_I(SDAP, "UE %ld PDU session %d: bringing TUN %s down\n", entity->ue_id, entity->pdusession_id, iface->ifname);
    tuntap_destroy(iface->ifname);
  }

  int cancel_ret = pthread_cancel(entity->pdusession_thread);
  AssertFatal(cancel_ret == 0, "pthread_cancel() failed: %d (%s)\n", cancel_ret, strerror(cancel_ret));
  int ret = pthread_join(entity->pdusession_thread, NULL);
  AssertFatal(ret == 0, "pthread_join() failed: %d (%s)\n", ret, strerror(ret));
}

void nr_sdap_tun_destroy(ue_id_t ue_id, int pdusession_id)
{
  sdap_tun_iface_t *iface = NULL;
  for (sdap_tun_iface_t **pp = &sdap_tun_iface_list; *pp != NULL; pp = &(*pp)->next) {
    if ((*pp)->ue_id == ue_id && (*pp)->pdusession_id == pdusession_id) {
      iface = *pp;
      *pp = iface->next;
      iface->next = NULL;
      break;
    }
  }
  if (iface == NULL) {
    LOG_D(SDAP, "nr_sdap_tun_destroy: no iface (ue=%ld, pdu=%d)\n", ue_id, pdusession_id);
    return;
  }
  close(iface->sock);
  tuntap_destroy(iface->ifname);
  LOG_I(SDAP, "Destroyed TUN dataplane for UE %ld PDU session %d (%s)\n", iface->ue_id, iface->pdusession_id, iface->ifname);
  free(iface->ifname);
  free(iface);
}

void start_sdap_tun_gnb_first_ue_default_pdu_session(ue_id_t ue_id, int pdu_session_id)
{
  nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, pdu_session_id);
  DevAssert(entity != NULL);
  DevAssert(entity->is_gnb);
  char *ifprefix = get_softmodem_params()->nsa ? "oaitun_gnb" : "oaitun_enb";
  char ifname[IFNAMSIZ];
  tun_generate_ifname(ifname, ifprefix, ue_id - 1);
  const int sock = tuntap_alloc(IFF_TUN, ifname);
  tun_config(ifname, "10.0.1.1", NULL);
  sdap_tun_iface_register(entity->ue_id, entity->pdusession_id, sock, ifname);
  nr_sdap_tun_attach(entity);
}

static void start_sdap_tun_ue(ue_id_t ue_id, int pdu_session_id, int sock, const char *ifname)
{
  nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, pdu_session_id);
  DevAssert(entity != NULL);
  DevAssert(!entity->is_gnb);
  // First PDU session setup: register UE TUN and attach the reader thread
  sdap_tun_iface_register(ue_id, pdu_session_id, sock, ifname);
  nr_sdap_tun_attach(entity);
}

void create_ue_ip_if(const char *ipv4, const char *ipv6, int ue_id, int pdu_session_id, bool is_default)
{
  char ifname[IFNAMSIZ];
  tuntap_generate_ue_ifname(ifname, IFF_TUN, ue_id, is_default ? -1 : pdu_session_id);

  if (sdap_tun_iface_lookup(ue_id, pdu_session_id) == NULL) {
    const int sock = tuntap_alloc(IFF_TUN, ifname);
    start_sdap_tun_ue(ue_id, pdu_session_id, sock, ifname);
  } else {
    nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, pdu_session_id);
    if (entity != NULL)
      nr_sdap_tun_attach(entity);
  }

  tun_config(ifname, ipv4, ipv6);
  if (ipv4) {
    setup_ue_ipv4_route(ifname, ue_id, ipv4);
  }
}

void create_ue_eth_if(int ue_id, int pdu_session_id, bool is_default)
{
  char ifname[IFNAMSIZ];
  tuntap_generate_ue_ifname(ifname, IFF_TAP, ue_id, is_default ? -1 : pdu_session_id);

  if (sdap_tun_iface_lookup(ue_id, pdu_session_id) == NULL) {
    const int sock = tuntap_alloc(IFF_TAP, ifname);
    start_sdap_tun_ue(ue_id, pdu_session_id, sock, ifname);
  } else {
    nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, pdu_session_id);
    if (entity != NULL)
      nr_sdap_tun_attach(entity);
  }

  tap_config(ifname);
}
