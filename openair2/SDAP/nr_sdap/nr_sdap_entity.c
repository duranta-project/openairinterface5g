/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_sdap_entity.h"
#include <openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.h>
#include <openair3/ocp-gtpu/gtp_itf.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "T.h"
#include "assertions.h"
#include "common/utils/T/T.h"
#include "gtpv1_u_messages_types.h"
#include "intertask_interface.h"
#include "rlc.h"
#include "nr_sdap.h"

#define NO_SDAP_HEADER 0

typedef struct {
  nr_sdap_entity_t *sdap_entity_llist;
} nr_sdap_entity_info;

static nr_sdap_entity_info sdap_info;

instance_t *N3GTPUInst = NULL;

static void remove_ip_if(nr_sdap_entity_t *entity)
{
  DevAssert(entity != NULL);
  nr_sdap_tun_detach(entity);
  if (!entity->is_gnb)
    return;
  nr_sdap_tun_destroy(entity->ue_id, entity->pdusession_id);
}

/** @brief Returns a bitmap indicating the SDAP entity role,
 *        i.e. for UL transmission, header for UL data is present in RX/TX
 *             for DL transmission, header for DL data is present in RX/TX */
static int get_sdap_role(bool is_gnb, const NR_SDAP_Config_t *sdap_config)
{
  sdap_role_t role_ul = is_gnb ? SDAP_UL_RX : SDAP_UL_TX;
  sdap_role_t role_dl = is_gnb ? SDAP_DL_TX : SDAP_DL_RX;
  int role = NO_SDAP_HEADER;
  if (sdap_config->sdap_HeaderUL == NR_SDAP_Config__sdap_HeaderUL_present)
    role |= role_ul;
  if (sdap_config->sdap_HeaderDL == NR_SDAP_Config__sdap_HeaderDL_present)
    role |= role_dl;
  return role;
}

void nr_pdcp_submit_sdap_ctrl_pdu(ue_id_t ue_id, int sdap_ctrl_pdu_drb, nr_sdap_ul_hdr_t ctrl_pdu)
{

  protocol_ctxt_t ctxt = { .rntiMaybeUEid = ue_id };
  nr_pdcp_data_req_drb(&ctxt,
                       SRB_FLAG_NO,
                       sdap_ctrl_pdu_drb,
                       RLC_MUI_UNDEFINED,
                       SDU_CONFIRM_NO,
                       SDAP_HDR_LENGTH,
                       (unsigned char *)&ctrl_pdu,
                       PDCP_TRANSMISSION_MODE_UNKNOWN,
                       NULL,
                       NULL);
  LOG_D(SDAP, "Control PDU - Submitting Control PDU to DRB ID: %d\n", sdap_ctrl_pdu_drb);
  LOG_D(SDAP, "QFI: %u\n R: %u\n D/C: %u\n", ctrl_pdu.QFI, ctrl_pdu.R, ctrl_pdu.DC);
  return;
}

