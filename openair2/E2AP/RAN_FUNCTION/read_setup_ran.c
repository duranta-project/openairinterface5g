/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "read_setup_ran.h"
#include "../../E2AP/flexric/src/lib/e2ap/e2ap_node_component_config_add_wrapper.h"
#include "setup_msg_store.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#if defined(E2AP_V2) || defined(E2AP_V3)

/* read_setup_ran() is called synchronously by the agent thread while
 * it builds the E2 Setup Request (gen_setup_request_v2()), so we simply
 * block here until F1AP/NGAP/E1AP have captured the real Setup Request/Response
 * for this interface -- they run in their own ITTI threads and fill the store
 * independently of this wait. */
static void wait_for_setup_capture(e2ap_setup_msg_iface_t iface, const char *name)
{
  bool ready = false;
  bool logged = false;
  while (!ready) {
    sleep(1);
    byte_array_t req = e2ap_get_setup_req(iface);
    byte_array_t resp = e2ap_get_setup_resp(iface);
    ready = req.len > 0 && resp.len > 0;
    if (!ready && !logged) {
      printf("[E2AP] read_setup_ran: waiting for %s Setup Request/Response before generating E2 Setup Request\n", name);
      logged = true;
    }
    free_byte_array(req);
    free_byte_array(resp);
  }
}

// NGAP
static e2ap_node_component_config_add_t fill_ngap_e2ap_node_component_config_add(void)
{
  e2ap_node_component_config_add_t dst = {0};

  // Mandatory
  // 9.2.26
  dst.e2_node_comp_interface_type = NG_E2AP_NODE_COMP_INTERFACE_TYPE;
  // Bug!! Optional in the standard, mandatory in ASN.1
  // 9.2.32
  dst.e2_node_comp_id.type = NG_E2AP_NODE_COMP_INTERFACE_TYPE;
 
  const char ng_msg[] = "Dummy message";
  dst.e2_node_comp_id.ng_amf_name = cp_str_to_ba(ng_msg);

  // Mandatory
  // 9.2.27
  dst.e2_node_comp_conf.request = e2ap_get_setup_req(E2AP_SETUP_MSG_NGAP);
  dst.e2_node_comp_conf.response = e2ap_get_setup_resp(E2AP_SETUP_MSG_NGAP);
  return dst;
}

// F1AP
static e2ap_node_component_config_add_t fill_f1ap_e2ap_node_component_config_add(void)
{
  e2ap_node_component_config_add_t dst = {0};

  // Mandatory
  // 9.2.26
  dst.e2_node_comp_interface_type = F1_E2AP_NODE_COMP_INTERFACE_TYPE;
  // Bug!! Optional in the standard, mandatory in ASN.1
  // 9.2.32
  dst.e2_node_comp_id.type = F1_E2AP_NODE_COMP_INTERFACE_TYPE;

  dst.e2_node_comp_id.f1_gnb_du_id = 1023;

  // Mandatory
  // 9.2.27
  dst.e2_node_comp_conf.request = e2ap_get_setup_req(E2AP_SETUP_MSG_F1AP);
  dst.e2_node_comp_conf.response = e2ap_get_setup_resp(E2AP_SETUP_MSG_F1AP);
  return dst;
}

// E1AP
static e2ap_node_component_config_add_t fill_e1ap_e2ap_node_component_config_add(void)
{
  e2ap_node_component_config_add_t dst = {0};

  // Mandatory
  // 9.2.26
  dst.e2_node_comp_interface_type = E1_E2AP_NODE_COMP_INTERFACE_TYPE;
  // Bug!! Optional in the standard, mandatory in ASN.1
  // 9.2.32
  dst.e2_node_comp_id.type = E1_E2AP_NODE_COMP_INTERFACE_TYPE;

  dst.e2_node_comp_id.e1_gnb_cu_up_id = 1025;

  // Mandatory
  // 9.2.27
  dst.e2_node_comp_conf.request = e2ap_get_setup_req(E2AP_SETUP_MSG_E1AP);
  dst.e2_node_comp_conf.response = e2ap_get_setup_resp(E2AP_SETUP_MSG_E1AP);
  return dst;
}

