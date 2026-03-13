/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NFAPI_NR_INTERFACE_H_
#define _NFAPI_NR_INTERFACE_H_
#include "nfapi_common_interface.h"

#include <stdbool.h>

#define NFAPI_NR_P5_HEADER_LENGTH 10
#define NFAPI_NR_P7_HEADER_LENGTH 18

//These TLVs are used exclusively by nFAPI
#define NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV4_TAG 0x0100
#define NFAPI_NR_NFAPI_P7_VNF_ADDRESS_IPV6_TAG 0x0101
#define NFAPI_NR_NFAPI_P7_VNF_PORT_TAG 0x0102
#define NFAPI_NR_NFAPI_P7_PNF_ADDRESS_IPV4_TAG 0x0103
#define NFAPI_NR_NFAPI_P7_PNF_ADDRESS_IPV6_TAG 0x0104
#define NFAPI_NR_NFAPI_P7_PNF_PORT_TAG 0x0105
#define NFAPI_NR_NFAPI_DL_TTI_TIMING_OFFSET 0x0106
#define NFAPI_NR_NFAPI_UL_TTI_TIMING_OFFSET 0x0107
#define NFAPI_NR_NFAPI_UL_DCI_TIMING_OFFSET 0x0108
#define NFAPI_NR_NFAPI_TX_DATA_TIMING_OFFSET 0x0109
#define NFAPI_NR_NFAPI_TIMING_WINDOW_TAG 0x011E
#define NFAPI_NR_NFAPI_TIMING_INFO_MODE_TAG 0x011F
#define NFAPI_NR_NFAPI_TIMING_INFO_PERIOD_TAG 0x0120
#define NFAPI_NR_FAPI_ANALOG_BF_VENDOR_EXTENSION_TAG 0xA001
#define NFAPI_NR_FAPI_SSB_CASE_VENDOR_EXTENSION_TAG 0xA002

typedef struct {
  uint16_t phy_id;
  uint16_t message_id;
  uint32_t message_length;
  uint16_t spare;
} nfapi_nr_p4_p5_message_header_t;

typedef struct {
  uint16_t phy_id;
  uint16_t message_id;
  uint32_t message_length;
  uint16_t m_segment_sequence; /* This consists of 3 fields - namely, M, Segement & Sequence number*/
  uint32_t checksum;
  uint32_t transmit_timestamp;
} nfapi_nr_p7_message_header_t;

typedef enum {
  NFAPI_NR_CCE_REG_MAPPING_INTERLEAVED=1,
  NFAPI_NR_CCE_REG_MAPPING_NON_INTERLEAVED=0
} nfapi_nr_cce_reg_mapping_type_e;

typedef enum {
  NFAPI_NR_CSET_CONFIG_MIB_SIB1 = 0,
  NFAPI_NR_CSET_CONFIG_PDCCH_CONFIG, // implicit assumption of coreset Id other than 0
} nfapi_nr_coreset_config_type_e;

typedef enum {
  NFAPI_NR_DMRS_TYPE1,
  NFAPI_NR_DMRS_TYPE2
} nfapi_nr_dmrs_type_e;

typedef enum {
  NFAPI_NR_SRS_BEAMMANAGEMENT = 0,
  NFAPI_NR_SRS_CODEBOOK = 1,
  NFAPI_NR_SRS_NONCODEBOOK = 2,
  NFAPI_NR_SRS_ANTENNASWITCH = 3
} nfapi_nr_srs_usage_e;

/*! \brief Encodes an NFAPI P5 message to a buffer
 *  \param pMessageBuf A pointer to a nfapi p5 message structure
 *  \param messageBufLen The size of the p5 message structure
 *  \param pPackedBuf A pointer to the buffer that the p5 message will be packed into
 *  \param packedBufLen The size of the buffer
 *  \param config A pointer to the nfapi configuration structure
 *  \return != 0 means success, -1 means failure.
 *
 * The function will encode a nFAPI P5 message structure pointed to be pMessageBuf into a byte stream pointed to by pPackedBuf.
 * The function returns the message length that was packed
 */
int nfapi_nr_p5_message_pack(void *pMessageBuf,
                             uint32_t messageBufLen,
                             void *pPackedBuf,
                             uint32_t packedBufLen,
                             nfapi_p4_p5_codec_config_t *config);