static bool nr_sdap_tx_entity(nr_sdap_entity_t *entity,
                              protocol_ctxt_t *ctxt_p,
                              const srb_flag_t srb_flag,
                              const mui_t mui,
                              const confirm_t confirm,
                              const sdu_size_t sdu_buffer_size,
                              unsigned char *const sdu_buffer,
                              const pdcp_transmission_mode_t pt_mode,
                              const uint32_t *sourceL2Id,
                              const uint32_t *destinationL2Id,
                              const uint8_t qfi,
                              const bool rqi) {
  /* The offset of the SDAP header, it might be 0 if has_sdap_tx is not true in the pdcp entity. */
  int offset=0;
  bool ret = false;
  bool sdap_ul_tx = false;
  bool sdap_dl_tx = false;

  if(sdu_buffer == NULL) {
    LOG_E(SDAP, "%s:%d:%s: NULL sdu_buffer \n", __FILE__, __LINE__, __FUNCTION__);
    exit(1);
  }

  uint8_t sdap_buf[SDAP_MAX_PDU];
  int drb_id = entity->qfi2drb_map(entity, qfi);

  if (drb_id != SDAP_MAP_RULE_EMPTY) {
    sdap_ul_tx = entity->qfi2drb_table[qfi].entity_role & SDAP_UL_TX; // UE TX entity
    sdap_dl_tx = entity->qfi2drb_table[qfi].entity_role & SDAP_DL_TX; // gNB TX entity
    LOG_D(SDAP, "TX - QFI: %u is mapped to DRB ID: %d\n", qfi, entity->qfi2drb_table[qfi].drb_id);
  } else {
    LOG_E(SDAP, "QFI %u not mapped to any DRB - dropping SDU of size %d\n", qfi, sdu_buffer_size);
    return false;
  }

  if (!sdap_ul_tx && !sdap_dl_tx) {
    LOG_D(SDAP, "TX - DRB ID: %d does not have SDAP header\n", entity->qfi2drb_table[qfi].drb_id);
    ret = nr_pdcp_data_req_drb(ctxt_p,
                               srb_flag,
                               drb_id,
                               mui,
                               confirm,
                               sdu_buffer_size,
                               sdu_buffer,
                               pt_mode,
                               sourceL2Id,
                               destinationL2Id);

    if(!ret)
      LOG_E(SDAP, "%s:%d:%s: PDCP refused PDU\n", __FILE__, __LINE__, __FUNCTION__);

    return ret;
  }

  if(sdu_buffer_size == 0 || sdu_buffer_size > 8999) {
    LOG_E(SDAP, "%s:%d:%s: NULL or 0 or exceeded sdu_buffer_size (over max PDCP SDU)\n", __FILE__, __LINE__, __FUNCTION__);
    return 0;
  }

  if (sdap_dl_tx) { // create DL Data PDU with SDAP header
    offset = SDAP_HDR_LENGTH;
    /*
     * TS 37.324 4.4 Functions
     * marking QoS flow ID in DL packets.
     *
     * Construct the DL SDAP data PDU.
     */
    nr_sdap_dl_hdr_t sdap_hdr;
    sdap_hdr.QFI = qfi;
    sdap_hdr.RQI = rqi;
    sdap_hdr.RDI = 0; // SDAP Hardcoded Value
    /* Add the SDAP DL Header to the buffer */
    memcpy(&sdap_buf[0], &sdap_hdr, SDAP_HDR_LENGTH);
    memcpy(&sdap_buf[SDAP_HDR_LENGTH], sdu_buffer, sdu_buffer_size);
    LOG_D(SDAP, "TX Entity QFI: %u \n", sdap_hdr.QFI);
    LOG_D(SDAP, "TX Entity RQI: %u \n", sdap_hdr.RQI);
    LOG_D(SDAP, "TX Entity RDI: %u \n", sdap_hdr.RDI);
  } else if (sdap_ul_tx) { // create UL Data PDU with SDAP header
    offset = SDAP_HDR_LENGTH;
    /*
     * TS 37.324 4.4 Functions
     * marking QoS flow ID in UL packets.
     *
     * 5.2.1 Uplink
     * construct the UL SDAP data PDU as specified in the subclause 6.2.2.3.
     */
    nr_sdap_ul_hdr_t sdap_hdr;
    sdap_hdr.QFI = qfi;
    sdap_hdr.R = 0;
    sdap_hdr.DC = rqi;
    /* Add the SDAP UL Header to the buffer */
    memcpy(&sdap_buf[0], &sdap_hdr, SDAP_HDR_LENGTH);
    memcpy(&sdap_buf[SDAP_HDR_LENGTH], sdu_buffer, sdu_buffer_size);
    LOG_D(SDAP, "TX Entity QFI: %u \n", sdap_hdr.QFI);
    LOG_D(SDAP, "TX Entity R:   %u \n", sdap_hdr.R);
    LOG_D(SDAP, "TX Entity DC:  %u \n", sdap_hdr.DC);
  }

  /*
   * TS 37.324 5.2 Data transfer
   * 5.2.1 Uplink UE side
   * submit the constructed UL SDAP data PDU to the lower layers
   *
   * Downlink gNB side
   */
  ret = nr_pdcp_data_req_drb(ctxt_p,
                             srb_flag,
                             drb_id,
                             mui,
                             confirm,
                             sdu_buffer_size + offset,
                             sdap_buf,
                             pt_mode,
                             sourceL2Id,
                             destinationL2Id);

  if(!ret)
    LOG_E(SDAP, "%s:%d:%s: PDCP refused PDU\n", __FILE__, __LINE__, __FUNCTION__);

  return ret;
}

