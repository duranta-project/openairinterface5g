/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "nr_pdcp_oai_api.h"
#include <errno.h>
#include <fcntl.h>
#include <openair3/ocp-gtpu/gtp_itf.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "NR_DRB-ToAddMod.h"
#include "NR_QFI.h"
#include "NR_SDAP-Config.h"
#include "NR_SRB-ToAddMod.h"
#include "SDAP/nr_sdap/nr_sdap_entity.h"
#include "assertions.h"
#include "common/ngran_types.h"
#include "common/platform_constants.h"
#include "common/ran_context.h"
#include "common/utils/T/T.h"
#include "common/utils/tuntap_if.h"
#include "cuup_cucp_if.h"
#include "executables/lte-softmodem.h"
#include "executables/softmodem-common.h"
#include "f1ap_messages_types.h"
#include "gnb_config.h"
#include "gtpv1_u_messages_types.h"
#include "ds/hashtable.h"
#include "intertask_interface.h"
#include "common/utils/LOG/log.h"
#include "nfapi/oai_integration/vendor_ext.h"
#include "nr_pdcp_asn1_utils.h"
#include "nr_pdcp_timer_thread.h"
#include "nr_pdcp_ue_manager.h"
#include "openair2/F1AP/f1ap_common.h"
#include "openair2/F1AP/f1ap_ids.h"
#include "openair2/SDAP/nr_sdap/nr_sdap.h"
#include "pdcp.h"
#include "pdcp_messages_types.h"
#ifdef PDCP_CUCP_CUUP
#include "nr_pdcp_nrup_f1ap.h"
#include "nr_up/nr_up_backend_if.h"
#include "nr_up/nr_up_pdcp_if.h"
#ifdef NR_UP_MONO_BACKEND
#include "nr_pdcp_nrup_direct.h"
#endif
#else
#include "nr_pdcp_nrup_ue.h"
#endif
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"
#include "utils.h"

#define TODO do { \
    printf("%s:%d:%s: todo\n", __FILE__, __LINE__, __FUNCTION__); \
    exit(1); \
  } while (0)

static nr_pdcp_ue_manager_t *nr_pdcp_ue_manager;

/* necessary globals for OAI, not used internally */
hash_table_t  *pdcp_coll_p;
static uint64_t pdcp_optmask;

static ngran_node_t node_type;

static deliver_pdu drb_deliver;

void nr_pdcp_bind_drb_deliver(deliver_pdu fn)
{
  DevAssert(fn);
  drb_deliver = fn;
}

nr_pdcp_entity_t *nr_pdcp_get_rb(nr_pdcp_ue_t *ue, int rb_id, bool srb_flag)
{
  nr_pdcp_entity_t *rb;

  if (srb_flag) {
    if (rb_id < 1 || rb_id > 2)
      rb = NULL;
    else
      rb = ue->srb[rb_id - 1];
  } else {
    if (rb_id < 1 || rb_id > MAX_DRBS_PER_UE)
      rb = NULL;
    else
      rb = ue->drb[rb_id - 1];
  }

  return rb;
}

/****************************************************************************/
/* pdcp_data_ind thread - begin                                             */
/****************************************************************************/

typedef struct {
  protocol_ctxt_t ctxt_pP;
  srb_flag_t      srb_flagP;
  rb_id_t         rb_id;
  sdu_size_t      sdu_buffer_size;
  uint8_t *sdu_buffer;
} pdcp_data_ind_queue_item;

#define PDCP_DATA_IND_QUEUE_SIZE 10000

typedef struct {
  pdcp_data_ind_queue_item q[PDCP_DATA_IND_QUEUE_SIZE];
  volatile int start;
  volatile int length;
  pthread_mutex_t m;
  pthread_cond_t c;
} pdcp_data_ind_queue;

static pdcp_data_ind_queue pq;

