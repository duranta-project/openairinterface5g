/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "gtest/gtest.h"
#include <cstdint>
#include <simde/x86/avx512.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "openair1/PHY/TOOLS/tools_defs.h" // c16_t

void nr_beamformer_simd(const c16_t *in, const c16_t weight, const int re_cnt, c16_t *out);

#ifdef __cplusplus
}
#endif

// Returns random complex exp with unit maginute
static c16_t get_random_unit_c16(const int16_t amp)
{
  float theta = ((float)rand() / RAND_MAX) * 2.0f * M_PI;
  return (c16_t){.r = (int16_t)roundf(cosf(theta) * amp), .i = (int16_t)roundf(sinf(theta) * amp)};
}

TEST(nr_modulation, nr_beamformer_simd)
{
  const int num_re = 2001;
  const int16_t amp = 1 << 15;

  // Initialize random input
  std::vector<c16_t> buffer_in(num_re);
  for (int i = 0; i < num_re; i++) {
    buffer_in[i] = get_random_unit_c16(amp);
  }

  // Initialize output
  std::vector<c16_t> buffer_out(num_re, {0, 0});

  // Random weight
  const c16_t weight = get_random_unit_c16(amp);

  // Test SIMD function
  nr_beamformer_simd(buffer_in.data(), weight, num_re, buffer_out.data());

  // Compare with non SIMD calculation
  for (int i = 0; i < num_re; i++) {
    const c16_t expected = c16maddShift(buffer_in[i], weight, (c16_t){0, 0}, 15);
    EXPECT_EQ(buffer_out[i].r, expected.r);
    EXPECT_EQ(buffer_out[i].i, expected.i);
  }
}

int main(int argc, char **argv)
{
  // Initialize random seed
  srand(time(0));

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