static void nr_sdap_rx_entity(nr_sdap_entity_t *entity,
                              int pdcp_entity,
                              int is_gnb,
                              int pdusession_id,
                              ue_id_t ue_id,
                              char *buf,
                              int size)
{
  bool sdap_ul_rx = false;
  bool sdap_dl_rx = false;
  int qfi = -1;
  /* If SDAP header is disabled for this entity, bypass header parsing */
  if (entity->enable_sdap) {
    /** Extract QFI from SDAP header for UL direction
     * Per TS 37.324 §6.2.2, QFI is carried in SDAP header when SDAP header is present
     * This QFI will be used in GTP-U extension header (PDU Session Container)
     * for N3-U tunnel (the first 6 bits in the first octet) */
    qfi = buf[0] & 0x3F;
    if (qfi >= SDAP_MAX_QFI) {
      LOG_E(SDAP, "Invalid QFI %d received in SDAP header\n", qfi);
      return;
    }
    // Accept SDAP packets only for QFIs that still have an active mapping.
    const qfi2drb_t *mapping = &entity->qfi2drb_table[qfi];
    /* If the QFI has no active mapping, drop the packet: it can happen
     * after QoS-flow removal/reconfiguration due to packets that were already enqueued in PDCP */
    if (mapping->drb_id == SDAP_NO_MAPPING_RULE) {
      LOG_W(SDAP,
            "Dropping UL SDAP PDU with unmapped QFI=%d (ue=%ld pdu_session=%d drb=%d)\n",
            qfi,
            ue_id,
            pdusession_id,
            pdcp_entity);
      return;
    }
    sdap_ul_rx = mapping->entity_role & SDAP_UL_RX; // gNB RX entity
    sdap_dl_rx = mapping->entity_role & SDAP_DL_RX; // UE RX entity
  }


  if (is_gnb) { // gNB
    /** QFI extracted from SDAP header for UL direction (N3-U tunnel)
     * QFI range: 0..63 (6 bits) */
    if (sdap_ul_rx) { // UL Data/Control PDU with SDAP header
      DevAssert(qfi >= 0 && qfi < SDAP_MAX_QFI);
      int offset = SDAP_HDR_LENGTH;
      nr_sdap_ul_hdr_t *sdap_hdr = (nr_sdap_ul_hdr_t *)buf;
      DevAssert(sdap_hdr->QFI == qfi);
      LOG_D(SDAP, "RX Entity Received QFI:    %u\n", qfi);
      LOG_D(SDAP, "RX Entity Received R bit:  %u\n", sdap_hdr->R);
      LOG_D(SDAP, "RX Entity Received DC bit: %u\n", sdap_hdr->DC);

      switch (sdap_hdr->DC) {
        case SDAP_HDR_UL_DATA_PDU:
          LOG_D(SDAP, "RX Entity Received SDAP Data PDU\n");
          break;

        case SDAP_HDR_UL_CTRL_PDU:
          LOG_D(SDAP, "RX Entity Received SDAP Control PDU\n");
          break;
      }
      // Pushing SDAP SDU to GTP-U Layer
      size_t gtp_len = size - offset;
      uint8_t *gtp_buf = (uint8_t *)buf + offset;
      LOG_D(SDAP,
            "UL SDAP->GTP: ue=%lu pdu_session=%d qfi=%u payload_len=%zu\n",
            ue_id,
            pdusession_id,
            qfi,
            gtp_len);
      /** Send to GTP-U with QFI marking for N3-U tunnel
       * Per TS 29.281 §5.2, QFI is carried in PDU Session Container extension header
       * For N3-U tunnels (qfi>=0), QFI is added to extension header for UL routing at UPF */
      gtpv1uSendDirectWithQFI(*N3GTPUInst, ue_id, pdusession_id, qfi, gtp_buf, gtp_len);
    } else {
      /* N3-U tunnel with no SDAP header: no QFI marking */
      DevAssert(qfi == -1);
      LOG_D(SDAP, "UL SDAP->GTP: ue=%lu pdu_session=%d payload_len=%d\n", ue_id, pdusession_id, size);
      gtpv1uSendDirect(*N3GTPUInst, ue_id, pdusession_id, (uint8_t *)buf, size, false, false);
    }
  } else { //nrUE
    /*
     * TS 37.324 5.2 Data transfer
     * 5.2.2 Downlink
     * if the DRB from which this SDAP data PDU is received is configured by RRC with the presence of SDAP header.
     */
    int offset = 0;
    if (sdap_dl_rx) { // DL Data/Control PDU with SDAP header
      offset = SDAP_HDR_LENGTH;
      /*
       * TS 37.324 5.2 Data transfer
       * 5.2.2 Downlink
       * retrieve the SDAP SDU from the DL SDAP data PDU as specified in the subclause 6.2.2.2.
       */
      nr_sdap_dl_hdr_t *sdap_hdr = (nr_sdap_dl_hdr_t *)buf;
      LOG_D(SDAP, "RX Entity Received QFI : %u\n", sdap_hdr->QFI);
      LOG_D(SDAP, "RX Entity Received RQI : %u\n", sdap_hdr->RQI);
      LOG_D(SDAP, "RX Entity Received RDI : %u\n", sdap_hdr->RDI);

      /*
       * TS 37.324 5.2 Data transfer
       * 5.2.2 Downlink
       * Perform reflective QoS flow to DRB mapping as specified in the subclause 5.3.2.
       */
      if(sdap_hdr->RDI == SDAP_REFLECTIVE_MAPPING) {
        LOG_D(SDAP, "RX - Performing Reflective Mapping\n");
        /*
         * TS 37.324 5.3 QoS flow to DRB Mapping 
         * 5.3.2 Reflective mapping
         * If there is no stored QoS flow to DRB mapping rule for the QoS flow and a default DRB is configured.
         */
        if(!entity->qfi2drb_table[sdap_hdr->QFI].drb_id && entity->default_drb){
          nr_sdap_ul_hdr_t sdap_ctrl_pdu = entity->sdap_construct_ctrl_pdu(sdap_hdr->QFI);
          int sdap_ctrl_pdu_drb = entity->sdap_map_ctrl_pdu(entity, SDAP_CTRL_PDU_MAP_DEF_DRB, sdap_hdr->QFI);
          entity->sdap_submit_ctrl_pdu(ue_id, sdap_ctrl_pdu_drb, sdap_ctrl_pdu);
        }

        /*
         * TS 37.324 5.3 QoS flow to DRB mapping 
         * 5.3.2 Reflective mapping
         * if the stored QoS flow to DRB mapping rule for the QoS flow 
         * is different from the QoS flow to DRB mapping of the DL SDAP data PDU
         * and
         * the DRB according to the stored QoS flow to DRB mapping rule is configured by RRC
         * with the presence of UL SDAP header
         */
        if (pdcp_entity != entity->qfi2drb_table[sdap_hdr->QFI].drb_id) {
          nr_sdap_ul_hdr_t sdap_ctrl_pdu = entity->sdap_construct_ctrl_pdu(sdap_hdr->QFI);
          int sdap_ctrl_pdu_drb = entity->sdap_map_ctrl_pdu(entity, SDAP_CTRL_PDU_MAP_RULE_DRB, sdap_hdr->QFI);
          entity->sdap_submit_ctrl_pdu(ue_id, sdap_ctrl_pdu_drb, sdap_ctrl_pdu);
        }

        /*
         * TS 37.324 5.3 QoS flow to DRB Mapping 
         * 5.3.2 Reflective mapping
         * store the QoS flow to DRB mapping of the DL SDAP data PDU as the QoS flow to DRB mapping rule for the UL. 
         */ 
        entity->qfi2drb_table[sdap_hdr->QFI].drb_id = pdcp_entity;
      }

      /*
       * TS 37.324 5.2 Data transfer
       * 5.2.2 Downlink
       * perform RQI handling as specified in the subclause 5.4
       */
      if(sdap_hdr->RQI == SDAP_RQI_HANDLING) {
        LOG_W(SDAP, "UE - TODD 5.4\n");
      }
    } /*  else - retrieve the SDAP SDU from the DL SDAP data PDU as specified in the subclause 6.2.2.1 */

    /*
     * TS 37.324 5.2 Data transfer
     * 5.2.2 Downlink
     * deliver the retrieved SDAP SDU to the upper layer.
     */
    if (entity->pdusession_sock < 0) {
      LOG_D(SDAP, "[UE %ld] PDU session %d: TUN not attached, drop DL SDU (%d B)\n", ue_id, pdusession_id, size - offset);
      return;
    }
    int len = write(entity->pdusession_sock, &buf[offset], size - offset);
    LOG_D(SDAP, "RX Entity len : %d\n", len);
    LOG_D(SDAP, "RX Entity size : %d\n", size);
    LOG_D(SDAP, "RX Entity offset : %d\n", offset);

    if (len != size-offset)
      LOG_E(SDAP, "write failed to fd %d! errno = %s\n", entity->pdusession_sock, strerror(errno));
  }
}