static void do_pdcp_data_ind(const protocol_ctxt_t *const ctxt_pP,
                             const srb_flag_t srb_flagP,
                             const rb_id_t rb_id,
                             const sdu_size_t sdu_buffer_size,
                             uint8_t *const sdu_buffer)
{
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;
  ue_id_t UEid = ctxt_pP->rntiMaybeUEid;

  if (ctxt_pP->module_id != 0 ||
      //ctxt_pP->enb_flag != 1 ||
      ctxt_pP->instance != 0 ||
      ctxt_pP->eNB_index != 0 ||
      ctxt_pP->brOption != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  if (ctxt_pP->enb_flag)
    T(T_ENB_PDCP_UL, T_INT(ctxt_pP->module_id), T_INT(UEid), T_INT(rb_id), T_INT(sdu_buffer_size));

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, UEid);
  rb = nr_pdcp_get_rb(ue, rb_id, srb_flagP);

  if (rb != NULL) {
    rb->recv_pdu(rb, (char *)sdu_buffer, sdu_buffer_size);
  } else {
    LOG_E(PDCP, "pdcp_data_ind: no RB found (rb_id %ld, srb_flag %d)\n", rb_id, srb_flagP);
  }

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

static void *pdcp_data_ind_thread(void *_)
{
  int i;

  pthread_setname_np(pthread_self(), "PDCP data ind");
  while (1) {
    if (pthread_mutex_lock(&pq.m) != 0) abort();
    while (pq.length == 0)
      if (pthread_cond_wait(&pq.c, &pq.m) != 0) abort();
    i = pq.start;
    if (pthread_mutex_unlock(&pq.m) != 0) abort();

    do_pdcp_data_ind(&pq.q[i].ctxt_pP,
                     pq.q[i].srb_flagP,
                     pq.q[i].rb_id,
                     pq.q[i].sdu_buffer_size,
                     pq.q[i].sdu_buffer);
    free(pq.q[i].sdu_buffer);

    if (pthread_mutex_lock(&pq.m) != 0) abort();

    pq.length--;
    pq.start = (pq.start + 1) % PDCP_DATA_IND_QUEUE_SIZE;

    if (pthread_cond_signal(&pq.c) != 0) abort();
    if (pthread_mutex_unlock(&pq.m) != 0) abort();
  }
}

static void init_nr_pdcp_data_ind_queue(void)
{
  pthread_t t;

  pthread_mutex_init(&pq.m, NULL);
  pthread_cond_init(&pq.c, NULL);

  if (pthread_create(&t, NULL, pdcp_data_ind_thread, NULL) != 0) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }
}

static void enqueue_pdcp_data_ind(const protocol_ctxt_t *const ctxt_pP,
                                  const srb_flag_t srb_flagP,
                                  const rb_id_t rb_id,
                                  const sdu_size_t sdu_buffer_size,
                                  uint8_t *const sdu_buffer)
{
  int i;
  int logged = 0;

  if (pthread_mutex_lock(&pq.m) != 0) abort();
  while (pq.length == PDCP_DATA_IND_QUEUE_SIZE) {
    if (!logged) {
      logged = 1;
      LOG_W(PDCP, "%s: pdcp_data_ind queue is full\n", __FUNCTION__);
    }
    if (pthread_cond_wait(&pq.c, &pq.m) != 0) abort();
  }

  i = (pq.start + pq.length) % PDCP_DATA_IND_QUEUE_SIZE;
  pq.length++;

  pq.q[i].ctxt_pP         = *ctxt_pP;
  pq.q[i].srb_flagP       = srb_flagP;
  pq.q[i].rb_id           = rb_id;
  pq.q[i].sdu_buffer_size = sdu_buffer_size;
  pq.q[i].sdu_buffer      = sdu_buffer;

  if (pthread_cond_signal(&pq.c) != 0) abort();
  if (pthread_mutex_unlock(&pq.m) != 0) abort();
}

bool nr_pdcp_data_ind(const protocol_ctxt_t *const ctxt_pP,
                      const srb_flag_t srb_flagP,
                      const rb_id_t rb_id,
                      const sdu_size_t sdu_buffer_size,
                      uint8_t *const sdu_buffer)
{
  enqueue_pdcp_data_ind(ctxt_pP, srb_flagP, rb_id, sdu_buffer_size, sdu_buffer);
  return true;
}

/****************************************************************************/
/* pdcp_data_ind thread - end                                               */
/****************************************************************************/

