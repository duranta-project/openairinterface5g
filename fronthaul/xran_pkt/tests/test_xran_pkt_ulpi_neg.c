/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Bad input for the ULPI section extension codecs of xran_pkt_ulpi.h. Each case starts from a valid extension made by
// the encoder and changes one byte or one length. A decoder must return the expected error, write nothing to its
// outputs and read nothing past len: each decode reads an exact-size heap copy, so AddressSanitizer sees an over-read.
// An encoder must return the expected error and write nothing.

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "xran_pkt_cp.h"
#include "xran_pkt_ext.h"
#include "xran_pkt_ulpi.h"
#include "xran_pkt_test_util.h"

/**********************************************************************
 * One encoder and one decoder for each extension, with the same signatures
 **********************************************************************/

#define SE24_N_ENTRIES 3

// Encodes a valid extension with the given ef
typedef xran_ext_err_t (*encode_fn)(uint8_t ef, uint8_t *buf, size_t avail, size_t *len_out);
// Decodes with room for the maximum count. On error, asserts that the outputs are unchanged.
typedef xran_ext_err_t (*decode_fn)(const uint8_t *ext, size_t len);

// 11b, numPortc 3: content 4 + 3 * 2 = 10 bytes
static xran_ext_err_t enc_se10(uint8_t ef, uint8_t *buf, size_t avail, size_t *len_out)
{
  const xran_se10_hdr_t hdr = {.ef = ef, .beamGroupType = XRAN_SE10_BGT_11B, .numPortc = 3, .firstPortListIndex = 1};
  const xran_se10_port_t ports[] = {{.id = 0x0102, .portListIndex = 2}, {.id = 0x7FFF, .portListIndex = 3}};
  return xran_encode_se10(&hdr, ports, buf, avail, len_out);
}

static xran_ext_err_t dec_se10(const uint8_t *ext, size_t len)
{
  xran_se10_hdr_t hdr;
  xran_se10_port_t ports[XRAN_SE10_MAX_PORTS];
  memset(&hdr, FILL, sizeof(hdr));
  memset(ports, FILL, sizeof(ports));
  xran_ext_err_t err = xran_decode_se10(ext, len, XRAN_SE10_MAX_PORTS, &hdr, ports);
  if (err != XRAN_EXT_OK)
    assert(is_fill(&hdr, sizeof(hdr)) && is_fill(ports, sizeof(ports)));
  return err;
}

// entryType 2, 0, 3 at offsets 4, 14, 15: content 4 + 10 + 1 + 10 = 25 bytes
static const xran_se24_entry_t se24_entries[SE24_N_ENTRIES] = {
    {.entryType = XRAN_SE24_ENTRY_CP_OFDM, .dmrsPortNumber = 1, .dmrsSymbolMask = 0x0004, .cdmWithoutData = 2, .lastPrb = 105},
    {.entryType = XRAN_SE24_ENTRY_INHERIT_PERSIST, .dmrsPortNumber = 2},
    {.entryType = XRAN_SE24_ENTRY_DFT_S_OFDM, .dmrsPortNumber = 3, .dmrsSymbolMask = 0x0004, .lastPrb = 50},
};

static xran_ext_err_t enc_se24(uint8_t ef, uint8_t *buf, size_t avail, size_t *len_out)
{
  const xran_se24_hdr_t hdr = {.ef = ef, .userGroupSize = 3};
  return xran_encode_se24(&hdr, se24_entries, SE24_N_ENTRIES, buf, avail, len_out);
}

static xran_ext_err_t dec_se24_n(const uint8_t *ext, size_t len, int n_entries)
{
  xran_se24_hdr_t hdr;
  xran_se24_entry_t entries[8];
  memset(&hdr, FILL, sizeof(hdr));
  memset(entries, FILL, sizeof(entries));
  xran_ext_err_t err = xran_decode_se24(ext, len, n_entries, &hdr, entries);
  if (err != XRAN_EXT_OK)
    assert(is_fill(&hdr, sizeof(hdr)) && is_fill(entries, sizeof(entries)));
  return err;
}

static xran_ext_err_t dec_se24(const uint8_t *ext, size_t len)
{
  return dec_se24_n(ext, len, SE24_N_ENTRIES);
}

typedef struct {
  const char *name;
  uint8_t ext_type;
  encode_fn encode;
  decode_fn decode;
} codec_t;

