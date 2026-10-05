/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <assert.h>
#include <signal.h>
#include <sys/types.h>
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include "assertions.h"

#include <debug.h>
#include "nfapi_nr_interface_scf.h"
#include "nr_nfapi_p7.h"
#include "nr_fapi.h"
#include "nr_fapi_p7.h"
#include <nfapi.h>

static uint32_t nfapi_nr_calculate_checksum(uint8_t *buffer, uint16_t len)
{
  uint32_t chksum = 0;
  // calcaulte upto the checksum
  chksum = crc32(chksum, buffer, 10);
  // skip the checksum
  uint8_t zeros[4] = {0, 0, 0, 0};
  chksum = crc32(chksum, zeros, 4);
  // continu with the rest of the mesage
  chksum = crc32(chksum, &buffer[NFAPI_NR_P7_HEADER_LENGTH], len - NFAPI_NR_P7_HEADER_LENGTH);
  // return the inverse
  return ~(chksum);
}

int nfapi_nr_p7_update_checksum(uint8_t *buffer, uint32_t len)
{
  uint32_t checksum = nfapi_nr_calculate_checksum(buffer, len);
  // 10 is the beginning position of checksum
  uint8_t *p_write = &buffer[10];
  return (push32(checksum, &p_write, buffer + len) > 0 ? 0 : -1);
}

int nfapi_nr_p7_update_transmit_timestamp(uint8_t *buffer, uint32_t timestamp)
{
  // 14 is the beginning position of transmit_timestamp
  uint8_t *p_write = &buffer[14];
  return (push32(timestamp, &p_write, buffer + NFAPI_NR_P7_HEADER_LENGTH) > 0 ? 0 : -1);
}

uint32_t nfapi_nr_p7_calculate_checksum(uint8_t *buffer, uint32_t len)
{
  return nfapi_nr_calculate_checksum(buffer, len);
}

uint8_t pack_nr_dl_node_sync(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config)
{
  nfapi_nr_dl_node_sync_t *pNfapiMsg = (nfapi_nr_dl_node_sync_t *)msg;
  return (push32(pNfapiMsg->t1, ppWritePackedMsg, end) && pushs32(pNfapiMsg->delta_sfn_slot, ppWritePackedMsg, end)
          && pack_nr_p7_vendor_extension_tlv(pNfapiMsg->vendor_extension, ppWritePackedMsg, end, config));
}

uint8_t pack_nr_ul_node_sync(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config)
{
  nfapi_nr_ul_node_sync_t *pNfapiMsg = (nfapi_nr_ul_node_sync_t *)msg;
  return (push32(pNfapiMsg->t1, ppWritePackedMsg, end) && push32(pNfapiMsg->t2, ppWritePackedMsg, end)
          && push32(pNfapiMsg->t3, ppWritePackedMsg, end)
          && pack_nr_p7_vendor_extension_tlv(pNfapiMsg->vendor_extension, ppWritePackedMsg, end, config));
}

uint8_t pack_nr_timing_info(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end, nfapi_p7_codec_config_t *config)
{
  nfapi_nr_timing_info_t *pNfapiMsg = (nfapi_nr_timing_info_t *)msg;
  return (push32(pNfapiMsg->last_sfn, ppWritePackedMsg, end) && push32(pNfapiMsg->last_slot, ppWritePackedMsg, end)
          && push32(pNfapiMsg->time_since_last_timing_info, ppWritePackedMsg, end)
          && push32(pNfapiMsg->dl_tti_jitter, ppWritePackedMsg, end)
          && push32(pNfapiMsg->tx_data_request_jitter, ppWritePackedMsg, end)
          && push32(pNfapiMsg->ul_tti_jitter, ppWritePackedMsg, end) && push32(pNfapiMsg->ul_dci_jitter, ppWritePackedMsg, end)
          && pushs32(pNfapiMsg->dl_tti_latest_delay, ppWritePackedMsg, end)
          && pushs32(pNfapiMsg->tx_data_request_latest_delay, ppWritePackedMsg, end)
          && pushs32(pNfapiMsg->ul_tti_latest_delay, ppWritePackedMsg, end)
          && pushs32(pNfapiMsg->ul_dci_latest_delay, ppWritePackedMsg, end)
          && pushs32(pNfapiMsg->dl_tti_earliest_arrival, ppWritePackedMsg, end)
          && pushs32(pNfapiMsg->tx_data_request_earliest_arrival, ppWritePackedMsg, end)
          && pushs32(pNfapiMsg->ul_tti_earliest_arrival, ppWritePackedMsg, end)
          && pushs32(pNfapiMsg->ul_dci_earliest_arrival, ppWritePackedMsg, end)
          && pack_nr_p7_vendor_extension_tlv(pNfapiMsg->vendor_extension, ppWritePackedMsg, end, config));
}

