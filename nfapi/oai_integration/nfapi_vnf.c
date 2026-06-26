/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "nfapi_nr_interface_scf.h"
#include "nfapi_vnf.h"
#include <vnf.h>
#include <vnf_p7.h>
#ifdef ENABLE_WLS
#include <wls_integration/include/wls_vnf.h>
#include "nr_fapi_p5.h"
#include "nr_fapi_p7.h"
#endif

#ifdef ENABLE_SOCKET
#include <socket/include/socket_vnf.h>
#endif

#include "nfapi.h"
#include "vendor_ext.h"

#include "PHY/defs_eNB.h"
#include "PHY/LTE_TRANSPORT/transport_proto.h"
#include "openair2/LAYER2/MAC/mac.h"
#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "executables/lte-softmodem.h"
#include "openair1/PHY/defs_gNB.h"
#include "common/ran_context.h"
#include "openair2/PHY_INTERFACE/queue_t.h"
#include "gnb_ind_vars.h"
#include "nr_fapi_p7_utils.h"
#include "nr_fapi_p5_utils.h"
#include <NR_MAC_gNB/mac_proto.h>

#ifdef ENABLE_AERIAL
#include "aerial/fapi_nvIPC.h"
#include "aerial/fapi_vnf_p7.h"
#include "nr_fapi_p5.h"
#include "nr_fapi_p7.h"
#endif

#define TEST
static nfapi_vnf_config_t *config;
extern RAN_CONTEXT_t RC;
extern UL_RCC_IND_t  UL_RCC_INFO;

static int nr_start_resp_received = 0;

nfapi_vnf_config_t * get_config()
{
  return config;
}
vnf_p7_t *get_p7_vnf()
{
  vnf_info *vnf = config->user_data;
  return (vnf_p7_t *)vnf->p7_vnfs->config;
}

nfapi_vnf_p7_config_t *get_p7_vnf_config()
{
  return &get_p7_vnf()->_public;
}

int vnf_pack_vendor_extension_tlv(void *ve, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p4_p5_codec_config_t *codec) {
  UNUSED(codec);
  //NFAPI_TRACE(NFAPI_TRACE_INFO, "vnf_pack_vendor_extension_tlv\n");
  nfapi_tl_t *tlv = (nfapi_tl_t *)ve;

  switch(tlv->tag) {
    case VENDOR_EXT_TLV_2_TAG: {
      //NFAPI_TRACE(NFAPI_TRACE_INFO, "Packing VENDOR_EXT_TLV_2\n");
      vendor_ext_tlv_2 *ve = (vendor_ext_tlv_2 *)tlv;

      if(!push32(ve->dummy, ppWritePackedMsg, end))
        return 0;

      return 1;
    }
    break;
  }

  return -1;
}

int vnf_nr_pack_vendor_extension_tlv(void *ve, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p4_p5_codec_config_t *codec)
{
  UNUSED(codec);
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "vnf_pack_vendor_extension_tlv\n");
  nfapi_tl_t *tlv = (nfapi_tl_t *)ve;

  switch (tlv->tag) {
    case VENDOR_EXT_TLV_2_TAG: {
      // NFAPI_TRACE(NFAPI_TRACE_INFO, "Packing VENDOR_EXT_TLV_2\n");
      vendor_ext_tlv_2 *ve = (vendor_ext_tlv_2 *)tlv;

      if (!push32(ve->dummy, ppWritePackedMsg, end))
        return 0;

      return 1;
    } break;
  }

  return -1;
}
int vnf_unpack_vendor_extension_tlv(nfapi_tl_t *tl, uint8_t **ppReadPackedMessage, uint8_t *end, void **ve, nfapi_p4_p5_codec_config_t *codec) {
  UNUSED(tl);
  UNUSED(ppReadPackedMessage);
  UNUSED(end);
  UNUSED(ve);
  UNUSED(codec);
  return -1;
}
int vnf_nr_unpack_vendor_extension_tlv(nfapi_tl_t *tl,
                                       uint8_t **ppReadPackedMessage,
                                       uint8_t *end,
                                       void **ve,
                                       nfapi_p4_p5_codec_config_t *codec)
{
  UNUSED(tl);
  UNUSED(ppReadPackedMessage);
  UNUSED(end);
  UNUSED(ve);
  UNUSED(codec);
  return -1;
}
void install_schedule_handlers(IF_Module_t *if_inst);
extern int sf_ahead;

void oai_create_enb(void) {
  int bodge_counter=0;
  PHY_VARS_eNB *eNB = RC.eNB[0][0];
  NFAPI_TRACE(NFAPI_TRACE_INFO,
              "[VNF] RC.eNB[0][0]. Mod_id:%d CC_id:%d nb_CC[0]:%d abstraction_flag:%d if_inst:%p\n",
              eNB->Mod_id,
              eNB->CC_id,
              RC.nb_CC[0],
              eNB->abstraction_flag,
              eNB->if_inst);
  eNB->Mod_id  = bodge_counter;
  eNB->CC_id   = bodge_counter;
  eNB->abstraction_flag   = 0;
  RC.nb_CC[bodge_counter] = 1;

  if (eNB->if_inst==0) {
    eNB->if_inst = IF_Module_init(bodge_counter);
  }

  // This will cause phy_config_request to be installed. That will result in RRC configuring the PHY
  // that will result in eNB->configured being set to true.
  // See we need to wait for that to happen otherwise the NFAPI message exchanges won't contain the right parameter values
  if (RC.eNB[0][0]->if_inst==0 || RC.eNB[0][0]->if_inst->PHY_config_req==0 || RC.eNB[0][0]->if_inst->schedule_response==0) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "RC.eNB[0][0]->if_inst->PHY_config_req is not installed - install it\n");
    install_schedule_handlers(RC.eNB[0][0]->if_inst);
  }

  do {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() Waiting for eNB to become configured (by RRC/PHY) - need to wait otherwise NFAPI messages won't contain correct values\n", __FUNCTION__);
    usleep(50000);
  } while(eNB->configured != 1);

  NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() eNB is now configured\n", __FUNCTION__);
}

void oai_enb_init(void) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() About to call init_eNB_afterRU()\n", __FUNCTION__);
  init_eNB_afterRU();
}

int pnf_connection_indication_cb(nfapi_vnf_config_t *config, int p5_idx) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] pnf connection indication idx:%d\n", p5_idx);
  oai_create_enb();
  nfapi_pnf_param_request_t req;
  memset(&req, 0, sizeof(req));
  req.header.message_id = NFAPI_PNF_PARAM_REQUEST;
  nfapi_vnf_pnf_param_req(config, p5_idx, &req);
  return 0;
}

int pnf_nr_connection_indication_cb(nfapi_vnf_config_t *config, int p5_idx) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] pnf connection indication idx:%d\n", p5_idx);
  nfapi_nr_pnf_param_request_t req;
  memset(&req, 0, sizeof(req));
  req.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PNF_PARAM_REQUEST;
  nfapi_nr_vnf_pnf_param_req(config, p5_idx, &req);
  return 0;
}

int pnf_disconnection_indication_cb(nfapi_vnf_config_t *config, int p5_idx) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] pnf disconnection indication idx:%d\n", p5_idx);
  vnf_info *vnf = (vnf_info *)(config->user_data);
  pnf_info *pnf = vnf->pnfs;
  phy_info *phy = pnf->phys;
  vnf_p7_info *p7_vnf = vnf->p7_vnfs;
  nfapi_vnf_p7_del_pnf((p7_vnf->config), phy->id);
  return 0;
}

int pnf_nr_param_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_nr_pnf_param_response_t *resp) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] pnf param response idx:%d error:%d\n", p5_idx, resp->error_code);
  vnf_info *vnf = (vnf_info *)(config->user_data);
  pnf_info *pnf = vnf->pnfs;

  for(int i = 0; i < resp->pnf_phy.number_of_phys; ++i) {
    phy_info phy;
    memset(&phy,0,sizeof(phy));
    phy.index = resp->pnf_phy.phy[i].phy_config_index;
    NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] (PHY:%d) phy_config_idx:%d\n", i, resp->pnf_phy.phy[i].phy_config_index);
    nfapi_vnf_allocate_phy(config, p5_idx, &(phy.id));

    for(int j = 0; j < resp->pnf_phy.phy[i].number_of_rfs; ++j) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] (PHY:%d) (RF%d) %d\n", i, j, resp->pnf_phy.phy[i].rf_config[j].rf_config_index);
      phy.rfs[0] = resp->pnf_phy.phy[i].rf_config[j].rf_config_index;
    }

    pnf->phys[0] = phy;
  }
  nfapi_nr_pnf_config_request_t req;
  memset(&req, 0, sizeof(req));
  req.header.message_id = NFAPI_PNF_CONFIG_REQUEST;
  req.pnf_phy_rf_config.tl.tag = NFAPI_PNF_PHY_RF_TAG;
  req.pnf_phy_rf_config.number_phy_rf_config_info = 2; // pnf.phys.size();
  NFAPI_TRACE(NFAPI_TRACE_INFO, "Hard coded num phy rf to 2\n");

  for(unsigned i = 0; i < 2; ++i) {
    req.pnf_phy_rf_config.phy_rf_config[i].phy_id = pnf->phys[i].id;
    req.pnf_phy_rf_config.phy_rf_config[i].phy_config_index = pnf->phys[i].index;
    req.pnf_phy_rf_config.phy_rf_config[i].rf_config_index = pnf->phys[i].rfs[0];
  }

  nfapi_nr_vnf_pnf_config_req(config, p5_idx, &req);
  return 0;
}

int pnf_param_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_pnf_param_response_t *resp) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] pnf param response idx:%d error:%d\n", p5_idx, resp->error_code);
  vnf_info *vnf = (vnf_info *)(config->user_data);
  pnf_info *pnf = vnf->pnfs;

  for(int i = 0; i < resp->pnf_phy.number_of_phys; ++i) {
    phy_info phy;
    memset(&phy,0,sizeof(phy));
    phy.index = resp->pnf_phy.phy[i].phy_config_index;
    NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] (PHY:%d) phy_config_idx:%d\n", i, resp->pnf_phy.phy[i].phy_config_index);
    nfapi_vnf_allocate_phy(config, p5_idx, &(phy.id));

    for(int j = 0; j < resp->pnf_phy.phy[i].number_of_rfs; ++j) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] (PHY:%d) (RF%d) %d\n", i, j, resp->pnf_phy.phy[i].rf_config[j].rf_config_index);
      phy.rfs[0] = resp->pnf_phy.phy[i].rf_config[j].rf_config_index;
    }

    pnf->phys[0] = phy;
  }
  for(int i = 0; i < resp->pnf_rf.number_of_rfs; ++i) {
    rf_info rf;
    memset(&rf,0,sizeof(rf));
    rf.index = resp->pnf_rf.rf[i].rf_config_index;
    NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] (RF:%d) rf_config_idx:%d\n", i, resp->pnf_rf.rf[i].rf_config_index);
    pnf->rfs[0] = rf;
  }
  nfapi_pnf_config_request_t req;
  memset(&req, 0, sizeof(req));
  req.header.message_id = NFAPI_PNF_CONFIG_REQUEST;
  req.pnf_phy_rf_config.tl.tag = NFAPI_PNF_PHY_RF_TAG;
  req.pnf_phy_rf_config.number_phy_rf_config_info = 2; // pnf.phys.size();
  NFAPI_TRACE(NFAPI_TRACE_INFO, "Hard coded num phy rf to 2\n");

  for(unsigned i = 0; i < 2; ++i) {
    req.pnf_phy_rf_config.phy_rf_config[i].phy_id = pnf->phys[i].id;
    req.pnf_phy_rf_config.phy_rf_config[i].phy_config_index = pnf->phys[i].index;
    req.pnf_phy_rf_config.phy_rf_config[i].rf_config_index = pnf->phys[i].rfs[0];
  }

  nfapi_vnf_pnf_config_req(config, p5_idx, &req);
  return 0;
}

int pnf_nr_config_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_nr_pnf_config_response_t *resp) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] pnf config response idx:%d resp[header[phy_id:%u message_id:%02x message_length:%u]]\n", p5_idx, resp->header.phy_id, resp->header.message_id, resp->header.message_length);

  if(1) {
    nfapi_nr_pnf_start_request_t req;
    memset(&req, 0, sizeof(req));
    req.header.phy_id = resp->header.phy_id;
    req.header.message_id = NFAPI_PNF_START_REQUEST;
    nfapi_nr_vnf_pnf_start_req(config, p5_idx, &req);
  } else {
    // Rather than send the pnf_start_request we will demonstrate
    // sending a vendor extention message. The start request will be
    // send when the vendor extension response is received
    //vnf_info* vnf = (vnf_info*)(config->user_data);
    vendor_ext_p5_req req;
    memset(&req, 0, sizeof(req));
    req.header.message_id = P5_VENDOR_EXT_REQ;
    req.dummy1 = 45;
    req.dummy2 = 1977;
    nfapi_vnf_vendor_extension(config, p5_idx, &req.header);
  }

  return 0;
}

int pnf_config_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_pnf_config_response_t *resp) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] pnf config response idx:%d resp[header[phy_id:%u message_id:%02x message_length:%u]]\n", p5_idx, resp->header.phy_id, resp->header.message_id, resp->header.message_length);

  if(1) {
    nfapi_pnf_start_request_t req;
    memset(&req, 0, sizeof(req));
    req.header.phy_id = resp->header.phy_id;
    req.header.message_id = NFAPI_PNF_START_REQUEST;
    nfapi_vnf_pnf_start_req(config, p5_idx, &req);
  } else {
    // Rather than send the pnf_start_request we will demonstrate
    // sending a vendor extention message. The start request will be
    // send when the vendor extension response is received
    //vnf_info* vnf = (vnf_info*)(config->user_data);
    vendor_ext_p5_req req;
    memset(&req, 0, sizeof(req));
    req.header.message_id = P5_VENDOR_EXT_REQ;
    req.dummy1 = 45;
    req.dummy2 = 1977;
    nfapi_vnf_vendor_extension(config, p5_idx, &req.header);
  }

  return 0;
}