int pdcp_fifo_flush_sdus(const protocol_ctxt_t *const ctxt_pP)
{
  return 0;
}

static void set_node_type() {
  node_type = get_node_type();
}

void nr_pdcp_layer_init(void)
{
  /* hack: be sure to initialize only once */
  static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
  static int initialized = 0;
  if (pthread_mutex_lock(&m) != 0) abort();
  if (initialized) {
    if (pthread_mutex_unlock(&m) != 0) abort();
    return;
  }
  initialized = 1;
  if (pthread_mutex_unlock(&m) != 0) abort();

  nr_pdcp_ue_manager = new_nr_pdcp_ue_manager(1);

  set_node_type();

#ifdef PDCP_CUCP_CUUP
  nr_up_manager_init();
  if (NODE_IS_CU(node_type)) {
    nr_pdcp_nrup_f1ap_init(node_type);
#ifdef NR_UP_MONO_BACKEND
  } else if (NODE_IS_MONOLITHIC(node_type)) {
    nr_pdcp_nrup_direct_init(node_type);
#endif
  }
  nr_pdcp_e1_if_init(node_type == ngran_gNB_CUUP || node_type == ngran_gNB_CUCP);
#else
  nr_pdcp_nrup_ue_init();
#endif
  init_nr_pdcp_data_ind_queue();
  nr_pdcp_init_timer_thread(nr_pdcp_ue_manager);
}

#include "nfapi/oai_integration/vendor_ext.h"
#include "executables/lte-softmodem.h"
#include "common/utils/tuntap_if.h"
#include "openair2/SDAP/nr_sdap/nr_sdap.h"

static void deliver_sdu_drb(void *_ue, nr_pdcp_entity_t *entity,
                            char *buf, int size,
                            const nr_pdcp_integrity_data_t *msg_integrity)
{
  nr_pdcp_ue_t *ue = _ue;
  int rb_id;
  int i;

  if (IS_SOFTMODEM_NOS1 || UE_NAS_USE_TUN) {
    LOG_D(PDCP, "IP packet received with size %d, to be sent to SDAP interface, UE ID/RNTI: %ld\n", size, ue->ue_id);
    // in NoS1 mode: the SDAP should write() packets to an FD (TUN interface),
    // so below, set is_gnb == 0 to do that
    sdap_data_ind(entity->rb_id, 0, entity->pdusession_id, ue->ue_id, buf, size);
  }
  else{
    for (i = 0; i < MAX_DRBS_PER_UE; i++) {
        if (entity == ue->drb[i]) {
          rb_id = i+1;
          goto rb_found;
        }
      }

      LOG_E(PDCP, "%s:%d:%s: fatal, no RB found for UE ID/RNTI %ld\n", __FILE__, __LINE__, __FUNCTION__, ue->ue_id);
      exit(1);

    rb_found:
    {
      LOG_D(PDCP, "%s() (drb %d) sending message to SDAP size %d\n", __func__, rb_id, size);
      sdap_data_ind(rb_id, ue->drb[rb_id - 1]->is_gnb, ue->drb[rb_id - 1]->pdusession_id, ue->ue_id, buf, size);
    }
  }
}

