/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "rrc_gNB_radio_bearers.h"
#include <stddef.h>
#include "E1AP_RLC-Mode.h"
#include "RRC/NR/nr_rrc_defs.h"
#include "T.h"
#include "asn_internal.h"
#include "assertions.h"
#include "common/platform_constants.h"
#include "common/utils/T/T.h"
#include "common/utils/utils.h"
#include "ngap_messages_types.h"
#include "oai_asn1.h"
#include "openair2/LAYER2/nr_pdcp/nr_pdcp_asn1_utils.h"
#include "common/utils/alg/find.h"

#define NO_FIVEQI UINT16_MAX
/* Implementation policy: dedicated-DRB heuristic for Dynamic 5QI without a numeric 5QI.
 * These thresholds are not specified by 3GPP but reflect a conservative mapping choice. */
#define DYN5QI_DEDICATED_DRB_PDB_THRESHOLD_MS 50
#define DYN5QI_DEDICATED_DRB_PER_SCALAR_MAX 1
#define DYN5QI_DEDICATED_DRB_PER_EXPONENT_MIN 6
#define DYN5QI_DEDICATED_DRB_QOS_PRIORITY_MAX 10

static bool eq_qfi(const void *vval, const void *vit)
{
  const int id = *(const int *)vval;
  const nr_rrc_qos_t *elem = (const nr_rrc_qos_t *)vit;
  return elem->qos.qfi == id;
}

static uint16_t get_qos_fiveqi(const pdusession_level_qos_parameter_t *qos)
{
  if (qos->fiveQI_type == NON_DYNAMIC) {
    DevAssert(qos->qos_characteristics.non_dynamic.fiveQI <= MAX_STANDARDIZED_FIVEQI);
    return qos->qos_characteristics.non_dynamic.fiveQI;
  }
  if (qos->qos_characteristics.dynamic.fiveQI != NULL) {
    DevAssert(*qos->qos_characteristics.dynamic.fiveQI <= MAX_FIVEQI);
    return *qos->qos_characteristics.dynamic.fiveQI;
  }
  return NO_FIVEQI;
}

/** @brief Retrieves mapped QoS in UE context for the input @param qfi
 *  @return pointer to the found QoS, NULL if not found */
nr_rrc_qos_t *find_qos(seq_arr_t *seq, int qfi)
{
  DevAssert(seq);
  elm_arr_t elm = find_if(seq, &qfi, eq_qfi);
  if (elm.found)
    return (nr_rrc_qos_t *)elm.it;
  return NULL;
}

/** @brief Adds a new QoS @param in to UE context the list
 *  @note A UE can have up to 64 QoS flows which can be multiplexed
 *  into one or more DRBs. The QFI is the unique ID of the mapping.
 *  @return pointer to the newly added QoS */
nr_rrc_qos_t *add_qos(seq_arr_t *qos, const pdusession_level_qos_parameter_t *in)
{
  DevAssert(qos);
  DevAssert(in);

  if (seq_arr_size(qos) == MAX_QOS_FLOWS) {
    LOG_W(NR_RRC, "Reached maximum number of QoS flows = %ld\n", seq_arr_size(qos));
    return NULL;
  }

  // Validate QFI range (0-63)
  if (in->qfi > 63) {
    LOG_E(NR_RRC, "Invalid QFI=%d: QFI must be in range 0-63. Skipping QoS flow.\n", in->qfi);
    return NULL;
  }

  // TS 23.501: "The QFI shall be unique within a PDU Session."
  // If QFI already exists, keep the existing mapping to avoid ambiguity.
  nr_rrc_qos_t *existing = find_qos(qos, in->qfi);
  if (existing) {
    LOG_W(NR_RRC, "Duplicate QoS flow QFI=%d for the same PDU session; skipping add (keeping existing)\n", in->qfi);
    return existing;
  }

  if (in->fiveQI_type == NON_DYNAMIC && !is_5qi_standardized(in->qos_characteristics.non_dynamic.fiveQI)) {
    // This is a pre-configured 5QI, not a standardized non-dynamic value: not implemented
    LOG_W(NR_RRC,
          "QoS flow QFI=%d: 5QI %d is not a standardized value. Skipping QoS flow.\n",
          in->qfi,
          in->qos_characteristics.non_dynamic.fiveQI);
    return NULL;
  }

  nr_rrc_qos_t item = {.qos = *in};
  const dynamic_5qi_t *in_dyn = &in->qos_characteristics.dynamic;
  dynamic_5qi_t *out_dyn = &item.qos.qos_characteristics.dynamic;
  if (in->fiveQI_type == DYNAMIC && in_dyn->fiveQI != NULL) {
    out_dyn->fiveQI = calloc_or_fail(1, sizeof(*out_dyn->fiveQI));
    *out_dyn->fiveQI = *in_dyn->fiveQI;
  }
  const non_dynamic_5qi_t *in_non_dyn = &in->qos_characteristics.non_dynamic;
  non_dynamic_5qi_t *out_non_dyn = &item.qos.qos_characteristics.non_dynamic;
  if (in->fiveQI_type == NON_DYNAMIC && in_non_dyn->qos_priority != NULL) {
    out_non_dyn->qos_priority = calloc_or_fail(1, sizeof(*out_non_dyn->qos_priority));
    *out_non_dyn->qos_priority = *in_non_dyn->qos_priority;
  }
  seq_arr_push_back(qos, &item, sizeof(nr_rrc_qos_t));

  // Double check successful add
  nr_rrc_qos_t *added = find_qos(qos, in->qfi);
  DevAssert(added);
  return added;
}

