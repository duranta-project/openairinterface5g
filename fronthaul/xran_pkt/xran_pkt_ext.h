/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Result of the section extension codecs (xran_pkt_bfw.h, xran_pkt_ulpi.h). XRAN_EXT_ERR_ARG and XRAN_EXT_ERR_TOO_MANY
// come from the caller; the other decode errors come from the packet.
typedef enum {
  XRAN_EXT_OK = 0,
  XRAN_EXT_ERR_ARG = -1, // NULL pointer or bad count from the caller
  XRAN_EXT_ERR_TRUNCATED = -2, // decode: runs past the end of the packet; encode: output buffer too small
  XRAN_EXT_ERR_EXT_LEN = -3, // extLen is 0 (reserved) or doesn't match the content and the padding
  XRAN_EXT_ERR_COMP_METH = -4, // SE 1 and SE 11 bfwCompMeth is beamspace or reserved
  XRAN_EXT_ERR_NUM_BUND_PRB = -5, // SE 11 numBundPrb is 0 (reserved)
  XRAN_EXT_ERR_TOO_MANY = -6, // decode: more elements than the caller has room for
  XRAN_EXT_ERR_EXT_TYPE = -7, // extType is not the one of the decoder
  XRAN_EXT_ERR_RANGE = -8, // encode: a field value is out of its range, reserved, or unused and not 0
  XRAN_EXT_ERR_RESERVED = -9, // a field that sets the layout has a reserved or forbidden value (SE 24 entryType)
  XRAN_EXT_ERR_MIN = XRAN_EXT_ERR_RESERVED, // the lowest value: change it when a lower value is added
} xran_ext_err_t;

// Description of an xran_ext_err_t value
const char *xran_ext_strerror(xran_ext_err_t err);

// Writes the length in bytes of the section extension of type ext_type at ext, with avail bytes left in the packet,
// to len_out. extLen is 16 bits for extType 11, 19 and 20 and 8 bits otherwise (CUS v16.01 7.6.2.3). Returns
// XRAN_EXT_OK, XRAN_EXT_ERR_TRUNCATED if the extension header doesn't fit or the extension runs past avail, or
// XRAN_EXT_ERR_EXT_LEN if extLen is 0.
xran_ext_err_t xran_section_ext_len(uint8_t ext_type, const uint8_t *ext, size_t avail, size_t *len_out);

// Length of content_len bytes of extension content with the zero padding to the 4-byte boundary
static inline size_t xran_section_ext_pad4(size_t content_len)
{
  return (content_len + 3) & ~(size_t)3;
}

// Starts the decode of the extension at ext, with len bytes left in the packet: at least hdr_len bytes, extType
// ext_type, and an extLen that is not 0 and fits in len. Writes the extension length to ext_len_out; the caller then
// checks it against the content. Returns XRAN_EXT_OK, XRAN_EXT_ERR_TRUNCATED, XRAN_EXT_ERR_EXT_TYPE or
// XRAN_EXT_ERR_EXT_LEN.
xran_ext_err_t xran_section_ext_open(const uint8_t *ext, size_t len, uint8_t ext_type, size_t hdr_len, size_t *ext_len_out);

// Starts the encode of an extension with an 8-bit extLen and content_len bytes of content into buf (avail bytes):
// checks ef, the extLen range and avail, then zeroes the extension with its padding and writes the ef/extType and
// extLen bytes. Writes the extension length to len_out; the caller then writes the content. Returns XRAN_EXT_OK,
// XRAN_EXT_ERR_RANGE or XRAN_EXT_ERR_TRUNCATED. Nothing is written on error.
xran_ext_err_t xran_section_ext_begin(uint8_t *buf,
                                      size_t avail,
                                      uint8_t ext_type,
                                      uint8_t ef,
                                      size_t content_len,
                                      size_t *len_out);

#ifdef __cplusplus
}
#endif
