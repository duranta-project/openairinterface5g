/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef OPENAIRINTERFACE_NR_FAPI_H
#define OPENAIRINTERFACE_NR_FAPI_H

#include "stdint.h"
#include "string.h"
#include "nfapi_interface.h"
#include "nfapi_nr_interface.h"
#include "nfapi_nr_interface_scf.h"
#include "nfapi.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include "assertions.h"
#include "debug.h"

#define INDENTED_PRINTF(format, ...) printf("%*s" format, depth * 2, "", ##__VA_ARGS__)
#define INDENTED_GENERIC_PRINT(string, format, ...) INDENTED_PRINTF(string " = " format "\n", ##__VA_ARGS__)
#define INDENTED_TLV_PRINT(tlv_name, tlv) INDENTED_GENERIC_PRINT("(0x%04x) " tlv_name, "0x%02x", tlv.tl.tag, tlv.value)
#define INDENTED_TLV_FORMAT_PRINT(tlv_name, format, tlv) INDENTED_GENERIC_PRINT("(0x%04x) " tlv_name, format, tlv.tl.tag, tlv.value)

#define EQ_TLV(_tlv_a, _tlv_b)        \
  do {                                \
    EQ(_tlv_a.tl.tag, _tlv_b.tl.tag); \
    EQ(_tlv_a.value, _tlv_b.value);   \
  } while (0)

#define EQ(_a, _b)      \
  do {                  \
    if ((_a) != (_b)) { \
      return false;     \
    }                   \
  } while (0)

#define COPY_TL(_dst_tl, _src_tl)    \
  do {                               \
    _dst_tl.tag = _src_tl.tag;       \
    _dst_tl.length = _src_tl.length; \
  } while (0)

#define COPY_TLV(_dst, _src)   \
  do {                         \
    COPY_TL(_dst.tl, _src.tl); \
    _dst.value = _src.value;   \
  } while (0)

typedef struct {
  uint8_t num_msg;
  uint8_t opaque_handle;
  uint16_t message_id;
  uint32_t message_length;
} fapi_message_header_t;

void copy_vendor_extension_value(nfapi_vendor_extension_tlv_t *dst, const nfapi_vendor_extension_tlv_t *src);

bool isFAPIMessageIDValid(uint16_t id);

int check_nr_fapi_unpack_length(nfapi_nr_phy_msg_type_e msgId, uint32_t unpackedBufLen);

/*! \brief Decodes a FAPI P5 message header
 *  \param pMessageBuf A pointer to an encoded P5 message header
 *  \param messageBufLen The size of the encoded P5 message header
 *  \param pUnpackedBuf A pointer to the fapi_message_header
 *  \param unpackedBufLen The size of fapi_message_header structure.
 *  \param config A pointer to the nfapi configuration structure
 *  \return true on success, false on failure.
 *
 * The function will decode a byte stream pointed to by pMessageBuf into a FAPI p5 header structure pointed to by pUnpackedBuf
 */
bool fapi_nr_message_header_unpack(void *pMessageBuf,
                                  uint32_t messageBufLen,
                                  void *pUnpackedBuf,
                                  uint32_t unpackedBufLen,
                                  nfapi_p4_p5_codec_config_t *config);

/*! \brief Decodes a FAPI P7 message header
 *  \param pMessageBuf A pointer to an encoded P5 message header
 *  \param messageBufLen The size of the encoded P5 message header
 *  \param pUnpackedBuf A pointer to the fapi_message_header
 *  \param unpackedBufLen The size of fapi_message_header structure.
 *  \param config A pointer to the nfapi configuration structure
 *  \return true on success, false on failure.
 *
 * This function is a dummy function that points to fapi_nr_message_header_unpack,
 * since in FAPI there's no difference in the structure of the message headers for P5 and P7
 */
bool fapi_nr_p7_message_header_unpack(void *pMessageBuf,
                                  uint32_t messageBufLen,
                                  void *pUnpackedBuf,
                                  uint32_t unpackedBufLen,
                                  nfapi_p7_codec_config_t *config);

#endif // OPENAIRINTERFACE_NR_FAPI_H