static const codec_t se10_codec = {"SE 10", XRAN_CP_SECTIONEXTCMD_10, enc_se10, dec_se10};
static const codec_t se24_codec = {"SE 24", XRAN_CP_SECTIONEXTCMD_24, enc_se24, dec_se24};
static const codec_t *const codecs[] = {&se10_codec, &se24_codec};

// Decodes the first len bytes of ext from an exact-size heap copy
static xran_ext_err_t decode_exact(decode_fn decode, const uint8_t *ext, size_t len)
{
  uint8_t *copy = malloc(len > 0 ? len : 1);
  memcpy(copy, ext, len);
  xran_ext_err_t err = decode(copy, len);
  free(copy);
  return err;
}

static size_t encode_valid(const codec_t *c, uint8_t buf[64])
{
  size_t len = 0;
  assert(c->encode(1, buf, 64, &len) == XRAN_EXT_OK && len > 0 && len < 64);
  assert(decode_exact(c->decode, buf, len) == XRAN_EXT_OK);
  return len;
}

/**********************************************************************
 * Bad input common to all decoders and encoders
 **********************************************************************/

// Every length shorter than the extension (header cut, or extLen past the end) is TRUNCATED. extLen 1 word more
// than the packet is TRUNCATED, extLen 0 is EXT_LEN, and another extType is EXT_TYPE. With two faults, the first check
// wins: a short header before extType, extType before extLen.
static void test_common_decode_errors(void)
{
  int n_cases = 0;
  for (size_t c = 0; c < sizeof(codecs) / sizeof(codecs[0]); c++) {
    const codec_t *codec = codecs[c];
    uint8_t valid[64], ext[64];
    size_t len = encode_valid(codec, valid);

    for (size_t cut = 0; cut < len; cut++, n_cases++)
      assert(decode_exact(codec->decode, valid, cut) == XRAN_EXT_ERR_TRUNCATED);

    memcpy(ext, valid, len);
    ext[1]++; // extLen past the end of the packet
    assert(decode_exact(codec->decode, ext, len) == XRAN_EXT_ERR_TRUNCATED);

    memcpy(ext, valid, len);
    ext[1] = 0; // reserved
    assert(decode_exact(codec->decode, ext, len) == XRAN_EXT_ERR_EXT_LEN);

    memcpy(ext, valid, len);
    ext[0] = (uint8_t)((ext[0] & 0x80) | XRAN_CP_SECTIONEXTCMD_11); // another extType, same ef
    assert(decode_exact(codec->decode, ext, len) == XRAN_EXT_ERR_EXT_TYPE);
    ext[1] = 0;
    assert(decode_exact(codec->decode, ext, len) == XRAN_EXT_ERR_EXT_TYPE);
    assert(decode_exact(codec->decode, ext, 1) == XRAN_EXT_ERR_TRUNCATED);
    n_cases += 5;
  }
  printf("common decode errors, %d cases, passed\n", n_cases);
}

// ef > 1 is RANGE and a buffer 1 byte too short is TRUNCATED. The buffer is unchanged in both cases.
static void test_common_encode_errors(void)
{
  for (size_t c = 0; c < sizeof(codecs) / sizeof(codecs[0]); c++) {
    const codec_t *codec = codecs[c];
    uint8_t valid[64], buf[64];
    size_t len = encode_valid(codec, valid);

    size_t out;
    reset_encode_out(buf, sizeof(buf), &out);
    assert(codec->encode(2, buf, sizeof(buf), &out) == XRAN_EXT_ERR_RANGE);
    assert(encode_out_untouched(buf, sizeof(buf), out));

    assert(codec->encode(1, buf, len - 1, &out) == XRAN_EXT_ERR_TRUNCATED);
    assert(encode_out_untouched(buf, sizeof(buf), out));
  }
  printf("common encode errors passed\n");
}

/**********************************************************************
 * Bad input specific to one extension
 **********************************************************************/