/** @brief Free QoS flows list items */
static void free_qos(void *ptr)
{
  nr_rrc_qos_t *q = ptr;
  if (q->qos.fiveQI_type == DYNAMIC)
    free_and_zero(q->qos.qos_characteristics.dynamic.fiveQI);
  if (q->qos.fiveQI_type == NON_DYNAMIC)
    free_and_zero(q->qos.qos_characteristics.non_dynamic.qos_priority);
}

/** @brief Free QoS flows list */
static void free_rrc_qos_list(seq_arr_t *seq)
{
  seq_arr_free(seq, free_qos);
}

/** @brief Remove a single QoS flow identified by QFI
 *  @return true if a QoS flow was removed, false if not found */
bool rm_qos(seq_arr_t *flows, int qfi)
{
  DevAssert(flows);

  // Validate QFI range (0-63)
  if (qfi < 0 || qfi > 63) {
    LOG_E(NR_RRC, "Invalid QFI=%d: QFI must be in range 0-63. Skipping QoS flow release.\n", qfi);
    return false;
  }

  nr_rrc_qos_t *qos = find_qos(flows, qfi);
  if (!qos) {
    LOG_W(NR_RRC,
          "QoS Flow To Release with QFI=%d not found, skipping release\n",
          qfi);
    return false;
  }

  seq_arr_erase_deep(flows, qos, free_qos);
  return true;
}

/** @brief Whether any QoS flow of @param session is mapped to DRB @param drb_id */
bool nr_rrc_drb_has_qos_flow(const rrc_pdu_session_param_t *session, int drb_id)
{
  DevAssert(session);
  FOR_EACH_SEQ_ARR(const nr_rrc_qos_t *, q, &session->param.qos) {
    if (q->drb_id == drb_id)
      return true;
  }
  return false;
}

/** @brief Collect the DRBs of @param session that no QoS flow maps to any more
 *
 *  @param drbs the UE's DRB list
 *  @param out  filled with the DRB IDs, at most MAX_DRBS_PER_UE
 *  @return the number of DRB IDs written to @param out */
int nr_rrc_collect_released_drbs(const seq_arr_t *drbs, const rrc_pdu_session_param_t *session, int out[MAX_DRBS_PER_UE])
{
  DevAssert(drbs);
  DevAssert(session);
  DevAssert(out);
  int n = 0;
  FOR_EACH_SEQ_ARR(const drb_t *, drb, drbs) {
    if (drb->pdusession_id != session->param.pdusession_id)
      continue;
    if (nr_rrc_drb_has_qos_flow(session, drb->drb_id))
      continue;
    if (n == MAX_DRBS_PER_UE) {
      LOG_E(NR_RRC, "PDU Session %d: more than %d released DRBs\n", session->param.pdusession_id, MAX_DRBS_PER_UE);
      break;
    }
    out[n++] = drb->drb_id;
  }
  return n;
}

static bool eq_pdu_session_id(const void *vval, const void *vit)
{
  const int id = *(const int *)vval;
  const rrc_pdu_session_param_t *elem = vit;
  return elem->param.pdusession_id == id;
}

