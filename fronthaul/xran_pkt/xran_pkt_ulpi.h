/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include "xran_pkt_ext.h"

#ifdef __cplusplus
extern "C" {
#endif

// Section extension codecs for ULPI (DMRS-BF), CUS v16.01. Each buffer starts at the ef/extType byte of the extension.
// The decoders return the wire values only: implicit values (entryType 0/1 inheritance, ueIdReset of entryType 0/1,
// dType and cdmWithoutData of entryType 3) are for the caller to apply.

/**********************************************************************
 * SE 10: Group configuration of multiple ports (CUS v16.01 7.7.10)
 **********************************************************************/

// beamGroupType (7.7.10.2). Section Type 5 does not allow 00b and 01b; the codec supports all four.
#define XRAN_SE10_BGT_00B 0 // common beam, Table 7.7.10.1-1
#define XRAN_SE10_BGT_01B 1 // beam matrix indication, Table 7.7.10.1-1
#define XRAN_SE10_BGT_10B 2 // beam vector listing, Table 7.7.10.1-2
#define XRAN_SE10_BGT_11B 3 // listing with portListIndex, Table 7.7.10.1-3
#define XRAN_SE10_MAX_PORTS 64

typedef struct {
  uint8_t ef;
  uint8_t beamGroupType; // 2 bits, XRAN_SE10_BGT_*
  int numPortc; // 1 to 64; 64 is 0 on the wire
  uint8_t firstPortListIndex; // 11b only: the 1st portListIndex, for the section header ueId (or beamId); 1 to 255
} xran_se10_hdr_t;

// One port listed in SE 10 (10b and 11b). Port k of the list is port k + 2 of the group; the section header carries
// port 1.
typedef struct {
  uint16_t id; // 15 bits, ueId (or beamId); ueId 0x7FFF = port not scheduled (7.7.10.1)
  uint8_t portListIndex; // 11b only, 1 to 255
} xran_se10_port_t;

// Number of ports listed in the extension: 0 for 00b and 01b, numPortc for 10b, numPortc - 1 for 11b
static inline int xran_se10_num_ports(const xran_se10_hdr_t *hdr)
{
  switch (hdr->beamGroupType) {
    case XRAN_SE10_BGT_10B:
      return hdr->numPortc;
    case XRAN_SE10_BGT_11B:
      return hdr->numPortc - 1;
    default:
      return 0;
  }
}

// Writes SE 10 with the xran_se10_num_ports() ports of ports to buf (avail bytes), with the filler to the 4-byte
// boundary, and its length to len_out. ports can be NULL for 00b and 01b. Returns XRAN_EXT_OK, XRAN_EXT_ERR_TRUNCATED
// or XRAN_EXT_ERR_RANGE. Nothing is written on error.
xran_ext_err_t xran_encode_se10(const xran_se10_hdr_t *hdr,
                                const xran_se10_port_t *ports,
                                uint8_t *buf,
                                size_t avail,
                                size_t *len_out);

// Decodes SE 10 from ext, with len bytes from xran_section_ext_len(). The xran_se10_num_ports() listed ports go to
// ports_out, which has room for max_ports. Returns XRAN_EXT_OK, XRAN_EXT_ERR_TOO_MANY if there are more than
// max_ports ports, or an error if the extension is malformed. hdr_out and ports_out are not changed on error.
xran_ext_err_t xran_decode_se10(const uint8_t *ext,
                                size_t len,
                                int max_ports,
                                xran_se10_hdr_t *hdr_out,
                                xran_se10_port_t *ports_out);

/**********************************************************************
 * SE 24: PUSCH DMRS configuration (CUS v16.01 7.7.24)
 **********************************************************************/

#define XRAN_SE24_ENTRY_INHERIT_PERSIST 0 // inherit, ueIdReset = 0, Table 7.7.24.3-1
#define XRAN_SE24_ENTRY_INHERIT_RESET 1 // inherit, ueIdReset = 1, Table 7.7.24.3-1
#define XRAN_SE24_ENTRY_CP_OFDM 2 // transform precoding disabled, Table 7.7.24.3-2
#define XRAN_SE24_ENTRY_DFT_S_OFDM 3 // transform precoding enabled, Table 7.7.24.3-3

typedef struct {
  uint8_t ef;
  uint8_t aIpnPerSym; // 1 bit
  uint8_t antDmrsSnr; // 1 bit
  uint8_t userGroupSize; // 5 bits, 0 to 12
  uint8_t userGroupId; // 8 bits, 255 is reserved
} xran_se24_hdr_t;

// One entry of the table of DMRS configurations. Fields not carried by the entryType are 0 on decode and ignored on
// encode.
typedef struct {
  uint8_t entryType; // 3 bits, 0 to 3
  uint8_t dmrsPortNumber; // 5 bits, 0 to 11
  // entryType 2 and 3
  uint8_t ueIdReset; // 1 bit
  uint16_t dmrsSymbolMask; // 14 bits
  uint16_t scrambling;
  uint8_t nscid; // 1 bit; 0 for entryType 3 unless lowPaprType is 3
  uint16_t firstPrb; // 9 bits
  uint16_t lastPrb; // 9 bits
  // entryType 2
  uint8_t dType; // 1 bit
  uint8_t cdmWithoutData; // 2 bits, 1 to 3
  uint8_t lambda; // 2 bits, 0 or 1
  // entryType 3
  uint8_t lowPaprType; // 2 bits
  uint8_t hoppingMode; // 2 bits, 0 to 2
} xran_se24_entry_t;

// Writes SE 24 with n_entries entries to buf (avail bytes), with the zero padding to the 4-byte boundary, and its
// length to len_out. Returns XRAN_EXT_OK, XRAN_EXT_ERR_TRUNCATED, XRAN_EXT_ERR_RANGE or XRAN_EXT_ERR_RESERVED.
// Nothing is written on error.
xran_ext_err_t xran_encode_se24(const xran_se24_hdr_t *hdr,
                                const xran_se24_entry_t *entries,
                                int n_entries,
                                uint8_t *buf,
                                size_t avail,
                                size_t *len_out);

// Decodes SE 24 from ext, with len bytes from xran_section_ext_len(). The padding can't be told apart from entries, so
// the caller gives n_entries: one for each ueId other than 0x7FFF in the section header and SE 10 (7.7.24.3).
// Returns XRAN_EXT_OK, or an error if the extension is malformed, has a reserved entryType, starts with entryType 0
// or 1 (7.7.24.3) or its extLen doesn't match n_entries. hdr_out and entries_out are not changed on error.
xran_ext_err_t xran_decode_se24(const uint8_t *ext,
                                size_t len,
                                int n_entries,
                                xran_se24_hdr_t *hdr_out,
                                xran_se24_entry_t *entries_out);

#ifdef __cplusplus
}
#endif