int wake_eNB_rxtx(PHY_VARS_eNB *eNB, uint16_t sfn, uint16_t sf) {
  L1_proc_t *proc=&eNB->proc;
  L1_rxtx_proc_t *L1_proc= (sf&1)? &proc->L1_proc : &proc->L1_proc_tx;
  LTE_DL_FRAME_PARMS *fp = &eNB->frame_parms;
  //NFAPI_TRACE(NFAPI_TRACE_INFO, "%s(eNB:%p, sfn:%d, sf:%d)\n", __FUNCTION__, eNB, sfn, sf);
  //int i;
  struct timespec wait;
  wait.tv_sec=0;
  wait.tv_nsec=5000000L;

  // wake up TX for subframe n+sf_ahead
  // lock the TX mutex and make sure the thread is ready
  //if (pthread_mutex_timedlock(&L1_proc->mutex,&wait) != 0) {
  //  LOG_E( PHY, "[eNB] ERROR pthread_mutex_lock for eNB RXTX thread %d (IC %d)\n", L1_proc->subframe_rx&1,L1_proc->instance_cnt );
  //  exit_fun( "error locking mutex_rxtx" );
  //  return(-1);
  //}

  {
    static uint16_t old_sf = 0;
    static uint16_t old_sfn = 0;
    proc->subframe_rx = old_sf;
    proc->frame_rx = old_sfn;
    // Try to be 1 frame back
    old_sf = sf;
    old_sfn = sfn;

    if (old_sf == 0 && old_sfn % 100==0)
      LOG_D(PHY,
            "[eNB] sfn/sf:%d%d old_sfn/sf:%d%d proc[rx:%d%d]\n",
            sfn,
            sf,
            old_sfn,
            old_sf,
            proc->frame_rx,
            proc->subframe_rx);
  }
  // wake up TX for subframe n+sf_ahead
  // lock the TX mutex and make sure the thread is ready
  if (pthread_mutex_timedlock(&L1_proc->mutex,&wait) != 0) {
      LOG_E( PHY, "[eNB] ERROR pthread_mutex_lock for eNB RXTX thread %d (IC %d)\n", L1_proc->subframe_rx&1,L1_proc->instance_cnt );
      //exit_fun( "error locking mutex_rxtx" );
      return(-1);
  }
  static int busy_log_cnt=0;
  if(L1_proc->instance_cnt < 0){
    ++L1_proc->instance_cnt;
    if(busy_log_cnt!=0){
      LOG_E(MAC,"RCC singal to rxtx frame %d subframe %d busy end %d (frame %d subframe %d)\n",L1_proc->frame_rx,L1_proc->subframe_rx,busy_log_cnt,proc->frame_rx,proc->subframe_rx);
    }
    busy_log_cnt=0;
  }else{
    if(busy_log_cnt==0){
      LOG_E(MAC,"RCC singal to rxtx frame %d subframe %d busy %d (frame %d subframe %d)\n",L1_proc->frame_rx,L1_proc->subframe_rx,L1_proc->instance_cnt,proc->frame_rx,proc->subframe_rx);
    }
    pthread_mutex_unlock( &L1_proc->mutex );
    busy_log_cnt++;
    return(0);
  }

  pthread_mutex_unlock( &L1_proc->mutex );

  //LOG_D( PHY,"[VNF-subframe_ind] sfn/sf:%d:%d proc[frame_rx:%d subframe_rx:%d] L1_proc->instance_cnt_rxtx:%d \n", sfn, sf, proc->frame_rx, proc->subframe_rx, L1_proc->instance_cnt_rxtx);
  // We have just received and processed the common part of a subframe, say n.
  // TS_rx is the last received timestamp (start of 1st slot), TS_tx is the desired
  // transmitted timestamp of the next TX slot (first).
  // The last (TS_rx mod samples_per_frame) was n*samples_per_tti,
  // we want to generate subframe (n+N), so TS_tx = TX_rx+N*samples_per_tti,
  // and proc->subframe_tx = proc->subframe_rx+sf_ahead
  L1_proc->timestamp_tx = proc->timestamp_rx + (sf_ahead*fp->samples_per_tti);
  L1_proc->frame_rx     = proc->frame_rx;
  L1_proc->subframe_rx  = proc->subframe_rx;
  L1_proc->frame_tx     = (L1_proc->subframe_rx > (9-sf_ahead)) ? (L1_proc->frame_rx+1)&1023 : L1_proc->frame_rx;
  L1_proc->subframe_tx  = (L1_proc->subframe_rx + sf_ahead)%10;

  //LOG_D(PHY, "sfn/sf:%d%d proc[rx:%d%d] L1_proc[instance_cnt_rxtx:%d rx:%d%d] About to wake rxtx thread\n\n", sfn, sf, proc->frame_rx, proc->subframe_rx, L1_proc->instance_cnt_rxtx, L1_proc->frame_rx, L1_proc->subframe_rx);

  // the thread can now be woken up
  if (pthread_cond_signal(&L1_proc->cond) != 0) {
    LOG_E( PHY, "[eNB] ERROR pthread_cond_signal for eNB RXn-TXnp4 thread\n");
    exit_fun( "ERROR pthread_cond_signal" );
    return(-1);
  }

  return(0);
}

extern pthread_cond_t nfapi_sync_cond;
extern pthread_mutex_t nfapi_sync_mutex;
extern int nfapi_sync_var;

int phy_sync_indication(struct nfapi_vnf_p7_config *config, uint8_t sync) {
  UNUSED(config);
  //NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] SYNC %s\n", sync==1 ? "ACHIEVED" : "LOST");

  if (sync==1 && nfapi_sync_var!=0) {

    NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Signal to OAI main code that it can go\n");
    pthread_mutex_lock(&nfapi_sync_mutex);
    nfapi_sync_var=0;
    pthread_cond_broadcast(&nfapi_sync_cond);
    pthread_mutex_unlock(&nfapi_sync_mutex);
  }

  return(0);
}


int phy_subframe_indication(struct nfapi_vnf_p7_config *config, uint16_t phy_id, uint16_t sfn_sf) {
  UNUSED(config);
  UNUSED(phy_id);
  static uint8_t first_time = 1;

  if (first_time) {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] subframe indication %d\n", NFAPI_SFNSF2DEC(sfn_sf));
    first_time = 0;
  }

  if (RC.eNB && RC.eNB[0][0]->configured) {
    uint16_t sfn = NFAPI_SFNSF2SFN(sfn_sf);
    uint16_t sf = NFAPI_SFNSF2SF(sfn_sf);
    //LOG_D(PHY,"[VNF] subframe indication sfn_sf:%d sfn:%d sf:%d\n", sfn_sf, sfn, sf);
    wake_eNB_rxtx(RC.eNB[0][0], sfn, sf);
  } else {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] %s() RC.eNB:%p\n", __FUNCTION__, RC.eNB);

    if (RC.eNB) NFAPI_TRACE(NFAPI_TRACE_INFO, "RC.eNB[0][0]->configured:%d\n", RC.eNB[0][0]->configured);
  }

  return 0;
}

int phy_rach_indication(struct nfapi_vnf_p7_config *config, nfapi_rach_indication_t *ind) {
  UNUSED(config);
  LOG_D(MAC, "%s() NFAPI SFN/SF:%d number_of_preambles:%u\n", __FUNCTION__, NFAPI_SFNSF2DEC(ind->sfn_sf), ind->rach_indication_body.number_of_preambles);
  struct PHY_VARS_eNB_s *eNB = RC.eNB[0][0];
  LOG_D(MAC, "[VNF] RACH_IND eNB:%p sfn_sf:%d number_of_preambles:%d\n", eNB, NFAPI_SFNSF2DEC(ind->sfn_sf), ind->rach_indication_body.number_of_preambles);
  AssertFatal(pthread_mutex_lock(&eNB->UL_INFO_mutex)==0, "Mutex lock failed");
  if(NFAPI_MODE == NFAPI_MODE_VNF){
    int8_t index = NFAPI_SFNSF2SF(ind->sfn_sf);

    UL_RCC_INFO.rach_ind[index] = *ind;

    if (ind->rach_indication_body.number_of_preambles > 0)
      UL_RCC_INFO.rach_ind[index].rach_indication_body.preamble_list = malloc(sizeof(nfapi_preamble_pdu_t)*ind->rach_indication_body.number_of_preambles );

    for (int i=0; i<ind->rach_indication_body.number_of_preambles; i++) {
      if (ind->rach_indication_body.preamble_list[i].preamble_rel8.tl.tag == NFAPI_PREAMBLE_REL8_TAG) {

        LOG_D(MAC, "preamble[%d]: rnti:%02x preamble:%d timing_advance:%d\n",
              i,
              ind->rach_indication_body.preamble_list[i].preamble_rel8.rnti,
              ind->rach_indication_body.preamble_list[i].preamble_rel8.preamble,
              ind->rach_indication_body.preamble_list[i].preamble_rel8.timing_advance
              );
      }
      if(ind->rach_indication_body.preamble_list[i].preamble_rel13.tl.tag == NFAPI_PREAMBLE_REL13_TAG) {
        LOG_D(MAC, "RACH PREAMBLE REL13 present\n");
      }

      UL_RCC_INFO.rach_ind[index].rach_indication_body.preamble_list[i] = ind->rach_indication_body.preamble_list[i];
    }
  }else{
  eNB->UL_INFO.rach_ind = *ind;
  eNB->UL_INFO.rach_ind.rach_indication_body.preamble_list = eNB->preamble_list;

  for (int i=0; i<ind->rach_indication_body.number_of_preambles; i++) {
    if (ind->rach_indication_body.preamble_list[i].preamble_rel8.tl.tag == NFAPI_PREAMBLE_REL8_TAG) {
      LOG_D(MAC, "preamble[%d]: rnti:%02x preamble:%d timing_advance:%d\n",
             i,
             ind->rach_indication_body.preamble_list[i].preamble_rel8.rnti,
             ind->rach_indication_body.preamble_list[i].preamble_rel8.preamble,
             ind->rach_indication_body.preamble_list[i].preamble_rel8.timing_advance
            );
    }

    if(ind->rach_indication_body.preamble_list[i].preamble_rel13.tl.tag == NFAPI_PREAMBLE_REL13_TAG) {
      LOG_D(MAC, "RACH PREAMBLE REL13 present\n");
    }

    eNB->preamble_list[i] = ind->rach_indication_body.preamble_list[i];
  }
  }
  AssertFatal(pthread_mutex_unlock(&eNB->UL_INFO_mutex)==0, "Mutex unlock failed");
  // vnf_p7_info* p7_vnf = (vnf_p7_info*)(config->user_data);
  //mac_rach_ind(p7_vnf->mac, ind);
  return 1;
}

int phy_nr_rach_indication(nfapi_nr_rach_indication_t *ind)
{
  if (NFAPI_MODE == NFAPI_MODE_VNF || NFAPI_MODE == NFAPI_MODE_AERIAL) {
    nfapi_nr_rach_indication_t *rach_ind = CALLOC(1, sizeof(*rach_ind));
    copy_rach_indication(ind, rach_ind);
    if (!put_queue(&gnb_rach_ind_queue, rach_ind)) {
      LOG_E(NR_MAC, "Put_queue failed for rach_ind\n");
      free_rach_indication(rach_ind);
      free(rach_ind);
    }
  } else {
    LOG_E(NR_MAC, "NFAPI_MODE = %d not NFAPI_MODE_VNF(2)\n", nfapi_getmode());
  }
  return 1;
}

int phy_nr_uci_indication(nfapi_nr_uci_indication_t *ind)
{
  LOG_D(NR_MAC, "In %s() NFAPI SFN/SF: %d/%d number_of_pdus :%u\n", __FUNCTION__, ind->sfn, ind->slot, ind->num_ucis);
  if (NFAPI_MODE == NFAPI_MODE_VNF || NFAPI_MODE == NFAPI_MODE_AERIAL) {
    nfapi_nr_uci_indication_t *uci_ind = CALLOC(1, sizeof(*uci_ind));
    AssertFatal(uci_ind, "Memory not allocated for uci_ind in phy_nr_uci_indication.");
    copy_uci_indication(ind, uci_ind);
    if (!put_queue(&gnb_uci_ind_queue, uci_ind)) {
      LOG_E(NR_MAC, "Put_queue failed for uci_ind\n");
      free_uci_indication(uci_ind);
      free(uci_ind);
      uci_ind = NULL;
    }
  } else {
    LOG_E(NR_MAC, "NFAPI_MODE = %d not NFAPI_MODE_VNF(2)\n", nfapi_getmode());
  }
  return 1;
}

int phy_harq_indication(struct nfapi_vnf_p7_config *config, nfapi_harq_indication_t *ind) {
  UNUSED(config);
  struct PHY_VARS_eNB_s *eNB = RC.eNB[0][0];
  LOG_D(MAC, "%s() NFAPI SFN/SF:%d number_of_harqs:%u\n", __FUNCTION__, NFAPI_SFNSF2DEC(ind->sfn_sf), ind->harq_indication_body.number_of_harqs);
  AssertFatal(pthread_mutex_lock(&eNB->UL_INFO_mutex)==0, "Mutex lock failed");
  if(NFAPI_MODE == NFAPI_MODE_VNF){
    int8_t index = NFAPI_SFNSF2SF(ind->sfn_sf);

    UL_RCC_INFO.harq_ind[index] = *ind;

    assert(ind->harq_indication_body.number_of_harqs <= NFAPI_HARQ_IND_MAX_PDU);
    if (ind->harq_indication_body.number_of_harqs > 0) {
      UL_RCC_INFO.harq_ind[index].harq_indication_body.harq_pdu_list = malloc(sizeof(nfapi_harq_indication_pdu_t) * NFAPI_HARQ_IND_MAX_PDU);
    }
    for (int i=0; i<ind->harq_indication_body.number_of_harqs; i++) {
        memcpy(&UL_RCC_INFO.harq_ind[index].harq_indication_body.harq_pdu_list[i], &ind->harq_indication_body.harq_pdu_list[i], sizeof(nfapi_harq_indication_pdu_t));
    }
  }else{
    eNB->UL_INFO.harq_ind = *ind;
    eNB->UL_INFO.harq_ind.harq_indication_body.harq_pdu_list = eNB->harq_pdu_list;

    assert(ind->harq_indication_body.number_of_harqs <= NFAPI_HARQ_IND_MAX_PDU);
    for (int i=0; i<ind->harq_indication_body.number_of_harqs; i++) {
      memcpy(&eNB->UL_INFO.harq_ind.harq_indication_body.harq_pdu_list[i],
             &ind->harq_indication_body.harq_pdu_list[i],
             sizeof(eNB->UL_INFO.harq_ind.harq_indication_body.harq_pdu_list[i]));
    }
  }
  AssertFatal(pthread_mutex_unlock(&eNB->UL_INFO_mutex)==0, "Mutex unlock failed");
  // vnf_p7_info* p7_vnf = (vnf_p7_info*)(config->user_data);
  //mac_harq_ind(p7_vnf->mac, ind);
  return 1;
}