int pack_nr_srs_channel_svd_representation(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen)
{
  nfapi_nr_srs_channel_svd_representation_t *value = (nfapi_nr_srs_channel_svd_representation_t *)pMessageBuf;

  const uint16_t iq_size = value->normalized_iq_representation == 0 ? 2 : 4;
  const uint16_t ng = value->num_gnb_antenna_elements;
  const uint8_t nu = value->num_ue_srs_ports;

  uint8_t *pWritePackedMessage = pPackedBuf;
  uint8_t *end = pPackedBuf + packedBufLen;

  if (!(push8(value->normalized_iq_representation, &pWritePackedMessage, end)
        && push8(value->normalized_singular_value_representation, &pWritePackedMessage, end)
        && pushs8(value->singular_value_scaling, &pWritePackedMessage, end)
        && push16(value->num_gnb_antenna_elements, &pWritePackedMessage, end)
        && push8(value->num_ue_srs_ports, &pWritePackedMessage, end) && push16(value->prg_size, &pWritePackedMessage, end)
        && push16(value->num_prgs, &pWritePackedMessage, end))) {
    return 0;
  }

  for (int prg_idx = 0; prg_idx < value->num_prgs; ++prg_idx) {
    const nfapi_nr_srs_channel_svd_representation_prg_t *prg = &value->prg_list[prg_idx];
    if (!(pusharray8(prg->left_eigenvectors_matrix_u, nu * nu * iq_size, nu * nu * iq_size, &pWritePackedMessage, end))) {
      return 0;
    }
    if (!(pusharray8(prg->diagonal_entries_of_singular_matrix_sum_f, nu * iq_size, nu * iq_size, &pWritePackedMessage, end))) {
      return 0;
    }
    if (!(pusharray8(prg->complex_conjugate_of_matrix_of_right_eigenvectors_v_hf,
                     nu * ng * iq_size,
                     nu * ng * iq_size,
                     &pWritePackedMessage,
                     end))) {
      return 0;
    }
  }

  // Message length
  uintptr_t msgHead = (uintptr_t)pPackedBuf;
  uintptr_t msgEnd = (uintptr_t)pWritePackedMessage;
  return (int)(msgEnd - msgHead);
}

int pack_nr_srs_normalized_channel_iq_matrix(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen)
{
  nfapi_nr_srs_normalized_channel_iq_matrix_t *nr_srs_normalized_channel_iq_matrix =
      (nfapi_nr_srs_normalized_channel_iq_matrix_t *)pMessageBuf;

  uint8_t *pWritePackedMessage = pPackedBuf;
  uint8_t *end = pPackedBuf + packedBufLen;

  if (!(push8(nr_srs_normalized_channel_iq_matrix->normalized_iq_representation, &pWritePackedMessage, end)
        && push16(nr_srs_normalized_channel_iq_matrix->num_gnb_antenna_elements, &pWritePackedMessage, end)
        && push16(nr_srs_normalized_channel_iq_matrix->num_ue_srs_ports, &pWritePackedMessage, end)
        && push16(nr_srs_normalized_channel_iq_matrix->prg_size, &pWritePackedMessage, end)
        && push16(nr_srs_normalized_channel_iq_matrix->num_prgs, &pWritePackedMessage, end))) {
    return 0;
  }

  uint32_t channel_matrix_size = (uint32_t)nr_srs_normalized_channel_iq_matrix->num_prgs
                                 * nr_srs_normalized_channel_iq_matrix->num_ue_srs_ports
                                 * nr_srs_normalized_channel_iq_matrix->num_gnb_antenna_elements;
  if (nr_srs_normalized_channel_iq_matrix->normalized_iq_representation == 0) {
    // 0: 16-bit normalized complex number (iqSize = 2)
    channel_matrix_size <<= 1;
  } else {
    // 1: 32-bit normalized complex number (iqSize = 4)
    channel_matrix_size <<= 2;
  }
  if (channel_matrix_size > sizeof(nr_srs_normalized_channel_iq_matrix->channel_matrix)) {
    return 0;
  }

  for (uint32_t i = 0; i < channel_matrix_size; i++) {
    if (!push8(nr_srs_normalized_channel_iq_matrix->channel_matrix[i], &pWritePackedMessage, end)) {
      return 0;
    }
  }

  // Message length
  uintptr_t msgHead = (uintptr_t)pPackedBuf;
  uintptr_t msgEnd = (uintptr_t)pWritePackedMessage;
  return (int)(msgEnd - msgHead);
}

