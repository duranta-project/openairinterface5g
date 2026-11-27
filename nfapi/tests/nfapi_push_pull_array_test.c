/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* Round-trip and wire-format tests for the 16-bit scalar and array push/pull
 * primitives, complementing nfapi_pull_push_test.c which covers 32-bit scalar.
 *
 * Strategy: array functions must produce exactly the same wire bytes as N
 * scalar calls. The scalar functions are already trusted by the existing test,
 * so a byte-for-byte match implies the array path is correct for every value
 * and endianness without hard-coding expected wire bytes in the test. */

#include "nfapi/tests/nr_fapi_test.h"
#include "nfapi.h"

#define ARRAY_LEN 1024

static void test_push16(uint16_t v)
{
  uint8_t buf[2];
  uint8_t *p = buf;
  DevAssert(push16(v, &p, buf + sizeof(buf)) == 2);
  DevAssert(p == buf + 2);
  p = buf;
  uint16_t out = 0;
  DevAssert(pull16(&p, &out, buf + sizeof(buf)) == 2);
  DevAssert(out == v);
}

static void test_array16_matches_scalar(const uint16_t *src, uint32_t n)
{
  uint8_t wire_scalar[ARRAY_LEN * 2], wire_array[ARRAY_LEN * 2];
  uint8_t *p;

  p = wire_scalar;
  for (uint32_t i = 0; i < n; i++)
    DevAssert(push16(src[i], &p, wire_scalar + sizeof(wire_scalar)) == 2);

  p = wire_array;
  DevAssert(pusharray16(src, n, n, &p, wire_array + sizeof(wire_array)) == n * 2);

  DevAssert(memcmp(wire_scalar, wire_array, n * 2) == 0);
}

static void test_array16_roundtrip(const uint16_t *src, uint32_t n)
{
  uint8_t wire[ARRAY_LEN * 2];
  uint16_t dst[ARRAY_LEN];
  uint8_t *p = wire;

  DevAssert(pusharray16(src, n, n, &p, wire + sizeof(wire)) == n * 2);
  p = wire;
  DevAssert(pullarray16(&p, dst, n, n, wire + sizeof(wire)) == n * 2);
  for (uint32_t i = 0; i < n; i++)
    AssertFatal(dst[i] == src[i], "pullarray16[%u]: got 0x%04x, expected 0x%04x\n", i, dst[i], src[i]);
}

static void test_array32_matches_scalar(const uint32_t *src, uint32_t n)
{
  uint8_t wire_scalar[ARRAY_LEN * 4], wire_array[ARRAY_LEN * 4];
  uint8_t *p;

  p = wire_scalar;
  for (uint32_t i = 0; i < n; i++)
    DevAssert(push32(src[i], &p, wire_scalar + sizeof(wire_scalar)) == 4);

  p = wire_array;
  DevAssert(pusharray32(src, n, n, &p, wire_array + sizeof(wire_array)) == n * 4);

  DevAssert(memcmp(wire_scalar, wire_array, n * 4) == 0);
}

static void test_array32_roundtrip(const uint32_t *src, uint32_t n)
{
  uint8_t wire[ARRAY_LEN * 4];
  uint32_t dst[ARRAY_LEN];
  uint8_t *p = wire;

  DevAssert(pusharray32(src, n, n, &p, wire + sizeof(wire)) == n * 4);
  p = wire;
  DevAssert(pullarray32(&p, dst, n, n, wire + sizeof(wire)) == n * 4);
  for (uint32_t i = 0; i < n; i++)
    AssertFatal(dst[i] == src[i], "pullarray32[%u]: got 0x%08x, expected 0x%08x\n", i, dst[i], src[i]);
}

static void test_pushs16(int16_t v)
{
  uint8_t buf[2];
  uint8_t *p = buf;
  DevAssert(pushs16(v, &p, buf + sizeof(buf)) == 2);
  DevAssert(p == buf + 2);
  p = buf;
  int16_t out = 0;
  DevAssert(pulls16(&p, &out, buf + sizeof(buf)) == 2);
  DevAssert(out == v);
}

static void test_arrays16_matches_scalar(const int16_t *src, uint32_t n)
{
  uint8_t wire_scalar[ARRAY_LEN * 2], wire_array[ARRAY_LEN * 2];
  uint8_t *p;

  p = wire_scalar;
  for (uint32_t i = 0; i < n; i++)
    DevAssert(pushs16(src[i], &p, wire_scalar + sizeof(wire_scalar)) == 2);

  p = wire_array;
  DevAssert(pusharrays16((int16_t *)src, n, n, &p, wire_array + sizeof(wire_array)) == n * 2);

  DevAssert(memcmp(wire_scalar, wire_array, n * 2) == 0);
}