static void nr_sdap_qfi2drb_map_add(nr_sdap_entity_t *entity,
                                    const uint8_t qfi,
                                    const uint8_t drb_id,
                                    const uint8_t role)
{
  qfi2drb_t *qfi2drb = &entity->qfi2drb_table[qfi];
  LOG_D(SDAP, "%s mapping: QFI %u -> DRB %d \n", qfi2drb->drb_id == SDAP_NO_MAPPING_RULE ? "Add" : "Update", qfi, drb_id);
  qfi2drb->drb_id = drb_id;
  qfi2drb->entity_role = role;
}

static void nr_sdap_qfi2drb_map_del(nr_sdap_entity_t *entity, const uint8_t qfi)
{
  qfi2drb_t *qfi2drb = &entity->qfi2drb_table[qfi];
  qfi2drb->drb_id = SDAP_NO_MAPPING_RULE;
  qfi2drb->entity_role = NO_SDAP_HEADER;
  LOG_D(SDAP, "Deleted mapping for QFI=%d, DRB=%d\n", qfi, qfi2drb->drb_id);
}

/**
 * @brief   get the DRB ID mapped to the QFI, for both DL and UL
 * @return  DRB that is mapped to the QFI
 *          or the default DRB if no mapping rule exists
 *          or 0 if no mapping and no default DRB exists for that QFI
 * @ref     TS 37.324, 5.2.1 Uplink
 *          If there is no stored QoS flow to DRB mapping rule for the
 *          QoS flow as specified in the subclause 5.3, and a default DRB is configured,
 *          map the SDAP SDU to the default DRB else, map the SDAP SDU to the DRB according
 *          to the stored QoS flow to DRB mapping rule. */
static int nr_sdap_qfi2drb(nr_sdap_entity_t *entity, uint8_t qfi)
{
  /* Fetch DRB ID mapped to QFI */
  int drb_id = entity->qfi2drb_table[qfi].drb_id;
  if (drb_id) {
    /* QoS flow to DRB mapping rule exists, return corresponding DRB ID */
    LOG_D(SDAP, "Existing QoS flow to DRB mapping rule: QFI %u to DRB %d\n", qfi, drb_id);
    return drb_id;
  } else if (entity->default_drb) {
    /* QoS flow to DRB mapping rule does not exist, map SDAP SDU to default DRB, e.g. return default DRB of the SDAP entity */
    LOG_D(SDAP, "QoS flow to DRB mapping rule does not exists! mapping SDU to Default DRB: %d\n", entity->default_drb);
    return entity->default_drb;
  } else {
    /* Note: UE undefined behaviour when neither a default DRB
       nor a stored QoS flow to DRB mapping rule exists */
    LOG_E(SDAP, "Mapping rule and default DRB do not exist for QFI:%u\n", qfi);
    return SDAP_MAP_RULE_EMPTY;
  }
}

nr_sdap_ul_hdr_t nr_sdap_construct_ctrl_pdu(uint8_t qfi){
  nr_sdap_ul_hdr_t sdap_end_marker_hdr;
  sdap_end_marker_hdr.QFI = qfi;
  sdap_end_marker_hdr.R = 0;
  sdap_end_marker_hdr.DC = SDAP_HDR_UL_CTRL_PDU;
  LOG_D(SDAP, "Constructed Control PDU with QFI:%u R:%u DC:%u \n", sdap_end_marker_hdr.QFI,
                                                                   sdap_end_marker_hdr.R,
                                                                   sdap_end_marker_hdr.DC);
  return sdap_end_marker_hdr;
}

int nr_sdap_map_ctrl_pdu(nr_sdap_entity_t *entity, int map_type, uint8_t dl_qfi)
{
  int drb_of_endmarker = 0;
  if(map_type == SDAP_CTRL_PDU_MAP_DEF_DRB){
    drb_of_endmarker = entity->default_drb;
    LOG_D(SDAP, "Mapping Control PDU QFI: %u to Default DRB: %d\n", dl_qfi, drb_of_endmarker);
  }
  if(map_type == SDAP_CTRL_PDU_MAP_RULE_DRB){
    drb_of_endmarker = entity->qfi2drb_map(entity, dl_qfi);
    LOG_D(SDAP, "Mapping Control PDU QFI: %u to DRB: %d\n", dl_qfi, drb_of_endmarker);
  }
  return drb_of_endmarker;
}

