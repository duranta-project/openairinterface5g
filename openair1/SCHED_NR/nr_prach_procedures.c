/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Implementation of gNB prach procedures from 38.213 LTE specifications
 */

#include "PHY/defs_gNB.h"
#include "PHY/NR_TRANSPORT/nr_transport_proto.h"
#include "nfapi_nr_interface_scf.h"
#include "nfapi_pnf.h"
#include "common/utils/LOG/log.h"
#include "assertions.h"
#include "PHY/MODULATION/nr_modulation.h"
#include <time.h>

int get_nr_prach_duration(uint8_t prach_format)
{
  const int val[14] = {0, 0, 0, 0, 2, 4, 6, 2, 12, 2, 6, 2, 4, 6};
  AssertFatal(prach_format < sizeofArray(val), "Invalid Prach format %d\n", prach_format);
  return val[prach_format];
}

static void prach_occ_beamforming(const nfapi_nr_dbt_pdu_t *dbt,
                                  const nfapi_nr_ul_beamforming_t *b,
                                  int num_prach_occ,
                                  int nb_rx,
                                  int occ,
                                  int prach_len,
                                  c16_t prach_in[][NUMBER_OF_NR_RU_PRACH_OCCASIONS_MAX][NR_PRACH_SEQ_LEN_L])
{
  DevAssert(dbt);

  /* L2 associates each PRACH occasion to a beam in schedule_nr_prach(). If
  multiple beams in a FAPI PDU then nof occ must be same as nof beams. If single
  beam then all occ is assigned to one beam. */
  const uint16_t num_beams = b->dig_bf_interface;
  AssertFatal(num_beams == num_prach_occ || num_beams == 1, "Incompatible beam and PRACH occasion association\n");
  const uint16_t cur_beam = (num_beams == 1) ? 0 : occ;
  // Place each occasion in first log port
  const uint16_t beam_id = b->prgs_list[0].dig_bf_interface_list[cur_beam].beam_idx & 0x7fff; // FAPI beam id is LSB 15 bits

  c16_t prach_bf[NR_PRACH_SEQ_LEN_L] __attribute__((aligned(32)));
  memset(prach_bf, 0, sizeof(prach_bf));
  AssertFatal(beam_id == dbt->dig_beam_list[beam_id].beam_idx, "Beam id is not consistent with DBT\n");
  DevAssert(nb_rx == dbt->num_txrus);
  for (int b = 0; b < nb_rx; b++) {
    const c16_t wt = *(c16_t *)&dbt->dig_beam_list[beam_id].txru_list[b];
    nr_beamformer_simd(prach_in[b][occ], wt, prach_len, prach_bf);
  }
  // Copy to input buffer
  memcpy(prach_in[0][occ], prach_bf, sizeof(*prach_bf) * NR_PRACH_SEQ_LEN_L);
}