/** @brief Free pdusession_t */
void free_pdusession(void *ptr)
{
  rrc_pdu_session_param_t *elem = ptr;
  free_rrc_qos_list(&elem->param.qos);
  free_byte_array(elem->param.nas_pdu);
}

/** @brief Retrieves PDU Session for the input ID
 *  @return pointer to the found PDU Session, NULL if not found */
rrc_pdu_session_param_t *find_pduSession(seq_arr_t *seq, int id)
{
  DevAssert(seq);
  elm_arr_t elm = find_if(seq, &id, eq_pdu_session_id);
  if (elm.found)
    return (rrc_pdu_session_param_t *)elm.it;
  return NULL;
}

/** @brief Adds a new PDU Session to the list
 *  @return pointer to the new PDU Session */
rrc_pdu_session_param_t *add_pduSession(seq_arr_t *sessions_ptr, const pdusession_t *in)
{
  DevAssert(sessions_ptr);
  DevAssert(in);

  if (seq_arr_size(sessions_ptr) == NR_MAX_NB_PDU_SESSIONS) {
    LOG_W(NR_RRC, "Reached maximum number of PDU Session = %ld\n", seq_arr_size(sessions_ptr));
    return NULL;
  }

  rrc_pdu_session_param_t *exists = find_pduSession(sessions_ptr, in->pdusession_id);
  if (exists) {
    LOG_E(NR_RRC, "Trying to add an already existing PDU Session with ID=%d\n", in->pdusession_id);
    return NULL;
  }

  rrc_pdu_session_param_t new = {.param = *in, .status = PDU_SESSION_STATUS_NEW, .xid = -1};
  seq_arr_push_back(sessions_ptr, &new, sizeof(rrc_pdu_session_param_t));
  rrc_pdu_session_param_t *added = find_pduSession(sessions_ptr, in->pdusession_id);
  DevAssert(added);
  LOG_I(NR_RRC, "Added PDU Session %d, (total nb of sessions = %ld)\n", in->pdusession_id, seq_arr_size(sessions_ptr));

  return added;
}

static bool eq_drb_pdu_session_id(const void *vval, const void *vit)
{
  const int *id = (const int *)vval;
  const drb_t *elem = (const drb_t *)vit;
  return elem->pdusession_id == *id;
}

/** @brief Finds the first DRB with the given PDU session ID.
 *  @return Pointer to matching drb_t or NULL if not found. */
drb_t *find_drb_by_pdusession_id(seq_arr_t *seq, int pdusession_id)
{
  DevAssert(seq);
  DevAssert(pdusession_id > 0 && pdusession_id <= NR_MAX_NB_PDU_SESSIONS);
  elm_arr_t elm = find_if(seq, &pdusession_id, eq_drb_pdu_session_id);
  if (elm.found)
    return (drb_t *)elm.it;
  return NULL;
}

/** @brief Removes a DRB from the list
 *  @param drbs The DRB list
 *  @param drb Pointer to the DRB to remove */
static void nr_rrc_rm_drb(seq_arr_t *drbs, drb_t *drb)
{
  DevAssert(drbs);
  DevAssert(drb);
  LOG_I(NR_RRC, "Removing DRB ID %d (PDU Session ID=%d)\n", drb->drb_id, drb->pdusession_id);
  seq_arr_erase_deep(drbs, drb, free_drb);
}

/** @brief Remove DRB by ID from list
 *  @return true if a DRB was removed, false if not found */
bool nr_rrc_remove_drb_by_id(seq_arr_t *drb_ptr, const int drb_id)
{
  DevAssert(drb_ptr);
  drb_t *drb = get_drb(drb_ptr, drb_id);
  if (!drb) {
    LOG_W(NR_RRC, "DRB ID %d not found to remove\n", drb_id);
    return false;
  }

  nr_rrc_rm_drb(drb_ptr, drb);
  return true;
}

/** @brief Removes a PDU Session from the list by ID
 *  Also removes all associated DRBs for this PDU session.
 *  @return true if successfully removed, false if not found */
bool rm_pduSession(seq_arr_t *sessions, seq_arr_t *drbs, int pdusession_id)
{
  DevAssert(sessions);
  DevAssert(drbs);

  rrc_pdu_session_param_t *session = find_pduSession(sessions, pdusession_id);

  if (session) {
    LOG_I(NR_RRC, "Removing PDU Session %d from RRC setup list\n", pdusession_id);
    // Remove all associated DRBs first
    drb_t *drb;
    while ((drb = find_drb_by_pdusession_id(drbs, pdusession_id))) {
      nr_rrc_rm_drb(drbs, drb);
    }
    // Then remove the PDU session
    seq_arr_erase_deep(sessions, session, free_pdusession);
    return true;
  }

  LOG_W(NR_RRC, "pdusession_id=%d not found to remove\n", pdusession_id);
  return false;
}