/**
 * @brief Submit the end-marker control PDU to PDCP according to TS 37.324, clause 5.3
 */
void nr_sdap_submit_ctrl_pdu(ue_id_t ue_id, int sdap_ctrl_pdu_drb, nr_sdap_ul_hdr_t ctrl_pdu)
{
  if(sdap_ctrl_pdu_drb){
    nr_pdcp_submit_sdap_ctrl_pdu(ue_id, sdap_ctrl_pdu_drb, ctrl_pdu);
    LOG_D(SDAP, "Sent Control PDU to PDCP Layer.\n");
  }
}

/** @brief UL QoS flow to DRB mapping configuration for a SDAP entity has already been established
 *         according to TS 37.324, 5.3 QoS flow to DRB Mapping, 5.3.1 Configuration Procedures. */
static void nr_sdap_ue_control_pdu_config(nr_sdap_entity_t *entity, const ue_id_t ue_id, const sdap_config_t *sdap)
{
  // handle QFIs to DRB mapping rule to add
  for (int i = 0; i < sdap->mappedQFIs2AddCount; i++) {
    uint8_t qfi = sdap->mappedQFIs2Add[i];
    /* a default DRB exists and there is no stored QFI to DRB mapping rule for the QFI */
    if (entity->default_drb && entity->qfi2drb_table[qfi].drb_id == SDAP_NO_MAPPING_RULE) {
      // construct an end-marker control PDU (6.2.3 TS 37.324)
      nr_sdap_ul_hdr_t sdap_ctrl_pdu = entity->sdap_construct_ctrl_pdu(qfi);
      // map the end-marker control PDU to the default DRB
      int sdap_ctrl_pdu_drb = entity->sdap_map_ctrl_pdu(entity, SDAP_CTRL_PDU_MAP_DEF_DRB, qfi);
      // submit the end-marker control PDU to the lower layers
      entity->sdap_submit_ctrl_pdu(ue_id, sdap_ctrl_pdu_drb, sdap_ctrl_pdu);
    }
    /* the stored UL QFI to DRB mapping rule is different from the configured one and has UL SDAP header */
    bool ul_sdap_header = (entity->qfi2drb_table[qfi].entity_role & SDAP_UL_TX) != 0;
    if (entity->qfi2drb_table[qfi].drb_id != sdap->drb_id && ul_sdap_header) {
      // construct an end-marker control PDU (6.2.3 TS 37.324)
      nr_sdap_ul_hdr_t sdap_ctrl_pdu = entity->sdap_construct_ctrl_pdu(qfi);
      // map the end-marker control PDU to the DRB according to the stored QoS flow to DRB mapping rule
      int sdap_ctrl_pdu_drb = entity->sdap_map_ctrl_pdu(entity, SDAP_CTRL_PDU_MAP_RULE_DRB, qfi);
      // submit the end-marker control PDU to the lower layers
      entity->sdap_submit_ctrl_pdu(ue_id, sdap_ctrl_pdu_drb, sdap_ctrl_pdu);
    }
  }
}

/** @brief Add QoS flows to a DRB */
static void nr_sdap_add_qos_flows_to_drb(nr_sdap_entity_t *entity, const sdap_config_t *sdap)
{
  for (int i = 0; i < sdap->mappedQFIs2AddCount; i++) {
    uint8_t qfi = sdap->mappedQFIs2Add[i];
    LOG_D(SDAP, "Adding QFI to DRB mapping rules: %d mapped QFIs for DRB %d\n", sdap->mappedQFIs2AddCount, sdap->drb_id);
    if (qfi < SDAP_MAX_QFI && sdap->drb_id > SDAP_MAP_RULE_EMPTY && sdap->drb_id <= MAX_DRBS_PER_UE) {
      entity->qfi2drb_map_add(entity, qfi, sdap->drb_id, sdap->role);
    } else {
      LOG_E(SDAP, "Failed to add qfi2drb mapping: QFI=%d, DRB=%d\n", qfi, sdap->drb_id);
    }
  }
}

/** @brief Remove QoS flows from a DRB */
static void nr_sdap_rm_qos_flows_from_drb(nr_sdap_entity_t *entity, const sdap_config_t *sdap)
{
  for (int i = 0; i < sdap->mappedQFIs2ReleaseCount; i++) {
    uint8_t qfi = sdap->mappedQFIs2Release[i];
    LOG_D(SDAP, "Removing QFI from DRB mapping rules: QFI=%d for DRB=%d\n", qfi, sdap->drb_id);
    entity->qfi2drb_map_delete(entity, qfi);
  }
}

/** @brief Update QFI to DRB mapping rules
 * @param qfi the QoS Flow index, used as unique index of the qfi2drb mapping table
 * @param drb the DRB ID to be mapped */
static void nr_sdap_qfi2drb_map_update(nr_sdap_entity_t *entity, const sdap_config_t *sdap)
{
  /* disabled SDAP: only one DRB per PDU session existing qfi2drb_table entries must match the DRB to be updated */
  if (!entity->enable_sdap) {
    for (int i = 0; i < SDAP_MAX_QFI; i++) {
      int mapped = entity->qfi2drb_table[i].drb_id;
      if (mapped != SDAP_NO_MAPPING_RULE) {
        AssertFatal(mapped == sdap->drb_id,
                    "PDU session %d: disabled SDAP allows only one DRB per PDU session (qfi2drb_table[%d] maps to DRB %d)\n",
                    entity->pdusession_id,
                    i,
                    mapped);
      }
    }
  }
  if (!entity->is_gnb) { // UE control PDU configuration
    nr_sdap_ue_control_pdu_config(entity, entity->ue_id, sdap);
  }
  nr_sdap_add_qos_flows_to_drb(entity, sdap);
  nr_sdap_rm_qos_flows_from_drb(entity, sdap);
}