static void test_arrays16_roundtrip(const int16_t *src, uint32_t n)
{
  uint8_t wire[ARRAY_LEN * 2];
  int16_t dst[ARRAY_LEN];
  uint8_t *p = wire;

  DevAssert(pusharrays16((int16_t *)src, n, n, &p, wire + sizeof(wire)) == n * 2);
  p = wire;
  DevAssert(pullarrays16(&p, dst, n, n, wire + sizeof(wire)) == n * 2);
  for (uint32_t i = 0; i < n; i++)
    AssertFatal(dst[i] == src[i], "pullarrays16[%u]: got 0x%04x, expected 0x%04x\n", i, dst[i], src[i]);
}

static void test_pushs32(int32_t v)
{
  uint8_t buf[4];
  uint8_t *p = buf;
  DevAssert(pushs32(v, &p, buf + sizeof(buf)) == 4);
  DevAssert(p == buf + 4);
  p = buf;
  int32_t out = 0;
  DevAssert(pulls32(&p, &out, buf + sizeof(buf)) == 4);
  DevAssert(out == v);
}

static void test_arrays32_matches_scalar(const int32_t *src, uint32_t n)
{
  uint8_t wire_scalar[ARRAY_LEN * 4], wire_array[ARRAY_LEN * 4];
  uint8_t *p;

  p = wire_scalar;
  for (uint32_t i = 0; i < n; i++)
    DevAssert(pushs32(src[i], &p, wire_scalar + sizeof(wire_scalar)) == 4);

  p = wire_array;
  DevAssert(pusharrays32((int32_t *)src, n, n, &p, wire_array + sizeof(wire_array)) == n * 4);

  DevAssert(memcmp(wire_scalar, wire_array, n * 4) == 0);
}

static void test_arrays32_roundtrip(const int32_t *src, uint32_t n)
{
  uint8_t wire[ARRAY_LEN * 4];
  int32_t dst[ARRAY_LEN];
  uint8_t *p = wire;

  DevAssert(pusharrays32((int32_t *)src, n, n, &p, wire + sizeof(wire)) == n * 4);
  p = wire;
  DevAssert(pullarrays32(&p, dst, n, n, wire + sizeof(wire)) == n * 4);
  for (uint32_t i = 0; i < n; i++)
    AssertFatal(dst[i] == src[i], "pullarrays32[%u]: got 0x%08x, expected 0x%08x\n", i, dst[i], src[i]);
}

int main()
{
  fapi_test_init();

  /* --- 16-bit scalar round-trip --- */
  const uint16_t fixed16[] = {0x0000, 0x0001, 0x00FF, 0xFF00, 0x7FFF, 0x8000, 0xABCD, 0xFFFF};
  for (size_t i = 0; i < sizeofArray(fixed16); i++)
    test_push16(fixed16[i]);
  for (int i = 0; i < 1000; i++)
    test_push16(rand16());

  /* --- 16-bit signed scalar round-trip --- */
  const int16_t fixeds16[] = {0, 1, -1, 0x7FFF, (int16_t)0x8000};
  for (size_t i = 0; i < sizeofArray(fixeds16); i++)
    test_pushs16(fixeds16[i]);
  for (int i = 0; i < 1000; i++)
    test_pushs16((int16_t)rand16());

  /* --- 32-bit signed scalar round-trip --- */
  const int32_t fixeds32[] = {0, 1, -1, 0x7FFFFFFF, (int32_t)0x80000000};
  for (size_t i = 0; i < sizeofArray(fixeds32); i++)
    test_pushs32(fixeds32[i]);
  for (int i = 0; i < 1000; i++)
    test_pushs32((int32_t)rand32());

  /* --- build test arrays with random data --- */
  uint16_t src16[ARRAY_LEN];
  uint32_t src32[ARRAY_LEN];
  int16_t srcs16[ARRAY_LEN];
  int32_t srcs32[ARRAY_LEN];
  for (int i = 0; i < ARRAY_LEN; i++) {
    src16[i] = rand16();
    src32[i] = rand32();
    srcs16[i] = (int16_t)rand16();
    srcs32[i] = (int32_t)rand32();
  }

  /* --- array 16-bit unsigned: wire bytes must match scalar, and round-trip --- */
  test_array16_matches_scalar(src16, ARRAY_LEN);
  test_array16_roundtrip(src16, ARRAY_LEN);

  /* --- array 16-bit signed: wire bytes must match scalar, and round-trip --- */
  test_arrays16_matches_scalar(srcs16, ARRAY_LEN);
  test_arrays16_roundtrip(srcs16, ARRAY_LEN);

  /* --- array 32-bit unsigned: wire bytes must match scalar, and round-trip --- */
  test_array32_matches_scalar(src32, ARRAY_LEN);
  test_array32_roundtrip(src32, ARRAY_LEN);

  /* --- array 32-bit signed: wire bytes must match scalar, and round-trip --- */
  test_arrays32_matches_scalar(srcs32, ARRAY_LEN);
  test_arrays32_roundtrip(srcs32, ARRAY_LEN);

  return 0;
}