/** @brief Add drb_t item in the UE context list for @param pdusession_id */
drb_t *nr_rrc_add_drb(seq_arr_t *drb_ptr, int pdusession_id, nr_pdcp_configuration_t *pdcp)
{
  DevAssert(drb_ptr);
  DevAssert(pdcp != NULL);
  DevAssert(pdusession_id > 0 && pdusession_id <= NR_MAX_NB_PDU_SESSIONS);

  // Get next available DRB ID
  int drb_id = 0;

  // First, try to find the lowest available ID (prefer smaller IDs)
  for (drb_id = 1; drb_id <= MAX_DRBS_PER_UE; drb_id++) {
    if (get_drb(drb_ptr, drb_id) == NULL) {
      break; // Found available ID
    }
  }

  if (drb_id > MAX_DRBS_PER_UE) {
    LOG_E(NR_RRC, "Cannot set up new DRB for pdusession_id=%d - reached maximum capacity\n", pdusession_id);
    return NULL;
  }

  // Add item to the list
  drb_t in = {.drb_id = drb_id, .pdusession_id = pdusession_id, .pdcp_config = *pdcp};
  seq_arr_push_back(drb_ptr, &in, sizeof(drb_t));
  drb_t *out = get_drb(drb_ptr, drb_id);
  DevAssert(out);
  return out;
}

static bool eq_drb_id(const void *vval, const void *vit)
{
  const int id = *(const int *)vval;
  const drb_t *elem = (const drb_t *)vit;
  return elem->drb_id == id;
}

/** @brief Retrieves DRB for the input ID
 *  @return pointer to the found DRB, NULL if not found */
drb_t *get_drb(seq_arr_t *seq, int id)
{
  DevAssert(id > 0 && id <= MAX_DRBS_PER_UE);
  elm_arr_t elm = find_if(seq, &id, eq_drb_id);
  if (elm.found)
    return (drb_t *)elm.it;
  return NULL;
}

/** @brief Free pdusession_t */
void free_drb(void *ptr)
{
  // do nothing
  UNUSED(ptr);
}

rrc_pdu_session_param_t *find_pduSession_from_drbId(gNB_RRC_UE_t *ue, int drb_id)
{
  const drb_t *drb = get_drb(&ue->drbs, drb_id);
  if (!drb) {
    LOG_E(NR_RRC, "UE %d: DRB %d not found\n", ue->rrc_ue_id, drb_id);
    return NULL;
  }
  int id = drb->pdusession_id;
  return find_pduSession(&ue->pduSessions, id);
}

bearer_context_pdcp_config_t set_bearer_context_pdcp_config(const nr_pdcp_configuration_t pdcp,
                                                            bool um_on_default_drb,
                                                            const nr_redcap_ue_cap_t *redcap_cap)
{
  bearer_context_pdcp_config_t out = {0};
  if (redcap_cap && redcap_cap->support_of_redcap_r17 && !redcap_cap->pdcp_drb_long_sn_redcap_r17) {
    LOG_I(NR_RRC, "UE is RedCap without long PDCP SN support: overriding PDCP SN size to 12\n");
    out.pDCP_SN_Size_DL = NR_PDCP_Config__drb__pdcp_SN_SizeDL_len12bits;
    out.pDCP_SN_Size_UL = NR_PDCP_Config__drb__pdcp_SN_SizeUL_len12bits;
  } else {
    out.pDCP_SN_Size_DL = encode_sn_size_dl(pdcp.drb.sn_size);
    out.pDCP_SN_Size_UL = encode_sn_size_ul(pdcp.drb.sn_size);
  }
  out.discardTimer = encode_discard_timer(pdcp.drb.discard_timer);
  out.reorderingTimer = encode_t_reordering(pdcp.drb.t_reordering);
  out.rLC_Mode = um_on_default_drb ? E1AP_RLC_Mode_rlc_um_bidirectional : E1AP_RLC_Mode_rlc_am;
  out.pDCP_Reestablishment = false;
  return out;
}

