/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_RRC_MESSAGES_TYPES_H_
#define NR_RRC_MESSAGES_TYPES_H_

typedef struct Rrc_get_single_ue_rnti_s {
  rnti_t rnti;
  ue_id_t id;
  int32_t ue_reestablishment_counter;
  int32_t ue_reconfiguration_counter;
  int32_t rrc_ue_id;
  bool is_single;
  bool has_rrc;
} Rrc_get_single_ue_rnti;
#define RRC_GET_SINGLE_UE_RNTI(mSGpTR) (mSGpTR)->ittiMsg.rrc_get_single_ue_rnti;
#define RRC_GET_UE_CONTEXT_BY_UE_ID(mSGpTR) (mSGpTR)->ittiMsg.rrc_get_ue_context_by_ue_id;

struct rrc_gNB_ue_context_s;
typedef struct Rrc_get_ue_context_by_rnti_any_du_s {
  rnti_t rnti;
  bool ue_context_exists;
} Rrc_get_ue_context_by_rnti_any_du;
#define RRC_GET_UE_CONTEXT_BY_RNTI_ANY_DU(mSGpTR) (mSGpTR)->ittiMsg.rrc_get_ue_context_by_rnti_any_du;

typedef struct Rrc_get_du_id_by_ue_id_s {
  int32_t ue_id;
  int32_t du_id;
  bool no_du;
} Rrc_get_du_id_by_ue_id;
#define RRC_GET_DU_ID_BY_UE_ID(mSGpTR) (mSGpTR)->ittiMsg.rrc_get_du_id_by_ue_id;

typedef struct Rrc_trigger_ho_s {
  ue_id_t id;
  uint32_t neighbour_pci;
} Rrc_trigger_ho;
#define RRC_NR_F1_HO_TRIGGER(mSGpTR) (mSGpTR)->ittiMsg.rrc_trigger_ho;
#define RRC_NR_N2_HO_TRIGGER(mSGpTR) (mSGpTR)->ittiMsg.rrc_trigger_ho;

typedef struct Rrc_get_ngap_ue_id_s {
  int amf_ue_ngap_id;
  int gNB_ue_ngap_id;
} Rrc_get_ngap_ue_id;
#define RRC_GET_NGAP_UE_ID(mSGpTR) (mSGpTR)->ittiMsg.rrc_ngap_ue_id;

typedef struct Rrc_check_ue_context_s {
  int id;
  bool check;
} Rrc_check_ue_context;
#define RRC_CHECK_UE_CONTEXT(mSGpTR) (mSGpTR)->ittiMsg.rrc_check_ue_context;

typedef struct Rrc_gnb_generate_rrcrelease_s {
  ue_id_t ue_id;
} Rrc_gnb_generate_rrcrelease;
#define RRC_GNB_GENERATE_RRCRELEASE(mSGpTR) (mSGpTR)->ittiMsg.rrc_gnb_generate_rrcrelease;

typedef struct Rrc_gnb_generate_rrcrelease_all_s {
  Rrc_gnb_generate_rrcrelease rrc_gnb_generate_rrcreleases[64];
  int nb_releases;
} Rrc_gnb_generate_rrcrelease_all;
#define RRC_GNB_GENERATE_RRCRELEASE_ALL(mSGpTR) (mSGpTR)->ittiMsg.rrc_gnb_generate_rrcrelease_all;

typedef struct Rrc_gnb_trigger_ue_context_release_req_s {
  ue_id_t ue_id;
  bool rrc_ue_context;
  bool ngap_ue_context;
} Rrc_gnb_trigger_ue_context_release_req;
#define RRC_GNB_TRIGGER_UE_CONTEXT_RELEASE_REQ(mSGpTR) (mSGpTR)->ittiMsg.rrc_gnb_trigger_ue_context_release_req;

typedef struct Mac_force_ul_failure_s {
  rnti_t rnti;
} Mac_force_ul_failure;
#define MAC_FORCE_UL_FAILURE(mSGpTR) (mSGpTR)->ittiMsg.mac_force_ul_failure;

typedef struct Mac_get_ue_rnti_s {
  rnti_t rnti;
} Mac_get_ue_rnti;
#define MAC_GET_UE_RNTI(mSGpTR) (mSGpTR)->ittiMsg.mac_get_ue_rnti;

typedef struct Mac_get_ue_rnti_by_uid_s {
  uid_t uid;
  rnti_t rnti;
} Mac_get_ue_rnti_by_uid;
#define MAC_GET_UE_RNTI_BY_UID(mSGpTR) (mSGpTR)->ittiMsg.mac_get_ue_rnti_by_uid;

#endif /* NR_RRC_MESSAGES_TYPES_H_ */