static void deliver_sdu_srb(void *_ue, nr_pdcp_entity_t *entity,
                            char *buf, int size,
                            const nr_pdcp_integrity_data_t *msg_integrity)
{
  nr_pdcp_ue_t *ue = _ue;
  int srb_id;
  int i;

  for (i = 0; i < sizeofArray(ue->srb) ; i++) {
    if (entity == ue->srb[i]) {
      srb_id = i+1;
      goto srb_found;
    }
  }

  LOG_E(PDCP, "%s:%d:%s: fatal, no SRB found for UE ID/RNTI %ld\n", __FILE__, __LINE__, __FUNCTION__, ue->ue_id);
  exit(1);

srb_found:
  if (entity->is_gnb) {
    MessageDef *message_p = itti_alloc_new_message(TASK_PDCP_GNB, 0, F1AP_UL_RRC_MESSAGE);
    AssertFatal(message_p != NULL, "OUT OF MEMORY\n");
    f1ap_ul_rrc_message_t *ul_rrc = &F1AP_UL_RRC_MESSAGE(message_p);
    ul_rrc->gNB_CU_ue_id = ue->ue_id;
    /* look up the correct secondary UE ID to provide complete information to
     * RRC, the RLC-PDCP interface does not transport this information */
    f1_ue_data_t ue_data = cu_get_f1_ue_data(ue->ue_id);
    ul_rrc->gNB_DU_ue_id = ue_data.secondary_ue;
    message_p->ittiMsgHeader.originInstance = ue_data.du_assoc_id;
    ul_rrc->srb_id = srb_id;
    ul_rrc->rrc_container = malloc(size);
    AssertFatal(ul_rrc->rrc_container != NULL, "OUT OF MEMORY\n");
    memcpy(ul_rrc->rrc_container, buf, size);
    ul_rrc->rrc_container_length = size;
    itti_send_msg_to_task(TASK_RRC_GNB, 0, message_p);
  } else {
    uint8_t *rrc_buffer_p = itti_malloc(TASK_PDCP_UE, TASK_RRC_NRUE, size);
    AssertFatal(rrc_buffer_p != NULL, "OUT OF MEMORY\n");
    memcpy(rrc_buffer_p, buf, size);
    MessageDef *message_p = itti_alloc_new_message(TASK_PDCP_UE, 0, NR_RRC_DCCH_DATA_IND);
    AssertFatal(message_p != NULL, "OUT OF MEMORY\n");
    NR_RRC_DCCH_DATA_IND(message_p).dcch_index = srb_id;
    NR_RRC_DCCH_DATA_IND(message_p).sdu_p = rrc_buffer_p;
    NR_RRC_DCCH_DATA_IND(message_p).sdu_size = size;
    memcpy(&NR_RRC_DCCH_DATA_IND(message_p).msg_integrity, msg_integrity, sizeof(*msg_integrity));
    ue_id_t ue_id = ue->ue_id;
    itti_send_msg_to_task(TASK_RRC_NRUE, ue_id, message_p);
  }
}