/*! \brief Packs a NFAPI P5 message body
 *  \param header A pointer to the header of the P5 message
 *  \param ppWritePackedMsg A pointer to the buffer where to pack the P5 message
 *  \param end Pointer to the end of the packing buffer
 *  \param config A pointer to the nfapi configuration structure
 *  \return 1 means success, 0 means failure.
 *
 * The function will decode a byte stream pointed to by pMessageBuf into a nfapi p5 message structure pointer to by pUnpackedBuf
 */
uint8_t pack_nr_p5_message_body(nfapi_nr_p4_p5_message_header_t *header,
                                uint8_t **ppWritePackedMsg,
                                uint8_t *end,
                                nfapi_p4_p5_codec_config_t *config);

/*! \brief Decodes an NFAPI P5 message header
 *  \param pMessageBuf A pointer to an encoded P5 message header
 *  \param messageBufLen The size of the encoded P5 message header
 *  \param pUnpackedBuf A pointer to the nfapi_message_header
 *  \param unpackedBufLen The size of nfapi_message_header structure.
 *  \param config A pointer to the nfapi configuration structure
 *  \return true on success, false on failure.
 *
 * The function will decode a byte stream pointed to by pMessageBuf into a nfapi p5 header structure pointed to by pUnpackedBuf
 */
bool nfapi_nr_p5_message_header_unpack(void *pMessageBuf,
                                      uint32_t messageBufLen,
                                      void *pUnpackedBuf,
                                      uint32_t unpackedBufLen,
                                      nfapi_p4_p5_codec_config_t *config);

/*! \brief Decodes a NFAPI P5 message
 *  \param pMessageBuf A pointer to an encoded P5 message
 *  \param messageBufLen The size of the encoded P5 message
 *  \param pUnpackedBuf A pointer to the nfapi_message_header
 *  \param unpackedBufLen The size of nfapi_message_header structure.
 *  \param config A pointer to the nfapi configuration structure
 *  \return true on success, false on failure.
 *
 * The function will decode a byte stream pointed to by pMessageBuf into a nfapi p5 message structure pointer to by pUnpackedBuf
 */
bool nfapi_nr_p5_message_unpack(void *pMessageBuf,
                               uint32_t messageBufLen,
                               void *pUnpackedBuf,
                               uint32_t unpackedBufLen,
                               nfapi_p4_p5_codec_config_t *config);

/*! \brief Encodes an NFAPI P7 message to a buffer
 *  \param pMessageBuf A pointer to a nfapi p7 message structure
 *  \param pPackedBuf A pointer to the buffer that the p7 message will be packed into
 *  \param packedBufLen The size of the buffer
 *  \param config A pointer to the nfapi configuration structure
 *  \return != -1 means success, -1 means failure.
 *
 * The function will encode a nFAPI P7 message structure pointed to be pMessageBuf into a byte stream pointed to by pPackedBuf.
 * The function returns the message length packed
 */
int nfapi_nr_p7_message_pack(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen, nfapi_p7_codec_config_t *config);

/*! \brief Decodes an NFAPI P7 message header
 *  \param pMessageBuf A pointer to an encoded P7 message header
 *  \param messageBufLen The size of the encoded P7 message header
 *  \param pUnpackedBuf A pointer to the nfapi_message_header
 *  \param unpackedBufLen The size of nfapi_message_header structure.
 *  \param config A pointer to the nfapi configuration structure
 *  \return true on success, false on failure.
 *
 * The function will decode a byte stream pointed to by pMessageBuf into a nfapi_p7_message_header structure pointer to by
 pUnpackedBuf

 */
bool nfapi_nr_p7_message_header_unpack(void *pMessageBuf,
                                      uint32_t messageBufLen,
                                      void *pUnpackedBuf,
                                      uint32_t unpackedBufLen,
                                      nfapi_p7_codec_config_t *config);

/*! \brief Decodes a NFAPI P7 message
 *  \param pMessageBuf A pointer to an encoded P7 message
 *  \param messageBufLen The size of the encoded P7 message
 *  \param pUnpackedBuf A pointer to the nfapi_message_header
 *  \param unpackedBufLen The size of nfapi_message_header structure.
 *  \param config A pointer to the nfapi configuration structure
 *  \return true on success, false failure.
 *
 * The function will decode a byte stream pointed to by pMessageBuf into a nfapi p7 message structure pointer to by pUnpackedBuf
 */
