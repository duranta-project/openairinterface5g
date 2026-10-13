/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "xran_pkt_bfw.h"
#include "xran_pkt_cp.h"
#include "fh_compression.h"
#include <string.h>

int xran_section_ext_len(const uint8_t *ext, size_t avail)
{
  if (ext == NULL || avail < 2)
    return -1;
  uint8_t ext_type = ext[0] & 0x7F;
  size_t words = ext[1];
  if (ext_type == XRAN_CP_SECTIONEXTCMD_11 || ext_type == 19 || ext_type == 20) {
    if (avail < 3)
      return -1;
    words = (words << 8) | ext[2];
  }
  if (words == 0 || words * 4 > avail)
    return -1;
  return words * 4;
}

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

int xran_decode_bfw_ext11(const uint8_t *ext,
                          size_t len,
                          int num_prb,
                          int n_weights,
                          int max_bundles,
                          xran_bfw_ext11_hdr_t *hdr_out,
                          uint16_t *beam_ids_out,
                          c16_t *weights_out)
{
  if (ext == NULL || hdr_out == NULL || beam_ids_out == NULL || weights_out == NULL)
    return -1;
  if (num_prb <= 0 || n_weights <= 0 || max_bundles <= 0)
    return -1;
  // bfwCompHdr is absent when disableBFWs = 1, so the fixed part is 5 or 6 bytes
  const size_t hdr_len = sizeof(struct xran_cp_radioapp_section_ext11);
  if (len < hdr_len - 1)
    return -1;

  xran_bfw_ext11_hdr_t hdr = {.disableBFWs = ext[3] >> 7,
                              .RAD = (ext[3] >> 6) & 1,
                              .bundleOffset = ext[3] & 0x3F,
                              .numBundPrb = ext[4]};
  if (hdr.numBundPrb == 0) // reserved
    return -1;
  int n_bundles = (hdr.bundleOffset + num_prb + hdr.numBundPrb - 1) / hdr.numBundPrb;
  if (n_bundles > max_bundles)
    return -1;

  // Per bundle: [bfwCompParam] beamId [(bfwI, bfwQ) x n_weights], or only beamId if disableBFWs
  fh_comp_method_t method = FH_COMP_NONE;
  int iq_bits = 0;
  size_t offset = hdr_len - 1;
  size_t param_len = 0, iq_len = 0;
  if (!hdr.disableBFWs) {
    if (len < hdr_len)
      return -1;
    hdr.bfwCompMeth = ext[5] & 0x0F;
    if (hdr.bfwCompMeth > XRAN_BFWCOMPMETHOD_ULAW) // beamspace and reserved methods not supported
      return -1;
    hdr.bfwIqWidth = ext[5] >> 4 == 0 ? 16 : ext[5] >> 4; // 0 means 16 bits (5.4.7.1.1)
    method = (fh_comp_method_t)hdr.bfwCompMeth;
    iq_bits = hdr.bfwIqWidth;
    offset = hdr_len;
    param_len = method != FH_COMP_NONE ? 1 : 0;
    iq_len = ((size_t)2 * n_weights * iq_bits + 7) / 8;
  }
  const size_t bundle_len = param_len + 2 + iq_len;

  // extLen (4-byte words) must match the header, the bundles and the zero padding exactly
  size_t expected_len = (offset + n_bundles * bundle_len + 3) & ~(size_t)3;
  size_t ext_len = (((size_t)ext[1] << 8) | ext[2]) * 4;
  if (ext_len != expected_len || ext_len > len)
    return -1;

  int16_t *out = (int16_t *)weights_out;
  for (int b = 0; b < n_bundles; b++, offset += bundle_len) {
    const uint8_t *bundle = ext + offset;
    beam_ids_out[b] = ((bundle[param_len] << 8) | bundle[param_len + 1]) & 0x7FFF; // top bit is contInd
    if (hdr.disableBFWs)
      continue;
    const uint8_t *iq = bundle + param_len + 2;
    int16_t *w = out + (size_t)2 * b * n_weights;
    if (method != FH_COMP_NONE) {
      // fh_decompress_block() expects the comp param byte right before the IQ, but beamId sits in between
      uint8_t block[1 + iq_len];
      block[0] = bundle[0];
      memcpy(block + 1, iq, iq_len);
      fh_decompress_block(method, iq_bits, 2 * n_weights, (const int8_t *)block, w);
    } else {
      for (int i = 0; i < 2 * n_weights; i++)
        w[i] = (int16_t)unpack_bits(iq, i * iq_bits, iq_bits);
    }
  }
  *hdr_out = hdr;
  return n_bundles;
}