static uint8_t pack_nr_srs_reported_symbol(nfapi_nr_srs_reported_symbol_t *prgs, uint8_t **ppWritePackedMsg, uint8_t *end) {

  if(!push16(prgs->num_prgs, ppWritePackedMsg, end)) {
    return 0;
  }

  for(int i = 0; i < prgs->num_prgs; i++) {
    if (!push8(prgs->prg_list[i].rb_snr, ppWritePackedMsg, end)) {
      return 0;
    }
  }

  return 1;
}

int pack_nr_srs_beamforming_report(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen)
{
  nfapi_nr_srs_beamforming_report_t *nr_srs_beamforming_report = (nfapi_nr_srs_beamforming_report_t *)pMessageBuf;

  uint8_t *pWritePackedMessage = pPackedBuf;
  uint8_t *end = pPackedBuf + packedBufLen;

  if (!(push16(nr_srs_beamforming_report->prg_size, &pWritePackedMessage, end)
        && push8(nr_srs_beamforming_report->num_symbols, &pWritePackedMessage, end)
        && push8(nr_srs_beamforming_report->wide_band_snr, &pWritePackedMessage, end)
        && push8(nr_srs_beamforming_report->num_reported_symbols, &pWritePackedMessage, end))) {
    return 0;
  }

  for (int reported_symbol = 0; reported_symbol < nr_srs_beamforming_report->num_reported_symbols; ++reported_symbol) {
    if (!pack_nr_srs_reported_symbol(&nr_srs_beamforming_report->reported_symbol_list[reported_symbol],
                                     &pWritePackedMessage,
                                     end)) {
      return 0;
    }
  }

  // Message length
  uintptr_t msgHead = (uintptr_t)pPackedBuf;
  uintptr_t msgEnd = (uintptr_t)pWritePackedMessage;
  return (int)(msgEnd - msgHead);
}

