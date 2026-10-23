/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "xran_pkt_bfw.h"
#include "xran_pkt_cp.h"
#include "fh_compression.h"

int xran_decode_bfw_ext1(const uint8_t *ext, size_t len, int n_weights, c16_t *weights_out)
{
  if (ext == NULL || weights_out == NULL || n_weights <= 0)
    return -1;
  if (len < sizeof(struct xran_cp_radioapp_section_ext1))
    return -1;

  const struct xran_cp_radioapp_section_ext1 *hdr = (const struct xran_cp_radioapp_section_ext1 *)ext;
  if (hdr->bfwCompMeth > XRAN_BFWCOMPMETHOD_ULAW) // beamspace and reserved methods not supported
    return -1;

  // bfwIqWidth = 0 means 16 bits, otherwise 1..15 (5.4.7.1.1)
  int iq_bits = hdr->bfwIqWidth == 0 ? 16 : hdr->bfwIqWidth;
  fh_comp_method_t method = (fh_comp_method_t)hdr->bfwCompMeth;
  size_t offset = sizeof(*hdr) + (method != FH_COMP_NONE ? 1 : 0); // bfwCompParam present if compressed

  // extLen (4-byte words) must match the header, the n_weights (bfwI, bfwQ) pairs and the zero padding exactly
  size_t expected_len = offset + ((size_t)2 * n_weights * iq_bits + 7) / 8;
  expected_len = (expected_len + 3) & ~(size_t)3;
  size_t ext_len = (size_t)hdr->extLen * 4;
  if (ext_len != expected_len || ext_len > len)
    return -1;

  // c16_t is {int16_t r, i}, i.e. the same layout as the (bfwI, bfwQ) value stream
  int16_t *out = (int16_t *)weights_out;
  if (method != FH_COMP_NONE) {
    // fh_decompress_block() expects the comp param byte first
    fh_decompress_block(method, iq_bits, 2 * n_weights, (const int8_t *)(ext + offset - 1), out);
  } else {
    for (int i = 0; i < 2 * n_weights; i++)
      out[i] = (int16_t)unpack_bits(ext + offset, i * iq_bits, iq_bits);
  }
  return n_weights;
}
