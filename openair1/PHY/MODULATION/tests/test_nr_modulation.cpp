/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Initial unit test for nr_layer_precoder_simd_[cm, simd] functions
// Issue: gNB: SIMD Precoding Bug in nr_layer_precoder_simd for PMI = 4
// https://gitlab.eurecom.fr/oai/openairinterface5g/-/issues/955

#include "gtest/gtest.h"
#include <cstdint>
#include <simde/x86/avx512.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "openair1/PHY/TOOLS/tools_defs.h" // c16_t
#include "nfapi/open-nFAPI/nfapi/public_inc/fapi_nr_ue_interface.h" // fapi_nr_dl_config_dci_dl_pdu_rel15_t

//#include "openair1/PHY/MODULATION/nr_modulation.h" // nr_layer_precoder_simd_[cm, simd]

// Forward declare the function but hide the Variable-Length Array (VLA) feature from C not available in C++
c16_t nr_layer_precoder_cm(int n_layers,
                           int symbol_size,
                           c16_t (*dataF_in)[/*symbol_size*/], // c16_t dataF_in[n_layers][symbol_size]
                           int ant,
                           c16_t weights[NR_MAX_NB_LAYERS][NR_MAX_CSI_PORTS],
                           int offset);

// Forward declare the function but hide the Variable-Length Array (VLA) feature from C not available in C++
void nr_layer_precoder_simd(int n_layers,
                            int symbol_size,
                            const c16_t (*dataF_in)[/*symbol_size*/], // c16_t dataF_in[n_layers][symbol_size]
                            int ant,
                            c16_t weights[NR_MAX_NB_LAYERS][NR_MAX_CSI_PORTS],
                            int offset,
                            int re_cnt,
                            c16_t *dataF_out);

#ifdef __cplusplus
}
#endif

// The SIMD precoders apply their per-layer Q15 scaling with different rounding
// per ISA: x86 (madd + arithmetic shift) truncates, while aarch64/NEON
// (vqdmulh/vqrdmlah) rounds. Each layer's contribution can therefore differ by
// up to 1 LSB, and the difference accumulates over the layers, so a comparison
// against a reference computed with different rounding must tolerate up to
// n_layers LSB. Exact-equality checks previously broke CI on aarch64.
#define EXPECT_C16_NEAR(got, want, tol) EXPECT_LE(abs((int)(got) - (int)(want)), (tol))

TEST(NrLayerPrecoderTest, Basic)
{
  constexpr int n_layers = 2;
  constexpr int symbol_size = 24;
  constexpr int n_ants = 2;
  constexpr int ant = 1;
  constexpr int re_cnt = 24;

  // Initialize the 2D input data buffer
  std::vector<c16_t> buffer_in(n_layers * symbol_size);
  std::vector<c16_t> buffer_out(n_layers * symbol_size);

  for (int i = 0; i < n_layers * symbol_size; ++i) {
    buffer_in[i] = {static_cast<int16_t>(i + 1), static_cast<int16_t>(-i - 1)};
  }
  for (int i = 0; i < n_ants * symbol_size; ++i) {
    buffer_out[i] = {static_cast<int16_t>(0), static_cast<int16_t>(0)};
  }

  // Cast flat buffer to the required 2D Variable-Length Array (VLA) style pointer
  c16_t(*dataF_in)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_in.data());
  c16_t(*dataF_out)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_out.data());

  // Create and populate the weights
  c16_t weights[NR_MAX_NB_LAYERS][NR_MAX_CSI_PORTS];
  for (int layer = 0; layer < n_layers; ++layer) {
    weights[layer][ant] = (c16_t){-SHRT_MAX, 0};
  }

  // Call the C function
  for (int symbol = 0; symbol < re_cnt; symbol++)
    dataF_out[ant][symbol] = nr_layer_precoder_cm(n_layers, symbol_size, dataF_in, ant, weights, symbol);

  // You can calculate the expected value manually or test rough correctness
  for (int symbol = 0; symbol < re_cnt; symbol++) {
    int result_real = -26 - (symbol<<1);
    int result_imag = -24 - (symbol<<1);

    EXPECT_EQ(dataF_out[ant][symbol].r, result_real)
        << " at [" << ant << "][" << symbol << "] got real part: " << dataF_out[ant][symbol].r;
    EXPECT_EQ(dataF_out[ant][symbol].i, -result_imag)
        << " at [" << ant << "][" << symbol << "] got imag part: " << dataF_out[ant][symbol].i;
  }
}