static nr_sdap_configuration_t get_sdap_config(const bool enable_sdap)
{
  nr_sdap_configuration_t sdap = {.header_dl_absent = !enable_sdap, .header_ul_absent = !enable_sdap};
  return sdap;
}

/** @brief Classify 5QI value by resource type per 3GPP TS 23.501 Table 5.7.4-1 */
typedef enum {
  QOS_RESOURCE_TYPE_NON_GBR, // 5,6,7,8,9,10,11,69,70,79,80
  QOS_RESOURCE_TYPE_GBR, // 1,2,3,4,65,66,67,71,72,73,74,75,76
  QOS_RESOURCE_TYPE_DC_GBR // 82-90 (Delay-critical GBR)
} qos_resource_type_t;



static const qos_resource_type_t five_qi_resource_type[MAX_STANDARDIZED_FIVEQI + 1] = {
    /* Default (value 0) is QOS_RESOURCE_TYPE_NON_GBR */
    [1] = QOS_RESOURCE_TYPE_GBR,
    [2] = QOS_RESOURCE_TYPE_GBR,
    [3] = QOS_RESOURCE_TYPE_GBR,
    [4] = QOS_RESOURCE_TYPE_GBR,
    [65] = QOS_RESOURCE_TYPE_GBR,
    [66] = QOS_RESOURCE_TYPE_GBR,
    [67] = QOS_RESOURCE_TYPE_GBR,
    [71] = QOS_RESOURCE_TYPE_GBR,
    [72] = QOS_RESOURCE_TYPE_GBR,
    [73] = QOS_RESOURCE_TYPE_GBR,
    [74] = QOS_RESOURCE_TYPE_GBR,
    [75] = QOS_RESOURCE_TYPE_GBR,
    [76] = QOS_RESOURCE_TYPE_GBR,
    [82] = QOS_RESOURCE_TYPE_DC_GBR,
    [83] = QOS_RESOURCE_TYPE_DC_GBR,
    [84] = QOS_RESOURCE_TYPE_DC_GBR,
    [85] = QOS_RESOURCE_TYPE_DC_GBR,
    [86] = QOS_RESOURCE_TYPE_DC_GBR,
    [87] = QOS_RESOURCE_TYPE_DC_GBR,
    [88] = QOS_RESOURCE_TYPE_DC_GBR,
    [89] = QOS_RESOURCE_TYPE_DC_GBR,
    [90] = QOS_RESOURCE_TYPE_DC_GBR,
};

/** @brief Check if 5QI value is a standardized value (table 5.7.4-1 in 3GPP TS 23.501)
 * @param five_qi 5QI value to check
 * @return true if 5QI is standardized value (1-9, 65-90), false otherwise */
bool is_5qi_standardized(uint16_t five_qi)
{
  DevAssert(five_qi <= MAX_FIVEQI);
  // TS 23.501 Table 5.7.4-1: standardized 5QI values
  return (five_qi >= 1 && five_qi <= 10) || // GBR / Non-GBR
         (five_qi >= 65 && five_qi <= 67) || // GBR
         (five_qi >= 69 && five_qi <= 70) || // Non-GBR
         (five_qi >= 71 && five_qi <= 76) || // GBR
         (five_qi >= 79 && five_qi <= 80) || // Non-GBR
         (five_qi >= 82 && five_qi <= MAX_STANDARDIZED_FIVEQI); // DC-GBR
}

/** @brief Check if a standardized 5QI corresponds to a Non-GBR resource type (from TS 23.501 Table 5.7.4-1)
 * @param five_qi 5QI value to check
 * @return true if 5QI is a Non-GBR resource type, false otherwise */
bool nr_rrc_is_non_gbr_fiveqi(uint16_t five_qi)
{
  DevAssert(five_qi <= MAX_FIVEQI);
  if (!is_5qi_standardized(five_qi))
    return false;
  return five_qi_resource_type[five_qi] == QOS_RESOURCE_TYPE_NON_GBR;
}

// Design-specific DRB multiplexing limits per resource type
#define MAX_QOS_FLOWS_PER_DRB_DC_GBR 1 // Strict isolation for delay-critical GBR
#define MAX_QOS_FLOWS_PER_DRB_GBR 2 // Conservative multiplexing for GBR
#define MAX_QOS_FLOWS_PER_DRB_NON_GBR 5 // More flexible multiplexing for Non-GBR
// Design-specific aggregate cap to prevent over-multiplexing when mixing resource types on the same DRB
#define MAX_QOS_FLOWS_PER_DRB_TOTAL 5