/**
 * @brief   add a new SDAP entity according to 5.1.1. of 3GPP TS 37.324
 * @note    there is one SDAP entity per PDU session
 *
 * @param   is_gnb, indicates whether it is for gNB or UE
 * @param   ue_id, UE ID
 * @param   sdap, SDAP configuration */
static void nr_sdap_add_entity(const int is_gnb, const ue_id_t ue_id, const sdap_config_t *sdap)
{
  nr_sdap_entity_t *sdap_entity = calloc_or_fail(1, sizeof(*sdap_entity));
  LOG_I(SDAP, "Creating SDAP entity ue_id=%ld pdu_session_id=%d\n", ue_id, sdap->pdusession_id);

  // SDAP entity ids
  sdap_entity->ue_id = ue_id;
  sdap_entity->pdusession_id = sdap->pdusession_id;
  sdap_entity->is_gnb = is_gnb;
  sdap_entity->enable_sdap = (sdap->role != NO_SDAP_HEADER);

  // rx/tx entities
  sdap_entity->tx_entity = nr_sdap_tx_entity;
  sdap_entity->rx_entity = nr_sdap_rx_entity;

  // control pdu function pointers
  sdap_entity->sdap_construct_ctrl_pdu = nr_sdap_construct_ctrl_pdu;
  sdap_entity->sdap_map_ctrl_pdu = nr_sdap_map_ctrl_pdu;
  sdap_entity->sdap_submit_ctrl_pdu = nr_sdap_submit_ctrl_pdu;

  // QFI to DRB mapping functions pointers
  sdap_entity->qfi2drb_map_update = nr_sdap_qfi2drb_map_update;
  sdap_entity->qfi2drb_map_add = nr_sdap_qfi2drb_map_add;
  sdap_entity->qfi2drb_map_delete = nr_sdap_qfi2drb_map_del;
  sdap_entity->qfi2drb_map = nr_sdap_qfi2drb;
  sdap_entity->pdusession_sock = -1;

  // set default DRB
  if (sdap->defaultDRB) {
    sdap_entity->default_drb = sdap->drb_id;
    LOG_I(SDAP, "Default DRB for the created SDAP entity: DRB %d \n", sdap_entity->default_drb);
  }

  if (!sdap_entity->enable_sdap) {
    AssertFatal(sdap->mappedQFIs2AddCount <= 1,
                "PDU session %d: disabled SDAP allows at most one QoS flow to add at entity creation\n",
                sdap->pdusession_id);
  }

  // Add QoS flows to the DRB (initial configuration)
  nr_sdap_add_qos_flows_to_drb(sdap_entity, sdap);

  // update SDAP entity list pointers
  sdap_entity->next_entity = sdap_info.sdap_entity_llist;
  sdap_info.sdap_entity_llist = sdap_entity;

  if (IS_SOFTMODEM_NOS1 && is_gnb) {
    // In NOS1 mode, terminate SDAP for the first UE on the gNB. This allows injecting/receiving
    // PDCP SDUs to/from the TUN interface.
    start_sdap_tun_gnb_first_ue_default_pdu_session(ue_id, sdap_entity->pdusession_id);
  }

  if (!is_gnb) {
    /* No-op on first setup until NAS registers the TUN. After paging/service request,
     * re-attach the preserved UE TUN for the established PDU session (TS 38.304 clause 7.1,
     * TS 24.501 clauses 5.6.2.2.1/5.6.1.1 restore UP resources for an established PDU session). */
    nr_sdap_tun_attach(sdap_entity);
  }
}

/** @brief Add or modify an SDAP entity if it already exists */
void nr_sdap_addmod_entity(const int is_gnb, const ue_id_t ue_id, const sdap_config_t *sdap)
{
  nr_sdap_entity_t *sdap_entity = nr_sdap_get_entity(ue_id, sdap->pdusession_id);
  if (sdap_entity) {
    sdap_entity->qfi2drb_map_update(sdap_entity, sdap);
  } else {
    nr_sdap_add_entity(is_gnb, ue_id, sdap);
  }
}

/**
 * @brief   Fetches the SDAP entity for the give PDU session ID.
 * @note    There is one SDAP entity per PDU session.
 * @return  The pointer to the SDAP entity if existing, NULL otherwise
 */
nr_sdap_entity_t *nr_sdap_get_entity(ue_id_t ue_id, int pdusession_id)
{
  nr_sdap_entity_t *sdap_entity;
  sdap_entity = sdap_info.sdap_entity_llist;

  if (sdap_entity == NULL) {
    LOG_W(SDAP, " Could not find SDAP entity: entity list empty (ue_id=%ld pdu_session_id=%d)\n", ue_id, pdusession_id);
    return NULL;
  }

  while ((sdap_entity->ue_id != ue_id || sdap_entity->pdusession_id != pdusession_id) && sdap_entity->next_entity != NULL) {
    sdap_entity = sdap_entity->next_entity;
  }

  if (sdap_entity->ue_id == ue_id && sdap_entity->pdusession_id == pdusession_id)
    return sdap_entity;

  return NULL;
}