int phy_crc_indication(struct nfapi_vnf_p7_config *config, nfapi_crc_indication_t *ind) {
  UNUSED(config);
  struct PHY_VARS_eNB_s *eNB = RC.eNB[0][0];
  AssertFatal(pthread_mutex_lock(&eNB->UL_INFO_mutex)==0, "Mutex lock failed");
  if(NFAPI_MODE == NFAPI_MODE_VNF){
    int8_t index = NFAPI_SFNSF2SF(ind->sfn_sf);

    UL_RCC_INFO.crc_ind[index] = *ind;

    assert(ind->crc_indication_body.number_of_crcs <= NFAPI_CRC_IND_MAX_PDU);
    if (ind->crc_indication_body.number_of_crcs > 0) {
      UL_RCC_INFO.crc_ind[index].crc_indication_body.crc_pdu_list = malloc(sizeof(nfapi_crc_indication_pdu_t) * NFAPI_CRC_IND_MAX_PDU);
    }

    assert(ind->crc_indication_body.number_of_crcs <= NFAPI_CRC_IND_MAX_PDU);
    for (int i=0; i<ind->crc_indication_body.number_of_crcs; i++) {
      memcpy(&UL_RCC_INFO.crc_ind[index].crc_indication_body.crc_pdu_list[i], &ind->crc_indication_body.crc_pdu_list[i], sizeof(ind->crc_indication_body.crc_pdu_list[0]));

      LOG_D(MAC, "%s() NFAPI SFN/SF:%d CRC_IND:number_of_crcs:%u UL_INFO:crcs:%d PDU[%d] rnti:%04x UL_INFO:rnti:%04x\n",
          __FUNCTION__,
          NFAPI_SFNSF2DEC(ind->sfn_sf), ind->crc_indication_body.number_of_crcs, UL_RCC_INFO.crc_ind[index].crc_indication_body.number_of_crcs,
          i,
          ind->crc_indication_body.crc_pdu_list[i].rx_ue_information.rnti,
          UL_RCC_INFO.crc_ind[index].crc_indication_body.crc_pdu_list[i].rx_ue_information.rnti);
    }
  }else{
  eNB->UL_INFO.crc_ind = *ind;
  nfapi_crc_indication_t *dest_ind = &eNB->UL_INFO.crc_ind;
  nfapi_crc_indication_pdu_t *dest_pdu_list = eNB->crc_pdu_list;
  *dest_ind = *ind;
  dest_ind->crc_indication_body.crc_pdu_list = dest_pdu_list;

  if (ind->crc_indication_body.number_of_crcs==0)
    LOG_D(MAC, "%s() NFAPI SFN/SF:%d IND:number_of_crcs:%u UL_INFO:crcs:%d\n", __FUNCTION__, NFAPI_SFNSF2DEC(ind->sfn_sf), ind->crc_indication_body.number_of_crcs,
          eNB->UL_INFO.crc_ind.crc_indication_body.number_of_crcs);

  assert(ind->crc_indication_body.number_of_crcs <= NFAPI_CRC_IND_MAX_PDU);
  for (int i=0; i<ind->crc_indication_body.number_of_crcs; i++) {
    memcpy(&dest_ind->crc_indication_body.crc_pdu_list[i], &ind->crc_indication_body.crc_pdu_list[i], sizeof(ind->crc_indication_body.crc_pdu_list[0]));
    LOG_D(MAC, "%s() NFAPI SFN/SF:%d CRC_IND:number_of_crcs:%u UL_INFO:crcs:%d PDU[%d] rnti:%04x UL_INFO:rnti:%04x\n",
          __FUNCTION__,
          NFAPI_SFNSF2DEC(ind->sfn_sf), ind->crc_indication_body.number_of_crcs, eNB->UL_INFO.crc_ind.crc_indication_body.number_of_crcs,
          i,
          ind->crc_indication_body.crc_pdu_list[i].rx_ue_information.rnti,
          eNB->UL_INFO.crc_ind.crc_indication_body.crc_pdu_list[i].rx_ue_information.rnti);
  }
  }
  AssertFatal(pthread_mutex_unlock(&eNB->UL_INFO_mutex)==0, "Mutex unlock failed");
  // vnf_p7_info* p7_vnf = (vnf_p7_info*)(config->user_data);
  //mac_crc_ind(p7_vnf->mac, ind);
  return 1;
}

int phy_nr_crc_indication(nfapi_nr_crc_indication_t *ind)
{
  LOG_D(NR_MAC, "In %s() NFAPI SFN/SF: %d/%d number_of_pdus :%u\n", __FUNCTION__, ind->sfn, ind->slot, ind->number_crcs);

  if (NFAPI_MODE == NFAPI_MODE_VNF || NFAPI_MODE == NFAPI_MODE_AERIAL) {
    nfapi_nr_crc_indication_t *crc_ind = CALLOC(1, sizeof(*crc_ind));
    copy_crc_indication(ind, crc_ind);
    if (!put_queue(&gnb_crc_ind_queue, crc_ind)) {
      LOG_E(NR_MAC, "Put_queue failed for crc_ind\n");
      free_crc_indication(crc_ind);
      free(crc_ind);
    }
  } else {
    LOG_E(NR_MAC, "NFAPI_MODE = %d not NFAPI_MODE_VNF(2)\n", nfapi_getmode());
  }
  return 1;
}

int phy_rx_indication(struct nfapi_vnf_p7_config *config, nfapi_rx_indication_t *ind) {
  UNUSED(config);
  struct PHY_VARS_eNB_s *eNB = RC.eNB[0][0];

  if (ind->rx_indication_body.number_of_pdus==0) {
    LOG_D(MAC, "%s() NFAPI SFN/SF:%d number_of_pdus:%u\n", __FUNCTION__, NFAPI_SFNSF2DEC(ind->sfn_sf), ind->rx_indication_body.number_of_pdus);
  }

  AssertFatal(pthread_mutex_lock(&eNB->UL_INFO_mutex)==0, "Mutex lock failed");
  if(NFAPI_MODE == NFAPI_MODE_VNF){
    int8_t index = NFAPI_SFNSF2SF(ind->sfn_sf);

    UL_RCC_INFO.rx_ind[index] = *ind;

    size_t number_of_pdus = ind->rx_indication_body.number_of_pdus;
    assert(number_of_pdus <= NFAPI_RX_IND_MAX_PDU);

    if (number_of_pdus > 0) {
      UL_RCC_INFO.rx_ind[index].rx_indication_body.rx_pdu_list =
          malloc(sizeof(nfapi_rx_indication_pdu_t) * NFAPI_RX_IND_MAX_PDU);
    }

    for (int i=0; i<number_of_pdus; i++) {
      nfapi_rx_indication_pdu_t *dest_pdu = &UL_RCC_INFO.rx_ind[index].rx_indication_body.rx_pdu_list[i];
      nfapi_rx_indication_pdu_t *src_pdu = &ind->rx_indication_body.rx_pdu_list[i];

      memcpy(dest_pdu, src_pdu, sizeof(*src_pdu));
      if(dest_pdu->rx_indication_rel8.length > 0){
        assert(dest_pdu->rx_indication_rel8.length <= NFAPI_RX_IND_DATA_MAX);
        memcpy(dest_pdu->rx_ind_data, src_pdu->rx_ind_data, dest_pdu->rx_indication_rel8.length);
      }

      LOG_D(PHY, "%s() NFAPI SFN/SF:%d PDUs:%zu [PDU:%d] handle:%d rnti:%04x length:%d offset:%d ul_cqi:%d ta:%d data:%p\n",
          __FUNCTION__,
          NFAPI_SFNSF2DEC(ind->sfn_sf), number_of_pdus, i,
          dest_pdu->rx_ue_information.handle,
          dest_pdu->rx_ue_information.rnti,
          dest_pdu->rx_indication_rel8.length,
          dest_pdu->rx_indication_rel8.offset,
          dest_pdu->rx_indication_rel8.ul_cqi,
          dest_pdu->rx_indication_rel8.timing_advance,
          dest_pdu->rx_ind_data
          );
    }
  }else{
  nfapi_rx_indication_t *dest_ind = &eNB->UL_INFO.rx_ind;
  nfapi_rx_indication_pdu_t *dest_pdu_list = eNB->rx_pdu_list;
  *dest_ind = *ind;
  dest_ind->rx_indication_body.rx_pdu_list = dest_pdu_list;

  assert(ind->rx_indication_body.number_of_pdus <= NFAPI_RX_IND_MAX_PDU);
  for(int i=0; i<ind->rx_indication_body.number_of_pdus; i++) {
    nfapi_rx_indication_pdu_t *dest_pdu = &dest_ind->rx_indication_body.rx_pdu_list[i];
    nfapi_rx_indication_pdu_t *src_pdu = &ind->rx_indication_body.rx_pdu_list[i];
    memcpy(dest_pdu, src_pdu, sizeof(*src_pdu));

    assert(dest_pdu->rx_indication_rel8.length <= NFAPI_RX_IND_DATA_MAX);
    memcpy(dest_pdu->rx_ind_data, src_pdu->rx_ind_data, dest_pdu->rx_indication_rel8.length);

    LOG_D(PHY, "%s() NFAPI SFN/SF:%d PDUs:%d [PDU:%d] handle:%d rnti:%04x length:%d offset:%d ul_cqi:%d ta:%d data:%p\n",
          __FUNCTION__,
          NFAPI_SFNSF2DEC(ind->sfn_sf), ind->rx_indication_body.number_of_pdus, i,
          dest_pdu->rx_ue_information.handle,
          dest_pdu->rx_ue_information.rnti,
          dest_pdu->rx_indication_rel8.length,
          dest_pdu->rx_indication_rel8.offset,
          dest_pdu->rx_indication_rel8.ul_cqi,
          dest_pdu->rx_indication_rel8.timing_advance,
          dest_pdu->rx_ind_data
         );
  }
  }
  AssertFatal(pthread_mutex_unlock(&eNB->UL_INFO_mutex)==0, "Mutex unlock failed");
  // vnf_p7_info* p7_vnf = (vnf_p7_info*)(config->user_data);
  //mac_rx_ind(p7_vnf->mac, ind);
  return 1;
}

int phy_nr_rx_data_indication(nfapi_nr_rx_data_indication_t *ind)
{
  LOG_D(NR_MAC,
        "In %s() NFAPI SFN/SF: %d/%d number_of_pdus :%u, and pdu %p\n",
        __FUNCTION__,
        ind->sfn,
        ind->slot,
        ind->number_of_pdus,
        ind->pdu_list[0].pdu);

  if (NFAPI_MODE == NFAPI_MODE_VNF || NFAPI_MODE == NFAPI_MODE_AERIAL) {
    nfapi_nr_rx_data_indication_t *rx_ind = CALLOC(1, sizeof(*rx_ind));
    copy_rx_data_indication(ind, rx_ind);
    if (!put_queue(&gnb_rx_ind_queue, rx_ind)) {
      LOG_E(NR_MAC, "Put_queue failed for rx_ind\n");
      free_rx_data_indication(rx_ind);
      free(rx_ind);
    }
  } else {
    LOG_E(NR_MAC, "NFAPI_MODE = %d not NFAPI_MODE_VNF(2)\n", nfapi_getmode());
  }
  return 1;
}

int phy_srs_indication(struct nfapi_vnf_p7_config *config, nfapi_srs_indication_t *ind) {
  UNUSED(config);
  UNUSED(ind);
  // vnf_p7_info* p7_vnf = (vnf_p7_info*)(config->user_data);
  //mac_srs_ind(p7_vnf->mac, ind);
  return 1;
}

int phy_sr_indication(struct nfapi_vnf_p7_config *config, nfapi_sr_indication_t *ind) {
  UNUSED(config);
  struct PHY_VARS_eNB_s *eNB = RC.eNB[0][0];
  LOG_D(MAC, "%s() NFAPI SFN/SF:%d srs:%d\n", __FUNCTION__, NFAPI_SFNSF2DEC(ind->sfn_sf), ind->sr_indication_body.number_of_srs);
  AssertFatal(pthread_mutex_lock(&eNB->UL_INFO_mutex)==0, "Mutex lock failed");
  if(NFAPI_MODE == NFAPI_MODE_VNF){
    int8_t index = NFAPI_SFNSF2SF(ind->sfn_sf);

    UL_RCC_INFO.sr_ind[index] = *ind;
    LOG_D(MAC,"%s() UL_INFO[%d].sr_ind.sr_indication_body.number_of_srs:%d\n", __FUNCTION__, index, eNB->UL_INFO.sr_ind.sr_indication_body.number_of_srs);
    if (ind->sr_indication_body.number_of_srs > 0) {
      assert(ind->sr_indication_body.number_of_srs <= NFAPI_SR_IND_MAX_PDU);
      UL_RCC_INFO.sr_ind[index].sr_indication_body.sr_pdu_list = malloc(sizeof(nfapi_sr_indication_pdu_t) * NFAPI_SR_IND_MAX_PDU);
    }

    assert(ind->sr_indication_body.number_of_srs <= NFAPI_SR_IND_MAX_PDU);
    for (int i=0; i<ind->sr_indication_body.number_of_srs; i++) {
        nfapi_sr_indication_pdu_t *dest_pdu = &UL_RCC_INFO.sr_ind[index].sr_indication_body.sr_pdu_list[i];
        nfapi_sr_indication_pdu_t *src_pdu = &ind->sr_indication_body.sr_pdu_list[i];

        LOG_D(MAC, "SR_IND[PDU:%d %d][rnti:%x cqi:%d channel:%d]\n", index, i, src_pdu->rx_ue_information.rnti, src_pdu->ul_cqi_information.ul_cqi, src_pdu->ul_cqi_information.channel);

        memcpy(dest_pdu, src_pdu, sizeof(*src_pdu));
    }
  }else{
  nfapi_sr_indication_t *dest_ind = &eNB->UL_INFO.sr_ind;
  nfapi_sr_indication_pdu_t *dest_pdu_list = eNB->sr_pdu_list;
  *dest_ind = *ind;
  dest_ind->sr_indication_body.sr_pdu_list = dest_pdu_list;
  LOG_D(MAC,"%s() eNB->UL_INFO.sr_ind.sr_indication_body.number_of_srs:%d\n", __FUNCTION__, eNB->UL_INFO.sr_ind.sr_indication_body.number_of_srs);

  assert(eNB->UL_INFO.sr_ind.sr_indication_body.number_of_srs <= NFAPI_SR_IND_MAX_PDU);
  for (int i=0; i<eNB->UL_INFO.sr_ind.sr_indication_body.number_of_srs; i++) {
    nfapi_sr_indication_pdu_t *dest_pdu = &dest_ind->sr_indication_body.sr_pdu_list[i];
    nfapi_sr_indication_pdu_t *src_pdu = &ind->sr_indication_body.sr_pdu_list[i];
    LOG_D(MAC, "SR_IND[PDU:%d][rnti:%x cqi:%d channel:%d]\n", i, src_pdu->rx_ue_information.rnti, src_pdu->ul_cqi_information.ul_cqi, src_pdu->ul_cqi_information.channel);
    memcpy(dest_pdu, src_pdu, sizeof(*src_pdu));
  }
  }
  AssertFatal(pthread_mutex_unlock(&eNB->UL_INFO_mutex)==0, "Mutex unlock failed");
  // vnf_p7_info* p7_vnf = (vnf_p7_info*)(config->user_data);
  //mac_sr_ind(p7_vnf->mac, ind);
  return 1;
}

static bool is_ue_same(uint16_t ue_id_1, uint16_t ue_id_2)
{
  return (ue_id_1 == ue_id_2);
}

static void analyze_cqi_pdus_for_duplicates(nfapi_cqi_indication_t *ind)
{
  uint16_t num_cqis = ind->cqi_indication_body.number_of_cqis;
  assert(num_cqis <= NFAPI_CQI_IND_MAX_PDU);
  for (int i = 0; i < num_cqis; i++)
  {
    nfapi_cqi_indication_pdu_t *src_pdu = &ind->cqi_indication_body.cqi_pdu_list[i];

    LOG_D(MAC, "CQI_IND[PDU:%d][rnti:%x cqi:%d channel:%d]\n", i, src_pdu->rx_ue_information.rnti,
          src_pdu->ul_cqi_information.ul_cqi, src_pdu->ul_cqi_information.channel);

    for (int j = i + 1; j < num_cqis; j++)
    {
      uint16_t rnti_i = ind->cqi_indication_body.cqi_pdu_list[i].rx_ue_information.rnti;
      uint16_t rnti_j = ind->cqi_indication_body.cqi_pdu_list[j].rx_ue_information.rnti;
      if (is_ue_same(rnti_i, rnti_j))
      {
        LOG_E(MAC, "Problem, two cqis received from a single UE for rnti %x\n",
              rnti_i);
        //abort(); This will be fixed in merge request which handles multiple CQIs.
      }
    }
  }
}