/** @brief Decide whether a QoS flow should get a dedicated DRB.
 *
 * - If a numeric 5QI is available (Non-Dynamic 5QI, or Dynamic 5QI with 5QI present), we apply
 *   the implementation's 5QI-based isolation policy. In particular, Delay-Critical GBR (5QI 82-90)
 *   and selected high priority / low-PER services are isolated on dedicated DRBs.
 *
 * - For Dynamic 5QI where the numeric 5QI is absent, 3GPP allows relying on the signaled QoS
 *   characteristics. We use a conservative heuristic based on priority, PDB and PER thresholds */
static bool nr_rrc_qos_dedicated_drb(const pdusession_level_qos_parameter_t *qos)
{
  const uint16_t five_qi = get_qos_fiveqi(qos);
  if (five_qi != NO_FIVEQI) {
    if (!is_5qi_standardized(five_qi)) {
      return true; // Dynamic non-standardized 5QI: conservative behavior to prevent unintended multiplexing
    }
    // Delay-critical GBR (5QI 82-90) get dedicated DRBs
    qos_resource_type_t type = five_qi_resource_type[five_qi];
    if (type == QOS_RESOURCE_TYPE_DC_GBR)
      return true;
    // High priority and low-PER services get dedicated DRBs
    return (five_qi == 4 || five_qi == 6 || five_qi == 7 || five_qi == 8 || five_qi == 9 || five_qi == 10 || five_qi == 70
            || five_qi == 80 || (five_qi >= 71 && five_qi <= 73));
  }

  // Dynamic 5QI without a numeric 5QI: decide based on signaled characteristics.
  DevAssert(qos->fiveQI_type == DYNAMIC);
  const dynamic_5qi_t *dyn = &qos->qos_characteristics.dynamic;
  const qos_per_t *per = &dyn->per;
  /* Conservative policy for low-latency / high-reliability / high-priority flows. */
  const bool low_latency = dyn->packet_delay_budget <= DYN5QI_DEDICATED_DRB_PDB_THRESHOLD_MS;
  const bool high_reliability =
      (per->scalar <= DYN5QI_DEDICATED_DRB_PER_SCALAR_MAX && per->exponent >= DYN5QI_DEDICATED_DRB_PER_EXPONENT_MIN);
  const bool high_priority = dyn->qos_priority <= DYN5QI_DEDICATED_DRB_QOS_PRIORITY_MAX;
  return low_latency || high_reliability || high_priority;
}

/** @brief Count QoS flows mapped to a specific DRB, grouped by resource type.
 * This function iterates through the given list of QoS flows (`qos_flows`), and for each one
 * that is mapped to the given `drb_id`, increments a counter depending on its resource type:
 * - Delay-Critical GBR (dc_gbr_count)
 * - Non-delay-critical GBR (gbr_count)
 * - Non-GBR (non_gbr_count)
 * @param[in]  qos_flows    Pointer to the list of QoS flows (seq_arr_t)
 * @param[in]  drb_id       DRB ID to filter flows by
 * @param[out] dc_gbr_count number of DC-GBR flows for this DRB (pointer)
 * @param[out] gbr_count    number of GBR flows for this DRB (pointer)
 * @param[out] non_gbr_count number of Non-GBR flows for this DRB (pointer) */
static void nr_rrc_count_qos_flows_by_type(const seq_arr_t *qos_flows,
                                           const int drb_id,
                                           int *dc_gbr_count,
                                           int *gbr_count,
                                           int *non_gbr_count)
{
  // Initialize counters
  *dc_gbr_count = *gbr_count = *non_gbr_count = 0;
  // Iterate through the list of QoS flows
  FOR_EACH_SEQ_ARR(nr_rrc_qos_t *, qos, qos_flows) {
    // Check if the QoS flow is mapped to the given DRB ID
    if (qos->drb_id == drb_id) {
      // Increment the counter depending on the resource type of the QoS flow
      const uint16_t five_qi = get_qos_fiveqi(&qos->qos);
      if (five_qi == NO_FIVEQI || !is_5qi_standardized(five_qi)) {
        /* Dynamic 5QI may omit the numeric 5QI; in that case we can't classify by 5QI-derived resource type.
         * Count conservatively to avoid reusing/multiplexing such flows on existing DRBs. */
        (*dc_gbr_count)++;
        continue;
      }
      DevAssert(five_qi <= MAX_STANDARDIZED_FIVEQI);
      qos_resource_type_t type = five_qi_resource_type[five_qi];
      switch (type) {
        case QOS_RESOURCE_TYPE_DC_GBR:
          (*dc_gbr_count)++;
          break;
        case QOS_RESOURCE_TYPE_GBR:
          (*gbr_count)++;
          break;
        case QOS_RESOURCE_TYPE_NON_GBR:
          (*non_gbr_count)++;
          break;
        default:
          LOG_W(NR_RRC, "Unknown resource type for 5QI %u\n", five_qi);
          break;
      }
    }
  }
}

