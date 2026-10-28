/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_mac_fapi.h"
#include "mac_proto.h"

void reset_sched_response(NR_Sched_Rsp_t *sched_response, int frame, int slot, int module_id, int phy_id)
{
  sched_response->module_id = module_id;
  sched_response->phy_id = phy_id;
  sched_response->frame = frame;
  sched_response->slot = slot;


  nfapi_nr_dl_tti_request_t *DL_req = &sched_response->DL_req;
  DL_req->SFN = frame;
  DL_req->Slot = slot;
  DL_req->dl_tti_request_body.nPDUs = 0;
  DL_req->dl_tti_request_body.nGroup = 0;

  nfapi_nr_ul_dci_request_t *UL_dci_req = &sched_response->UL_dci_req;
  UL_dci_req->SFN = frame;
  UL_dci_req->Slot = slot;
  UL_dci_req->numPdus = 0;

  nfapi_nr_ul_tti_request_t *UL_tti_req = &sched_response->UL_tti_req;
  UL_tti_req->SFN = frame;
  UL_tti_req->Slot = slot;
  UL_tti_req->n_pdus = 0;
  UL_tti_req->n_group = 0;

  nfapi_nr_tx_data_request_t *TX_req = &sched_response->TX_req;
  TX_req->SFN = frame;
  TX_req->Slot = slot;
  TX_req->Number_of_PDUs = 0;
}

void run_scheduler_monolithic(const nfapi_nr_slot_indication_scf_t *ind, NR_Sched_Rsp_t *rsp)
{
  module_id_t module_id = 0;
  int phy_id = ind->header.phy_id;
  reset_sched_response(rsp, ind->sfn, ind->slot, module_id, phy_id);
  gNB_dlsch_ulsch_scheduler(rsp->module_id,phy_id, ind->sfn, ind->slot, rsp);
}