TEST(NrLayerPrecoderTest, SIMD)
{
  constexpr int n_layers = 2;
  constexpr int symbol_size = 24;
  constexpr int n_ants = 2;
  constexpr int ant = 1;
  constexpr int re_cnt = 24;

  // Initialize the 2D input data buffer
  std::vector<c16_t> buffer_in(n_layers * symbol_size);
  std::vector<c16_t> buffer_out(n_layers * symbol_size);

  for (int i = 0; i < n_layers * symbol_size; ++i) {
    buffer_in[i] = {static_cast<int16_t>(i + 1), static_cast<int16_t>(-i - 1)};
  }
  for (int i = 0; i < n_ants * symbol_size; ++i) {
    buffer_out[i] = {static_cast<int16_t>(0), static_cast<int16_t>(0)};
  }

  // Cast flat buffer to the required 2D Variable-Length Array (VLA) style pointer
  c16_t(*dataF_in)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_in.data());
  c16_t(*dataF_out)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_out.data());

  // Create and populate the weights
  c16_t weights[NR_MAX_NB_LAYERS][NR_MAX_CSI_PORTS];
  for (int layer = 0; layer < n_layers; ++layer) {
    weights[layer][ant] = (c16_t){-SHRT_MAX, 0};
  }

  // Call the C function
  nr_layer_precoder_simd(n_layers, symbol_size, dataF_in, ant, weights, 0, re_cnt, dataF_out[ant]);

  // You can calculate the expected value manually or test rough correctness
  for (int symbol = 0; symbol < re_cnt; symbol++) {
    int result_real = -26 - (symbol<<1);
    int result_imag = -24 - (symbol<<1);

    // n_layers contributions, each up to 1 LSB apart between truncating (x86)
    // and rounding (NEON) Q15 scaling
    EXPECT_C16_NEAR(dataF_out[ant][symbol].r, result_real, n_layers)
        << " at [" << ant << "][" << symbol << "] got real part: " << dataF_out[ant][symbol].r;
    EXPECT_C16_NEAR(dataF_out[ant][symbol].i, -result_imag, n_layers)
        << " at [" << ant << "][" << symbol << "] got imag part: " << dataF_out[ant][symbol].i;
  }
}

TEST(NrLayerPrecoderTest, Compare_CM_SIMD)
{
  constexpr int n_layers = 1;
  constexpr int symbol_size = 24;
  constexpr int n_ants = 2;
  constexpr int re_cnt = 24;

  // Initialize the 2D input data buffer
  std::vector<c16_t> buffer_in(n_layers * symbol_size);
  std::vector<c16_t> buffer_out_cm(n_ants * symbol_size);
  std::vector<c16_t> buffer_out_simd(n_ants * symbol_size);

  for (int i = 0; i < n_layers * symbol_size; ++i) {
	buffer_in[i] = {static_cast<int16_t>((rand() % (2 * SHRT_MAX + 1)) - SHRT_MAX), static_cast<int16_t>((rand() % (2 * SHRT_MAX + 1)) - SHRT_MAX)};
  }
  for (int i = 0; i < n_ants * symbol_size; ++i) {
    buffer_out_cm[i] = {static_cast<int16_t>(0), static_cast<int16_t>(0)};
    buffer_out_simd[i] = {static_cast<int16_t>(0), static_cast<int16_t>(0)};
  }

  // Cast flat buffer to the required 2D Variable-Length Array (VLA) style pointer
  c16_t(*dataF_in)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_in.data());
  c16_t(*dataF_out_cm)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_out_cm.data());
  c16_t(*dataF_out_simd)[symbol_size] = reinterpret_cast<c16_t(*)[symbol_size]>(buffer_out_simd.data());

  // Create and populate the weights
  c16_t weights[NR_MAX_NB_LAYERS][NR_MAX_CSI_PORTS];
  for (int layer = 0; layer < n_layers; ++layer) {
    // Could not use convert_precoder_weight() as complex.h could not be used in googletest
    // Use the logic in convert_precoder_weight()
    // precoder [−1,−j]
    weights[layer][0] = (c16_t){-SHRT_MAX, 0};
    weights[layer][1] = (c16_t){0, -SHRT_MAX};
  }

  // Get the results for all the antenna 
  for (int ant = 0; ant < n_ants; ant++) {

    // Call the C function
    for (int symbol = 0; symbol < re_cnt; symbol++)
      dataF_out_cm[ant][symbol] = nr_layer_precoder_cm(n_layers, symbol_size, dataF_in, ant, weights, symbol);

    // Call the C function
    nr_layer_precoder_simd(n_layers, symbol_size, dataF_in, ant, weights, 0, re_cnt, dataF_out_simd[ant]);

    // Compare the result from both C function
    for (int symbol = 0; symbol < re_cnt; symbol++) {
      EXPECT_C16_NEAR(dataF_out_cm[ant][symbol].r, dataF_out_simd[ant][symbol].r, 1)
          << " at [" << ant << "][" << symbol << "] got real part: " << dataF_out_cm[ant][symbol].r << " result " << dataF_out_simd[ant][symbol].r;
      EXPECT_C16_NEAR(dataF_out_cm[ant][symbol].i, dataF_out_simd[ant][symbol].i, 1)
          << " at [" << ant << "][" << symbol << "] got imag part: " << dataF_out_cm[ant][symbol].i << " result " << dataF_out_simd[ant][symbol].i;
    }
  }
}

int main(int argc, char **argv)
{
  // Initialize random seed
  srand(time(0));

  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
