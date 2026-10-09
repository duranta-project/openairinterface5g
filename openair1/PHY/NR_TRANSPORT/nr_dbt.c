/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_dbt.h"
#include "PHY/TOOLS/tools_defs.h"
#include "common/utils/assertions.h"
#include "common/utils/utils.h"

static const nfapi_nr_dig_beam_t *find_dig_beam(const NR_gNB_COMMON *common, uint16_t fapi_beam)
{
  AssertFatal(!IS_BIT_SET(fapi_beam, 15), "beam ID 0x%x is for the RU, but a digital beam table is configured\n", fapi_beam);
  AssertFatal(fapi_beam < NR_MAX_DBT_BEAM_IDX, "beam ID %u exceeds the supported maximum %d\n", fapi_beam, NR_MAX_DBT_BEAM_IDX - 1);
  const nfapi_nr_dig_beam_t *beam = common->dbt_lut[fapi_beam];
  AssertFatal(beam != NULL, "beam ID %u is not in the digital beam table\n", fapi_beam);
  return beam;
}

static c16_t dig_beam_weight(const nfapi_nr_dig_beam_t *beam, int txru)
{
  const nfapi_nr_txru_t *w = &beam->txru_list[txru];
  return (c16_t){.r = w->dig_beam_weight_Re, .i = w->dig_beam_weight_Im};
}

/* Each baseband port n gets w[beam][n] * in, the beam being the one of digital BF interface dig_bf_interface for
 * the whole PDU. The phase compensation of the symbol is a scalar common to all ports, so it is folded into the
 * weight. Zero weights are skipped, so a beam table mapping a port onto one baseband port costs one pass.
 * prg_origin is the first RB of the PRG grid: unused until per-PRG beams are supported, which nFAPI
 * (NFAPI_MAX_NUM_PRGS) does not carry for now. */
void nr_dbt_beamform(PHY_VARS_gNB *gNB,
                     const nfapi_nr_tx_precoding_and_beamforming_t *pb,
                     int dig_bf_interface,
                     int slot,
                     int symbol,
                     int prg_origin,
                     int rb_start,
                     int num_rb,
                     const c16_t *in)
{
  NR_gNB_COMMON *common = &gNB->common_vars;
  const NR_DL_FRAME_PARMS *fp = &gNB->frame_parms;
  AssertFatal(pb->num_prgs <= 1, "%d PRGs: per-PRG beams are not supported\n", pb->num_prgs);
  DevAssert(symbol < fp->symbols_per_slot && rb_start >= 0 && rb_start + num_rb <= fp->N_RB_DL);
  const nfapi_nr_dig_beam_t *beam = find_dig_beam(common, pb->prgs_list[0].dig_bf_interface_list[dig_bf_interface].beam_idx);
  const bool rotate = gNB->phase_comp;
  const c16_t rot = rotate ? fp->symbol_rotation[0][(slot % fp->slots_per_subframe) * fp->symbols_per_slot + symbol] : (c16_t){0};
  const int offset = symbol * fp->ofdm_symbol_size + rb_start * NR_NB_SC_PER_RB;
  for (int n = 0; n < common->num_tx_bb; n++) {
    c16_t w = dig_beam_weight(beam, n);
    if (w.r == 0 && w.i == 0)
      continue;
    if (rotate)
      w = c16mulShift(w, rot, 15);
    rotate_add_cpx_vector(in, w, common->txdataF[n] + offset, num_rb * NR_NB_SC_PER_RB, 15);
  }
}