void nr_sdap_release_drb(ue_id_t ue_id, int drb_id, int pdusession_id)
{
  // remove all QoS flow to DRB mappings associated with the released DRB
  nr_sdap_entity_t *sdap = nr_sdap_get_entity(ue_id, pdusession_id);
  if (sdap) {
    for (int i = 0; i < SDAP_MAX_QFI; i++) {
      if (sdap->qfi2drb_table[i].drb_id == drb_id)
        sdap->qfi2drb_table[i].drb_id = SDAP_NO_MAPPING_RULE;
    }
  }
  else
    LOG_E(SDAP, "Couldn't find a SDAP entity associated with PDU session ID %d\n", pdusession_id);
}

/** @brief Set QoS-flow-to-DRB mapping for a given DRB from the E1 QoS Flow list.
 * On E1 Bearer Context Modification (CP to UP), Flow Mapping Information in DRB To Modify
 * carries the QoS Flow QoS Parameters List for that DRB. Per TS 38.463, when present the
 * CU-UP replaces the previous mapping for that DRB.
 * Remove every QFI currently mapped to the DRB that is absent from the list of QFIs to be mapped.
 * @param qfis[] list of QFIs to be mapped to the DRB
 * @param n_qfis number of QFIs in the list
 * @param drb_id the DRB ID to be mapped */
void nr_sdap_entity_update_qos_flows(ue_id_t ue_id, int pdusession_id, int drb_id, const uint8_t *qfis, int n_qfis)
{
  nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, pdusession_id);
  if (!entity) {
    LOG_W(SDAP, "gNB SDAP: no entity for UE %lu PDU session %d when updating QoS flows for DRB %d\n", ue_id, pdusession_id, drb_id);
    return;
  }

  /* Derive SDAP role from entity configuration */
  uint8_t role = NO_SDAP_HEADER;
  if (entity->enable_sdap) {
    if (entity->is_gnb) {
      role |= SDAP_UL_RX;
      role |= SDAP_DL_TX;
    } else {
      role |= SDAP_UL_TX;
      role |= SDAP_DL_RX;
    }
  }

  sdap_config_t sdap = {.pdusession_id = pdusession_id,
                        .drb_id = drb_id,
                        .role = role,
                        .defaultDRB = (entity->default_drb == drb_id),
                        .mappedQFIs2AddCount = n_qfis};

  /* Build list of QFIs to be released */
  for (int q = 0; q < SDAP_MAX_QFI; q++) {
    if (entity->qfi2drb_table[q].drb_id != drb_id)
      continue;
    bool keep = false;
    for (int i = 0; i < n_qfis; i++) {
      if (qfis[i] == q) {
        keep = true;
        break;
      }
    }
    if (!keep) {
      DevAssert(sdap.mappedQFIs2ReleaseCount < SDAP_MAX_QFI);
      sdap.mappedQFIs2Release[sdap.mappedQFIs2ReleaseCount++] = q;
      LOG_I(SDAP, "gNB SDAP mapping update: UE %lu pduSession %d DRB %d release_qfi=%d\n", ue_id, pdusession_id, drb_id, q);
    }
  }

  /* Build list of QFIs to be added */
  for (int i = 0; i < n_qfis; i++)
    sdap.mappedQFIs2Add[i] = qfis[i];

  LOG_I(SDAP, "gNB SDAP mapping update: UE %lu pduSession %d DRB %d n_qfis=%d\n", ue_id, pdusession_id, drb_id, n_qfis);

  entity->qfi2drb_map_update(entity, &sdap);
}

bool nr_sdap_delete_entity(ue_id_t ue_id, int pdusession_id)
{
  nr_sdap_entity_t *entityPtr = sdap_info.sdap_entity_llist;
  nr_sdap_entity_t *entityPrev = NULL;
  int upperBound = 0;

  if (entityPtr == NULL && (pdusession_id < 0 || pdusession_id > NR_MAX_NB_PDU_SESSIONS)) {
    LOG_E(SDAP, "SDAP entities not established or Invalid range of pdusession_id [0, %d].\n", NR_MAX_NB_PDU_SESSIONS);
    return false;
  }

  if (entityPtr->ue_id == ue_id && entityPtr->pdusession_id == pdusession_id) {
    sdap_info.sdap_entity_llist = sdap_info.sdap_entity_llist->next_entity;
    remove_ip_if(entityPtr);
    free(entityPtr);
    LOG_D(SDAP, "Successfully deleted SDAP entity for UE %lx and PDU Session id %d\n", ue_id, pdusession_id);
    return true;
  } else {
    while ((entityPtr->ue_id != ue_id || entityPtr->pdusession_id != pdusession_id) && entityPtr->next_entity != NULL
           && upperBound < SDAP_MAX_NUM_OF_ENTITIES) {
      entityPrev = entityPtr;
      entityPtr = entityPtr->next_entity;
      upperBound++;
    }

    if (entityPtr->ue_id == ue_id && entityPtr->pdusession_id == pdusession_id) {
      entityPrev->next_entity = entityPtr->next_entity;
      remove_ip_if(entityPtr);
      free(entityPtr);
      LOG_D(SDAP, "Successfully deleted Entity for UE %lx and PDU Session id %d\n", ue_id, pdusession_id);
      return true;
    }
  }
  LOG_E(SDAP, "Entity does not exist or it was not found.\n");
  return false;
}