/** @brief Find existing DRB that can accept a QoS flow based on resource type and multiplexing limits */
int nr_rrc_find_suitable_drb_for_qos(gNB_RRC_UE_t *UE,
                                     const int pdusession_id,
                                     const pdusession_level_qos_parameter_t *qos_params,
                                     const seq_arr_t *flows)
{
  const uint16_t five_qi = get_qos_fiveqi(qos_params);
  if (five_qi == NO_FIVEQI || !is_5qi_standardized(five_qi)) {
    DevAssert(qos_params->fiveQI_type == DYNAMIC);
    return -1; // Dynamic 5QI without standardized numeric value: no table-based classification.
  }
  qos_resource_type_t type = five_qi_resource_type[five_qi];

  /* Dedicated flows (incl. DC-GBR 5QI 82-90): never reuse an existing DRB. Per-DRB DC-GBR
   * multiplexing limit is one dedicated DRB each. */
   if (nr_rrc_qos_dedicated_drb(qos_params) || type == QOS_RESOURCE_TYPE_DC_GBR) {
    return -1;
  }

  // For non-dedicated flows, try to find existing DRB with available capacity
  FOR_EACH_SEQ_ARR(drb_t *, drb, &UE->drbs) {
    if (drb->pdusession_id != pdusession_id)
      continue;

    int dc_gbr_count = 0, gbr_count = 0, non_gbr_count = 0;
    // Count the number of QoS flows mapped to the DRB by resource type
    nr_rrc_count_qos_flows_by_type(flows, drb->drb_id, &dc_gbr_count, &gbr_count, &non_gbr_count);
    // By OAI implementation choice, DC-GBR flows (5QI 82-90) always get dedicated DRBs and cannot be multiplexed
    // Skip DRBs that contain DC-GBR flows as they cannot be reused for other flows
    DevAssert(dc_gbr_count <= MAX_QOS_FLOWS_PER_DRB_DC_GBR);
    if (dc_gbr_count == MAX_QOS_FLOWS_PER_DRB_DC_GBR) {
      continue;
    }

    /* Do not reuse a DRB that already carries a dedicated QoS flow. */
    bool has_dedicated_flow_on_drb = false;
    FOR_EACH_SEQ_ARR(nr_rrc_qos_t *, existing_qos, flows) {
      if (existing_qos->drb_id != drb->drb_id)
        continue;
      if (nr_rrc_qos_dedicated_drb(&existing_qos->qos)) {
        has_dedicated_flow_on_drb = true;
        break;
      }
    }
    if (has_dedicated_flow_on_drb)
      continue;

    // Check if this DRB can accept more flows of this resource type
    bool has_capacity = false;
    if (type == QOS_RESOURCE_TYPE_GBR) {
      has_capacity = (gbr_count < MAX_QOS_FLOWS_PER_DRB_GBR);
    } else if (type == QOS_RESOURCE_TYPE_NON_GBR) {
      has_capacity = (non_gbr_count < MAX_QOS_FLOWS_PER_DRB_NON_GBR);
    }

    // Enforce aggregate total cap across all resource types
    int total_flows_on_drb = gbr_count + non_gbr_count;

    if (has_capacity && total_flows_on_drb < MAX_QOS_FLOWS_PER_DRB_TOTAL) {
      LOG_I(NR_RRC,
            "UE %d: found suitable DRB %d to multiplex 5QI %d (QFI %d) - current flows: GBR=%d, Non-GBR=%d\n",
            UE->rrc_ue_id,
            drb->drb_id,
            five_qi,
            qos_params->qfi,
            gbr_count,
            non_gbr_count);
      return drb->drb_id;
    }
  }

  return -1; // No suitable existing DRB found
}

