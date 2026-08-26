/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __NR_NAS_MSG_SIM_H__
#define __NR_NAS_MSG_SIM_H__

#include <common/utils/assertions.h>
#include "common/5g_platform_types.h"
#include <openair3/UICC/usim_interface.h>
#include <stdbool.h>
#include <stdint.h>
#include "as_message.h"
#include "NR_NAS_defs.h"
#include "secu_defs.h"
#include "NR_NAS_defs.h"

#define INITIAL_REGISTRATION 0b001

#define SECURITY_PROTECTED_5GS_NAS_MESSAGE_HEADER_LENGTH 7
#define NAS_INTEGRITY_SIZE 4

/* 3GPP TS 24.501: 9.11.3.50 Service type */
#define SERVICE_TYPE_DATA 0x1

typedef enum fgs_mm_state_e {
  FGS_DEREGISTERED,
  FGS_DEREGISTERED_INITIATED,
  FGS_REGISTERED_INITIATED,
  FGS_REGISTERED,
  FGS_SERVICE_REQUEST_INITIATED,
} fgs_mm_state_t;

/*
 * 5GS mobility management (5GMM) modes
 * 5.1.3.2.1.1 of TS 24.501
 */
typedef enum fgs_mm_mode_e {
  FGS_NOT_CONNECTED,
  FGS_IDLE,
  FGS_CONNECTED,
} fgs_mm_mode_t;

/* Security Key for SA UE */
typedef struct {
  uint8_t kausf[32];
  uint8_t kseaf[32];
  uint8_t kamf[32];
  uint8_t knas_int[16];
  uint8_t knas_enc[16];
  uint8_t res[16];
  uint8_t rand[16];
  uint8_t kgnb[32];
  uint32_t nas_count_ul;
  uint32_t nas_count_dl;
} ue_sa_security_key_t;

typedef struct {
  /* 5GS Mobility Management States (5.1.3.2.1 of 3GPP TS 24.501) */
  fgs_mm_state_t fiveGMM_state;
  /* 5GS Mobility Management mode */
  fgs_mm_mode_t fiveGMM_mode;
  uicc_t *uicc;
  ue_sa_security_key_t security;
  stream_security_container_t *security_container;
  Guti5GSMobileIdentity_t *guti;
  bool termination_procedure;
  instance_t UE_id;
  /* RRC Inactive Indication */
  bool is_rrc_inactive;
  /* Timer T3512 */
  int t3512;
  // Timer t3448 in seconds (-1 = disabled)
  int t3448;
  // Timer t3446 in seconds (-1 = disabled)
  int t3446;
  /* NAS Key Set Identifier associated to the security context */
  uint8_t *ksi;
  plmn_id_t *sn_id;
} nr_ue_nas_t;

nr_ue_nas_t *get_ue_nas_info(module_id_t module_id);
/* @brief Return true and write the 1-based selectedPLMN-Identity (TS 38.331) of the
 * plmns[] entry matching the UE IMSI (MCC/MNC from UICC); false (output untouched) if none. */
bool nas_get_selected_plmn(const nr_ue_nas_t *nas, const plmn_id_t *plmns, int num_plmns, long *selected_plmn_identity);
void generateRegistrationRequest(as_nas_info_t *initialNasMsg, nr_ue_nas_t *nas, bool is_security_mode);
void generateServiceRequest(as_nas_info_t *initialNasMsg, nr_ue_nas_t *nas);
void *nas_nrue_task(void *args_p);
void *nas_nrue(void *args_p);
void nas_init_nrue(int num_ues);
int nas_itti_kgnb_refresh_req(instance_t instance, const uint8_t kgnb[32]);
void nr_ue_create_ip_if(const char *ifnameprefix, const char *ipv4, const char *ipv6, int ue_id, int pdu_session_id);
void request_pdusession(nr_ue_nas_t *nas, const pdu_session_config_t *pdu);
nr_ue_nas_t *get_nr_ue_nas_info(uint8_t ue_inst);

#endif /* __NR_NAS_MSG_SIM_H__*/