int phy_cqi_indication(struct nfapi_vnf_p7_config *config, nfapi_cqi_indication_t *ind) {
  UNUSED(config);
  // vnf_p7_info* p7_vnf = (vnf_p7_info*)(config->user_data);
  //mac_cqi_ind(p7_vnf->mac, ind);
  struct PHY_VARS_eNB_s *eNB = RC.eNB[0][0];
  LOG_D(MAC, "%s() NFAPI SFN/SF:%d number_of_cqis:%u\n", __FUNCTION__, NFAPI_SFNSF2DEC(ind->sfn_sf), ind->cqi_indication_body.number_of_cqis);
  AssertFatal(pthread_mutex_lock(&eNB->UL_INFO_mutex)==0, "Mutex lock failed");
  if(NFAPI_MODE == NFAPI_MODE_VNF){
    int8_t index = NFAPI_SFNSF2SF(ind->sfn_sf);

    UL_RCC_INFO.cqi_ind[index] = *ind;
    assert(ind->cqi_indication_body.number_of_cqis <= NFAPI_CQI_IND_MAX_PDU);
    if (ind->cqi_indication_body.number_of_cqis > 0){
      UL_RCC_INFO.cqi_ind[index].cqi_indication_body.cqi_pdu_list =
        malloc(sizeof(nfapi_cqi_indication_pdu_t) * NFAPI_CQI_IND_MAX_PDU);
      UL_RCC_INFO.cqi_ind[index].cqi_indication_body.cqi_raw_pdu_list =
        malloc(sizeof(nfapi_cqi_indication_raw_pdu_t) * NFAPI_CQI_IND_MAX_PDU);
    }

    analyze_cqi_pdus_for_duplicates(ind);

    assert(ind->cqi_indication_body.number_of_cqis <= NFAPI_CQI_IND_MAX_PDU);
    for (int i=0; i<ind->cqi_indication_body.number_of_cqis; i++) {
        nfapi_cqi_indication_pdu_t *src_pdu = &ind->cqi_indication_body.cqi_pdu_list[i];
        LOG_D(MAC, "SR_IND[PDU:%d][rnti:%x cqi:%d channel:%d]\n", i, src_pdu->rx_ue_information.rnti,
                    src_pdu->ul_cqi_information.ul_cqi, src_pdu->ul_cqi_information.channel);
        memcpy(&UL_RCC_INFO.cqi_ind[index].cqi_indication_body.cqi_pdu_list[i],
               src_pdu, sizeof(nfapi_cqi_indication_pdu_t));

        memcpy(&UL_RCC_INFO.cqi_ind[index].cqi_indication_body.cqi_raw_pdu_list[i],
               &ind->cqi_indication_body.cqi_raw_pdu_list[i], sizeof(nfapi_cqi_indication_raw_pdu_t));
    }
  }else{
  nfapi_cqi_indication_t *dest_ind = &eNB->UL_INFO.cqi_ind;
  *dest_ind = *ind;
  dest_ind->cqi_indication_body.cqi_pdu_list = ind->cqi_indication_body.cqi_pdu_list;
  dest_ind->cqi_indication_body.cqi_raw_pdu_list = ind->cqi_indication_body.cqi_raw_pdu_list;
  assert(ind->cqi_indication_body.number_of_cqis <= NFAPI_CQI_IND_MAX_PDU);
  for(int i=0; i<ind->cqi_indication_body.number_of_cqis; i++) {
    nfapi_cqi_indication_pdu_t *src_pdu = &ind->cqi_indication_body.cqi_pdu_list[i];
    LOG_D(MAC, "CQI_IND[PDU:%d][rnti:%x cqi:%d channel:%d]\n", i, src_pdu->rx_ue_information.rnti,
                src_pdu->ul_cqi_information.ul_cqi, src_pdu->ul_cqi_information.channel);
    memcpy(&dest_ind->cqi_indication_body.cqi_pdu_list[i],
           src_pdu, sizeof(nfapi_cqi_indication_pdu_t));
    memcpy(&dest_ind->cqi_indication_body.cqi_raw_pdu_list[i],
           &ind->cqi_indication_body.cqi_raw_pdu_list[i], sizeof(nfapi_cqi_indication_raw_pdu_t));
  }
  }
  AssertFatal(pthread_mutex_unlock(&eNB->UL_INFO_mutex)==0, "Mutex unlock failed");
  return 1;
}

//NR phy indication


int oai_nfapi_dl_tti_req(nfapi_nr_dl_tti_request_t *dl_config_req);
int oai_nfapi_ul_tti_req(nfapi_nr_ul_tti_request_t *ul_tti_req);
int oai_nfapi_tx_data_req(nfapi_nr_tx_data_request_t* tx_data_req);
int oai_nfapi_ul_dci_req(nfapi_nr_ul_dci_request_t* ul_dci_req);

int phy_nr_slot_indication(nfapi_nr_slot_indication_scf_t *ind)
{
  LOG_D(MAC, "VNF SFN/Slot %d.%d \n", ind->sfn, ind->slot);

  // this variable is very big (multiple MB), so we put it into static storage
  // to not overflow the stack while still having it in local (function) scope
  // also, phy_nr_slot_indication() is only executed by one thread, serially
  static NR_Sched_Rsp_t sched_response;
  NR_IF_Module_t *ifi = RC.nrmac[0]->if_inst;
  ifi->NR_slot_indication(ind, &sched_response);

#ifdef ENABLE_AERIAL
    bool send_slt_resp = false;
    if (sched_response.DL_req.dl_tti_request_body.nPDUs> 0) {
      oai_fapi_dl_tti_req(&sched_response.DL_req);
      send_slt_resp = true;
    }
    if (sched_response.UL_tti_req.n_pdus > 0) {
      oai_fapi_ul_tti_req(&sched_response.UL_tti_req);
      send_slt_resp = true;
    }
    if (sched_response.TX_req.Number_of_PDUs > 0) {
      oai_fapi_tx_data_req(&sched_response.TX_req);
      send_slt_resp = true;
    }
    if (sched_response.UL_dci_req.numPdus > 0) {
      oai_fapi_ul_dci_req(&sched_response.UL_dci_req);
      send_slt_resp = true;
    }
    if (send_slt_resp) {
      oai_fapi_send_end_request(ind->sfn, ind->slot);
    }
#else
  if (sched_response.DL_req.dl_tti_request_body.nPDUs > 0)
    oai_nfapi_dl_tti_req(&sched_response.DL_req);

  if (sched_response.UL_tti_req.n_pdus > 0)
    oai_nfapi_ul_tti_req(&sched_response.UL_tti_req);

  if (sched_response.TX_req.Number_of_PDUs > 0)
    oai_nfapi_tx_data_req(&sched_response.TX_req);

  if (sched_response.UL_dci_req.numPdus > 0)
    oai_nfapi_ul_dci_req(&sched_response.UL_dci_req);
#endif

  /* the below works because the function behind the callback collects
   * messages from queue into which messages have been copied.
   * TODO we should have different callbacks for received messages and call
   * into the scheduler separately for each message instead of one big one. */
  NR_UL_IND_t ul_ind = {.frame = ind->sfn, .slot = ind->slot, };
  ifi->NR_UL_indication(&ul_ind);

  return 1;
}

#ifndef ENABLE_WLS
static inline void timespec_add_us(struct timespec *t, long us)
{
  t->tv_nsec += us * 1000;
  if (t->tv_nsec >= 1000000000) {
    t->tv_sec += t->tv_nsec / 1000000000;
    t->tv_nsec %= 1000000000;
  } else if (t->tv_nsec < 0) {
    long sec_diff = (-t->tv_nsec / 1000000000) + 1;
    t->tv_sec -= sec_diff;
    t->tv_nsec += sec_diff * 1000000000;
  }
}
#define P7_SYNC_PERIOD_SLOTS_DEFAULT 80
int vnf_nr_build_send_dl_node_sync(vnf_p7_t* vnf_p7, nfapi_vnf_p7_connection_info_t* p7_info);

static inline void p7_sync_init(nfapi_vnf_p7_connection_info_t *p7_info)
{
  p7_info->sync_slot_counter = 0;
  p7_info->sync_period_slots = P7_SYNC_PERIOD_SLOTS_DEFAULT;
  p7_info->consecutive_drift_violations = 0;
  p7_info->nr_offset_filtered = 0;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[P7_SYNC] Initialized: period=%u slots\n", p7_info->sync_period_slots);
}

void *vnf_timing_thread(void *arg)
{
  vnf_p7_info *p7_vnf = (vnf_p7_info *)arg;
  vnf_p7_t *vnf_p7 = (vnf_p7_t *)p7_vnf->config;

  int mu = -1;
  nfapi_vnf_p7_connection_info_t *p7_info = NULL;

  while (1) {
    if (__atomic_load_n(&nr_start_resp_received, __ATOMIC_ACQUIRE)) {
      if (vnf_p7->p7_connections) {
        p7_info = vnf_p7->p7_connections;
        if (RC.nrmac && RC.nrmac[0]) {
          nfapi_nr_config_request_scf_t *req = &RC.nrmac[0]->config[0];
          const nfapi_uint8_tlv_t *scs = &req->ssb_config.scs_common;
          if (scs && scs->tl.tag == NFAPI_NR_CONFIG_SCS_COMMON_TAG) {
            mu = scs->value;
          }
        }
        if (mu < 0 && RC.gNB && RC.gNB[0] && RC.gNB[0]->configured && RC.gNB[0]->frame_parms.numerology_index >= 0) {
          mu = RC.gNB[0]->frame_parms.numerology_index;
        }
        if (mu >= 0) {
          break;
        }
      }
    }
    usleep(1000000); // poll once per second until the gNB numerology (mu) is configured
  }
  pthread_mutex_lock(&p7_info->mutex);
  while (!p7_info->initial_timinginfo_received) {
    pthread_cond_wait(&p7_info->initial_timinginfo_cond, &p7_info->mutex);
  }
  pthread_mutex_unlock(&p7_info->mutex);
  DevAssert(mu >= 0 && mu <= 5);
  p7_info->mu = mu;
  p7_info->slot_duration_us = 1000 >> p7_info->mu;
  LOG_I(NFAPI_VNF, "Starting VNF autonomous timing thread: mu = %d, slot duration = %d us\n", mu, p7_info->slot_duration_us);
  if (p7_info->initial_timinginfo_received) {
    int sfnslot_dec = NFAPI_SFNSLOT2DEC(p7_info->mu, p7_info->sfn, p7_info->slot);
    sfnslot_dec = (sfnslot_dec + 1) % NFAPI_MAX_SFNSLOTDEC(p7_info->mu);
    p7_info->sfn = NFAPI_SFNSLOTDEC2SFN(p7_info->mu, sfnslot_dec);
    p7_info->slot = NFAPI_SFNSLOTDEC2SLOT(p7_info->mu, sfnslot_dec);
  }
  p7_info->running = 1;
  p7_info->thread = pthread_self();
  p7_sync_init(p7_info);
  clock_gettime(CLOCK_MONOTONIC, &p7_info->next_slot_time);
  vnf_p7->slot_start_time_hr = vnf_get_current_time_hr();
  vnf_nr_build_send_dl_node_sync(vnf_p7, p7_info);

  const int max_sfnslotdec = NFAPI_MAX_SFNSLOTDEC(p7_info->mu);
  int last_mac_ind_dec = -1;

  int sfnslot_dec = NFAPI_SFNSLOT2DEC(p7_info->mu, p7_info->sfn, p7_info->slot);

  while (p7_info->running) {
    pthread_mutex_lock(&p7_info->mutex);
    if (p7_info->slot_adjustment != 0) {
      sfnslot_dec = (sfnslot_dec + p7_info->slot_adjustment + max_sfnslotdec) % max_sfnslotdec;
      p7_info->slot_adjustment = 0;
    }
    int32_t current_pending_us = p7_info->pending_us;
    p7_info->pending_us = 0;
    pthread_mutex_unlock(&p7_info->mutex);

    timespec_add_us(&p7_info->next_slot_time, p7_info->slot_duration_us + current_pending_us);
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    int64_t diff_ns = (p7_info->next_slot_time.tv_sec - now.tv_sec) * 1000000000LL
                      + (p7_info->next_slot_time.tv_nsec - now.tv_nsec);
    const int64_t extreme_lag_threshold_ns = (int64_t)p7_info->slot_duration_us * 5 * 1000LL;
    if (diff_ns < -extreme_lag_threshold_ns) {
      // next_slot_time is in the past by more than 5 slots!
      // Yield CPU to prevent starvation of the SCTP/UDP network thread under extreme lag.
      sched_yield();
    }
    if (clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &p7_info->next_slot_time, NULL) != 0)
      continue;
    vnf_p7->slot_start_time_hr = vnf_get_current_time_hr();
    pthread_mutex_lock(&p7_info->mutex);
    p7_info->sfn = NFAPI_SFNSLOTDEC2SFN(p7_info->mu, sfnslot_dec);
    p7_info->slot = NFAPI_SFNSLOTDEC2SLOT(p7_info->mu, sfnslot_dec);
    /* slot_ahead is updated by vnf_nr_delay_management under the same mutex */
    int32_t slot_ahead = p7_info->slot_ahead;
    pthread_mutex_unlock(&p7_info->mutex);

    if (p7_info->sync_slot_counter >= p7_info->sync_period_slots) {
      p7_info->sync_slot_counter = 0;
      vnf_nr_build_send_dl_node_sync(vnf_p7, p7_info);
    } else {
      p7_info->sync_slot_counter++;
    }

    int target_ind_dec = (sfnslot_dec + slot_ahead) % max_sfnslotdec;
    if (last_mac_ind_dec == -1) {
      last_mac_ind_dec = (target_ind_dec - 1 + max_sfnslotdec) % max_sfnslotdec;
    }

    int diff_mac = (target_ind_dec - last_mac_ind_dec + max_sfnslotdec) % max_sfnslotdec;
    if (diff_mac > 0 && diff_mac < max_sfnslotdec / 2) {
      // NEVER skip slots! Skipping slots breaks MAC scheduling (e.g. RACH, HARQ timing assertions) and drops UE.
      // Catch up in smooth bursts up to max_burst. If a huge drift happens during iperf CPU starvation,
      // generating backlog sequentially is much safer than jumping.
      int burst_counter = 0;
      const int max_burst = 10 << p7_info->mu;
      while (last_mac_ind_dec != target_ind_dec && burst_counter < max_burst) {
        last_mac_ind_dec = (last_mac_ind_dec + 1) % max_sfnslotdec;
        nfapi_nr_slot_indication_scf_t ind = {0};
        ind.sfn = NFAPI_SFNSLOTDEC2SFN(p7_info->mu, last_mac_ind_dec);
        ind.slot = NFAPI_SFNSLOTDEC2SLOT(p7_info->mu, last_mac_ind_dec);
        ind.header.phy_id = p7_info->phy_id;
        phy_nr_slot_indication(&ind);
        burst_counter++;
      }
    }
    sfnslot_dec = (sfnslot_dec + 1) % max_sfnslotdec;
  }
  return NULL;
}
#endif