/** @brief Find or create a DRB for a QoS flow and assign it
 *  @param rrc RRC instance
 *  @param UE UE context
 *  @param session PDU session containing the QoS flow
 *  @param qos QoS flow to assign DRB to (must already be in session->qos)
 *  @return true if a new DRB was created, false if existing DRB was reused
 *  @note TS 23.501 §5.7.1.5: AN binds QoS Flows to AN resources (DRBs). No strict 1:1
 *  relation between QoS Flows and DRBs is required (reuse/multiplexing is allowed).
 *  @note qos->drb_id contains the assigned DRB ID */
bool nr_rrc_assign_drb_to_qos_flow(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, const pdusession_t *session, nr_rrc_qos_t *qos)
{
  DevAssert(rrc);
  DevAssert(UE);
  DevAssert(session);
  DevAssert(qos);

  const uint16_t five_qi = get_qos_fiveqi(&qos->qos);

  // First, try to find an existing suitable DRB
  int drb_id = nr_rrc_find_suitable_drb_for_qos(UE, session->pdusession_id, &qos->qos, &session->qos);

  // If no suitable existing DRB found, create a new one
  bool is_new_drb = drb_id == -1;
  if (drb_id == -1) {
    // Create a new DRB
    drb_t *rrc_drb = nr_rrc_add_drb(&UE->drbs, session->pdusession_id, &rrc->pdcp_config);
    DevAssert(rrc_drb);
    // Store the assigned DRB ID
    drb_id = rrc_drb->drb_id;
    is_new_drb = true;
    LOG_I(NR_RRC, "UE %d: created new DRB %d for QFI %d (5QI %d)\n", UE->rrc_ue_id, drb_id, qos->qos.qfi, five_qi);
  }

  // Assign the DRB ID to the QoS flow
  DevAssert(drb_id > 0 && drb_id <= MAX_DRBS_PER_UE);
  qos->drb_id = drb_id;

  LOG_I(NR_RRC,
        "UE %d: assigned DRB %d to QFI %d (5QI %d) in PDU session %d\n",
        UE->rrc_ue_id,
        drb_id,
        qos->qos.qfi,
        five_qi,
        session->pdusession_id);

  return is_new_drb;
}

/** @brief Add PDU Sessions and DRBs to UE context list. For each QoS flow in the setup list, adds a DRB */
void nr_rrc_add_bearers(gNB_RRC_INST *rrc, gNB_RRC_UE_t *UE, int n, pdusession_t *sessions)
{
  for (int i = 0; i < n; i++) {
    bool default_set = false;
    // Retrieve SDAP configuration and PDU Session to the list
    sessions[i].sdap_config = get_sdap_config(rrc->configuration.enable_sdap);
    rrc_pdu_session_param_t *pduSession = add_pduSession(&UE->pduSessions, &sessions[i]);
    if (!pduSession) {
      LOG_E(NR_RRC, "Could not add PDU Session %d for UE %d\n", sessions[i].pdusession_id, UE->rrc_ue_id);
      continue;
    }
    pdusession_t *session = &pduSession->param;

    // Intelligent QoS to DRB mapping based on 3GPP TS 23.501
    FOR_EACH_SEQ_ARR(nr_rrc_qos_t *, qos, &session->qos) {
      nr_rrc_assign_drb_to_qos_flow(rrc, UE, session, qos);
      /* TS 23.501 §5.7.2.7: the QoS Flow associated with the default QoS rule should be a Non-GBR QoS Flow
       * from the standardized value range. Therefore, we prefer the DRB mapped from the first Non-GBR QoS flow here. */
      const uint16_t five_qi = get_qos_fiveqi(&qos->qos);
      if (five_qi == NO_FIVEQI)
        continue; // Dynamic 5QI: not eligible for default QoS rule selection here.
      DevAssert(five_qi <= MAX_FIVEQI);
      if (qos->qos.fiveQI_type == NON_DYNAMIC && nr_rrc_is_non_gbr_fiveqi(five_qi) && !default_set) {
        session->sdap_config.default_drb = qos->drb_id;
        default_set = true;
      }
    }

    if (!default_set) {
      LOG_E(NR_RRC,
            "No standardized non-GBR QoS flow found in PDU session %d; cannot set SDAP default DRB\n",
            session->pdusession_id);
    }
  }
}