void add_srb(int is_gnb,
             ue_id_t UEid,
             struct NR_SRB_ToAddMod *s,
             const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  nr_pdcp_entity_t *pdcp_srb;
  nr_pdcp_ue_t *ue;

  int srb_id = s->srb_Identity;
  int t_Reordering = -1; // infinity as per default SRB configuration in 9.2.1 of 38.331
  if (s->pdcp_Config != NULL && s->pdcp_Config->t_Reordering != NULL)
    t_Reordering = decode_t_reordering(*s->pdcp_Config->t_Reordering);

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, UEid);
  if (nr_pdcp_get_rb(ue, srb_id, true) != NULL) {
    LOG_E(PDCP, "warning SRB %d already exist for UE ID %ld, do nothing\n", srb_id, UEid);
  } else {
    pdcp_srb = new_nr_pdcp_entity(NR_PDCP_SRB,
                                  is_gnb,
                                  srb_id,
                                  0,      // PDU session ID (not relevant)
                                  false,  // has SDAP RX (not relevant)
                                  false,  // has SDAP TX (not relevant)
                                  deliver_sdu_srb,
                                  ue,
                                  SHORT_SN_SIZE,
                                  t_Reordering,
                                  -1,
                                  security_parameters);
    nr_pdcp_ue_add_srb_pdcp_entity(ue, srb_id, pdcp_srb);

    LOG_D(PDCP, "added srb %d to UE ID %ld\n", srb_id, UEid);
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_add_drb(int is_gnb,
                     const ue_id_t UEid,
                     const NR_PDCP_Config_t *pdcp,
                     const sdap_config_t *sdap,
                     const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  nr_pdcp_ue_t *ue;

  if (!pdcp->drb) {
    LOG_E(PDCP, "Missing PDCP config. DRB %d not configured.\n", sdap->drb_id);
    return;
  }

  struct NR_PDCP_Config__drb *drb = pdcp->drb;
  int sn_size_ul = decode_sn_size_ul(*drb->pdcp_SN_SizeUL);
  int sn_size_dl = decode_sn_size_dl(*drb->pdcp_SN_SizeDL);
  int discard_timer = decode_discard_timer(*drb->discardTimer);

  /* if pdcp_Config->t_Reordering is not present, it means infinity (-1) */
  int t_reordering = -1;
  if (pdcp->t_Reordering != NULL) {
    t_reordering = decode_t_reordering(*pdcp->t_Reordering);
  }

  int has_integrity = (drb->integrityProtection != NULL) ? 1 : 0;
  int has_ciphering = (pdcp->ext1 != NULL && pdcp->ext1->cipheringDisabled != NULL) ? 0 : 1;

  /* get actual ciphering and integrity algorithm based on pdcp_Config */
  nr_pdcp_entity_security_keys_and_algos_t actual_security_parameters = *security_parameters;
  actual_security_parameters.ciphering_algorithm = has_ciphering ? security_parameters->ciphering_algorithm : 0;
  actual_security_parameters.integrity_algorithm = has_integrity ? security_parameters->integrity_algorithm : 0;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, UEid);
  if (nr_pdcp_get_rb(ue, sdap->drb_id, false) != NULL) {
    LOG_W(PDCP, "warning DRB %d already exist for UE ID %ld, do nothing\n", sdap->drb_id, UEid);
  } else {
    // code assumption: same SN size for both DL and UL
    if (sn_size_ul != sn_size_dl) {
      LOG_E(PDCP, "fatal, bad SN sizes, must be same. ul=%d, dl=%d\n", sn_size_ul, sn_size_dl);
      exit(1);
    }

    // add PDCP entity
    nr_pdcp_entity_t *pdcp_drb = new_nr_pdcp_entity(NR_PDCP_DRB_AM,
                                                    is_gnb,
                                                    sdap->drb_id,
                                                    sdap->pdusession_id,
                                                    (sdap->role & (SDAP_UL_RX | SDAP_DL_RX)) != 0,
                                                    (sdap->role & (SDAP_UL_TX | SDAP_DL_TX)) != 0,
                                                    deliver_sdu_drb,
                                                    ue,
                                                    sn_size_dl,
                                                    t_reordering,
                                                    discard_timer,
                                                    &actual_security_parameters);
    nr_pdcp_ue_add_drb_pdcp_entity(ue, sdap->drb_id, pdcp_drb);

    LOG_I(PDCP, "Added DRB %d to UE ID %ld\n", sdap->drb_id, UEid);
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_add_srbs(eNB_flag_t enb_flag,
                      ue_id_t UEid,
                      NR_SRB_ToAddModList_t *const srb2add_list,
                      const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  if (srb2add_list != NULL) {
    for (int i = 0; i < srb2add_list->list.count; i++) {
      add_srb(enb_flag, UEid, srb2add_list->list.array[i], security_parameters);
    }
  } else
    LOG_W(PDCP, "nr_pdcp_add_srbs() with void list\n");
}

/** @brief Get DRB IDs for a specific PDU session */
int nr_pdcp_get_drb_ids_for_pdusession(ue_id_t ue_id, long pdusession_id, int *drb_ids)
{
  int num_found = 0;
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  if (ue) {
    for (int i = 0; i < MAX_DRBS_PER_UE; i++) {
      nr_pdcp_entity_t *drb = ue->drb[i];
      if (drb && drb->pdusession_id == pdusession_id) {
        if (num_found < MAX_DRBS_PER_UE) {
          drb_ids[num_found] = drb->rb_id;
          num_found++;
        } else {
          LOG_W(PDCP, "Found more DRBs than MAX_DRBS_PER_UE=%d\n", MAX_DRBS_PER_UE);
          break;
        }
      }
    }
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
  return num_found;
}

uint64_t get_pdcp_optmask(void)
{
  return pdcp_optmask;
}

void nr_pdcp_remove_UE(ue_id_t ue_id)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_manager_remove_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_config_set_security(ue_id_t ue_id,
                                 rb_id_t rb_id,
                                 bool is_srb,
                                 const nr_pdcp_entity_security_keys_and_algos_t *parameters)
{
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);

  rb = nr_pdcp_get_rb(ue, rb_id, is_srb);

  if (rb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "no %s found (ue_id %ld, rb_id %ld)\n", is_srb ? "SRB" : "DRB", ue_id, rb_id);
    return;
  }

  rb->set_security(rb, parameters);

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

bool nr_pdcp_check_integrity_srb(ue_id_t ue_id,
                                 int srb_id,
                                 const uint8_t *msg,
                                 int msg_size,
                                 const nr_pdcp_integrity_data_t *msg_integrity)
{
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);

  rb = nr_pdcp_get_rb(ue, srb_id, true);

  if (rb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "no SRB found (ue_id %ld, rb_id %d)\n", ue_id, srb_id);
    return false;
  }

  bool ret = rb->check_integrity(rb, msg, msg_size, msg_integrity);

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  return ret;
}

