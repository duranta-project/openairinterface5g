/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include "SCHED_NR_UE/defs.h"

static int check_power(const char *name, const c16_t pattern[4], uint32_t expected)
{
  enum { n_rb = 24, fft_size = 512, samples_per_slot = 14 * fft_size };
  PHY_VARS_NR_UE *ue = calloc(1, sizeof(*ue));
  if (ue == NULL) {
    fprintf(stderr, "%s: cannot allocate UE\n", name);
    return 1;
  }

  nr_csi_info_t csi_info = {0};
  ue->nr_csi_info = &csi_info;
  ue->frame_parms.nb_antennas_rx = 1;
  ue->frame_parms.N_RB_DL = n_rb;
  ue->frame_parms.ofdm_symbol_size = fft_size;
  ue->frame_parms.samples_per_slot_wCP = samples_per_slot;

  // Pattern 1 (TS 38.331 section 6.3.2, CSI-IM-Resource): four subcarriers in one symbol.
  // Offsets 4..7 avoid the DC exclusion, giving exactly 96 measured REs.
  const fapi_nr_dl_config_csiim_pdu_rel15_t pdu = {
      .bwp_size = n_rb,
      .start_rb = 0,
      .nr_of_rbs = n_rb,
      .k_csiim = {4, 5, 6, 7},
      .l_csiim = {0, 0, 0, 0},
  };
  c16_t rxdataF[1][samples_per_slot] = {0};
  for (int rb = 0; rb < n_rb; rb++)
    for (int k = 0; k < 4; k++)
      rxdataF[0][rb * NR_NB_SC_PER_RB + pdu.k_csiim[k]] = pattern[k];

  nr_ue_csi_im_procedures(ue, rxdataF, &pdu);
  const int failed = !csi_info.csi_im_meas_computed || csi_info.interference_plus_noise_power != expected;
  printf("%s: %s, expected=%" PRIu32 ", actual=%" PRIu32 ", measured=%d\n",
         failed ? "FAIL" : "PASS",
         name,
         expected,
         csi_info.interference_plus_noise_power,
         csi_info.csi_im_meas_computed);
  free(ue);
  return failed;
}

int main(void)
{
  int failures = 0;
  failures += check_power("zero", (c16_t[]){{0, 0}, {0, 0}, {0, 0}, {0, 0}}, 0);
  failures += check_power("low amplitude", (c16_t[]){{100, 50}, {-100, 50}, {100, -50}, {-100, -50}}, 12500);
  failures += check_power("nonzero mean", (c16_t[]){{100, -50}, {200, -50}, {100, 50}, {200, 50}}, 5000);

  // Each component's sum of squares is 96 * 5000^2 = 2,400,000,000 > INT32_MAX.
  // The expected variances remain representable in the uint32_t output.
  failures += check_power("real accumulation", (c16_t[]){{5000, 0}, {-5000, 0}, {5000, 0}, {-5000, 0}}, 25000000);
  failures += check_power("imaginary accumulation", (c16_t[]){{0, 5000}, {0, 5000}, {0, -5000}, {0, -5000}}, 25000000);
  failures += check_power("complex accumulation", (c16_t[]){{5000, 5000}, {-5000, 5000}, {5000, -5000}, {-5000, -5000}}, 50000000);
  failures += check_power("constant signal", (c16_t[]){{5000, 5000}, {5000, 5000}, {5000, 5000}, {5000, 5000}}, 0);
  failures += check_power("large nonzero mean", (c16_t[]){{6000, -50}, {-4000, -50}, {6000, 50}, {-4000, 50}}, 25002500);
  failures += check_power("full-scale complex signal",
                          (c16_t[]){{32767, 32767}, {-32767, 32767}, {32767, -32767}, {-32767, -32767}},
                          2147352578);
  failures +=
      check_power("minimum int16 constant", (c16_t[]){{-32768, -32768}, {-32768, -32768}, {-32768, -32768}, {-32768, -32768}}, 0);
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
