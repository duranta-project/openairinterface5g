/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include "nr_fapi.h"
#include "nfapi/oai_integration/vendor_ext.h"

void copy_vendor_extension_value(nfapi_vendor_extension_tlv_t *dst, const nfapi_vendor_extension_tlv_t *src)
{
  nfapi_tl_t *dst_tlv = (nfapi_tl_t *)dst;
  nfapi_tl_t *src_tlv = (nfapi_tl_t *)src;

  switch (dst_tlv->tag) {
    case VENDOR_EXT_TLV_2_TAG: {
      vendor_ext_tlv_2 *dst_ve = (vendor_ext_tlv_2 *)dst_tlv;
      vendor_ext_tlv_2 *src_ve = (vendor_ext_tlv_2 *)src_tlv;

      dst_ve->dummy = src_ve->dummy;
    } break;
    case VENDOR_EXT_TLV_1_TAG: {
      vendor_ext_tlv_1 *dst_ve = (vendor_ext_tlv_1 *)dst_tlv;
      vendor_ext_tlv_1 *src_ve = (vendor_ext_tlv_1 *)src_tlv;

      dst_ve->dummy = src_ve->dummy;
    } break;
    default:
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "Unknown Vendor Extension tag %d\n", dst_tlv->tag);
  }
}

bool isFAPIMessageIDValid(const uint16_t id)
{
  // SCF 222.10.04 Table 3-5 PHY API message types
  return (id >= NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST && id <= 0xFF) || id == NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE
         || id == NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC || id == NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC
         || id == NFAPI_NR_PHY_MSG_TYPE_TIMING_INFO;
}

int check_nr_fapi_unpack_length(nfapi_nr_phy_msg_type_e msgId, uint32_t unpackedBufLen)
{
  int retLen = 0;
  // check for size of nFAPI struct without the nFAPI specific parameters
  switch (msgId) {
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_param_request_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_PARAM_RESPONSE:
      if (unpackedBufLen >= sizeof(nfapi_nr_param_response_scf_t) - sizeof(nfapi_vendor_extension_tlv_t) - sizeof(nfapi_nr_nfapi_t))
        retLen = sizeof(nfapi_nr_param_request_scf_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_config_request_scf_t) - sizeof(nfapi_vendor_extension_tlv_t) - sizeof(nfapi_nr_nfapi_t))
        retLen = sizeof(nfapi_nr_config_request_scf_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_CONFIG_RESPONSE:
      if (unpackedBufLen >= sizeof(nfapi_nr_config_response_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(nfapi_nr_config_response_scf_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_START_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_start_request_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_START_RESPONSE:
      if (unpackedBufLen >= sizeof(nfapi_nr_start_response_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_STOP_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_stop_request_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_STOP_INDICATION:
      if (unpackedBufLen >= sizeof(nfapi_nr_stop_indication_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_ERROR_INDICATION:
      if (unpackedBufLen >= sizeof(nfapi_nr_error_indication_scf_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(fapi_message_header_t);

      break;
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_dl_tti_request_t))
        retLen = sizeof(nfapi_nr_dl_tti_request_t);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_ul_tti_request_t))
        retLen = sizeof(nfapi_nr_ul_tti_request_t);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION:
    case NFAPI_NR_PHY_MSG_TYPE_VENDOR_EXT_SLOT_RESPONSE:
      if (unpackedBufLen >= sizeof(nfapi_nr_slot_indication_scf_t))
        retLen = sizeof(nfapi_nr_slot_indication_scf_t);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_ul_dci_request_t))
        retLen = sizeof(nfapi_nr_ul_dci_request_t);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST:
      if (unpackedBufLen >= sizeof(nfapi_nr_tx_data_request_t))
        retLen = sizeof(nfapi_nr_tx_data_request_t);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION:
      if (unpackedBufLen >= sizeof(nfapi_nr_rx_data_indication_t))
        retLen = sizeof(nfapi_nr_rx_data_indication_t);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION:
      if (unpackedBufLen >= sizeof(nfapi_nr_crc_indication_t))
        retLen = sizeof(nfapi_nr_crc_indication_t);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION:
      if (unpackedBufLen >= sizeof(nfapi_nr_rach_indication_t))
        retLen = sizeof(nfapi_nr_rach_indication_t);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION:
      if (unpackedBufLen >= sizeof(nfapi_nr_uci_indication_t))
        retLen = sizeof(nfapi_nr_uci_indication_t);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION:
      if (unpackedBufLen >= sizeof(nfapi_nr_srs_indication_t))
        retLen = sizeof(nfapi_nr_srs_indication_t);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_SRS_TOA_VENDOR_EXTENSION_INDICATION:
      if (unpackedBufLen >= sizeof(nfapi_nr_srs_toa_vendor_ext_indication_t))
        retLen = sizeof(nfapi_nr_srs_toa_vendor_ext_indication_t);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC:
      if (unpackedBufLen >= sizeof(nfapi_nr_dl_node_sync_t))
        retLen = sizeof(nfapi_nr_dl_node_sync_t);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC:
      if (unpackedBufLen >= sizeof(nfapi_nr_ul_node_sync_t))
        retLen = sizeof(nfapi_nr_ul_node_sync_t);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_TIMING_INFO:
      if (unpackedBufLen >= sizeof(nfapi_nr_timing_info_t) - sizeof(nfapi_vendor_extension_tlv_t))
        retLen = sizeof(nfapi_nr_timing_info_t);
      break;
    default:
      NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s Unknown message ID %d\n", __FUNCTION__, msgId);
      break;
  }

  return retLen;
}

bool fapi_nr_p7_message_header_unpack(void *pMessageBuf,
                                     uint32_t messageBufLen,
                                     void *pUnpackedBuf,
                                     uint32_t unpackedBufLen,
                                     nfapi_p7_codec_config_t *config)
{
  UNUSED(config);
  return fapi_nr_message_header_unpack(pMessageBuf,
                                       messageBufLen,
                                       pUnpackedBuf,
                                       unpackedBufLen,
                                       NULL);
}

bool fapi_nr_message_header_unpack(void *pMessageBuf,
                                  uint32_t messageBufLen,
                                  void *pUnpackedBuf,
                                  uint32_t unpackedBufLen,
                                  nfapi_p4_p5_codec_config_t *config)
{
  UNUSED(config);
  uint8_t *pReadPackedMessage = pMessageBuf;
  nfapi_nr_p4_p5_message_header_t *header = pUnpackedBuf;
  fapi_message_header_t fapi_msg = {0};

  if (pMessageBuf == NULL || pUnpackedBuf == NULL || messageBufLen < NFAPI_HEADER_LENGTH
      || unpackedBufLen < sizeof(fapi_message_header_t)) {
    return false;
  }
  uint8_t *end = pMessageBuf + messageBufLen;
  // process the header
  int result =
      (pull8(&pReadPackedMessage, &fapi_msg.num_msg, end) && pull8(&pReadPackedMessage, &fapi_msg.opaque_handle, end)
       && pull16(&pReadPackedMessage, &header->message_id, end) && pull32(&pReadPackedMessage, &header->message_length, end));
  // the SCF opaque handle byte carries the phy_id
  header->phy_id = fapi_msg.opaque_handle;
  return result != 0;
}