// SE 10 11b: portListIndex 0 is reserved (7.7.10.3a), as the 1st portListIndex or for a listed port. The buffer and
// len_out are unchanged on error.
static void test_se10_encode_errors(void)
{
  const xran_se10_port_t ok_ports[] = {{.id = 0x0102, .portListIndex = 2}, {.id = 0x7FFF, .portListIndex = 255}};
  for (int bad = -1; bad < 2; bad++) { // -1: the 1st portListIndex, else the listed port
    xran_se10_hdr_t hdr = {.ef = 1, .beamGroupType = XRAN_SE10_BGT_11B, .numPortc = 3, .firstPortListIndex = 1};
    xran_se10_port_t ports[2] = {ok_ports[0], ok_ports[1]};
    if (bad < 0)
      hdr.firstPortListIndex = 0;
    else
      ports[bad].portListIndex = 0;
    uint8_t buf[64];
    size_t out;
    reset_encode_out(buf, sizeof(buf), &out);
    assert(xran_encode_se10(&hdr, ports, buf, sizeof(buf), &out) == XRAN_EXT_ERR_RANGE);
    assert(encode_out_untouched(buf, sizeof(buf), out));
  }
  // 10b has no portListIndex: a 0 in the port struct is not checked
  const xran_se10_hdr_t hdr10b = {.ef = 1, .beamGroupType = XRAN_SE10_BGT_10B, .numPortc = 2};
  const xran_se10_port_t ports10b[] = {{.id = 1}, {.id = 2}};
  uint8_t buf[64];
  size_t out;
  assert(xran_encode_se10(&hdr10b, ports10b, buf, sizeof(buf), &out) == XRAN_EXT_OK);
  printf("SE 10 encode errors passed\n");
}

// SE 10: a numPortc that doesn't match extLen
static void test_se10_errors(void)
{
  uint8_t ext[64];
  size_t len = encode_valid(&se10_codec, ext);
  assert(len == 12);
  ext[2] = (uint8_t)((ext[2] & 0xC0) | 4); // 11b, 4 ports: content 13 bytes, 16 with the filler, not 12
  assert(decode_exact(dec_se10, ext, len) == XRAN_EXT_ERR_EXT_LEN);
  ext[2] = (uint8_t)(ext[2] & 0xC0); // numPortc 0 = 64 ports
  assert(decode_exact(dec_se10, ext, len) == XRAN_EXT_ERR_EXT_LEN);
  printf("SE 10 errors passed\n");
}

// SE 24: entryType 4 to 7 (reserved) in the first or a later entry, and an n_entries that doesn't match the content.
// The padding looks the same as entryType 0 entries, so an n_entries that runs into the padding decodes: only the
// caller's count from the ueIds guards it (7.7.24.3).
static void test_se24_errors(void)
{
  uint8_t valid[64], ext[64];
  size_t len = encode_valid(&se24_codec, valid);
  assert(len == 28);
  const size_t entry_offsets[] = {4, 14, 15};
  for (int e = 0; e < SE24_N_ENTRIES; e++) {
    for (int type = 4; type <= 7; type++) {
      memcpy(ext, valid, len);
      ext[entry_offsets[e]] = (uint8_t)((type << 5) | (ext[entry_offsets[e]] & 0x1F));
      assert(decode_exact(dec_se24, ext, len) == XRAN_EXT_ERR_RESERVED);
    }
  }
  assert(decode_exact(dec_se24, ext, 3) == XRAN_EXT_ERR_TRUNCATED); // a short header before entryType

  uint8_t *copy = malloc(len);
  memcpy(copy, valid, len);
  assert(dec_se24_n(copy, len, 2) == XRAN_EXT_ERR_EXT_LEN); // 15 bytes of entries: 16 with the padding, not 28
  assert(dec_se24_n(copy, len, 6) == XRAN_EXT_OK); // 3 padding bytes read as entryType 0
  assert(dec_se24_n(copy, len, 7) == XRAN_EXT_ERR_EXT_LEN); // past the padding
  free(copy);

  // The first entry has no entryType 2 or 3 to inherit from (7.7.24.3). A later entryType 0 or 1 decodes.
  for (int type = XRAN_SE24_ENTRY_INHERIT_PERSIST; type <= XRAN_SE24_ENTRY_INHERIT_RESET; type++) {
    memcpy(ext, valid, len);
    ext[entry_offsets[0]] = (uint8_t)((type << 5) | (ext[entry_offsets[0]] & 0x1F));
    assert(decode_exact(dec_se24, ext, len) == XRAN_EXT_ERR_RESERVED);
    memcpy(ext, valid, len);
    ext[entry_offsets[1]] = (uint8_t)((type << 5) | (ext[entry_offsets[1]] & 0x1F));
    assert(decode_exact(dec_se24, ext, len) == XRAN_EXT_OK);
  }
  printf("SE 24 errors passed\n");
}

