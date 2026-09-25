/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define INDEX_MAX_TBS_TABLE (93)

#include "common/utils/nr/nr_common.h"
#include "common/utils/LOG/log.h"
#include "common/utils/bits.h"
#include <math.h>

//Table 5.1.2.2-2
static const uint16_t Tbstable_nr[INDEX_MAX_TBS_TABLE] = {
    24,   32,   40,   48,   56,   64,   72,   80,   88,   96,   104,  112,  120,  128,  136,  144,  152,  160,  168,
    176,  184,  192,  208,  224,  240,  256,  272,  288,  304,  320,  336,  352,  368,  384,  408,  432,  456,  480,
    504,  528,  552,  576,  608,  640,  672,  704,  736,  768,  808,  848,  888,  928,  984,  1032, 1064, 1128, 1160,
    1192, 1224, 1256, 1288, 1320, 1352, 1416, 1480, 1544, 1608, 1672, 1736, 1800, 1864, 1928, 2024, 2088, 2152, 2216,
    2280, 2408, 2472, 2536, 2600, 2664, 2728, 2792, 2856, 2976, 3104, 3240, 3368, 3496, 3624, 3752, 3824};

static const uint16_t NPRB_LBRM[7] = {32, 66, 107, 135, 162, 217, 273};

// Transport block size determination according to 6.1.4.2 of TS 38.214
// returns the TBS in bits
uint32_t nr_compute_tbs(uint16_t Qm,
                        uint16_t R,
                        uint16_t nb_rb,
                        uint16_t nb_symb_sch,
                        uint16_t nb_dmrs_prb,
                        uint16_t nb_rb_oh,
                        uint8_t tb_scaling,
                        uint8_t Nl)
{
  LOG_D(NR_MAC,
        "nb_symb_sch %d, nb_dmrs_prb %d, nb_rb %d, nb_rb_oh %d, tb_scaling %d Nl %d\n",
        nb_symb_sch,
        nb_dmrs_prb,
        nb_rb,
        nb_rb_oh,
        tb_scaling,
        Nl);

  if (Qm == 0 || R == 0 || nb_rb == 0 || Nl == 0) {
    LOG_E(NR_MAC, "Error in compute TBS with a NULL input, returning NULL TBS\n");
    return 0;
  }

  const int nb_subcarrier_per_rb = 12;
  const uint32_t nbp_re =  nb_subcarrier_per_rb * nb_symb_sch - nb_dmrs_prb - nb_rb_oh;
  const uint32_t nb_re = min(156, nbp_re) * nb_rb;
  // Intermediate number of information bits
  // Rx1024 is tabulated as 10 times the actual code rate
  const uint32_t R_5 = R / 5; // R can be fractional so we can't divide by 10
  // So we ned to right shift by 11 (10 for x1024 and 1 additional as above)
  const uint32_t Ninfo = ((nb_re * R_5 * Qm * Nl) >> 11) >> tb_scaling;
  uint32_t nr_tbs = 0;
  uint32_t Np_info, C, n;
  if (Ninfo <= NR_MAX_PDSCH_TBS) {
    int val = floor_log2_u32(Ninfo);
    n = max(3, val - 6);
    Np_info = max(24, (Ninfo >> n) << n);
    for (int i = 0; i < INDEX_MAX_TBS_TABLE; i++) {
      if (Tbstable_nr[i] >= Np_info){
        nr_tbs = Tbstable_nr[i];
        break;
      }
    }
  } else {
    n = log2(Ninfo - 24) - 5;
    Np_info = max(3840, (ROUNDIDIV((Ninfo - 24), (1 << n))) << n);

    if (R <= 2560) {
      C = CEILIDIV((Np_info + 24), 3816);
      nr_tbs = (C << 3) * CEILIDIV((Np_info + 24), (C << 3)) - 24;
    } else {
      if (Np_info > 8424){
        C = CEILIDIV((Np_info + 24), 8424);
        nr_tbs = (C << 3) * CEILIDIV((Np_info + 24), (C << 3)) - 24;
      } else {
        nr_tbs = ((CEILIDIV((Np_info + 24), 8)) << 3) - 24;
      }
    }
  }

  LOG_D(NR_MAC, "Ninfo %u nbp_re %d nb_re %d Qm %d, R %d, tbs %d bits\n", Ninfo, nbp_re, nb_re, Qm, R, nr_tbs);
  return nr_tbs;
}

//tbslbrm calculation according to 5.4.2.1 of 38.212
uint32_t nr_compute_tbslbrm(uint16_t table, uint16_t nb_rb, uint8_t Nl)
{
  uint16_t nb_rb_lbrm = 0;
  for (int i = 0; i < 7; i++) {
    if (NPRB_LBRM[i] >= nb_rb){
    nb_rb_lbrm = NPRB_LBRM[i];
      break;
    }
  }

  int Qm = (table == 1) ? 8 : 6;
  uint32_t R = 948;
  uint32_t nb_re = 156 * nb_rb_lbrm;

  // Intermediate number of information bits
  uint32_t Ninfo = (nb_re * R * Qm * Nl) >> 10;
  uint32_t nr_tbs = 0;
  uint32_t Np_info, n;
  if (Ninfo <= NR_MAX_PDSCH_TBS) {
    int val = floor_log2_u32(Ninfo);
    n = max(3, val - 6);
    Np_info = max(24, (Ninfo >> n) << n);
    for (int i = 0; i < INDEX_MAX_TBS_TABLE; i++) {
      if (Tbstable_nr[i] >= Np_info){
        nr_tbs = Tbstable_nr[i];
        break;
      }
    }
  } else {
    n = log2(Ninfo - 24) - 5;
    Np_info = max(3840, (ROUNDIDIV((Ninfo - 24), (1 << n))) << n);
    int C;
    if (R <= 256) {
      C = CEILIDIV((Np_info + 24), 3816);
      nr_tbs = (C << 3) * CEILIDIV((Np_info + 24), (C << 3)) - 24;
    }
    else {
      if (Np_info > 8424){
        C = CEILIDIV((Np_info + 24), 8424);
        nr_tbs = (C << 3) * CEILIDIV((Np_info + 24), (C << 3)) - 24;
      }
      else
        nr_tbs = ((CEILIDIV((Np_info + 24), 8)) << 3) - 24;
    }
  }
  return nr_tbs;
}