int nfapi_nr_p7_message_pack(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen, nfapi_p7_codec_config_t *config)
{
  nfapi_nr_p7_message_header_t *pMessageHeader = pMessageBuf;
  uint8_t *pWritePackedMessage = pPackedBuf;
  uint8_t *pPackedLengthField = &pWritePackedMessage[4];

  if (pMessageBuf == NULL || pPackedBuf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack supplied pointers are null\n");
    return -1;
  }

  uint8_t *end = (uint8_t *)pPackedBuf + packedBufLen;

  // process the header
  if (!(push16(pMessageHeader->phy_id, &pWritePackedMessage, end) && push16(pMessageHeader->message_id, &pWritePackedMessage, end)
        && push32(0 /*pMessageHeader->message_length*/, &pWritePackedMessage, end)
        && push16(pMessageHeader->m_segment_sequence, &pWritePackedMessage, end)
        && push32(0 /*pMessageHeader->checksum*/, &pWritePackedMessage, end)
        && push32(pMessageHeader->transmit_timestamp, &pWritePackedMessage, end))) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack header failed\n");
    return -1;
  }

  if (pMessageHeader->message_id != NFAPI_NR_PHY_MSG_TYPE_TIMING_INFO) {
    // NFAPI_TRACE(NFAPI_TRACE_INFO, "%s() message_id:0x%04x phy_id:%u m_segment_sequence:%u timestamp:%u\n", __FUNCTION__,
    // pMessageHeader->message_id, pMessageHeader->phy_id, pMessageHeader->m_segment_sequence, pMessageHeader->transmit_timestamp);
  }
  // look for the specific message
  uint8_t result = 0;
  switch (pMessageHeader->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
      result = pack_dl_tti_request(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST:
      result = pack_ul_tti_request(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST:
      result = pack_tx_data_request(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST:
      result = pack_ul_dci_request(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION:
      result = pack_nr_slot_indication(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION:
      result = pack_nr_rx_data_indication(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION:
      result = pack_nr_crc_indication(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION:
      result = pack_nr_uci_indication(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION:
      result = pack_nr_srs_indication(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_SRS_TOA_VENDOR_EXTENSION_INDICATION:
      result = pack_nr_srs_toa_vendor_ext_indication(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION:
      result = pack_nr_rach_indication(pMessageHeader, &pWritePackedMessage, end);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC:
      result = pack_nr_dl_node_sync(pMessageHeader, &pWritePackedMessage, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC:
      result = pack_nr_ul_node_sync(pMessageHeader, &pWritePackedMessage, end, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_TIMING_INFO:
      result = pack_nr_timing_info(pMessageHeader, &pWritePackedMessage, end, config);
      break;

    default: {
      if (pMessageHeader->message_id >= NFAPI_VENDOR_EXT_MSG_MIN && pMessageHeader->message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        if (config && config->pack_p7_vendor_extension) {
          result = (config->pack_p7_vendor_extension)(pMessageHeader, &pWritePackedMessage, end, config);
        } else {
          NFAPI_TRACE(NFAPI_TRACE_ERROR,
                      "%s VE NFAPI message ID %d. No ve ecoder provided\n",
                      __FUNCTION__,
                      pMessageHeader->message_id);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s NFAPI Unknown message ID %d\n", __FUNCTION__, pMessageHeader->message_id);
      }
    } break;
  }

  if (result == 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack failed to pack message\n");
    return -1;
  }

  // check for a valid message length
  uintptr_t msgHead = (uintptr_t)pPackedBuf;
  uintptr_t msgEnd = (uintptr_t)pWritePackedMessage;
  uint32_t packedMsgLen = msgEnd - msgHead;
  if (packedMsgLen > packedBufLen) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Packed message length error %d, buffer supplied %d\n", packedMsgLen, packedBufLen);
    return -1;
  }

  // Update the message length in the header
  pMessageHeader->message_length = packedMsgLen;

  if (!push32(pMessageHeader->message_length, &pPackedLengthField, end))
    return -1;

  if (1) {
    // quick test
    if (pMessageHeader->message_length != packedMsgLen) {
      NFAPI_TRACE(NFAPI_TRACE_ERROR,
                  "nfapi packedMsgLen(%d) != message_length(%d) id %d\n",
                  packedMsgLen,
                  pMessageHeader->message_length,
                  pMessageHeader->message_id);
    }
  }

  return (int)packedMsgLen;
}

int unpack_nr_srs_channel_svd_representation(void *pMessageBuf, uint32_t messageBufLen, void *pUnpackedBuf, uint32_t unpackedBufLen)
{
  nfapi_nr_srs_channel_svd_representation_t *value = (nfapi_nr_srs_channel_svd_representation_t *)pUnpackedBuf;
  uint8_t *pReadPackedMessage = pMessageBuf;
  uint8_t *end = pMessageBuf + messageBufLen;

  memset(pUnpackedBuf, 0, unpackedBufLen);
  if (!(pull8(&pReadPackedMessage, &value->normalized_iq_representation, end)
        && pull8(&pReadPackedMessage, &value->normalized_singular_value_representation, end)
        && pulls8(&pReadPackedMessage, &value->singular_value_scaling, end)
        && pull16(&pReadPackedMessage, &value->num_gnb_antenna_elements, end)
        && pull8(&pReadPackedMessage, &value->num_ue_srs_ports, end) && pull16(&pReadPackedMessage, &value->prg_size, end)
        && pull16(&pReadPackedMessage, &value->num_prgs, end))) {
    return -1;
  }
  const uint16_t iq_size = value->normalized_iq_representation == 0 ? 2 : 4;
  const uint16_t ng = value->num_gnb_antenna_elements;
  const uint8_t nu = value->num_ue_srs_ports;

  value->prg_list = calloc(value->num_prgs, sizeof(*value->prg_list));

  for (int prg_idx = 0; prg_idx < value->num_prgs; ++prg_idx) {
    nfapi_nr_srs_channel_svd_representation_prg_t *prg = &value->prg_list[prg_idx];
    prg->left_eigenvectors_matrix_u = calloc(nu * nu * iq_size, sizeof(*prg->left_eigenvectors_matrix_u));
    if (!(pullarray8(&pReadPackedMessage, prg->left_eigenvectors_matrix_u, nu * nu * iq_size, nu * nu * iq_size, end))) {
      return 0;
    }
    prg->diagonal_entries_of_singular_matrix_sum_f = calloc(nu * iq_size, sizeof(*prg->diagonal_entries_of_singular_matrix_sum_f));
    if (!(pullarray8(&pReadPackedMessage, prg->diagonal_entries_of_singular_matrix_sum_f, nu * iq_size, nu * iq_size, end))) {
      return 0;
    }
    prg->complex_conjugate_of_matrix_of_right_eigenvectors_v_hf =
        calloc(nu * ng * iq_size, sizeof(*prg->complex_conjugate_of_matrix_of_right_eigenvectors_v_hf));
    if (!(pullarray8(&pReadPackedMessage,
                     prg->complex_conjugate_of_matrix_of_right_eigenvectors_v_hf,
                     nu * ng * iq_size,
                     nu * ng * iq_size,
                     end))) {
      return 0;
    }
  }
  return 1;
}

int unpack_nr_srs_normalized_channel_iq_matrix(void *pMessageBuf,
                                               uint32_t messageBufLen,
                                               void *pUnpackedBuf,
                                               uint32_t unpackedBufLen)
{
  nfapi_nr_srs_normalized_channel_iq_matrix_t *nr_srs_normalized_channel_iq_matrix =
      (nfapi_nr_srs_normalized_channel_iq_matrix_t *)pUnpackedBuf;
  uint8_t *pReadPackedMessage = pMessageBuf;
  uint8_t *end = pMessageBuf + messageBufLen;

  memset(pUnpackedBuf, 0, unpackedBufLen);

  if (!(pull8(&pReadPackedMessage, &nr_srs_normalized_channel_iq_matrix->normalized_iq_representation, end)
        && pull16(&pReadPackedMessage, &nr_srs_normalized_channel_iq_matrix->num_gnb_antenna_elements, end)
        && pull16(&pReadPackedMessage, &nr_srs_normalized_channel_iq_matrix->num_ue_srs_ports, end)
        && pull16(&pReadPackedMessage, &nr_srs_normalized_channel_iq_matrix->prg_size, end)
        && pull16(&pReadPackedMessage, &nr_srs_normalized_channel_iq_matrix->num_prgs, end))) {
    return -1;
  }

  uint32_t channel_matrix_size = (uint32_t)nr_srs_normalized_channel_iq_matrix->num_prgs
                                 * nr_srs_normalized_channel_iq_matrix->num_ue_srs_ports
                                 * nr_srs_normalized_channel_iq_matrix->num_gnb_antenna_elements;
  if (nr_srs_normalized_channel_iq_matrix->normalized_iq_representation == 0) {
    // 0: 16-bit normalized complex number (iqSize = 2)
    channel_matrix_size <<= 1;
  } else {
    // 1: 32-bit normalized complex number (iqSize = 4)
    channel_matrix_size <<= 2;
  }
  if (channel_matrix_size > sizeof(nr_srs_normalized_channel_iq_matrix->channel_matrix)) {
    return -1;
  }

  for (uint32_t i = 0; i < channel_matrix_size; i++) {
    if (!pull8(&pReadPackedMessage, &nr_srs_normalized_channel_iq_matrix->channel_matrix[i], end)) {
      return 0;
    }
  }

  return 1;
}

static uint8_t unpack_nr_srs_reported_symbol(nfapi_nr_srs_reported_symbol_t *prgs, uint8_t **ppReadPackedMsg, uint8_t *end) {

  if(!pull16(ppReadPackedMsg, &prgs->num_prgs, end)) {
    return 0;
  }

  for(int i = 0; i < prgs->num_prgs; i++) {
    if (!pull8(ppReadPackedMsg, &prgs->prg_list[i].rb_snr, end)) {
      return 0;
    }
  }

  return 1;
}

int unpack_nr_srs_beamforming_report(void *pMessageBuf, uint32_t messageBufLen, void *pUnpackedBuf, uint32_t unpackedBufLen)
{
  nfapi_nr_srs_beamforming_report_t *nr_srs_beamforming_report = (nfapi_nr_srs_beamforming_report_t *)pUnpackedBuf;
  uint8_t *pReadPackedMessage = pMessageBuf;
  uint8_t *end = pMessageBuf + messageBufLen;

  memset(pUnpackedBuf, 0, unpackedBufLen);

  if (!(pull16(&pReadPackedMessage, &nr_srs_beamforming_report->prg_size, end)
        && pull8(&pReadPackedMessage, &nr_srs_beamforming_report->num_symbols, end)
        && pull8(&pReadPackedMessage, &nr_srs_beamforming_report->wide_band_snr, end)
        && pull8(&pReadPackedMessage, &nr_srs_beamforming_report->num_reported_symbols, end))) {
    return -1;
  }

  for (int reported_symbol = 0; reported_symbol < nr_srs_beamforming_report->num_reported_symbols; ++reported_symbol) {
    if (!unpack_nr_srs_reported_symbol(&nr_srs_beamforming_report->reported_symbol_list[reported_symbol],
                                       &pReadPackedMessage,
                                       end)) {
      return -1;
    }
  }

  return 0;
}

static uint8_t unpack_nr_dl_node_sync(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config)
{
  nfapi_nr_dl_node_sync_t *pNfapiMsg = (nfapi_nr_dl_node_sync_t *)msg;
  return (pull32(ppReadPackedMsg, &pNfapiMsg->t1, end) && pulls32(ppReadPackedMsg, &pNfapiMsg->delta_sfn_slot, end)
          && unpack_nr_p7_tlv_list(NULL, 0, ppReadPackedMsg, end, config, &pNfapiMsg->vendor_extension));
}

static uint8_t unpack_nr_ul_node_sync(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config)
{
  nfapi_nr_ul_node_sync_t *pNfapiMsg = (nfapi_nr_ul_node_sync_t *)msg;
  return (pull32(ppReadPackedMsg, &pNfapiMsg->t1, end) && pull32(ppReadPackedMsg, &pNfapiMsg->t2, end)
          && pull32(ppReadPackedMsg, &pNfapiMsg->t3, end)
          && unpack_nr_p7_tlv_list(NULL, 0, ppReadPackedMsg, end, config, &pNfapiMsg->vendor_extension));
}

static uint8_t unpack_nr_timing_info(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg, nfapi_p7_codec_config_t *config)
{
  nfapi_nr_timing_info_t *pNfapiMsg = (nfapi_nr_timing_info_t *)msg;
  return (pull32(ppReadPackedMsg, &pNfapiMsg->last_sfn, end) && pull32(ppReadPackedMsg, &pNfapiMsg->last_slot, end)
          && pull32(ppReadPackedMsg, &pNfapiMsg->time_since_last_timing_info, end)
          && pull32(ppReadPackedMsg, &pNfapiMsg->dl_tti_jitter, end)
          && pull32(ppReadPackedMsg, &pNfapiMsg->tx_data_request_jitter, end)
          && pull32(ppReadPackedMsg, &pNfapiMsg->ul_tti_jitter, end) && pull32(ppReadPackedMsg, &pNfapiMsg->ul_dci_jitter, end)
          && pulls32(ppReadPackedMsg, &pNfapiMsg->dl_tti_latest_delay, end)
          && pulls32(ppReadPackedMsg, &pNfapiMsg->tx_data_request_latest_delay, end)
          && pulls32(ppReadPackedMsg, &pNfapiMsg->ul_tti_latest_delay, end)
          && pulls32(ppReadPackedMsg, &pNfapiMsg->ul_dci_latest_delay, end)
          && pulls32(ppReadPackedMsg, &pNfapiMsg->dl_tti_earliest_arrival, end)
          && pulls32(ppReadPackedMsg, &pNfapiMsg->tx_data_request_earliest_arrival, end)
          && pulls32(ppReadPackedMsg, &pNfapiMsg->ul_tti_earliest_arrival, end)
          && pulls32(ppReadPackedMsg, &pNfapiMsg->ul_dci_earliest_arrival, end)
          && unpack_nr_p7_tlv_list(NULL, 0, ppReadPackedMsg, end, config, &pNfapiMsg->vendor_extension));
}

bool nfapi_nr_p7_message_header_unpack(void *pMessageBuf,
                                      uint32_t messageBufLen,
                                      void *pUnpackedBuf,
                                      uint32_t unpackedBufLen,
                                      nfapi_p7_codec_config_t *config)
{
  UNUSED(config);
  nfapi_nr_p7_message_header_t *pMessageHeader = pUnpackedBuf;
  uint8_t *pReadPackedMessage = pMessageBuf;

  if (pMessageBuf == NULL || pUnpackedBuf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 header unpack supplied pointers are null\n");
    return false;
  }

  uint8_t *end = (uint8_t *)pMessageBuf + messageBufLen;

  if (messageBufLen < NFAPI_NR_P7_HEADER_LENGTH || unpackedBufLen < sizeof(nfapi_nr_p7_message_header_t)) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 header unpack supplied message buffer is too small %d, %d\n", messageBufLen, unpackedBufLen);
    return false;
  }
  // process the header
  if (!(pull16(&pReadPackedMessage, &pMessageHeader->phy_id, end) && pull16(&pReadPackedMessage, &pMessageHeader->message_id, end)
        && pull32(&pReadPackedMessage, &pMessageHeader->message_length, end)
        && pull16(&pReadPackedMessage, &pMessageHeader->m_segment_sequence, end)
        && pull32(&pReadPackedMessage, &pMessageHeader->checksum, end)
        && pull32(&pReadPackedMessage, &pMessageHeader->transmit_timestamp, end)))
    return false;

  return true;
}


bool peek_nr_nfapi_p7_sfn_slot(void *pMessageBuf, uint32_t messageBufLen, uint16_t *SFN, uint16_t *Slot)
{
#ifdef ENABLE_SOCKET
  // Socket transport will always use nFAPI encoding
  uint8_t *pReadPackedMessage = &((uint8_t *)pMessageBuf)[NFAPI_NR_P7_HEADER_LENGTH];
#endif
#ifdef ENABLE_WLS
  // WLS transport will always use FAPI encoding
  uint8_t *pReadPackedMessage = &((uint8_t *)pMessageBuf)[NFAPI_HEADER_LENGTH];
#endif
  uint8_t *end = (uint8_t*)pMessageBuf + messageBufLen;
  if(!(pull16(&pReadPackedMessage, SFN, end) && pull16(&pReadPackedMessage, Slot, end))) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "Failed to peek SFN.Slot\n");
    return false;
  }
  NFAPI_TRACE(NFAPI_TRACE_DEBUG, "Peeked SFN.Slot is (%d.%d)\n", *SFN, *Slot);
  return true;
}

bool nfapi_nr_p7_message_unpack(void *pMessageBuf,
                               uint32_t messageBufLen,
                               void *pUnpackedBuf,
                               uint32_t unpackedBufLen,
                               nfapi_p7_codec_config_t *config)
{
  int result = 0;
  nfapi_nr_p7_message_header_t *pMessageHeader = (nfapi_nr_p7_message_header_t *)pUnpackedBuf;
  uint8_t *pReadPackedMessage = pMessageBuf;

  if (pMessageBuf == NULL || pUnpackedBuf == NULL) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 unpack supplied pointers are null\n");
    return false;
  }

  uint8_t *end = (uint8_t *)pMessageBuf + messageBufLen;

  if (messageBufLen < NFAPI_NR_P7_HEADER_LENGTH || unpackedBufLen < sizeof(nfapi_nr_p7_message_header_t)) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 unpack supplied message buffer is too small %d, %d\n", messageBufLen, unpackedBufLen);
    return false;
  }

  // process the header
  if (!(pull16(&pReadPackedMessage, &pMessageHeader->phy_id, end) && pull16(&pReadPackedMessage, &pMessageHeader->message_id, end)
        && pull32(&pReadPackedMessage, &pMessageHeader->message_length, end)
        && pull16(&pReadPackedMessage, &pMessageHeader->m_segment_sequence, end)
        && pull32(&pReadPackedMessage, &pMessageHeader->checksum, end)
        && pull32(&pReadPackedMessage, &pMessageHeader->transmit_timestamp, end))) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 unpack header failed\n");
    return false;
  }
  if ((uint8_t *)(pMessageBuf + pMessageHeader->message_length) > end) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 unpack message length is greater than the message buffer \n");
    return false;
  }

  /*
  if(check_unpack_length(pMessageHeader->message_id, unpackedBufLen) == 0)
  {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 unpack unpack buffer is not large enough \n");
    return -1;
  }
  */

  // look for the specific message
  switch (pMessageHeader->message_id) {
    case NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_DL_TTI_REQUEST, unpackedBufLen))
        result = unpack_dl_tti_request(&pReadPackedMessage, end, pMessageHeader);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_UL_TTI_REQUEST, unpackedBufLen))
        result = unpack_ul_tti_request(&pReadPackedMessage, end, pMessageHeader);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_TX_DATA_REQUEST, unpackedBufLen))
        result = unpack_tx_data_request(&pReadPackedMessage, end, pMessageHeader);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_UL_DCI_REQUEST, unpackedBufLen))
        result = unpack_ul_dci_request(&pReadPackedMessage, end, pMessageHeader);
      break;
    case NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_SLOT_INDICATION, unpackedBufLen)) {
        nfapi_nr_slot_indication_scf_t *msg = (nfapi_nr_slot_indication_scf_t *)pMessageHeader;
        result = unpack_nr_slot_indication(&pReadPackedMessage, end, msg);
      }
      break;

    case NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_RX_DATA_INDICATION, unpackedBufLen)) {
        result = unpack_nr_rx_data_indication(&pReadPackedMessage, end, pMessageHeader);
      }
      break;

    case NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_CRC_INDICATION, unpackedBufLen)) {
        result = unpack_nr_crc_indication(&pReadPackedMessage, end, pMessageHeader);
      }
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_UCI_INDICATION, unpackedBufLen)) {
        result = unpack_nr_uci_indication(&pReadPackedMessage, end, pMessageHeader);
      }
      break;

    case NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_SRS_INDICATION, unpackedBufLen)) {
        result = unpack_nr_srs_indication(&pReadPackedMessage,  end, pMessageHeader);
      }
      break;

    case NFAPI_NR_PHY_MSG_TYPE_SRS_TOA_VENDOR_EXTENSION_INDICATION:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_SRS_TOA_VENDOR_EXTENSION_INDICATION, unpackedBufLen)) {
        result = unpack_nr_srs_toa_vendor_ext_indication(&pReadPackedMessage,  end, pMessageHeader);
      }
      break;

    case NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_RACH_INDICATION, unpackedBufLen)) {
        result = unpack_nr_rach_indication(&pReadPackedMessage,  end, pMessageHeader);
      }
      break;

    case NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_DL_NODE_SYNC, unpackedBufLen))
        result = unpack_nr_dl_node_sync(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_UL_NODE_SYNC, unpackedBufLen))
        result = unpack_nr_ul_node_sync(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    case NFAPI_NR_PHY_MSG_TYPE_TIMING_INFO:
      if (check_nr_fapi_unpack_length(NFAPI_NR_PHY_MSG_TYPE_TIMING_INFO, unpackedBufLen))
        result = unpack_nr_timing_info(&pReadPackedMessage, end, pMessageHeader, config);
      break;

    default:

      if (pMessageHeader->message_id >= NFAPI_VENDOR_EXT_MSG_MIN && pMessageHeader->message_id <= NFAPI_VENDOR_EXT_MSG_MAX) {
        if (config && config->unpack_p7_vendor_extension) {
          result = (config->unpack_p7_vendor_extension)(pMessageHeader, &pReadPackedMessage, end, config);
        } else {
          NFAPI_TRACE(NFAPI_TRACE_ERROR,
                      "%s VE NFAPI message ID %d. No ve decoder provided\n",
                      __FUNCTION__,
                      pMessageHeader->message_id);
        }
      } else {
        NFAPI_TRACE(NFAPI_TRACE_ERROR, "%s NFAPI Unknown message ID %d\n", __FUNCTION__, pMessageHeader->message_id);
      }
      break;
  }

  if (result == 0) {
    NFAPI_TRACE(NFAPI_TRACE_ERROR, "P7 Pack failed to pack message\n");
    return false;
  }
  return true;
}