// Encodes SE 24 with hdr and the SE24_N_ENTRIES entries. The buffer and len_out are unchanged on error.
static xran_ext_err_t enc_se24_checked(const xran_se24_hdr_t *hdr, const xran_se24_entry_t *entries)
{
  uint8_t buf[64];
  size_t out;
  reset_encode_out(buf, sizeof(buf), &out);
  xran_ext_err_t err = xran_encode_se24(hdr, entries, SE24_N_ENTRIES, buf, sizeof(buf), &out);
  if (err != XRAN_EXT_OK)
    assert(encode_out_untouched(buf, sizeof(buf), out));
  return err;
}

// Encodes the SE 24 of enc_se24() with one entry changed
static xran_ext_err_t enc_se24_entry(int idx, xran_se24_entry_t entry)
{
  xran_se24_entry_t entries[SE24_N_ENTRIES];
  memcpy(entries, se24_entries, sizeof(entries));
  entries[idx] = entry;
  const xran_se24_hdr_t hdr = {.ef = 1, .userGroupSize = 3};
  return enc_se24_checked(&hdr, entries);
}

// SE 24 entries the O-DU shall not send
static void test_se24_encode_errors(void)
{
  // A first entry of entryType 0 or 1 (7.7.24.3)
  for (int type = XRAN_SE24_ENTRY_INHERIT_PERSIST; type <= XRAN_SE24_ENTRY_INHERIT_RESET; type++)
    assert(enc_se24_entry(0, (xran_se24_entry_t){.entryType = type, .dmrsPortNumber = 1}) == XRAN_EXT_ERR_RESERVED);

  // entryType 3: nscid 1 is valid only with lowPaprType 3 (7.7.24.13)
  xran_se24_entry_t e = se24_entries[2];
  for (e.lowPaprType = 0; e.lowPaprType <= 3; e.lowPaprType++) {
    e.nscid = 1;
    assert(enc_se24_entry(2, e) == (e.lowPaprType == 3 ? XRAN_EXT_OK : XRAN_EXT_ERR_RANGE));
    e.nscid = 0;
    assert(enc_se24_entry(2, e) == XRAN_EXT_OK);
  }

  // entryType 3: hoppingMode 3 is reserved (7.7.24.18)
  e = se24_entries[2];
  e.hoppingMode = 2;
  assert(enc_se24_entry(2, e) == XRAN_EXT_OK);
  e.hoppingMode = 3;
  assert(enc_se24_entry(2, e) == XRAN_EXT_ERR_RANGE);

  // dmrsPortNumber 12 to 31 are reserved (7.7.24.9), in an entry of each entryType
  for (int idx = 0; idx < SE24_N_ENTRIES; idx++) {
    e = se24_entries[idx];
    e.dmrsPortNumber = 11;
    assert(enc_se24_entry(idx, e) == XRAN_EXT_OK);
    for (e.dmrsPortNumber = 12; e.dmrsPortNumber <= 31; e.dmrsPortNumber++)
      assert(enc_se24_entry(idx, e) == XRAN_EXT_ERR_RANGE);
  }

  // entryType 2: cdmWithoutData 0 is reserved (7.7.24.15) and lambda is 0 or 1 (7.7.24.16)
  e = se24_entries[0];
  for (e.cdmWithoutData = 0; e.cdmWithoutData <= 3; e.cdmWithoutData++)
    assert(enc_se24_entry(0, e) == (e.cdmWithoutData == 0 ? XRAN_EXT_ERR_RANGE : XRAN_EXT_OK));
  e = se24_entries[0];
  for (e.lambda = 0; e.lambda <= 3; e.lambda++)
    assert(enc_se24_entry(0, e) == (e.lambda <= 1 ? XRAN_EXT_OK : XRAN_EXT_ERR_RANGE));

  // userGroupSize 13 to 31 and userGroupId 255 are reserved (7.7.24.6, 7.7.24.7)
  for (int size = 0; size <= 31; size++) {
    const xran_se24_hdr_t hdr = {.userGroupSize = size, .userGroupId = 254};
    assert(enc_se24_checked(&hdr, se24_entries) == (size <= 12 ? XRAN_EXT_OK : XRAN_EXT_ERR_RANGE));
  }
  const xran_se24_hdr_t hdr_id_255 = {.userGroupId = 255};
  assert(enc_se24_checked(&hdr_id_255, se24_entries) == XRAN_EXT_ERR_RANGE);
  printf("SE 24 encode errors passed\n");
}

int main(void)
{
  test_common_decode_errors();
  test_common_encode_errors();
  test_se10_errors();
  test_se10_encode_errors();
  test_se24_errors();
  test_se24_encode_errors();
  printf("All ULPI bad-input tests passed!\n");
  return 0;
}