bool nr_pdcp_data_req_srb(ue_id_t ue_id,
                          const rb_id_t rb_id,
                          const mui_t muiP,
                          const sdu_size_t sdu_buffer_size,
                          unsigned char *const sdu_buffer,
                          deliver_pdu deliver_pdu_cb,
                          void *data)
{
  LOG_D(PDCP, "%s() called, size %d\n", __func__, sdu_buffer_size);
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  rb = nr_pdcp_get_rb(ue, rb_id, true);

  if (rb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "no SRB found (ue_id %ld, rb_id %ld)\n", ue_id, rb_id);
    return 0;
  }

  int max_size = nr_max_pdcp_pdu_size(sdu_buffer_size);
  char pdu_buf[max_size];
  int pdu_size = rb->process_sdu(rb, (char *)sdu_buffer, sdu_buffer_size, muiP, pdu_buf, max_size);
  if (pdu_size == -1) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return 0;
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  deliver_pdu_cb(data, ue_id, rb_id, pdu_buf, pdu_size, muiP);

  return 1;
}

void nr_pdcp_suspend_srb(ue_id_t ue_id, int srb_id)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_entity_t *srb = nr_pdcp_get_rb(ue, srb_id, true);
  if (srb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "Trying to suspend SRB with ID %d but it is not established\n", srb_id);
    return;
  }
  srb->suspend_entity(srb);
  LOG_D(PDCP, "SRB %d suspended\n", srb_id);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_suspend_drb(ue_id_t ue_id, int drb_id)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_entity_t *drb = nr_pdcp_get_rb(ue, drb_id, false);
  if (drb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "Trying to suspend DRB with ID %d but it is not established\n", drb_id);
    return;
  }
  drb->suspend_entity(drb);
  LOG_D(PDCP, "DRB %d suspended\n", drb_id);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_reconfigure_srb(ue_id_t ue_id, int srb_id, long t_Reordering)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_entity_t *srb = nr_pdcp_get_rb(ue, srb_id, true);
  if (srb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "Trying to reconfigure SRB with ID %d but it is not established\n", srb_id);
    return;
  }
  int decoded_t_reordering = decode_t_reordering(t_Reordering);
  srb->t_reordering = decoded_t_reordering;
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_reconfigure_drb(ue_id_t ue_id, int drb_id, NR_PDCP_Config_t *pdcp_config)
{
  // The enabling/disabling of ciphering or integrity protection
  // can be changed only by releasing and adding the DRB
  // (so not by reconfiguring).
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_entity_t *drb = nr_pdcp_get_rb(ue, drb_id, false);
  if (drb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "Trying to reconfigure DRB with ID %d but it is not established\n", drb_id);
    return;
  }
  if (pdcp_config) {
    if (pdcp_config->t_Reordering)
      drb->t_reordering = decode_t_reordering(*pdcp_config->t_Reordering);
    else
      drb->t_reordering = -1;
    struct NR_PDCP_Config__drb *drb_config = pdcp_config->drb;
    if (drb_config) {
      if (drb_config->discardTimer)
        drb->discard_timer = decode_discard_timer(*drb_config->discardTimer);
      bool size_set = false;
      if (drb_config->pdcp_SN_SizeUL) {
        drb->sn_size = decode_sn_size_ul(*drb_config->pdcp_SN_SizeUL);
        size_set = true;
      }
      if (drb_config->pdcp_SN_SizeDL) {
        int size = decode_sn_size_dl(*drb_config->pdcp_SN_SizeDL);
        AssertFatal(!size_set || (size == drb->sn_size),
                    "SN sizes must be the same. dl=%d, ul=%d",
                    size, drb->sn_size);
        drb->sn_size = size;
      }
    }
  }
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_release_srb(ue_id_t ue_id, int srb_id)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  if (ue->srb[srb_id - 1] != NULL) {
    ue->srb[srb_id - 1]->delete_entity(ue->srb[srb_id - 1]);
    ue->srb[srb_id - 1] = NULL;
  }
  else
    LOG_E(PDCP, "Attempting to release SRB%d but it is not configured\n", srb_id);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_release_drb(ue_id_t ue_id, int drb_id)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  nr_pdcp_entity_t *drb = ue->drb[drb_id - 1];
  if (drb) {
    nr_sdap_release_drb(ue_id, drb_id, drb->pdusession_id);
    drb->release_entity(drb);
    drb->delete_entity(drb);
    ue->drb[drb_id - 1] = NULL;
    LOG_I(PDCP, "release DRB %d of UE %ld\n", drb_id, ue_id);
  }
  else
    LOG_E(PDCP, "Attempting to release DRB%d but it is not configured\n", drb_id);
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