void L1_nr_prach_procedures(PHY_VARS_gNB *gNB, prach_item_t *prach_id, nfapi_nr_rach_indication_t *rach_ind)
{
  const frame_t frame = prach_id->frame;
  const slot_t slot = prach_id->slot;
  rach_ind->sfn = frame;
  rach_ind->slot = slot;
  nfapi_nr_prach_pdu_t *prach_pdu = &prach_id->pdu;
  LOG_D(NR_PHY_RACH, "%d.%d, prachstart slot %d prach entry occas %d\n", frame, slot, prach_id->slot, prach_pdu->num_prach_ocas);
  int N_dur = get_nr_prach_duration(prach_pdu->prach_format);

  nfapi_nr_ul_beamforming_t *b = &prach_pdu->beamforming;
  prach_id->is_bf = (b->dig_bf_interface > 0) ? (!IS_BIT_SET(b->prgs_list[0].dig_bf_interface_list[0].beam_idx, 15)) : false;

  for (int prach_oc = 0; prach_oc < prach_pdu->num_prach_ocas; prach_oc++) {
    // Beamforming
    if (prach_id->is_bf) {
      const int prach_len = ((prach_pdu->prach_format & 0xff) < 4) ? NR_PRACH_SEQ_LEN_L : NR_PRACH_SEQ_LEN_S;
      prach_occ_beamforming(&gNB->gNB_config.dbt_config,
                            b,
                            prach_pdu->num_prach_ocas,
                            gNB->frame_parms.nb_antennas_rx,
                            prach_oc,
                            prach_len,
                            prach_id->prach_buf);
    }
    uint prachStartSymbol = prach_pdu->prach_start_symbol + prach_oc * N_dur;
    // comment FK: the standard 38.211 section 5.3.2 has one extra term +14*N_RA_slot. This is because there prachStartSymbol is
    // given wrt to start of the 15kHz slot or 60kHz slot. Here we work slot based, so this function is anyway only called in slots
    // where there is PRACH. Its up to the MAC to schedule another PRACH PDU in the case there are there N_RA_slot \in {0,1}.
    rx_prach_out_t res = rx_nr_prach(prach_id, prach_oc);
    LOG_D(NR_PHY,
          "[RAPROC] Frame %d, slot %d, occasion %d (prachStartSymbol %d) : Most likely preamble %d, energy %d.%d dB delay %d "
          "(prach_energy counter %d)\n",
          frame,
          slot,
          prach_oc,
          prachStartSymbol,
          res.max_preamble,
          res.max_preamble_energy / 10,
          res.max_preamble_energy % 10,
          res.max_preamble_delay,
          gNB->prach_energy_counter);

    if ((gNB->prach_energy_counter == NUM_PRACH_RX_FOR_NOISE_ESTIMATE)
        && (res.max_preamble_energy > gNB->measurements.prach_I0 + gNB->prach_thres)
        && (rach_ind->number_of_pdus < MAX_NUM_NR_RX_RACH_PDUS)) {
      LOG_A(NR_PHY,
            "[RAPROC] %d.%d Initiating RA procedure with preamble %d, energy %d.%d dB (I0 %d, thres %d), delay %d start symbol "
            "%u freq index %u\n",
            frame,
            slot,
            res.max_preamble,
            res.max_preamble_energy / 10,
            res.max_preamble_energy % 10,
            gNB->measurements.prach_I0,
            gNB->prach_thres,
            res.max_preamble_delay,
            prachStartSymbol,
            prach_pdu->num_ra);

      T(T_ENB_PHY_INITIATE_RA_PROCEDURE,
        T_INT(gNB->Mod_id),
        T_INT(frame),
        T_INT(slot),
        T_INT(res.max_preamble),
        T_INT(res.max_preamble_energy),
        T_INT(res.max_preamble_delay));

      nfapi_nr_prach_indication_pdu_t *ind = rach_ind->pdu_list + rach_ind->number_of_pdus;
      *ind = (nfapi_nr_prach_indication_pdu_t){
          .phy_cell_id = gNB->gNB_config.cell_config.phy_cell_id.value,
          .symbol_index = prachStartSymbol,
          .slot_index = slot,
          .freq_index = prach_pdu->num_ra,
          .avg_rssi = (res.max_preamble_energy < 631) ? (128 + (res.max_preamble_energy / 5)) : 254,
          .avg_snr = 0xff, // invalid for now
          .num_preamble = 1,
          .preamble_list = {
              {.preamble_index = res.max_preamble, .timing_advance = res.max_preamble_delay, .preamble_pwr = 0xffffffff}}};
      rach_ind->number_of_pdus++;
    }
    gNB->measurements.prach_I0 = ((gNB->measurements.prach_I0 * 900) >> 10) + ((res.max_preamble_energy * 124) >> 10);
    if (frame == 0)
      LOG_I(PHY, "prach_I0 = %d.%d dB\n", gNB->measurements.prach_I0 / 10, gNB->measurements.prach_I0 % 10);
    if (gNB->prach_energy_counter < NUM_PRACH_RX_FOR_NOISE_ESTIMATE)
      gNB->prach_energy_counter++;
  } // if prach_id>0
  LOG_D(NR_PHY_RACH, "Freeing PRACH entry\n");
  free_nr_prach_entry(prach_id);
}