int phy_nr_srs_indication(nfapi_nr_srs_indication_t *ind)
{
  for (int i = 0; i < ind->number_of_pdus; ++i)
    handle_nr_srs_measurements(0, ind->sfn, ind->slot, &ind->pdu_list[i]);
  return 1;
}
//end NR phy indication

int phy_lbt_dl_indication(struct nfapi_vnf_p7_config *config, nfapi_lbt_dl_indication_t *ind) {
  UNUSED(config);
  UNUSED(ind);
  // vnf_p7_info* p7_vnf = (vnf_p7_info*)(config->user_data);
  //mac_lbt_dl_ind(p7_vnf->mac, ind);
  return 1;
}

int phy_nb_harq_indication(struct nfapi_vnf_p7_config *config, nfapi_nb_harq_indication_t *ind) {
  UNUSED(config);
  UNUSED(ind);
  // vnf_p7_info* p7_vnf = (vnf_p7_info*)(config->user_data);
  //mac_nb_harq_ind(p7_vnf->mac, ind);
  return 1;
}

int phy_nrach_indication(struct nfapi_vnf_p7_config *config, nfapi_nrach_indication_t *ind) {
  UNUSED(config);
  UNUSED(ind);
  // vnf_p7_info* p7_vnf = (vnf_p7_info*)(config->user_data);
  //mac_nrach_ind(p7_vnf->mac, ind);
  return 1;
}

void *vnf_allocate(size_t size) {
  //return (void*)memory_pool::allocate(size);
  return (void *)malloc(size);
}

void vnf_deallocate(void *ptr) {
  //memory_pool::deallocate((uint8_t*)ptr);
  free(ptr);
}

void *vnf_nr_allocate(size_t size)
{
  // return (void*)memory_pool::allocate(size);
  return (void *)malloc(size);
}

void vnf_nr_deallocate(void *ptr)
{
  // memory_pool::deallocate((uint8_t*)ptr);
  free(ptr);
}

void vnf_trace(nfapi_trace_level_t nfapi_level, const char *message, ...) {
  va_list args;
  va_start(args, message);
  VLOG( NFAPI_VNF, nfapitooai_level(nfapi_level), message, args);
  va_end(args);
}

int phy_vendor_ext(struct nfapi_vnf_p7_config *config, void *msg)
{
  UNUSED(config);
  if (((nfapi_p7_message_header_t *)msg)->message_id == P7_VENDOR_EXT_IND) {
    // vendor_ext_p7_ind* ind = (vendor_ext_p7_ind*)msg;
    // NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] vendor_ext (error_code:%d)\n", ind->error_code);
  } else {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] unknown %02x\n", ((nfapi_p7_message_header_t *)msg)->message_id);
  }

  return 0;
}

int phy_nr_vendor_ext(struct nfapi_vnf_p7_config *config, void *msg)
{
  UNUSED(config);
  if (((nfapi_nr_p7_message_header_t *)msg)->message_id == P7_VENDOR_EXT_IND) {
    // vendor_ext_p7_ind* ind = (vendor_ext_p7_ind*)msg;
    // NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] vendor_ext (error_code:%d)\n", ind->error_code);
  } else {
    NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] unknown %02x\n", ((nfapi_nr_p7_message_header_t *)msg)->message_id);
  }

  return 0;
}

void *phy_allocate_p7_vendor_ext(uint16_t message_id, uint16_t *msg_size) {
  if(message_id == P7_VENDOR_EXT_IND) {
    *msg_size = sizeof(vendor_ext_p7_ind);
    return (nfapi_p7_message_header_t *)malloc(sizeof(vendor_ext_p7_ind));
  }

  return 0;
}

void phy_deallocate_p7_vendor_ext(void *header)
{
  free(header);
}

void *phy_nr_allocate_p7_vendor_ext(uint16_t message_id, uint16_t *msg_size)
{
  if (message_id == P7_VENDOR_EXT_IND) {
    *msg_size = sizeof(vendor_ext_p7_ind);
    return (nfapi_p7_message_header_t *)malloc(sizeof(vendor_ext_p7_ind));
  }

  return 0;
}

void phy_nr_deallocate_p7_vendor_ext(void *header)
{
  free(header);
}

int phy_unpack_vendor_extension_tlv(nfapi_tl_t *tl, uint8_t **ppReadPackedMessage, uint8_t *end, void **ve, nfapi_p7_codec_config_t *codec) {
  UNUSED(tl);
  UNUSED(ppReadPackedMessage);
  UNUSED(end);
  UNUSED(ve);
  UNUSED(codec);
  return -1;
}

int phy_nr_unpack_vendor_extension_tlv(nfapi_tl_t *tl,
                                       uint8_t **ppReadPackedMessage,
                                       uint8_t *end,
                                       void **ve,
                                       nfapi_p7_codec_config_t *codec)
{
  UNUSED(tl);
  UNUSED(ppReadPackedMessage);
  UNUSED(end);
  UNUSED(ve);
  UNUSED(codec);
  return -1;
}

int phy_pack_vendor_extension_tlv(void *ve, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *codec) {
  UNUSED(codec);
  //NFAPI_TRACE(NFAPI_TRACE_INFO, "phy_pack_vendor_extension_tlv\n");
  nfapi_tl_t *tlv = (nfapi_tl_t *)ve;

  switch(tlv->tag) {
    case VENDOR_EXT_TLV_1_TAG: {
      //NFAPI_TRACE(NFAPI_TRACE_INFO, "Packing VENDOR_EXT_TLV_1\n");
      vendor_ext_tlv_1 *ve = (vendor_ext_tlv_1 *)tlv;

      if(!push32(ve->dummy, ppWritePackedMsg, end))
        return 0;

      return 1;
    }
    break;

    default:
      return -1;
      break;
  }
}

int phy_nr_pack_vendor_extension_tlv(void *ve, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *codec)
{
  UNUSED(codec);
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "phy_pack_vendor_extension_tlv\n");
  nfapi_tl_t *tlv = (nfapi_tl_t *)ve;

  switch (tlv->tag) {
    case VENDOR_EXT_TLV_1_TAG: {
      // NFAPI_TRACE(NFAPI_TRACE_INFO, "Packing VENDOR_EXT_TLV_1\n");
      vendor_ext_tlv_1 *ve = (vendor_ext_tlv_1 *)tlv;

      if (!push32(ve->dummy, ppWritePackedMsg, end))
        return 0;

      return 1;
    } break;

    default:
      return -1;
      break;
  }
}
int phy_unpack_p7_vendor_extension(void *header, uint8_t **ppReadPackedMessage, uint8_t *end, nfapi_p7_codec_config_t *config)
{
  UNUSED(config);
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  if (((nfapi_p7_message_header_t *)header)->message_id == P7_VENDOR_EXT_IND) {
    vendor_ext_p7_ind *req = (vendor_ext_p7_ind *)(header);

    if (!pull16(ppReadPackedMessage, &req->error_code, end))
      return 0;
  }

  return 1;
}

int phy_nr_unpack_p7_vendor_extension(void *header, uint8_t **ppReadPackedMessage, uint8_t *end, nfapi_p7_codec_config_t *config)
{
  UNUSED(config);
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  if (((nfapi_nr_p7_message_header_t *)header)->message_id == P7_VENDOR_EXT_IND) {
    vendor_ext_p7_ind *req = (vendor_ext_p7_ind *)(header);

    if (!pull16(ppReadPackedMessage, &req->error_code, end))
      return 0;
  }

  return 1;
}

int phy_pack_p7_vendor_extension(void *header, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config)
{
  UNUSED(config);
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  if (((nfapi_p7_message_header_t *)header)->message_id == P7_VENDOR_EXT_REQ) {
    // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
    vendor_ext_p7_req *req = (vendor_ext_p7_req *)(header);

    if (!(push16(req->dummy1, ppWritePackedMsg, end) && push16(req->dummy2, ppWritePackedMsg, end)))
      return 0;
  }

  return 1;
}

int phy_nr_pack_p7_vendor_extension(void *header, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config)
{
  UNUSED(config);
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  if (((nfapi_nr_p7_message_header_t *)header)->message_id == P7_VENDOR_EXT_REQ) {
    // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
    vendor_ext_p7_req *req = (vendor_ext_p7_req *)(header);

    if (!(push16(req->dummy1, ppWritePackedMsg, end) && push16(req->dummy2, ppWritePackedMsg, end)))
      return 0;
  }

  return 1;
}

int vnf_pack_p4_p5_vendor_extension(void *header, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p4_p5_codec_config_t *codec)
{
  UNUSED(codec);
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  if (((nfapi_p4_p5_message_header_t *)header)->message_id == P5_VENDOR_EXT_REQ) {
    vendor_ext_p5_req *req = (vendor_ext_p5_req *)(header);
    // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s %d %d\n", __FUNCTION__, req->dummy1, req->dummy2);
    return (!(push16(req->dummy1, ppWritePackedMsg, end) && push16(req->dummy2, ppWritePackedMsg, end)));
  }

  return 0;
}

int vnf_nr_pack_p4_p5_vendor_extension(void *header, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p4_p5_codec_config_t *codec)
{
  UNUSED(codec);
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  if (((nfapi_nr_p4_p5_message_header_t *)header)->message_id == P5_VENDOR_EXT_REQ) {
    vendor_ext_p5_req *req = (vendor_ext_p5_req *)(header);
    // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s %d %d\n", __FUNCTION__, req->dummy1, req->dummy2);
    return (!(push16(req->dummy1, ppWritePackedMsg, end) && push16(req->dummy2, ppWritePackedMsg, end)));
  }

  return 0;
}

static pthread_t vnf_p5_init_and_receive_pthread;
static pthread_t vnf_p7_start_pthread;


void *vnf_p7_start_thread(void *ptr) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "%s()\n", __FUNCTION__);
  pthread_setname_np(pthread_self(), "VNF_P7");
  nfapi_vnf_p7_config_t *config = (nfapi_vnf_p7_config_t *)ptr;
  nfapi_vnf_p7_start(config);
  return config;
}

void *configure_nr_p7_vnf(void *ptr)
{
  init_queue(&gnb_rach_ind_queue);
  init_queue(&gnb_rx_ind_queue);
  init_queue(&gnb_crc_ind_queue);
  init_queue(&gnb_uci_ind_queue);

  vnf_p7_info *p7_vnf = (vnf_p7_info *)ptr;
  p7_vnf->config->port = p7_vnf->local_port;
#ifndef ENABLE_AERIAL
  p7_vnf->config->sync_indication = &phy_sync_indication;
  p7_vnf->config->slot_indication = NULL;
  p7_vnf->config->harq_indication = &phy_harq_indication;
#endif
  p7_vnf->config->nr_crc_indication = &phy_nr_crc_indication;
  p7_vnf->config->nr_rx_data_indication = &phy_nr_rx_data_indication;
  p7_vnf->config->nr_rach_indication = &phy_nr_rach_indication;
  p7_vnf->config->nr_uci_indication = &phy_nr_uci_indication;
#ifndef ENABLE_AERIAL
  p7_vnf->config->srs_indication = &phy_srs_indication;
  p7_vnf->config->sr_indication = &phy_sr_indication;
  p7_vnf->config->cqi_indication = &phy_cqi_indication;
  p7_vnf->config->lbt_dl_indication = &phy_lbt_dl_indication;
  p7_vnf->config->nb_harq_indication = &phy_nb_harq_indication;
  p7_vnf->config->nrach_indication = &phy_nrach_indication;
#endif
  p7_vnf->config->nr_slot_indication = &phy_nr_slot_indication;
  p7_vnf->config->nr_srs_indication = &phy_nr_srs_indication;
  p7_vnf->config->malloc = &vnf_allocate;
  p7_vnf->config->free = &vnf_deallocate;
  p7_vnf->config->vendor_ext = &phy_nr_vendor_ext;
  p7_vnf->config->user_data = p7_vnf;
  p7_vnf->mac->user_data = p7_vnf;
  p7_vnf->config->codec_config.unpack_p7_vendor_extension = &phy_nr_unpack_p7_vendor_extension;
  p7_vnf->config->codec_config.pack_p7_vendor_extension = &phy_nr_pack_p7_vendor_extension;
  p7_vnf->config->codec_config.unpack_vendor_extension_tlv = &phy_nr_unpack_vendor_extension_tlv;
  p7_vnf->config->codec_config.pack_vendor_extension_tlv = &phy_nr_pack_vendor_extension_tlv;
  p7_vnf->config->codec_config.allocate = &vnf_allocate;
  p7_vnf->config->codec_config.deallocate = &vnf_deallocate;
  p7_vnf->config->allocate_p7_vendor_ext = &phy_nr_allocate_p7_vendor_ext;
  p7_vnf->config->deallocate_p7_vendor_ext = &phy_nr_deallocate_p7_vendor_ext;

#ifdef ENABLE_WLS
  p7_vnf->config->unpack_func = &fapi_nr_p7_message_unpack;
  p7_vnf->config->hdr_unpack_func = &fapi_nr_p7_message_header_unpack;
  p7_vnf->config->pack_func = &fapi_nr_p7_message_pack;
  p7_vnf->config->send_p7_msg = &wls_vnf_nr_send_p7_message;
#endif

#ifdef ENABLE_SOCKET
  p7_vnf->config->unpack_func = &nfapi_nr_p7_message_unpack;
  p7_vnf->config->hdr_unpack_func = &nfapi_nr_p7_message_header_unpack;
  p7_vnf->config->pack_func = &nfapi_nr_p7_message_pack;
  p7_vnf->config->send_p7_msg = &vnf_nr_send_p7_msg;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Creating VNF NFAPI P7 start thread %s\n", __FUNCTION__);
  threadCreate(&vnf_p7_start_pthread, &vnf_nr_start_p7_thread, p7_vnf->config, "vnf_p7_thread", -1, OAI_PRIORITY_RT);
#endif

#ifdef ENABLE_AERIAL
  p7_vnf->config->unpack_func = &fapi_nr_p7_message_unpack;
  p7_vnf->config->hdr_unpack_func = &fapi_nr_p7_message_header_unpack;
  p7_vnf->config->pack_func = &fapi_nr_p7_message_pack;
  p7_vnf->config->send_p7_msg = &aerial_nr_send_p7_message;
#endif
#ifndef ENABLE_WLS
  // Start VNF autonomous timing thread
  pthread_t t;
  threadCreate(&t, &vnf_timing_thread, p7_vnf, "vnf_timing", -1, OAI_PRIORITY_RT_MAX);
#endif
  return 0;
}