void nr_pdcp_reestablishment(ue_id_t ue_id,
                             int rb_id,
                             bool srb_flag,
                             const nr_pdcp_entity_security_keys_and_algos_t *security_parameters)
{
  nr_pdcp_ue_t     *ue;
  nr_pdcp_entity_t *rb;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  rb = nr_pdcp_get_rb(ue, rb_id, srb_flag);

  if (rb != NULL) {
    LOG_D(PDCP, "UE %4.4lx re-establishment of %sRB %d\n", ue_id, srb_flag ? "S" : "D", rb_id);
    rb->reestablish_entity(rb, security_parameters);
    LOG_I(PDCP, "%s %d re-established\n", srb_flag ? "SRB" : "DRB" , rb_id);
  } else {
    LOG_W(PDCP, "UE %4.4lx cannot re-establish %sRB %d, RB not found\n", ue_id, srb_flag ? "S" : "D", rb_id);
  }

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

bool nr_pdcp_data_req_drb(protocol_ctxt_t *ctxt_pP,
                          const srb_flag_t srb_flagP,
                          const rb_id_t rb_id,
                          const mui_t muiP,
                          const confirm_t confirmP,
                          const sdu_size_t sdu_buffer_size,
                          unsigned char *const sdu_buffer,
                          const pdcp_transmission_mode_t mode,
                          const uint32_t *const sourceL2Id,
                          const uint32_t *const destinationL2Id)
{
  DevAssert(srb_flagP == SRB_FLAG_NO);

  LOG_D(PDCP, "%s() called, size %d\n", __func__, sdu_buffer_size);
  nr_pdcp_ue_t *ue;
  nr_pdcp_entity_t *rb;
  ue_id_t ue_id = ctxt_pP->rntiMaybeUEid;

  if (ctxt_pP->module_id != 0 ||
      //ctxt_pP->enb_flag != 1 ||
      ctxt_pP->instance != 0 ||
      ctxt_pP->eNB_index != 0 /*||
      ctxt_pP->configured != 1 ||
      ctxt_pP->brOption != 0*/) {
    LOG_E(PDCP, "%s:%d:%s: fatal\n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  rb = nr_pdcp_get_rb(ue, rb_id, false);

  if (rb == NULL) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    LOG_E(PDCP, "[UE %lx] DRB %ld not found\n", ue_id, rb_id);
    return 0;
  }

#ifdef PDCP_CUCP_CUUP
  if (rb->is_gnb && NODE_IS_MONOLITHIC(node_type)) {
    const size_t est_pdu_size = nr_max_pdcp_pdu_size(sdu_buffer_size);
    if (nr_up_dl_congestion_precheck(ue_id, rb_id, est_pdu_size) == NR_UP_CONGESTION_DROP) {
      nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
      return 0;
    }
  }
#endif

  int max_size = nr_max_pdcp_pdu_size(sdu_buffer_size);
  unsigned char pdu_buf[max_size];
  int pdu_size = rb->process_sdu(rb, (char *)sdu_buffer, sdu_buffer_size, muiP, (char *)pdu_buf, max_size);
  if (pdu_size == -1) {
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return 0;
  }

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  DevAssert(drb_deliver != NULL);
  drb_deliver(NULL, ue_id, rb_id, (char *)pdu_buf, pdu_size, muiP);
  return 1;
}

bool cu_f1u_data_req(protocol_ctxt_t  *ctxt_pP,
                     const srb_flag_t srb_flagP,
                     const rb_id_t rb_id,
                     const mui_t muiP,
                     const confirm_t confirmP,
                     const sdu_size_t sdu_buffer_size,
                     unsigned char *const sdu_buffer,
                     const pdcp_transmission_mode_t mode,
                     const uint32_t *const sourceL2Id,
                     const uint32_t *const destinationL2Id)
{
  do_pdcp_data_ind(ctxt_pP, srb_flagP, rb_id, sdu_buffer_size, sdu_buffer);
  return true;
}

/*
 * For the SDAP API
 */
nr_pdcp_ue_manager_t *nr_pdcp_sdap_get_ue_manager() {
  return nr_pdcp_ue_manager;
}

/* returns false in case of error, true if everything ok */
bool nr_pdcp_get_statistics(ue_id_t ue_id, int srb_flag, int rb_id, nr_pdcp_statistics_t *out)
{
  nr_pdcp_ue_t     *ue;
  nr_pdcp_entity_t *rb;
  bool             ret;

  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  rb = nr_pdcp_get_rb(ue, rb_id, srb_flag);

  if (rb != NULL) {
    rb->get_stats(rb, out);
    ret = true;
  } else {
    ret = false;
  }

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);

  return ret;
}

int nr_pdcp_get_num_ues(ue_id_t *ue_list, int len)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);
  int num_ues = nr_pdcp_manager_get_ue_count(nr_pdcp_ue_manager);
  nr_pdcp_ue_t **nr_pdcp_ue_list = nr_pdcp_manager_get_ue_list(nr_pdcp_ue_manager);
  for (int i = 0; i < num_ues && i < len; i++)
    ue_list[i] = nr_pdcp_ue_list[i]->ue_id;
  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
  
  return num_ues;
}

