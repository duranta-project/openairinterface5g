/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#ifndef OPENAIRINTERFACE_NR_FAPI_P7_H
#define OPENAIRINTERFACE_NR_FAPI_P7_H

int fapi_nr_p7_message_pack(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen, nfapi_p7_codec_config_t *config);

bool fapi_nr_p7_message_unpack(void *pMessageBuf,
                              uint32_t messageBufLen,
                              void *pUnpackedBuf,
                              uint32_t unpackedBufLen,
                              nfapi_p7_codec_config_t *config);

uint8_t pack_dl_tti_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t unpack_dl_tti_request(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg);
uint8_t pack_ul_tti_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t unpack_ul_tti_request(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg);
uint8_t unpack_nr_slot_indication(uint8_t **ppReadPackedMsg, uint8_t *end, nfapi_nr_slot_indication_scf_t *msg);
uint8_t pack_nr_slot_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t pack_ul_dci_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t unpack_ul_dci_request(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg);
uint8_t pack_tx_data_request(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t unpack_tx_data_request(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg);
uint8_t pack_nr_rx_data_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t unpack_nr_rx_data_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg);
uint8_t pack_nr_crc_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t unpack_nr_crc_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg);
uint8_t pack_nr_uci_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t unpack_nr_uci_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg);
uint8_t pack_nr_srs_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t unpack_nr_srs_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg);
uint8_t pack_nr_srs_toa_vendor_ext_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t unpack_nr_srs_toa_vendor_ext_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg);
uint8_t unpack_nr_srs_report_tlv_value(nfapi_srs_report_tlv_t *report_tlv, uint8_t **ppReadPackedMsg, uint8_t *end);
#ifdef ENABLE_AERIAL
uint8_t aerial_unpack_nr_srs_indication(uint8_t **ppReadPackedMsg, uint8_t *end, uint8_t **pDataMsg, uint8_t *data_end, void *msg);
#endif
uint8_t pack_nr_rach_indication(void *msg, uint8_t **ppWritePackedMsg, uint8_t *end);
uint8_t unpack_nr_rach_indication(uint8_t **ppReadPackedMsg, uint8_t *end, void *msg);
#endif // OPENAIRINTERFACE_NR_FAPI_P7_H