void *vnf_p7_thread_start(void *ptr) {
  vnf_p7_info *p7_vnf = (vnf_p7_info *)ptr;
  p7_vnf->config->port = p7_vnf->local_port;
  p7_vnf->config->sync_indication = &phy_sync_indication;
  p7_vnf->config->subframe_indication = &phy_subframe_indication;
  p7_vnf->config->slot_indication = NULL;

  p7_vnf->config->harq_indication = &phy_harq_indication;
  p7_vnf->config->crc_indication = &phy_crc_indication;
  p7_vnf->config->rx_indication = &phy_rx_indication;
  p7_vnf->config->rach_indication = &phy_rach_indication;
  p7_vnf->config->srs_indication = &phy_srs_indication;
  p7_vnf->config->sr_indication = &phy_sr_indication;
  p7_vnf->config->cqi_indication = &phy_cqi_indication;
  p7_vnf->config->lbt_dl_indication = &phy_lbt_dl_indication;
  p7_vnf->config->nb_harq_indication = &phy_nb_harq_indication;
  p7_vnf->config->nrach_indication = &phy_nrach_indication;
  p7_vnf->config->malloc = &vnf_allocate;
  p7_vnf->config->free = &vnf_deallocate;
  p7_vnf->config->vendor_ext = &phy_vendor_ext;
  p7_vnf->config->user_data = p7_vnf;
  p7_vnf->mac->user_data = p7_vnf;
  p7_vnf->config->codec_config.unpack_p7_vendor_extension = &phy_unpack_p7_vendor_extension;
  p7_vnf->config->codec_config.pack_p7_vendor_extension = &phy_pack_p7_vendor_extension;
  p7_vnf->config->codec_config.unpack_vendor_extension_tlv = &phy_unpack_vendor_extension_tlv;
  p7_vnf->config->codec_config.pack_vendor_extension_tlv = &phy_pack_vendor_extension_tlv;
  p7_vnf->config->codec_config.allocate = &vnf_allocate;
  p7_vnf->config->codec_config.deallocate = &vnf_deallocate;
  p7_vnf->config->allocate_p7_vendor_ext = &phy_allocate_p7_vendor_ext;
  p7_vnf->config->deallocate_p7_vendor_ext = &phy_deallocate_p7_vendor_ext;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Creating VNF NFAPI start thread %s\n", __FUNCTION__);
  pthread_create(&vnf_p7_start_pthread, NULL, &vnf_p7_start_thread, p7_vnf->config);
  return 0;
}

int pnf_nr_start_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_nr_pnf_start_response_t *resp) {
  UNUSED(resp);
  vnf_info *vnf = (vnf_info *)(config->user_data);
  vnf_p7_info *p7_vnf = vnf->p7_vnfs;
  pnf_info *pnf = vnf->pnfs;
  nfapi_nr_param_request_scf_t req;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] pnf start response idx:%d config:%p user_data:%p p7_vnf[config:%p thread_started:%d]\n", p5_idx, config, config->user_data, vnf->p7_vnfs[0].config, vnf->p7_vnfs[0].thread_started);

  if(p7_vnf->thread_started == 0) {
    configure_nr_p7_vnf(p7_vnf);
    p7_vnf->thread_started = 1;
  } else {
    // P7 thread already running.
  }

  // start all the phys in the pnf.
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Sending NFAPI_VNF_PARAM_REQUEST phy_id:%d\n", pnf->phys[0].id);
  memset(&req, 0, sizeof(req));
  req.header.message_id = NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST;
#ifdef ENABLE_WLS
  req.header.phy_id = 0;
#endif
#ifdef ENABLE_SOCKET
  req.header.phy_id = pnf->phys[0].id;
#endif
  nfapi_nr_vnf_param_req(config, p5_idx, &req);
  return 0;
}

int pnf_start_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_pnf_start_response_t *resp) {
  UNUSED(resp);
  vnf_info *vnf = (vnf_info *)(config->user_data);
  vnf_p7_info *p7_vnf = vnf->p7_vnfs;
  pnf_info *pnf = vnf->pnfs;
  nfapi_param_request_t req;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] pnf start response idx:%d config:%p user_data:%p p7_vnf[config:%p thread_started:%d]\n", p5_idx, config, config->user_data, vnf->p7_vnfs[0].config, vnf->p7_vnfs[0].thread_started);

  if(p7_vnf->thread_started == 0) {
    pthread_t vnf_p7_thread;
    threadCreate(&vnf_p7_thread, &vnf_p7_thread_start, p7_vnf, "vnf_p7_thread", -1, OAI_PRIORITY_RT);
    p7_vnf->thread_started = 1;
  } else {
    // P7 thread already running.
  }

  // start all the phys in the pnf.
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Sending NFAPI_VNF_PARAM_REQUEST phy_id:%d\n", pnf->phys[0].id);
  memset(&req, 0, sizeof(req));
  req.header.message_id = NFAPI_PARAM_REQUEST;
  req.header.phy_id = pnf->phys[0].id;
  nfapi_vnf_param_req(config, p5_idx, &req);
  return 0;
}

extern uint32_t to_earfcn(int eutra_bandP,uint32_t dl_CarrierFreq,uint32_t bw);

int nr_param_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_nr_param_response_scf_t *resp) {

  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Received NFAPI_PARAM_RESP idx:%d phy_id:%d\n", p5_idx, resp->header.phy_id);
  vnf_info *vnf = (vnf_info *)(config->user_data);
  vnf_p7_info *p7_vnf = vnf->p7_vnfs;
  pnf_info *pnf = vnf->pnfs;
  phy_info *phy = pnf->phys;
  nfapi_nr_config_request_scf_t *req = &RC.nrmac[0]->config[0]; // check
#ifndef ENABLE_AERIAL
  struct sockaddr_in pnf_p7_sockaddr;
  phy->remote_port = resp->nfapi_config.p7_pnf_port.value;
  //phy->remote_port = 32123;//resp->nfapi_config.p7_pnf_port.value;
  memcpy(&pnf_p7_sockaddr.sin_addr.s_addr, &(resp->nfapi_config.p7_pnf_address_ipv4.address[0]), 4);
  phy->remote_addr = inet_ntoa(pnf_p7_sockaddr.sin_addr);
  // for now just 1
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] %d.%d pnf p7 %s:%d timing %u %u %u %u\n", p5_idx, phy->id, phy->remote_addr, phy->remote_port, p7_vnf->timing_window, p7_vnf->periodic_timing_period, p7_vnf->aperiodic_timing_enabled,
         p7_vnf->periodic_timing_period);
#endif
  // Hack? the VNF might need the subcarrier spacing for some calculations
  // (that we actually don't use as of now...). We therefore need to save the
  // mu, for the current PNF connection (together with where we have frame/slot
  // info). nfapi_vnf_p7_add_pnf() prepends the current P7 connection to the
  // beginning of the list. Pick it from there, and save the mu.
  // check that SCS has actually been set
  const nfapi_uint8_tlv_t *scs = &req->ssb_config.scs_common;
  DevAssert(scs->tl.tag == NFAPI_NR_CONFIG_SCS_COMMON_TAG);
  int mu = scs->value;
  nfapi_vnf_p7_add_pnf((p7_vnf->config), phy->remote_addr, phy->remote_port, phy->id, mu);

  req->header.message_id = NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST;
  req->header.phy_id = phy->id;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Send NFAPI_CONFIG_REQUEST\n");
  //NFAPI_TRACE(NFAPI_TRACE_INFO, "\n NR bandP =%d\n",req->nfapi_config.rf_bands.rf_band[0]);
#ifndef ENABLE_AERIAL
  req->nfapi_config.p7_vnf_port.tl.tag = NFAPI_NR_NFAPI_P7_VNF_PORT_TAG;
  req->nfapi_config.p7_vnf_port.value = p7_vnf->local_port;
  req->num_tlv++;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Local_port:%d\n", ntohs(p7_vnf->local_port));
  req->nfapi_config.p7_vnf_address_ipv4.tl.tag = NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV4_TAG;
  struct sockaddr_in vnf_p7_sockaddr;
  vnf_p7_sockaddr.sin_addr.s_addr = inet_addr(p7_vnf->local_addr);
  memcpy(&(req->nfapi_config.p7_vnf_address_ipv4.address[0]), &vnf_p7_sockaddr.sin_addr.s_addr, 4);
  req->num_tlv++;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Local_addr:%s\n", p7_vnf->local_addr);
  req->nfapi_config.timing_window.tl.tag = NFAPI_NR_NFAPI_TIMING_WINDOW_TAG;
  req->nfapi_config.timing_window.value = p7_vnf->timing_window;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "\n[VNF]Timing window tag : %d Timing window:%u\n",NFAPI_NR_NFAPI_TIMING_WINDOW_TAG, p7_vnf->timing_window);
  req->num_tlv++;

  if(p7_vnf->periodic_timing_enabled || p7_vnf->aperiodic_timing_enabled) {
    req->nfapi_config.timing_info_mode.tl.tag = NFAPI_NR_NFAPI_TIMING_INFO_MODE_TAG;
    req->nfapi_config.timing_info_mode.value = (p7_vnf->aperiodic_timing_enabled << 1) | (p7_vnf->periodic_timing_enabled);
    req->num_tlv++;

    if(p7_vnf->periodic_timing_enabled) {
      req->nfapi_config.timing_info_period.tl.tag = NFAPI_NR_NFAPI_TIMING_INFO_PERIOD_TAG;
      req->nfapi_config.timing_info_period.value = p7_vnf->periodic_timing_period;
      req->num_tlv++;
    }
  }
  // Assign tag and value for P7 message offsets
  req->nfapi_config.dl_tti_timing_offset.tl.tag = NFAPI_NR_NFAPI_DL_TTI_TIMING_OFFSET;
  req->nfapi_config.dl_tti_timing_offset.value = p7_vnf->dl_tti_timing_offset;

  req->nfapi_config.ul_tti_timing_offset.tl.tag = NFAPI_NR_NFAPI_UL_TTI_TIMING_OFFSET;
  req->nfapi_config.ul_tti_timing_offset.value = p7_vnf->ul_tti_timing_offset;

  req->nfapi_config.ul_dci_timing_offset.tl.tag = NFAPI_NR_NFAPI_UL_DCI_TIMING_OFFSET;
  req->nfapi_config.ul_dci_timing_offset.value = p7_vnf->ul_dci_timing_offset;

  req->nfapi_config.tx_data_timing_offset.tl.tag = NFAPI_NR_NFAPI_TX_DATA_TIMING_OFFSET;
  req->nfapi_config.tx_data_timing_offset.value = p7_vnf->tx_data_timing_offset;

  req->num_tlv += 4;

  vendor_ext_tlv_2 ve2;
  memset(&ve2, 0, sizeof(ve2));
  ve2.tl.tag = VENDOR_EXT_TLV_2_TAG;
  ve2.dummy = 2016;
  req->vendor_extension = &ve2.tl;
#endif
  nfapi_nr_vnf_config_req(config, p5_idx, req);
  printf("[VNF] Sent NFAPI_VNF_CONFIG_REQ num_tlv:%u\n",req->num_tlv);
  return 0;
}


int param_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_param_response_t *resp) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Received NFAPI_PARAM_RESP idx:%d phy_id:%d\n", p5_idx, resp->header.phy_id);
  vnf_info *vnf = (vnf_info *)(config->user_data);
  vnf_p7_info *p7_vnf = vnf->p7_vnfs;
  pnf_info *pnf = vnf->pnfs;
  phy_info *phy = pnf->phys;
  struct sockaddr_in pnf_p7_sockaddr;
  nfapi_config_request_t *req = &RC.mac[0]->config[0];
  phy->remote_port = resp->nfapi_config.p7_pnf_port.value;
  memcpy(&pnf_p7_sockaddr.sin_addr.s_addr, &(resp->nfapi_config.p7_pnf_address_ipv4.address[0]), 4);
  phy->remote_addr = inet_ntoa(pnf_p7_sockaddr.sin_addr);
  // for now just 1
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] %d.%d pnf p7 %s:%d timing %u %u %u %u\n", p5_idx, phy->id, phy->remote_addr, phy->remote_port, p7_vnf->timing_window, p7_vnf->periodic_timing_period, p7_vnf->aperiodic_timing_enabled,
         p7_vnf->periodic_timing_period);
  req->header.message_id = NFAPI_CONFIG_REQUEST;
  req->header.phy_id = phy->id;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Send NFAPI_CONFIG_REQUEST\n");
  req->nfapi_config.p7_vnf_port.tl.tag = NFAPI_NFAPI_P7_VNF_PORT_TAG;
  req->nfapi_config.p7_vnf_port.value = p7_vnf->local_port;
  req->num_tlv++;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Local_port:%d\n", ntohs(p7_vnf->local_port));
  req->nfapi_config.p7_vnf_address_ipv4.tl.tag = NFAPI_NFAPI_P7_VNF_ADDRESS_IPV4_TAG;
  struct sockaddr_in vnf_p7_sockaddr;
  vnf_p7_sockaddr.sin_addr.s_addr = inet_addr(p7_vnf->local_addr);
  memcpy(&(req->nfapi_config.p7_vnf_address_ipv4.address[0]), &vnf_p7_sockaddr.sin_addr.s_addr, 4);
  req->num_tlv++;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Local_addr:%s\n", p7_vnf->local_addr);
  req->nfapi_config.timing_window.tl.tag = NFAPI_NFAPI_TIMING_WINDOW_TAG;
  req->nfapi_config.timing_window.value = p7_vnf->timing_window;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Timing window:%u\n", p7_vnf->timing_window);
  req->num_tlv++;

  if(p7_vnf->periodic_timing_enabled || p7_vnf->aperiodic_timing_enabled) {
    req->nfapi_config.timing_info_mode.tl.tag = NFAPI_NFAPI_TIMING_INFO_MODE_TAG;
    req->nfapi_config.timing_info_mode.value = (p7_vnf->aperiodic_timing_enabled << 1) | (p7_vnf->periodic_timing_enabled);
    req->num_tlv++;

    if(p7_vnf->periodic_timing_enabled) {
      req->nfapi_config.timing_info_period.tl.tag = NFAPI_NFAPI_TIMING_INFO_PERIOD_TAG;
      req->nfapi_config.timing_info_period.value = p7_vnf->periodic_timing_period;
      req->num_tlv++;
    }
  }

  vendor_ext_tlv_2 ve2;
  memset(&ve2, 0, sizeof(ve2));
  ve2.tl.tag = VENDOR_EXT_TLV_2_TAG;
  ve2.dummy = 2016;
  req->vendor_extension = &ve2.tl;
  nfapi_vnf_config_req(config, p5_idx, req);
  printf("[VNF] Sent NFAPI_CONFIG_REQ num_tlv:%u\n",req->num_tlv);
  return 0;
}

int nr_config_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_nr_config_response_scf_t *resp) {
  nfapi_nr_start_request_scf_t req;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Received NFAPI_CONFIG_RESP idx:%d phy_id:%d\n", p5_idx, resp->header.phy_id);
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Calling oai_enb_init()\n");
  memset(&req, 0, sizeof(req));
  req.header.message_id = NFAPI_NR_PHY_MSG_TYPE_START_REQUEST;
  req.header.phy_id = resp->header.phy_id;
  nfapi_nr_vnf_start_req(config, p5_idx, &req);
  return 0;
}

