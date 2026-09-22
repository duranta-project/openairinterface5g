/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "xran_pkt_bfw.h"
#include "xran_pkt_cp.h"
#include "fh_compression.h"
#include "assertions.h"
#include <stdbool.h>

int xran_decode_bfw_ext1(const uint8_t *ext, size_t len, c16_t *weights_out, int max_weights)
{
  if (ext == NULL || weights_out == NULL || max_weights <= 0)
    return -1;
  if (len < sizeof(struct xran_cp_radioapp_section_ext1))
    return -1;

  const struct xran_cp_radioapp_section_ext1 *hdr = (const struct xran_cp_radioapp_section_ext1 *)ext;

  // extLen is in units of 4-byte words and covers the header + extension
  size_t total_len = (size_t)hdr->extLen * 4;
  if (total_len < sizeof(*hdr) || total_len > len)
    return -1;

  AssertFatal(hdr->bfwCompMeth <= XRAN_BFWCOMPMETHOD_ULAW,
              "unsupported bfwCompMeth %d (XRAN_BFWCOMPMETHOD_BEAMSPACE and reserved values are not implemented)\n",
              hdr->bfwCompMeth);

  // fwIqWidth definition from table
  // EXAMPLE 1: bfwIqWidth = 0000b means I and Q are each 16 bits wide.
  // EXAMPLE 2: bfwIqWidth = 0001b means I and Q are each 1 bit wide.
  // EXAMPLE 3: bfwIqWidth = 1111b means I and Q are each 15 bits wide.
  int iq_bits = hdr->bfwIqWidth == 0 ? 16 : hdr->bfwIqWidth;
  if (iq_bits < 1 || iq_bits > 16)
    return -1;

  fh_comp_method_t method = (fh_comp_method_t)hdr->bfwCompMeth;
  bool has_comp_param = method != FH_COMP_NONE;
  size_t offset = sizeof(*hdr) + (has_comp_param ? 1 : 0);
  if (offset > total_len)
    return -1;

  size_t payload_bytes = total_len - offset;
  int avail_vals = (int)((payload_bytes * 8) / (unsigned)iq_bits);
  int n_weights = avail_vals / 2; // each weight is one (bfwI, bfwQ) pair
  if (n_weights > max_weights)
    n_weights = max_weights;
  if (n_weights <= 0)
    return 0;

  int16_t raw[2 * n_weights];
  if (has_comp_param) {
    // fh_decompress_block expects ext w/ (offset-1).
    fh_decompress_block(method, iq_bits, 2 * n_weights, (const int8_t *)(ext + offset - 1), raw);
  } else {
    // No compression. Work on 16 bit boundaries
    for (int i = 0; i < 2 * n_weights; i++)
      raw[i] = (int16_t)unpack_bits(ext + offset, i * iq_bits, iq_bits);
  }

  for (int i = 0; i < n_weights; i++) {
    weights_out[i].r = raw[2 * i];
    weights_out[i].i = raw[2 * i + 1];
  }
  return n_weights;
}
