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

static int16_t sat16(int64_t v)
{
  return v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : (int16_t)v;
}

/* Layer l reaches TXRU n with c[n][l] = rot * sum_p w[beam_p][n] * W_pm[l][p]: the logical ports p are those of
 * the PDU, beam_p the beam of digital BF interface p, W_pm the precoding matrix (identity without one: layer l
 * on port l). The phase compensation of the symbol is a scalar common to all ports, so it is part of the
 * weight. Each layer is then added onto each TXRU in one pass, skipping zero weights: a beam table mapping a
 * port onto a single TXRU costs one pass per layer. prg_origin is the first RB of the PRG grid: unused until
 * per-PRG beams are supported, which nFAPI (NFAPI_MAX_NUM_PRGS) does not carry for now. */
void nr_dbt_beamform(PHY_VARS_gNB *gNB,
                     const nfapi_nr_tx_precoding_and_beamforming_t *pb,
                     const nfapi_nr_pm_pdu_t *pm,
                     int num_ports,
                     int num_layers,
                     int slot,
                     int symbol,
                     int prg_origin,
                     int rb_start,
                     int num_rb,
                     const c16_t *const in[])
{
  NR_gNB_COMMON *common = &gNB->common_vars;
  const NR_DL_FRAME_PARMS *fp = &gNB->frame_parms;
  AssertFatal(pb->num_prgs <= 1, "%d PRGs: per-PRG beams are not supported\n", pb->num_prgs);
  AssertFatal(num_ports <= NFAPI_MAX_NUM_BG_IF, "%d logical ports, at most %d digital BF interfaces\n", num_ports, NFAPI_MAX_NUM_BG_IF);
  if (pm)
    AssertFatal(pm->numLayers == num_layers && num_ports <= pm->num_ant_ports,
                "precoding matrix %d is for %d layers on %d ports, the PDU has %d layers on %d ports\n",
                pm->pm_idx,
                pm->numLayers,
                pm->num_ant_ports,
                num_layers,
                num_ports);
  else
    AssertFatal(num_layers <= num_ports, "%d layers on %d logical ports without precoding\n", num_layers, num_ports);
  DevAssert(symbol < fp->symbols_per_slot && rb_start >= 0 && rb_start + num_rb <= fp->N_RB_DL);

  const nfapi_nr_dig_beam_t *beam[num_ports];
  for (int p = 0; p < num_ports; p++)
    beam[p] = find_dig_beam(common, pb->prgs_list[0].dig_bf_interface_list[p].beam_idx);
  const bool rotate = gNB->phase_comp;
  const c16_t rot = rotate ? fp->symbol_rotation[0][(slot % fp->slots_per_subframe) * fp->symbols_per_slot + symbol] : (c16_t){0};
  const int offset = symbol * fp->ofdm_symbol_size + rb_start * NR_NB_SC_PER_RB;
  for (int n = 0; n < common->num_tx_bb; n++) {
    for (int l = 0; l < num_layers; l++) {
      c16_t c;
      if (pm) {
        int64_t re = 0, im = 0;
        for (int p = 0; p < num_ports; p++) {
          const c16_t w = dig_beam_weight(beam[p], n);
          const c16_t m = pm->weights[l][p];
          re += (int32_t)w.r * m.r - (int32_t)w.i * m.i;
          im += (int32_t)w.r * m.i + (int32_t)w.i * m.r;
        }
        c = (c16_t){sat16(re >> 15), sat16(im >> 15)};
      } else {
        c = dig_beam_weight(beam[l], n);
      }
      if (c.r == 0 && c.i == 0)
        continue;
      if (rotate)
        c = c16mulShift(c, rot, 15);
      rotate_add_cpx_vector(in[l], c, common->txdataF[n] + offset, num_rb * NR_NB_SC_PER_RB, 15);
    }
  }
}