int config_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_config_response_t *resp) {
  nfapi_start_request_t req;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Received NFAPI_CONFIG_RESP idx:%d phy_id:%d\n", p5_idx, resp->header.phy_id);
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Calling oai_enb_init()\n");
  oai_enb_init();
  memset(&req, 0, sizeof(req));
  req.header.message_id = NFAPI_START_REQUEST;
  req.header.phy_id = resp->header.phy_id;
  nfapi_vnf_start_req(config, p5_idx, &req);
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Send NFAPI_VNF_START_REQUEST idx:%d phy_id:%d\n", p5_idx, resp->header.phy_id);
  return 0;
}

int start_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_start_response_t *resp) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Received NFAPI_START_RESP idx:%d phy_id:%d\n", p5_idx, resp->header.phy_id);
  vnf_info *vnf = (vnf_info *)(config->user_data);
  pnf_info *pnf = vnf->pnfs;
  phy_info *phy = pnf->phys;
  vnf_p7_info *p7_vnf = vnf->p7_vnfs;
  nfapi_vnf_p7_add_pnf((p7_vnf->config), phy->remote_addr, htons(phy->remote_port), phy->id, 0);
  return 0;
}

int nr_start_resp_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_nr_start_response_scf_t *resp) {
  UNUSED(config);
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Received NFAPI_START_RESP idx:%d phy_id:%d\n", p5_idx, resp->header.phy_id);
  __atomic_store_n(&nr_start_resp_received, 1, __ATOMIC_RELEASE);
  return 0;
}

int nr_error_ind_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_nr_error_indication_scf_t *resp)
{
  UNUSED(config);
  NFAPI_TRACE(NFAPI_TRACE_WARN,
              "[VNF] Received NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION idx:%d phy_id:%d\n",
              p5_idx,
              resp->header.phy_id);
  NFAPI_TRACE(NFAPI_TRACE_WARN, "[VNF] Previous message 0x%02x resulted in an error on the PNF \n", resp->message_id);
  NFAPI_TRACE(NFAPI_TRACE_WARN,
              "[VNF] Received error code 0x%02x (%s)\n",
              resp->error_code,
              error_ind_code_to_str(resp->error_code));
  // TODO: add error handling to the VNF instead of only reporting the received error
  return 0;
}

int vendor_ext_cb(nfapi_vnf_config_t *config, int p5_idx, void *msg)
{
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] %s\n", __FUNCTION__);

  switch (((nfapi_p4_p5_message_header_t *)msg)->message_id) {
    case P5_VENDOR_EXT_RSP: {
      vendor_ext_p5_rsp *rsp = (vendor_ext_p5_rsp *)msg;
      NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] P5_VENDOR_EXT_RSP error_code:%d\n", rsp->error_code);
      // send the start request
      nfapi_pnf_start_request_t req;
      memset(&req, 0, sizeof(req));
      req.header.message_id = NFAPI_PNF_START_REQUEST;
      nfapi_vnf_pnf_start_req(config, p5_idx, &req);
    } break;
  }

  return 0;
}

int vendor_nr_ext_cb(nfapi_vnf_config_t *config, int p5_idx, void *msg)
{
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] %s\n", __FUNCTION__);

  switch (((nfapi_nr_p4_p5_message_header_t *)msg)->message_id) {
    case P5_VENDOR_EXT_RSP: {
      vendor_ext_p5_rsp *rsp = (vendor_ext_p5_rsp *)msg;
      NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] P5_VENDOR_EXT_RSP error_code:%d\n", rsp->error_code);
      // send the start request
      nfapi_nr_pnf_start_request_t req = {0};
      req.header.message_id = NFAPI_PNF_START_REQUEST;
      nfapi_nr_vnf_pnf_start_req(config, p5_idx, &req);
    } break;
  }

  return 0;
}

int vnf_unpack_p4_p5_vendor_extension(void *header, uint8_t **ppReadPackedMessage, uint8_t *end, nfapi_p4_p5_codec_config_t *codec)
{
  UNUSED(codec);
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  if (((nfapi_p4_p5_message_header_t *)header)->message_id == P5_VENDOR_EXT_RSP) {
    vendor_ext_p5_rsp *req = (vendor_ext_p5_rsp *)(header);
    return (!pull16(ppReadPackedMessage, &req->error_code, end));
  }

  return 0;
}

int vnf_nr_unpack_p4_p5_vendor_extension(void *header,
                                         uint8_t **ppReadPackedMessage,
                                         uint8_t *end,
                                         nfapi_p4_p5_codec_config_t *codec)
{
  UNUSED(codec);
  // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s\n", __FUNCTION__);
  if (((nfapi_nr_p4_p5_message_header_t *)header)->message_id == P5_VENDOR_EXT_RSP) {
    vendor_ext_p5_rsp *req = (vendor_ext_p5_rsp *)(header);
    return (!pull16(ppReadPackedMessage, &req->error_code, end));
  }

  return 0;
}

void *vnf_allocate_p4_p5_vendor_ext(uint16_t message_id, uint16_t *msg_size)
{
  if (message_id == P5_VENDOR_EXT_RSP) {
    *msg_size = sizeof(vendor_ext_p5_rsp);
    return (nfapi_p4_p5_message_header_t *)malloc(sizeof(vendor_ext_p5_rsp));
  }

  return 0;
}

void vnf_deallocate_p4_p5_vendor_ext(void *header)
{
  free(header);
}

void *vnf_nr_allocate_p4_p5_vendor_ext(uint16_t message_id, uint16_t *msg_size)
{
  if (message_id == P5_VENDOR_EXT_RSP) {
    *msg_size = sizeof(vendor_ext_p5_rsp);
    return (nfapi_p4_p5_message_header_t *)malloc(sizeof(vendor_ext_p5_rsp));
  }

  return 0;
}

void vnf_nr_deallocate_p4_p5_vendor_ext(void *header) {
  free(header);
}

void vnf_start_thread(void *ptr) {
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] VNF NFAPI thread - nfapi_vnf_start()%s\n", __FUNCTION__);
  pthread_setname_np(pthread_self(), "VNF");
  nfapi_vnf_start((nfapi_vnf_config_t *)ptr);
}

static bool has_stop_ind = false;
static bool waiting_stop_ind = false;
int nr_stop_ind_cb(nfapi_vnf_config_t *config, int p5_idx, nfapi_nr_stop_indication_scf_t *resp)
{
  UNUSED(config);
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Received NFAPI_STOP_IND idx:%d phy_id:%d\n", p5_idx, resp->header.phy_id);
  has_stop_ind = true;
#ifdef ENABLE_AERIAL
  nvIPC_Stop();
#endif
#ifdef ENABLE_SOCKET
  nfapi_vnf_p7_stop(get_p7_vnf_config());
#endif
#ifdef ENABLE_WLS
  wls_vnf_stop();
#endif
  if (!waiting_stop_ind) {
    // hasn't been initialized yet, means the PNF stopped before the VNF did
    // raise a SIGINT to stop the VNF
    raise(SIGINT);
  }
  return 0;
}

void stop_nr_nfapi_vnf()
{
  if (has_stop_ind) {
    // If it got here with the STOP.indication flag already set, it means it was triggered by the PNF,
    // no need to send a STOP.request
    return;
  }
#ifdef ENABLE_WLS
  wls_vnf_send_stop_request();
#endif
#ifdef ENABLE_AERIAL
  nvIPC_send_stop_request();
#endif
#ifdef ENABLE_SOCKET
  socket_nfapi_send_stop_request((vnf_t *)config);
#endif
  waiting_stop_ind = true;
  uint64_t counter = 0;
  vnf_p7_t *p7_vnf = get_p7_vnf();
  while (p7_vnf->terminate == 0 && counter < 50) {
    NFAPI_TRACE(NFAPI_TRACE_DEBUG, "Not terminated yet, counter %ld\n", counter);
    usleep(1000);
    counter++;
  }
  if (p7_vnf->terminate == 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "STOP.indication timed out, exiting\n");
    nfapi_nr_stop_indication_scf_t msg = {.header.message_id = NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION, .header.phy_id = 0};
    config->nr_stop_ind(config, 0, &msg);
  } else {
    NFAPI_TRACE(NFAPI_TRACE_DEBUG, "Terminated, exiting\n");
  }
}

void configure_nr_nfapi_vnf(const char *vnf_addr, uint16_t vnf_p5_port, uint16_t vnf_p7_port)
{
#ifndef ENABLE_AERIAL
  nfapi_setmode(NFAPI_MODE_VNF);
#else
  UNUSED(vnf_addr);
  UNUSED(vnf_p7_port);
#endif
  vnf_info *vnf = calloc(1, sizeof(vnf_info));
  memset(vnf->p7_vnfs, 0, sizeof(vnf->p7_vnfs));
  vnf->p7_vnfs[0].timing_window = 6500;
  vnf->p7_vnfs[0].dl_tti_timing_offset = 0;
  vnf->p7_vnfs[0].ul_tti_timing_offset = 0;
  vnf->p7_vnfs[0].ul_dci_timing_offset = 0;
  vnf->p7_vnfs[0].tx_data_timing_offset = 0;
  vnf->p7_vnfs[0].periodic_timing_enabled = 1;
  vnf->p7_vnfs[0].aperiodic_timing_enabled = 0;
  vnf->p7_vnfs[0].periodic_timing_period = 1;
  vnf->p7_vnfs[0].config = nfapi_vnf_p7_config_create();
#ifndef ENABLE_AERIAL
  NFAPI_TRACE(NFAPI_TRACE_INFO,
              "[VNF] %s() vnf->p7_vnfs[0].config:%p VNF ADDRESS:%s:%d\n",
              __FUNCTION__,
              vnf->p7_vnfs[0].config,
              vnf_addr,
              vnf_p5_port);
  strcpy(vnf->p7_vnfs[0].local_addr, vnf_addr);
  vnf->p7_vnfs[0].local_port = vnf_p7_port;
#endif
  vnf->p7_vnfs[0].mac = malloc(sizeof(mac_t));
  config = nfapi_vnf_config_create();
  config->malloc = malloc;
  config->free = free;
  config->vnf_p5_port = vnf_p5_port;
  config->vnf_ipv4 = 1;
  config->vnf_ipv6 = 0;
  config->pnf_list = 0;
  config->phy_list = 0;
  config->timing_window = vnf->p7_vnfs[0].timing_window;
  config->timing_info_mode = (vnf->p7_vnfs[0].aperiodic_timing_enabled << 1) | (vnf->p7_vnfs[0].periodic_timing_enabled);
  config->timing_info_period = vnf->p7_vnfs[0].periodic_timing_period;

  config->pnf_nr_connection_indication = &pnf_nr_connection_indication_cb;
  config->pnf_disconnect_indication = &pnf_disconnection_indication_cb;

  config->pnf_nr_param_resp = &pnf_nr_param_resp_cb;
  config->pnf_nr_config_resp = &pnf_nr_config_resp_cb;
  config->pnf_nr_start_resp = &pnf_nr_start_resp_cb;
  config->nr_param_resp = &nr_param_resp_cb;
  config->nr_config_resp = &nr_config_resp_cb;
  config->nr_start_resp = &nr_start_resp_cb;
  config->nr_stop_ind = &nr_stop_ind_cb;
  config->nr_error_ind = &nr_error_ind_cb;
  config->vendor_ext = &vendor_nr_ext_cb;
  config->user_data = vnf;
  // To allow custom vendor extentions to be added to nfapi
  config->codec_config.unpack_vendor_extension_tlv = &vnf_nr_unpack_vendor_extension_tlv;
  config->codec_config.pack_vendor_extension_tlv = &vnf_nr_pack_vendor_extension_tlv;
  config->codec_config.unpack_p4_p5_vendor_extension = &vnf_nr_unpack_p4_p5_vendor_extension;
  config->codec_config.pack_p4_p5_vendor_extension = &vnf_nr_pack_p4_p5_vendor_extension;
  config->allocate_p4_p5_vendor_ext = &vnf_nr_allocate_p4_p5_vendor_ext;
  config->deallocate_p4_p5_vendor_ext = &vnf_nr_deallocate_p4_p5_vendor_ext;
  config->codec_config.allocate = &vnf_nr_allocate;
  config->codec_config.deallocate = &vnf_nr_deallocate;
  memset(&UL_RCC_INFO, 0, sizeof(UL_RCC_IND_t));

#ifdef ENABLE_WLS
  config->unpack_func = &fapi_nr_p5_message_unpack;
  config->hdr_unpack_func = &fapi_nr_message_header_unpack;
  config->pack_func = &fapi_nr_p5_message_pack;
  config->send_p5_msg = &wls_vnf_nr_send_p5_message;
  printf("WLS MODE PNF\n");
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[PNF] Creating WLS VNF NFAPI start thread %s\n", __FUNCTION__);
  //wls_fapi_pnf_nr_start_thread(config);
  threadCreate(&vnf_p5_init_and_receive_pthread, wls_fapi_vnf_nr_start_thread, config, "NFAPI_WLS_VNF", -1, OAI_PRIORITY_RT_MAX);
#endif

#ifdef ENABLE_SOCKET
  config->unpack_func = &nfapi_nr_p5_message_unpack;
  config->hdr_unpack_func = &nfapi_nr_p5_message_header_unpack;
  config->pack_func = &nfapi_nr_p5_message_pack;
  config->send_p5_msg = &vnf_nr_send_p5_msg;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Creating VNF NFAPI start thread %s\n", __FUNCTION__);
  pthread_create(&vnf_p5_init_and_receive_pthread, NULL, (void *)&vnf_start_p5_thread, config);
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Created VNF NFAPI start thread %s\n", __FUNCTION__);
#endif
#ifdef ENABLE_AERIAL
  config->unpack_func = &fapi_nr_p5_message_unpack;
  config->hdr_unpack_func = &fapi_nr_message_header_unpack;
  config->pack_func = &fapi_nr_p5_message_pack;
  config->send_p5_msg = &aerial_nr_send_p5_message;
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Created VNF NFAPI start thread %s\n", __FUNCTION__);
  nfapi_vnf_pnf_info_t *pnf = (nfapi_vnf_pnf_info_t *)malloc(sizeof(nfapi_vnf_pnf_info_t));
  NFAPI_TRACE(NFAPI_TRACE_INFO, "MALLOC nfapi_vnf_pnf_info_t for pnf_list pnf:%p\n", pnf);
  memset(pnf, 0, sizeof(nfapi_vnf_pnf_info_t));
  pnf->p5_idx = 1;
  pnf->connected = 1;
  // Add needed parameters

  pnf_info *pnf_info = vnf->pnfs;

  for (int i = 0; i < 1; ++i) {
    phy_info phy;
    memset(&phy, 0, sizeof(phy));
    phy.index = 0;
    NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] (PHY:%d) phy_config_idx:%d\n", i, 0);
    nfapi_vnf_allocate_phy(config, 1, &(phy.id));

    for (int j = 0; j < 1; ++j) {
      NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] (PHY:%d) (RF%d) %d\n", i, j, 0);
      phy.rfs[0] = 0;
    }

    pnf_info->phys[0] = phy;
  }


  nfapi_vnf_pnf_list_add(config, pnf);

  vnf_p7_info *p7_vnf = vnf->p7_vnfs;

  NFAPI_TRACE(NFAPI_TRACE_INFO,
              "[VNF] pnf start response idx:%d config:%p user_data:%p p7_vnf[config:%p thread_started:%d]\n",
              1,
              config,
              config->user_data,
              vnf->p7_vnfs[0].config,
              vnf->p7_vnfs[0].thread_started);
  configure_nr_p7_vnf(p7_vnf);