bool nfapi_nr_p7_message_unpack(void *pMessageBuf,
                               uint32_t messageBufLen,
                               void *pUnpackedBuf,
                               uint32_t unpackedBufLen,
                               nfapi_p7_codec_config_t *config);

/*! \brief Peeks the SFN and Slot of an (n)FAPI P7 message
 *  \param pMessageBuf A pointer to an encoded P7 message header
 *  \param messageBufLen The size of the encoded P7 message header
 *  \param SFN A pointer to store the peeked SFN
 *  \param Slot A pointer to store the peeked Slot
 *  \return true on success, false, otherwise.
 *
 * The function will decode the encoded SFN and Slot from an encoded P7 (n)FAPI message

 */
bool peek_nr_nfapi_p7_sfn_slot(void *pMessageBuf, uint32_t messageBufLen, uint16_t *SFN, uint16_t *Slot);

/*! \brief Calculates the checksum of a  message
 *
 *  \param buffer Pointer to the packed message
 *  \param len The length of the message
 *  \return The checksum. If there is an error the function with return -1
 */
uint32_t nfapi_nr_p7_calculate_checksum(uint8_t *buffer, uint32_t len);

/*! \brief Calculates & updates the checksum in the message
 *
 *  \param buffer Pointer to the packed message
 *  \param len The length of the message
 *  \return 0 means success, -1 means failure.
 */
int nfapi_nr_p7_update_checksum(uint8_t *buffer, uint32_t len);

/*! \brief Updates the transmition time stamp in the p7 message header
 *
 *  \param buffer Pointer to the packed message
 *  \param timestamp The time stamp value
 *  \return 0 means success, -1 means failure.
 */
int nfapi_nr_p7_update_transmit_timestamp(uint8_t *buffer, uint32_t timestamp);

/*! \brief Encodes a nfapi_nr_srs_normalized_channel_iq_matrix_t to a buffer
 *
 *  \param pMessageBuf A pointer to a nfapi_nr_srs_normalized_channel_iq_matrix_t structure
 *  \param pPackedBuf A pointer to the buffer that the nfapi_nr_srs_normalized_channel_iq_matrix_t will be packed into
 *  \param packedBufLen The size of the buffer
 *  \return number of bytes written to the buffer
 */
int pack_nr_srs_normalized_channel_iq_matrix(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen);

/*! \brief Decodes a nfapi_nr_srs_normalized_channel_iq_matrix_t from a buffer
 *
 *  \param pMessageBuf A pointer to an encoded nfapi_nr_srs_normalized_channel_iq_matrix_t
 *  \param messageBufLen The size of the encoded nfapi_nr_srs_normalized_channel_iq_matrix_t
 *  \param pUnpackedBuf A pointer to the nfapi_nr_srs_normalized_channel_iq_matrix_t
 *  \param unpackedBufLen The size of nfapi_nr_srs_normalized_channel_iq_matrix_t structure.
 *  \return 0 means success, -1 means failure.
 */
int unpack_nr_srs_normalized_channel_iq_matrix(void *pMessageBuf,
                                               uint32_t messageBufLen,
                                               void *pUnpackedBuf,
                                               uint32_t unpackedBufLen);

/*! \brief Encodes a nfapi_nr_srs_beamforming_report_t to a buffer
 *
 *  \param pMessageBuf A pointer to a nfapi_nr_srs_beamforming_report_t structure
 *  \param pPackedBuf A pointer to the buffer that the nfapi_nr_srs_beamforming_report_t will be packed into
 *  \param packedBufLen The size of the buffer
 *  \return number of bytes written to the buffer
 */
int pack_nr_srs_beamforming_report(void *pMessageBuf, void *pPackedBuf, uint32_t packedBufLen);

/*! \brief Decodes a nfapi_nr_srs_beamforming_report_t from a buffer
 *
 *  \param pMessageBuf A pointer to an encoded nfapi_nr_srs_beamforming_report_t
 *  \param messageBufLen The size of the encoded nfapi_nr_srs_beamforming_report_t
 *  \param pUnpackedBuf A pointer to the nfapi_nr_srs_beamforming_report_t
 *  \param unpackedBufLen The size of nfapi_nr_srs_beamforming_report_t structure.
 *  \return 0 means success, -1 means failure.
 */
int unpack_nr_srs_beamforming_report(void *pMessageBuf, uint32_t messageBufLen, void *pUnpackedBuf, uint32_t unpackedBufLen);

#endif