bool nr_sdap_delete_ue_entities(ue_id_t ue_id)
{
  nr_sdap_entity_t *entityPtr = sdap_info.sdap_entity_llist;
  nr_sdap_entity_t *entityPrev = NULL;
  int upperBound = 0;
  bool ret = false;

  if (entityPtr == NULL && (ue_id) * (ue_id - SDAP_MAX_UE_ID) > 0) {
    LOG_W(SDAP, "SDAP entities not established or Invalid range of ue_id [0, 65536]\n");
    return ret;
  }

  /* Handle scenario where ue_id matches the head of the list */
  while (entityPtr != NULL && entityPtr->ue_id == ue_id && upperBound < MAX_DRBS_PER_UE) {
    sdap_info.sdap_entity_llist = entityPtr->next_entity;
    remove_ip_if(entityPtr);
    free(entityPtr);
    entityPtr = sdap_info.sdap_entity_llist;
    ret = true;
  }

  while (entityPtr != NULL && upperBound < SDAP_MAX_NUM_OF_ENTITIES) {
    if (entityPtr->ue_id != ue_id) {
      entityPrev = entityPtr;
      entityPtr = entityPtr->next_entity;
    } else {
      entityPrev->next_entity = entityPtr->next_entity;
      remove_ip_if(entityPtr);
      free(entityPtr);
      entityPtr = entityPrev->next_entity;
      LOG_I(SDAP, "Successfully deleted SDAP entity for UE %ld\n", ue_id);
      ret = true;
    }
  }
  return ret;
}

/** @brief This function gets the relevant SDAP config from the received SDAP-Config */
sdap_config_t nr_sdap_get_config(const int is_gnb, const NR_SDAP_Config_t *sdap_Config, const int drb_id)
{
  DevAssert(sdap_Config);
  sdap_config_t sdapConfig = {0};
  sdapConfig.drb_id = drb_id;
  sdapConfig.role = get_sdap_role(is_gnb, sdap_Config);
  LOG_D(SDAP, "SDAP headers %s\n", sdapConfig.role == NO_SDAP_HEADER ? "absent" : "present");
  sdapConfig.defaultDRB = sdap_Config->defaultDRB;
  // 3GPP TS 38.331 The network sets sdap-HeaderUL to present if the field defaultDRB is set to true
  if (sdapConfig.defaultDRB && (sdap_Config->sdap_HeaderUL != NR_SDAP_Config__sdap_HeaderUL_present))
    LOG_D(SDAP, "Received SDAP-Config with defaultDRB but sdap-HeaderUL is not present\n");
  if (sdap_Config->mappedQoS_FlowsToAdd) {
    sdapConfig.mappedQFIs2AddCount = sdap_Config->mappedQoS_FlowsToAdd->list.count;
    DevAssert(sdapConfig.mappedQFIs2AddCount <= SDAP_MAX_QFI);
    LOG_D(SDAP, "DRB %d: mapped QFIs = %d  \n", sdapConfig.drb_id, sdapConfig.mappedQFIs2AddCount);
    for (int i = 0; i < sdapConfig.mappedQFIs2AddCount; i++){
      DevAssert(i < SDAP_MAX_QFI);
      sdapConfig.mappedQFIs2Add[i] = *sdap_Config->mappedQoS_FlowsToAdd->list.array[i];
      LOG_D(SDAP, "Captured mappedQoS_FlowsToAdd[%d] from RRC: %ld\n", i, sdapConfig.mappedQFIs2Add[i]);
    }
  }
  sdapConfig.pdusession_id = sdap_Config->pdu_Session;
  if (sdap_Config->mappedQoS_FlowsToRelease) {
    sdapConfig.mappedQFIs2ReleaseCount = sdap_Config->mappedQoS_FlowsToRelease->list.count;
    DevAssert(sdapConfig.mappedQFIs2ReleaseCount <= SDAP_MAX_QFI);
    for (int i = 0; i < sdapConfig.mappedQFIs2ReleaseCount; i++) {
      DevAssert(i < SDAP_MAX_QFI);
      sdapConfig.mappedQFIs2Release[i] = *sdap_Config->mappedQoS_FlowsToRelease->list.array[i];
    }
  }
  return sdapConfig;
}

/**
 * @brief SDAP Entity reconfiguration at UE according to TS 37.324
 *        and triggered by RRC reconfiguration events according to clause 5.3.5.6.5 of TS 38.331.
 *        This function performs:
 *        - QoS flow to DRB mapping according to clause 5.3.1 of TS 37.324
 */
void nr_reconfigure_sdap_entity(NR_SDAP_Config_t *sdap_config, ue_id_t ue_id, int pdusession_id, int drb_id)
{
  bool is_gnb = false;
  /* fetch SDAP entity */
  nr_sdap_entity_t *sdap_entity = nr_sdap_get_entity(ue_id, pdusession_id);
  AssertError(sdap_entity != NULL,
              return,
              "Could not find SDAP Entity for RNTI/UE ID: %lu and PDU SESSION ID: %d\n",
              ue_id,
              pdusession_id);
  /* QFI to DRB mapping */
  sdap_config_t sdap = nr_sdap_get_config(is_gnb, sdap_config, drb_id);
  sdap_entity->qfi2drb_map_update(sdap_entity, &sdap);
}

void set_qfi(uint8_t qfi, uint8_t pduid, ue_id_t ue_id)
{
  DevAssert(qfi < SDAP_MAX_QFI);
  nr_sdap_entity_t *entity = nr_sdap_get_entity(ue_id, pduid);
  DevAssert(entity != NULL);
  entity->qfi = qfi;
  nr_sdap_tun_store_qfi(ue_id, pduid, qfi);
}
