/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Round trips of the nFAPI 32-bit push/pull primitives, including values whose
 * most significant octet is >= 0x80. pullx32() must assemble all the octets it
 * reads, independently of what the output held before. */

#include "nfapi/tests/nr_fapi_test.h"
#include "nfapi.h"

static bool wire_is_big_endian(void)
{
  uint8_t buf[2];
  uint8_t *p = buf;
  DevAssert(push16(0x0102, &p, buf + sizeof(buf)) == 2);
  return buf[0] == 0x01;
}

static void test_pull32(uint32_t v)
{
  uint8_t buf[4];
  uint8_t *p = buf;
  DevAssert(push32(v, &p, buf + sizeof(buf)) == 4);
  p = buf;
  uint32_t out = 0;
  DevAssert(pull32(&p, &out, buf + sizeof(buf)) == 4);
  DevAssert(out == v);
}

static void test_pullx32(uint32_t v, bool big_endian)
{
  uint8_t buf[4];
  uint8_t *p = buf;
  DevAssert(push32(v, &p, buf + sizeof(buf)) == 4);
  for (uint8_t len = 1; len <= 4; len++) {
    // the first len octets on the wire: the high-order ones in big endian, the low-order ones in little endian
    const uint32_t low_mask = len == 4 ? UINT32_MAX : (1U << (8 * len)) - 1;
    const uint32_t expected = big_endian ? v >> (8 * (4 - len)) : v & low_mask;
    p = buf;
    uint32_t out = UINT32_MAX; // must be overwritten, not merged into
    DevAssert(pullx32(len, &p, &out, buf + sizeof(buf)) == len);
    DevAssert(p == buf + len);
    AssertFatal(out == expected, "pullx32(%u) of 0x%08x: got 0x%08x, expected 0x%08x\n", len, v, out, expected);
  }
}

int main()
{
  fapi_test_init();
  const bool big_endian = wire_is_big_endian();
  const uint32_t u32[] = {0, 1, 0x7fffffff, 0x80000000, 0xdb0a0b0c, 0xff000000, 0x01020304, UINT32_MAX};
  for (size_t i = 0; i < sizeofArray(u32); i++) {
    test_pull32(u32[i]);
    test_pullx32(u32[i], big_endian);
  }
  for (int i = 0; i < 1000; i++) {
    test_pull32(rand32());
    test_pullx32(rand32(), big_endian);
  }
  return 0;
}