void nr_pdcp_count_update(ue_id_t ue_id,
                          rb_id_t drb_id,
                          nr_pdcp_count_t dl_count,
                          nr_pdcp_count_t ul_count,
                          int sn_size)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);
  if (ue == NULL) {
    LOG_E(PDCP, "UE %ld not found in PDCP manager\n", ue_id);
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return;
  }

  nr_pdcp_entity_t *entity = nr_pdcp_get_rb(ue, drb_id, false);
  if (entity == NULL) {
    LOG_E(PDCP, "No PDCP entity found for UE %ld DRB %ld\n", ue_id, drb_id);
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return;
  }

  entity->set_pdcp_count_dl(entity, dl_count, sn_size);
  entity->set_pdcp_count_ul(entity, ul_count, sn_size);

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}

/** * @brief Get PDCP COUNT values (PDCP-SN and HFN) for UL and DL from the PDCP entity
 * @param[in]  ue_id          Unique UE ID
 * @param[in]  rb_id          Radio Bearer ID
 * @param[out] ul_count       UL PDCP count (SN, HFN)
 * @param[out] dl_count       DL PDCP count (SN, HFN)
 */
void nr_pdcp_get_drb_count_values(ue_id_t ue_id, rb_id_t rb_id, nr_pdcp_count_t *ul_count, nr_pdcp_count_t *dl_count)
{
  nr_pdcp_manager_lock(nr_pdcp_ue_manager);

  nr_pdcp_ue_t *ue = nr_pdcp_manager_get_ue(nr_pdcp_ue_manager, ue_id);

  nr_pdcp_entity_t *entity = nr_pdcp_get_rb(ue, rb_id, false);

  if (!entity) {
    LOG_E(PDCP, "No entity found (DRB %ld)\n", rb_id);
    nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
    return;
  }

  *dl_count = entity->get_pdcp_count_dl(entity);
  *ul_count = entity->get_pdcp_count_ul(entity);

  nr_pdcp_manager_unlock(nr_pdcp_ue_manager);
}