// S1AP
static e2ap_node_component_config_add_t fill_s1ap_e2ap_node_component_config_add(void)
{
  e2ap_node_component_config_add_t dst = {0};

  // Mandatory
  // 9.2.26
  dst.e2_node_comp_interface_type = S1_E2AP_NODE_COMP_INTERFACE_TYPE;
  // Bug!! Optional in the standard, mandatory in ASN.1
  // 9.2.32
  dst.e2_node_comp_id.type = S1_E2AP_NODE_COMP_INTERFACE_TYPE;

  const char str[] = "S1 NAME";
  dst.e2_node_comp_id.s1_mme_name = cp_str_to_ba(str);

  // Mandatory
  // 9.2.27
  const char req[] = "S1AP Request Message sent";
  const char res[] = "S1AP Response Message reveived";

  dst.e2_node_comp_conf.request = cp_str_to_ba(req);
  dst.e2_node_comp_conf.response = cp_str_to_ba(res);
  return dst;
}
#endif

void read_setup_ran(void* data, const ngran_node_t node_type)
{
  assert(data != NULL);
#ifdef E2AP_V1
  (void)node_type;
#elif defined(E2AP_V2) || defined(E2AP_V3) 

  arr_node_component_config_add_t* dst = (arr_node_component_config_add_t*)data;
  if(node_type == ngran_gNB){
    wait_for_setup_capture(E2AP_SETUP_MSG_NGAP, "NGAP");
    dst->len_cca = 1;
    dst->cca = calloc(1, sizeof(e2ap_node_component_config_add_t));
    assert(dst->cca != NULL);
    // NGAP
    dst->cca[0] = fill_ngap_e2ap_node_component_config_add();
  } else if(node_type == ngran_gNB_CU){
    wait_for_setup_capture(E2AP_SETUP_MSG_NGAP, "NGAP");
    wait_for_setup_capture(E2AP_SETUP_MSG_F1AP, "F1AP");
    dst->len_cca = 2;
    dst->cca = calloc(2, sizeof(e2ap_node_component_config_add_t));
    assert(dst->cca != NULL);
    // NGAP
    dst->cca[0] = fill_ngap_e2ap_node_component_config_add();
    // F1AP
    dst->cca[1] = fill_f1ap_e2ap_node_component_config_add();
  } else if(node_type == ngran_gNB_DU){
    wait_for_setup_capture(E2AP_SETUP_MSG_F1AP, "F1AP");
    dst->len_cca = 1;
    dst->cca = calloc(1, sizeof(e2ap_node_component_config_add_t));
    assert(dst->cca != NULL);
    // F1AP
    dst->cca[0] = fill_f1ap_e2ap_node_component_config_add();
  } else if(node_type == ngran_gNB_CUCP){
    wait_for_setup_capture(E2AP_SETUP_MSG_NGAP, "NGAP");
    wait_for_setup_capture(E2AP_SETUP_MSG_F1AP, "F1AP");
    wait_for_setup_capture(E2AP_SETUP_MSG_E1AP, "E1AP");
    dst->len_cca = 3;
    dst->cca = calloc(3, sizeof(e2ap_node_component_config_add_t));
    assert(dst->cca != NULL);
    // NGAP
    dst->cca[0] = fill_ngap_e2ap_node_component_config_add();
    // F1AP
    dst->cca[1] = fill_f1ap_e2ap_node_component_config_add();
    // E1AP
    dst->cca[2] = fill_e1ap_e2ap_node_component_config_add();
  } else if(node_type == ngran_gNB_CUUP){
    wait_for_setup_capture(E2AP_SETUP_MSG_NGAP, "NGAP");
    wait_for_setup_capture(E2AP_SETUP_MSG_F1AP, "F1AP");
    wait_for_setup_capture(E2AP_SETUP_MSG_E1AP, "E1AP");
    dst->len_cca = 3;
    dst->cca = calloc(3, sizeof(e2ap_node_component_config_add_t));
    assert(dst->cca != NULL);
    // NGAP
    dst->cca[0] = fill_ngap_e2ap_node_component_config_add();
    // F1AP
    dst->cca[1] = fill_f1ap_e2ap_node_component_config_add();
    // E1AP
    dst->cca[2] = fill_e1ap_e2ap_node_component_config_add();
  } else if(node_type == ngran_eNB){
    dst->len_cca = 1;
    dst->cca = calloc(1, sizeof(e2ap_node_component_config_add_t));
    assert(dst->cca != NULL);
    // S1AP
    dst->cca[0] = fill_s1ap_e2ap_node_component_config_add();
  } else {
    assert(0 != 0 && "Not implemented");
  }

#else
  static_assert(0!=0, "Unknown E2AP version");
#endif

}