#endif
}


void configure_nfapi_vnf(char *vnf_addr, int vnf_p5_port, char *pnf_ip_addr, int pnf_p7_port, int vnf_p7_port) {
  UNUSED(pnf_ip_addr);
  UNUSED(pnf_p7_port);
  nfapi_setmode(NFAPI_MODE_VNF);
  vnf_info *vnf = calloc(1, sizeof(vnf_info));
  memset(vnf->p7_vnfs, 0, sizeof(vnf->p7_vnfs));
  vnf->p7_vnfs[0].timing_window = 32;
  vnf->p7_vnfs[0].periodic_timing_enabled = 1;
  vnf->p7_vnfs[0].aperiodic_timing_enabled = 0;
  vnf->p7_vnfs[0].periodic_timing_period = 10;
  vnf->p7_vnfs[0].config = nfapi_vnf_p7_config_create();
  NFAPI_TRACE(NFAPI_TRACE_INFO,
              "[VNF] %s() vnf.p7_vnfs[0].config:%p VNF ADDRESS:%s:%d\n",
              __FUNCTION__,
              vnf->p7_vnfs[0].config,
              vnf_addr,
              vnf_p5_port);
  strcpy(vnf->p7_vnfs[0].local_addr, vnf_addr);
  vnf->p7_vnfs[0].local_port = vnf_p7_port;
  vnf->p7_vnfs[0].mac = malloc(sizeof(mac_t));
  config = nfapi_vnf_config_create();
  config->malloc = malloc;
  config->free = free;
  config->vnf_p5_port = vnf_p5_port;
  config->vnf_ipv4 = 1;
  config->vnf_ipv6 = 0;
  config->pnf_list = 0;
  config->phy_list = 0;

  config->pnf_connection_indication = &pnf_connection_indication_cb;
  config->pnf_disconnect_indication = &pnf_disconnection_indication_cb;

  config->pnf_param_resp = &pnf_param_resp_cb;
  config->pnf_config_resp = &pnf_config_resp_cb;
  config->pnf_start_resp = &pnf_start_resp_cb;
  config->param_resp = &param_resp_cb;
  config->config_resp = &config_resp_cb;
  config->start_resp = &start_resp_cb;
  config->vendor_ext = &vendor_ext_cb;
  config->user_data = vnf;
  // To allow custom vendor extentions to be added to nfapi
  config->codec_config.unpack_vendor_extension_tlv = &vnf_unpack_vendor_extension_tlv;
  config->codec_config.pack_vendor_extension_tlv = &vnf_pack_vendor_extension_tlv;
  config->codec_config.unpack_p4_p5_vendor_extension = &vnf_unpack_p4_p5_vendor_extension;
  config->codec_config.pack_p4_p5_vendor_extension = &vnf_pack_p4_p5_vendor_extension;
  config->allocate_p4_p5_vendor_ext = &vnf_allocate_p4_p5_vendor_ext;
  config->deallocate_p4_p5_vendor_ext = &vnf_deallocate_p4_p5_vendor_ext;
  config->codec_config.allocate = &vnf_allocate;
  config->codec_config.deallocate = &vnf_deallocate;
  memset(&UL_RCC_INFO,0,sizeof(UL_RCC_IND_t));
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Creating VNF NFAPI start thread %s\n", __FUNCTION__);
  pthread_create(&vnf_p5_init_and_receive_pthread, NULL, (void *)&vnf_start_thread, config);
  NFAPI_TRACE(NFAPI_TRACE_INFO, "[VNF] Created VNF NFAPI start thread %s\n", __FUNCTION__);
}

int oai_nfapi_dl_config_req(nfapi_dl_config_request_t *dl_config_req) {
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  dl_config_req->header.phy_id = 1; // HACK TODO FIXME - need to pass this around!!!!
  dl_config_req->header.message_id = NFAPI_DL_CONFIG_REQUEST;
  LOG_D(PHY, "[VNF] %s() DL_CONFIG_REQ sfn_sf:%d_%d number_of_pdus:%d\n", __FUNCTION__,
        NFAPI_SFNSF2SFN(dl_config_req->sfn_sf),NFAPI_SFNSF2SF(dl_config_req->sfn_sf), dl_config_req->dl_config_request_body.number_pdu);
  if (dl_config_req->dl_config_request_body.number_pdu > 0)
  {
    for (int i = 0; i < dl_config_req->dl_config_request_body.number_pdu; i++)
        {
            uint8_t pdu_type = dl_config_req->dl_config_request_body.dl_config_pdu_list[i].pdu_type;
            if(pdu_type ==  NFAPI_DL_CONFIG_DLSCH_PDU_TYPE)
            {
                uint16_t dl_rnti = dl_config_req->dl_config_request_body.dl_config_pdu_list[i].dlsch_pdu.dlsch_pdu_rel8.rnti;
                uint16_t numPDUs = dl_config_req->dl_config_request_body.number_pdu;
                LOG_D(MAC, "(OAI eNB) Sending dl_config_req at VNF during Frame: %d and Subframe: %d,"
                           " with a RNTI value of: %x and with number of PDUs: %u\n",
                      NFAPI_SFNSF2SFN(dl_config_req->sfn_sf),NFAPI_SFNSF2SF(dl_config_req->sfn_sf), dl_rnti, numPDUs);
            }
        }
  }
  int retval = nfapi_vnf_p7_dl_config_req(p7_config, dl_config_req);
  dl_config_req->dl_config_request_body.number_pdcch_ofdm_symbols           = 1;
  dl_config_req->dl_config_request_body.number_dci                          = 0;
  dl_config_req->dl_config_request_body.number_pdu                          = 0;
  dl_config_req->dl_config_request_body.number_pdsch_rnti                   = 0;

  if (retval!=0) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  }

  return retval;
}

int oai_nfapi_dl_tti_req(nfapi_nr_dl_tti_request_t *dl_config_req)
{
  LOG_D(NR_PHY, "Entering oai_nfapi_nr_dl_config_req sfn:%d,slot:%d\n", dl_config_req->SFN, dl_config_req->Slot);
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  dl_config_req->header.message_id= NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST;
  dl_config_req->header.phy_id = 1; // HACK TODO FIXME - need to pass this around!!!!

  bool retval = nfapi_vnf_p7_nr_dl_config_req(p7_config, dl_config_req);

  dl_config_req->dl_tti_request_body.nPDUs                        = 0;
  dl_config_req->dl_tti_request_body.nGroup                       = 0;


  if (!retval) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  }
  return retval;
}

int oai_nfapi_tx_data_req(nfapi_nr_tx_data_request_t *tx_data_req)
{
  LOG_D(NR_PHY, "Entering oai_nfapi_nr_tx_data_req sfn:%d,slot:%d\n", tx_data_req->SFN, tx_data_req->Slot);
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  tx_data_req->header.phy_id = 1; // HACK TODO FIXME - need to pass this around!!!!
  tx_data_req->header.message_id = NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST;
  //LOG_D(PHY, "[VNF] %s() TX_REQ sfn_sf:%d number_of_pdus:%d\n", __FUNCTION__, NFAPI_SFNSF2DEC(tx_req->sfn_sf), tx_req->tx_request_body.number_of_pdus);
  bool retval = nfapi_vnf_p7_tx_data_req(p7_config, tx_data_req);

  if (!retval) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  } else {
    tx_data_req->Number_of_PDUs = 0;
  }

  return retval;
}

int oai_nfapi_tx_req(nfapi_tx_request_t *tx_req)
{
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  tx_req->header.phy_id = 1; // HACK TODO FIXME - need to pass this around!!!!
  tx_req->header.message_id = NFAPI_TX_REQUEST;
  //LOG_D(PHY, "[VNF] %s() TX_REQ sfn_sf:%d number_of_pdus:%d\n", __FUNCTION__, NFAPI_SFNSF2DEC(tx_req->sfn_sf), tx_req->tx_request_body.number_of_pdus);
  int retval = nfapi_vnf_p7_tx_req(p7_config, tx_req);

  if (retval!=0) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  } else {
    tx_req->tx_request_body.number_of_pdus = 0;
  }

  return retval;
}

int oai_nfapi_ul_dci_req(nfapi_nr_ul_dci_request_t *ul_dci_req) {
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  ul_dci_req->header.phy_id = 1; // HACK TODO FIXME - need to pass this around!!!!
  ul_dci_req->header.message_id = NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST;
  //LOG_D(PHY, "[VNF] %s() HI_DCI0_REQ sfn_sf:%d dci:%d hi:%d\n", __FUNCTION__, NFAPI_SFNSF2DEC(hi_dci0_req->sfn_sf), hi_dci0_req->hi_dci0_request_body.number_of_dci, hi_dci0_req->hi_dci0_request_body.number_of_hi);
  bool retval = nfapi_vnf_p7_ul_dci_req(p7_config, ul_dci_req);

  if (!retval) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  } else {
    ul_dci_req->numPdus = 0;
  }

  return retval;
}

int oai_nfapi_hi_dci0_req(nfapi_hi_dci0_request_t *hi_dci0_req) {
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  hi_dci0_req->header.phy_id = 1; // HACK TODO FIXME - need to pass this around!!!!
  hi_dci0_req->header.message_id = NFAPI_HI_DCI0_REQUEST;
  //LOG_D(PHY, "[VNF] %s() HI_DCI0_REQ sfn_sf:%d dci:%d hi:%d\n", __FUNCTION__, NFAPI_SFNSF2DEC(hi_dci0_req->sfn_sf), hi_dci0_req->hi_dci0_request_body.number_of_dci, hi_dci0_req->hi_dci0_request_body.number_of_hi);
  int retval = nfapi_vnf_p7_hi_dci0_req(p7_config, hi_dci0_req);

  if (retval!=0) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  } else {
    hi_dci0_req->hi_dci0_request_body.number_of_hi = 0;
    hi_dci0_req->hi_dci0_request_body.number_of_dci = 0;
  }

  return retval;
}

static void remove_ul_config_req_pdu(int index, nfapi_ul_config_request_t *ul_config_req)
{
  int num_pdus = ul_config_req->ul_config_request_body.number_of_pdus;
  nfapi_ul_config_request_pdu_t *pdu_list = ul_config_req->ul_config_request_body.ul_config_pdu_list;

  if (index >= num_pdus || index < 0)
  {
    LOG_E(MAC, "%s() Unable to drop bad ul_config_req PDU\n", __FUNCTION__);
    abort();
  }

  for(int i = index; i + 1 < num_pdus; i++)
  {
    pdu_list[i] = pdu_list[i + 1];
  }

  ul_config_req->ul_config_request_body.number_of_pdus--;
}

int oai_nfapi_ul_tti_req(nfapi_nr_ul_tti_request_t *ul_tti_req) {
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();

  ul_tti_req->header.phy_id = 1; // HACK TODO FIXME - need to pass this around!!!!
  ul_tti_req->header.message_id = NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST;

  bool retval = nfapi_vnf_p7_ul_tti_req(p7_config, ul_tti_req);

  if (!retval) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  } else {
    // Reset number of PDUs so that it is not resent
    ul_tti_req->n_pdus = 0;
    ul_tti_req->n_group = 0;
    ul_tti_req->n_ulcch = 0;
    ul_tti_req->n_ulsch = 0;
  }
  return retval;
}

int oai_nfapi_ul_config_req(nfapi_ul_config_request_t *ul_config_req) {
  nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();
  ul_config_req->header.phy_id = 1; // HACK TODO FIXME - need to pass this around!!!!
  ul_config_req->header.message_id = NFAPI_UL_CONFIG_REQUEST;
  //LOG_D(PHY, "[VNF] %s() header message_id:%02x\n", __FUNCTION__, ul_config_req->header.message_id);
  //LOG_D(PHY, "[VNF] %s() UL_CONFIG sfn_sf:%d PDUs:%d rach_prach_frequency_resources:%d srs_present:%d\n", __FUNCTION__, NFAPI_SFNSF2DEC(ul_config_req->sfn_sf), ul_config_req->ul_config_request_body.number_of_pdus, ul_config_req->ul_config_request_body.rach_prach_frequency_resources, ul_config_req->ul_config_request_body.srs_present);

  int num_pdus = ul_config_req->ul_config_request_body.number_of_pdus;
  nfapi_ul_config_request_pdu_t *pdu_list = ul_config_req->ul_config_request_body.ul_config_pdu_list;
  for (int i = 0; i < num_pdus; i++)
  {
    uint8_t pdu_type = pdu_list[i].pdu_type;

    LOG_D(MAC, "ul_config_req num_pdus: %u pdu_number: %d pdu_type: %u SFN.SF: %d.%d\n",
          num_pdus, i, pdu_type, ul_config_req->sfn_sf >> 4, ul_config_req->sfn_sf & 15);

    if (pdu_type != NFAPI_UL_CONFIG_ULSCH_CQI_RI_PDU_TYPE)
    {
      continue;
    }

    for (int j = i + 1; j < num_pdus; j++)
    {
      uint8_t pdu_type2 = pdu_list[j].pdu_type;
      if (pdu_type != pdu_type2)
      {
        continue;
      }

      uint16_t rnti_i = pdu_list[i].ulsch_cqi_ri_pdu.ulsch_pdu.ulsch_pdu_rel8.rnti;
      uint16_t rnti_j = pdu_list[j].ulsch_cqi_ri_pdu.ulsch_pdu.ulsch_pdu_rel8.rnti;
      if (!is_ue_same(rnti_i, rnti_j))
      {
        continue;
      }
      remove_ul_config_req_pdu(j, ul_config_req);
      j--;
      num_pdus--;

      LOG_E(MAC, "Problem, two cqis being sent to a single UE for rnti %x dropping one\n",
            rnti_i);
    }
  }

  int retval = nfapi_vnf_p7_ul_config_req(p7_config, ul_config_req);

  if (retval!=0) {
    LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
  } else {
    // Reset number of PDUs so that it is not resent
    ul_config_req->ul_config_request_body.number_of_pdus = 0;
    ul_config_req->ul_config_request_body.rach_prach_frequency_resources = 0;
    ul_config_req->ul_config_request_body.srs_present = 0;
  }

  return retval;
}

int oai_nfapi_ue_release_req(nfapi_ue_release_request_t *release_req){
    if(release_req->ue_release_request_body.number_of_TLVs <= 0)
        return 0;
    nfapi_vnf_p7_config_t *p7_config = get_p7_vnf_config();

    release_req->header.phy_id = 1; // HACK TODO FIXME - need to pass this around!!!!
    release_req->header.message_id = NFAPI_UE_RELEASE_REQUEST;
    release_req->ue_release_request_body.tl.tag = NFAPI_UE_RELEASE_BODY_TAG;

    int retval = nfapi_vnf_p7_ue_release_req(p7_config, release_req);
    if (retval!=0) {
      LOG_E(PHY, "%s() Problem sending retval:%d\n", __FUNCTION__, retval);
    } else {
        release_req->ue_release_request_body.number_of_TLVs = 0;
    }
    return retval;
}
