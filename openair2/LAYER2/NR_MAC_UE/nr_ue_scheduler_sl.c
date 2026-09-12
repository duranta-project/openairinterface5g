/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "mac_defs.h"

#include <stdio.h>
#include <math.h>
#include <pthread.h>

/* exe */
#include <common/utils/nr/nr_common.h>

/* MAC */
#include "NR_MAC_COMMON/nr_mac.h"
#include "NR_MAC_COMMON/nr_mac_common.h"
#include "NR_MAC_UE/mac_proto.h"
#include "NR_MAC_UE/nr_sl_slot_bitmap.h"

/* utils */
#include "assertions.h"
#include "oai_asn1.h"
#include "SIMULATION/TOOLS/sim.h" // for taus
#include "utils.h"

#include <executables/softmodem-common.h>
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.h"

#include "RRC/NR_UE/L2_interface_ue.h"

extern const int pscch_tda[2];
extern const int pscch_rb_table[5];
//#define SRS_DEBUG
#define SLOT_INFO_DEBUG
#define BITMAP_DEBUG

static void print_candidate_list(List_t *candidate_resources, int line) {
  for (int i = 0; i < candidate_resources->size; i++) {
    sl_resource_info_t *itr_rsrc = (sl_resource_info_t*)((char*)candidate_resources->data + i * candidate_resources->element_size);
    LOG_D(NR_MAC, "line %d, %4d.%2d, %ld, sl_subchan_len %d\n", line, itr_rsrc->sfn.frame, itr_rsrc->sfn.slot, normalize(&itr_rsrc->sfn, 1), itr_rsrc->sl_subchan_len);
  }
}

static void print_reserved_list(List_t *candidate_resources, int line) {
  for (int i = 0; i < candidate_resources->size; i++) {
    reserved_resource_t *itr_rsrc = (reserved_resource_t*)((char*)candidate_resources->data + i * candidate_resources->element_size);
    LOG_D(NR_MAC, "line %d, %4d.%2d, %ld, sl_subchan_len %d\n", line, itr_rsrc->sfn.frame, itr_rsrc->sfn.slot, normalize(&itr_rsrc->sfn, 1), itr_rsrc->sb_ch_length);
  }
}

static void print_sensing_data_list(List_t *sensing_data, int line) {
  for (int i = 0; i < sensing_data->size; i++) {
    sensing_data_t *itr_rsrc = (sensing_data_t*)((char*)sensing_data->data + i * sensing_data->element_size);
    LOG_D(NR_MAC, "line %d, %4d.%2d, %ld, sl_subchan_len %d\n", line, itr_rsrc->frame_slot.frame, itr_rsrc->frame_slot.slot, normalize(&itr_rsrc->frame_slot, 1), itr_rsrc->subch_len);
  }
}

static uint16_t sl_adjust_ssb_indices(sl_ssb_timealloc_t *ssb_timealloc, uint32_t slot_in_16frames, uint16_t *ssb_slot_ptr)
{
  uint16_t ssb_slot = ssb_timealloc->sl_TimeOffsetSSB;
  uint16_t numssb = 0;
  *ssb_slot_ptr = 0;

  if (ssb_timealloc->sl_NumSSB_WithinPeriod == 0) {
    *ssb_slot_ptr = 0;
    return 0;
  }

  while (slot_in_16frames > ssb_slot) {
    numssb = numssb + 1;
    if (numssb < ssb_timealloc->sl_NumSSB_WithinPeriod)
      ssb_slot = ssb_slot + ssb_timealloc->sl_TimeInterval;
    else
      break;
  }

  *ssb_slot_ptr = ssb_slot;

  return numssb;
}

static uint8_t sl_get_elapsed_slots(uint32_t slot, uint32_t sl_slot_bitmap)
{
  uint8_t elapsed_slots = 0;

  for (int i = 0; i < slot; i++) {
    if (sl_slot_bitmap & (1 << i))
      elapsed_slots++;
  }

  return elapsed_slots;
}

/*
 * This function determines if the mixed slot is a Sidelink slot
 */
static uint8_t sl_determine_if_sidelink_slot(uint8_t sl_startsym, uint8_t sl_lensym, uint8_t num_ulsym)
{
  uint8_t ul_startsym = NR_SYMBOLS_PER_SLOT - num_ulsym;

  if ((sl_startsym >= ul_startsym) && (sl_lensym <= NR_SYMBOLS_PER_SLOT)) {
    LOG_D(NR_MAC,
          "MIXED SLOT is a SIDELINK SLOT. Sidelink Symbols: %d-%d, Uplink Symbols: %d-%d\n",
          sl_startsym,
          sl_lensym - 1,
          ul_startsym,
          ul_startsym + num_ulsym - 1);
    return NR_SIDELINK_SLOT;
  } else {
    LOG_D(NR_MAC,
          "MIXED SLOT is NOT SIDELINK SLOT. Sidelink Symbols: %d-%d, Uplink Symbols: %d-%d\n",
          sl_startsym,
          sl_lensym - 1,
          ul_startsym,
          ul_startsym + num_ulsym - 1);
    return 0;
  }
}

int get_bit_from_map(const uint8_t *buf, size_t bit_pos) {
  size_t byte_index = bit_pos / 8;
  uint8_t bit_index = bit_pos % 8;
  LOG_D(NR_MAC, "buf[%ld] = %d, ((7 - %d)) & 1), (buf[byte_index] >> %d) = %d  %d\n",
        byte_index, buf[byte_index], bit_index, (7 - bit_index), buf[byte_index] >> (7 - bit_index), (buf[byte_index] >> (7 - bit_index)) & 1);
  return (buf[byte_index] >> (7 - bit_index)) & 1;
}

void append_bit(uint8_t *buf, size_t bit_pos, int bit_value) {
  size_t byte_index = bit_pos / 8;
  uint8_t bit_index = bit_pos % 8;
  LOG_D(NR_MAC, "Appending bit_value %d at byte_index %ld bit index %d\n", bit_value, byte_index, bit_index);
  if (bit_value) {
    buf[byte_index] |= (1 << (7 - bit_index));
  } else {
    buf[byte_index] &= ~(1 << (7 - bit_index));
  }
}

bool check_t1_within_tproc1(uint8_t mu, uint16_t t1_slots) {
    if ((mu == 0 && t1_slots <= 3) || (mu == 1 && t1_slots <= 5) ||
        (mu == 2 && t1_slots <= 9) || (mu == 3 && t1_slots <= 17))
    {
        return true;
    }
    return false;
}

void remove_old_sensing_data(frameslot_t *frame_slot,
                             uint16_t sensing_window,
                             List_t* sensing_data,
                             sl_nr_ue_mac_params_t *sl_mac) {

  int new_size = 0;
  int mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  for (int i = 0; i < sensing_data->size; i++) {
    sensing_data_t *data = (sensing_data_t*)((char*)sensing_data->data + i * sensing_data->element_size);
    LOG_D(NR_MAC, " i %d, old (%4d.%2d) %ld >=  current (%4d.%2d) %ld staled data slots %ld\n",
          i,
          data->frame_slot.frame,
          data->frame_slot.slot,
          normalize(&data->frame_slot, mu),
          frame_slot->frame,
          frame_slot->slot,
          normalize(frame_slot, mu),
          normalize(frame_slot, mu) - sensing_window);

    int64_t num_max_slots = nr_slots_per_frame[mu] * 1024;
    int64_t diff = (normalize(frame_slot, mu) - normalize(&data->frame_slot, mu) + num_max_slots) % num_max_slots;
    if (diff <= sensing_window) {
      break;
    } else {
      new_size ++;
    }
  }
  if (new_size > 0) {
    LOG_D(NR_MAC, "sensing data: size %ld, element_size %ld new_size %d\n", sensing_data->size, sensing_data->element_size, new_size);
    memmove(sensing_data->data, (char*)sensing_data->data + new_size * sensing_data->element_size, (sensing_data->size - new_size) * sensing_data->element_size);
    LOG_D(NR_MAC, "Subtracting %d from %ld\n", new_size, sensing_data->size);
    sensing_data->size -= new_size;
  }
}

/*
 * This function determines if the Slot is a SIDELINK SLOT
 * Every Uplink Slot is a Sidelink slot
 * Mixed Slot is a sidelink slot if the uplink symbols in Mixed slot
 * overlaps with Sidelink start symbol and number of symbols.
 */
int sl_nr_ue_slot_select(const sl_nr_phy_config_request_t *cfg, int slot, uint8_t frame_duplex_type)
{
  int ul_sym = 0, slot_type = 0;

  // All PC5 bands are TDD bands , hence handling only TDD in this function.
  AssertFatal(frame_duplex_type == TDD, "No Sidelink operation defined for FDD in 3GPP rel16\n");

  if (cfg->tdd_table.max_tdd_periodicity_list == NULL) { // this happens before receiving TDD configuration
    return slot_type;
  }

  int period = cfg->tdd_table.tdd_period_in_slots;
  int rel_slot = slot % period;
  const fapi_nr_tdd_table_t *tdd_table = &cfg->tdd_table;

  const fapi_nr_max_tdd_periodicity_t *current_slot = &tdd_table->max_tdd_periodicity_list[rel_slot];

  for (int symbol_count = 0; symbol_count < NR_SYMBOLS_PER_SLOT; symbol_count++) {
    if (current_slot->max_num_of_symbol_per_slot_list[symbol_count].slot_config == 1) {
      ul_sym++;
    }
  }

  if (ul_sym == NR_SYMBOLS_PER_SLOT) {
    slot_type = NR_SIDELINK_SLOT;
  } else if (ul_sym) {
    slot_type = sl_determine_if_sidelink_slot(cfg->sl_bwp_config.sl_start_symbol, cfg->sl_bwp_config.sl_num_symbols, ul_sym);
  }

  return slot_type;
}

static void sl_determine_slot_bitmap(sl_nr_ue_mac_params_t *sl_mac, int ue_id)
{

  sl_nr_phy_config_request_t *sl_cfg = &sl_mac->sl_phy_config.sl_config_req;

  uint8_t sl_scs = sl_cfg->sl_bwp_config.sl_scs;
  uint8_t num_slots_per_frame = 10 * (1 << sl_scs);
  uint8_t slot_type = 0;
  for (int i = 0; i < num_slots_per_frame; i++) {
    slot_type = sl_nr_ue_slot_select(sl_cfg, i, TDD);
    if (slot_type == NR_SIDELINK_SLOT) {
      sl_mac->N_SL_SLOTS_perframe += 1;
      sl_mac->sl_slot_bitmap |= (1 << i);
    }
  }

  sl_mac->future_ttis = calloc(num_slots_per_frame, sizeof(sl_stored_tti_req_t));

  LOG_I(NR_MAC,
        "[UE%d] SL-MAC: N_SL_SLOTS_perframe:%d, SL SLOT bitmap:%x\n",
        ue_id,
        sl_mac->N_SL_SLOTS_perframe,
        sl_mac->sl_slot_bitmap);
}

/* This function determines the number of sidelink slots in 1024 frames - DFN cycle
 * which can be used for determining reserved slots and REsource pool slots according to bitmap.
 * Sidelink slots are the uplink and mixed slots with sidelink support except the SSB slots.
 */
static uint32_t sl_determine_num_sidelink_slots(sl_nr_ue_mac_params_t *sl_mac, int ue_id, uint16_t *N_SSB_16frames)
{

  uint32_t N_SSB_1024frames = 0;
  uint32_t N_SL_SLOTS = 0;
  *N_SSB_16frames = 0;

  if (sl_mac->rx_sl_bch.status) {
    sl_ssb_timealloc_t *ssb_timealloc = &sl_mac->rx_sl_bch.ssb_time_alloc;
    *N_SSB_16frames += ssb_timealloc->sl_NumSSB_WithinPeriod;
    LOG_D(NR_MAC, "RX SSB Slots:%d\n", *N_SSB_16frames);
  }

  if (sl_mac->tx_sl_bch.status) {
    sl_ssb_timealloc_t *ssb_timealloc = &sl_mac->tx_sl_bch.ssb_time_alloc;
    *N_SSB_16frames += ssb_timealloc->sl_NumSSB_WithinPeriod;
    LOG_D(NR_MAC, "TX SSB Slots:%d\n", *N_SSB_16frames);
  }

  // Total SSB slots in SFN cycle (1024 frames)
  N_SSB_1024frames = SL_FRAME_NUMBER_CYCLE / SL_NR_SSB_REPETITION_IN_FRAMES * (*N_SSB_16frames);

  // Determine total number of Valid Sidelink slots which can be used for Respool in a SFN cycle (1024 frames)
  N_SL_SLOTS = (sl_mac->N_SL_SLOTS_perframe * SL_FRAME_NUMBER_CYCLE) - N_SSB_1024frames;

  LOG_I(NR_MAC,
        "[UE%d]SL-MAC:SSB slots in 1024 frames:%d, N_SL_SLOTS_perframe:%d, N_SL_SLOTs in 1024 frames:%d, SL SLOT bitmap:%x\n",
        ue_id,
        N_SSB_1024frames,
        sl_mac->N_SL_SLOTS_perframe,
        N_SL_SLOTS,
        sl_mac->sl_slot_bitmap);

  return N_SL_SLOTS;
}

/**
 * DETERMINE IF SLOT IS MARKED AS SSB SLOT
 * ACCORDING TO THE SSB TIME ALLOCATION PARAMETERS.
 * sl_numSSB_withinPeriod - NUM SSBS in 16frames
 * sl_timeoffset_SSB - time offset for first SSB at start of 16 frames cycle
 * sl_timeinterval - distance in slots between 2 SSBs
 */
uint8_t sl_determine_if_SSB_slot(uint16_t frame, uint16_t slot, uint16_t slots_per_frame, sl_bch_params_t *sl_bch)
{
  uint16_t frame_16 = frame % SL_NR_SSB_REPETITION_IN_FRAMES;
  uint32_t slot_in_16frames = (frame_16 * slots_per_frame) + slot;
  uint16_t sl_NumSSB_WithinPeriod = sl_bch->ssb_time_alloc.sl_NumSSB_WithinPeriod;
  uint16_t sl_TimeOffsetSSB = sl_bch->ssb_time_alloc.sl_TimeOffsetSSB;
  uint16_t sl_TimeInterval = sl_bch->ssb_time_alloc.sl_TimeInterval;
  uint16_t num_ssb = sl_bch->num_ssb, ssb_slot = sl_bch->ssb_slot;

#ifdef SL_DEBUG
  LOG_D(NR_MAC,
        "%d:%d. num_ssb:%d,ssb_slot:%d, %d-%d-%d, status:%d\n",
        frame,
        slot,
        sl_bch->num_ssb,
        sl_bch->ssb_slot,
        sl_NumSSB_WithinPeriod,
        sl_TimeOffsetSSB,
        sl_TimeInterval,
        sl_bch->status);
#endif

  if (sl_NumSSB_WithinPeriod && sl_bch->status) {
    if (slot_in_16frames == sl_TimeOffsetSSB) {
      num_ssb = 0;
      ssb_slot = sl_TimeOffsetSSB;
    }

    if (num_ssb < sl_NumSSB_WithinPeriod && slot_in_16frames == ssb_slot) {
      num_ssb += 1;
      ssb_slot = (num_ssb < sl_NumSSB_WithinPeriod) ? (ssb_slot + sl_TimeInterval) : sl_TimeOffsetSSB;

      sl_bch->ssb_slot = ssb_slot;
      sl_bch->num_ssb = num_ssb;

      LOG_D(NR_MAC, "%d:%d is a PSBCH SLOT. Next PSBCH Slot:%d, num_ssb:%d\n", frame, slot, sl_bch->ssb_slot, sl_bch->num_ssb);

      return 1;
    }
  }

  LOG_D(NR_MAC, "%d:%d is NOT a PSBCH SLOT. Next PSBCH Slot:%d, num_ssb:%d\n", frame, slot, sl_bch->ssb_slot, sl_bch->num_ssb);
  return 0;
}

static uint8_t sl_psbch_scheduler(sl_nr_ue_mac_params_t *sl_mac_params, int ue_id, int frame, int slot, int slots_per_frame)
{
  uint8_t config_type = 0, is_psbch_rx_slot = 0, is_psbch_tx_slot = 0;
  if (sl_mac_params->rx_sl_bch.status) {
    is_psbch_rx_slot = sl_determine_if_SSB_slot(frame, slot, slots_per_frame, &sl_mac_params->rx_sl_bch);

    if (is_psbch_rx_slot)
      config_type = SL_NR_CONFIG_TYPE_RX_PSBCH;

  } else if (sl_mac_params->tx_sl_bch.status) {
    is_psbch_tx_slot = sl_determine_if_SSB_slot(frame, slot, slots_per_frame, &sl_mac_params->tx_sl_bch);

    if (is_psbch_tx_slot)
      config_type = SL_NR_CONFIG_TYPE_TX_PSBCH;
  }

  sl_mac_params->future_ttis[slot].frame = frame;
  sl_mac_params->future_ttis[slot].slot = slot;
  sl_mac_params->future_ttis[slot].sl_action = config_type;

  LOG_D(NR_MAC, "[UE%d] SL-PSBCH SCHEDULER: %d:%d, config type:%d\n", ue_id, frame, slot, config_type);
  return config_type;
}

/*
 * This function calculates the indices based on the new timing (frame,slot)
 * acquired by the UE.
 * NUM SSB, SLOT_SSB needs to be calculated based on current timing
 */
static void sl_adjust_indices_based_on_timing(sl_nr_ue_mac_params_t *sl_mac,
                                              int ue_id,
                                              int frame, int slot,
                                              int slots_per_frame)
{

  uint8_t elapsed_slots = 0;

  elapsed_slots = sl_get_elapsed_slots(slot, sl_mac->sl_slot_bitmap);
  AssertFatal(elapsed_slots <= sl_mac->N_SL_SLOTS_perframe,
              "Elapsed slots cannot be > N_SL_SLOTS_perframe %d,%d\n",
              elapsed_slots,
              sl_mac->N_SL_SLOTS_perframe);

  uint16_t frame_16 = frame % SL_NR_SSB_REPETITION_IN_FRAMES;
  uint32_t slot_in_16frames = (frame_16 * slots_per_frame) + slot;
  LOG_I(NR_MAC,
        "[UE%d]PSBCH params adjusted based on current timing %d:%d. frame_16:%d, slot_in_16frames:%d\n",
        ue_id,
        frame,
        slot,
        frame_16,
        slot_in_16frames);

  // Adjust PSBCH Indices based on current timing
  if (sl_mac->rx_sl_bch.status) {
    sl_ssb_timealloc_t *ssb_timealloc = &sl_mac->rx_sl_bch.ssb_time_alloc;
    sl_mac->rx_sl_bch.num_ssb = sl_adjust_ssb_indices(ssb_timealloc, slot_in_16frames, &sl_mac->rx_sl_bch.ssb_slot);

    LOG_I(NR_MAC,
          "[UE%d]PSBCH RX params adjusted. NumSSB:%d, ssb_slot:%d\n",
          ue_id,
          sl_mac->rx_sl_bch.num_ssb,
          sl_mac->rx_sl_bch.ssb_slot);
  }

  if (sl_mac->tx_sl_bch.status) {
    sl_ssb_timealloc_t *ssb_timealloc = &sl_mac->tx_sl_bch.ssb_time_alloc;
    sl_mac->tx_sl_bch.num_ssb = sl_adjust_ssb_indices(ssb_timealloc, slot_in_16frames, &sl_mac->tx_sl_bch.ssb_slot);

    LOG_I(NR_MAC,
          "[UE%d]PSBCH TX params adjusted. NumSSB:%d, ssb_slot:%d\n",
          ue_id,
          sl_mac->tx_sl_bch.num_ssb,
          sl_mac->tx_sl_bch.ssb_slot);
  }
}

// Adjust indices as new timing is acquired
static void sl_actions_after_new_timing(sl_nr_ue_mac_params_t *sl_mac, int ue_id, int frame, int slot, int slots_per_frame)
{
  sl_determine_slot_bitmap(sl_mac, ue_id);
  sl_mac->N_SL_SLOTS = sl_determine_num_sidelink_slots(sl_mac, ue_id, &sl_mac->N_SSB_16frames);
  sl_adjust_indices_based_on_timing(sl_mac, ue_id, frame, slot, slots_per_frame);
}

static void sl_schedule_rx_actions(nr_sidelink_indication_t *sl_ind, NR_UE_MAC_INST_t *mac)
{

  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  int ue_id = mac->ue_id;
  int rx_action = 0;

  sl_nr_rx_config_request_t rx_config = {0};
  rx_config.number_pdus = 0;
  rx_config.sfn = sl_ind->frame_rx;
  rx_config.slot = sl_ind->slot_rx;

  if (sl_ind->sci_ind != NULL) {
    AssertFatal(false, "sci_inds not handled\n");
  } else {
    rx_action = sl_mac->future_ttis[sl_ind->slot_rx].sl_action;
  }

  if (rx_action == SL_NR_CONFIG_TYPE_RX_PSBCH) {
    rx_config.number_pdus = 1;
    rx_config.sl_rx_config_list[0].pdu_type = rx_action;

    LOG_D(NR_MAC, "[UE%d] %d:%d CMD to PHY: RX PSBCH \n", ue_id, sl_ind->frame_rx, sl_ind->slot_rx);

  } else if (rx_action >= SL_NR_CONFIG_TYPE_RX_PSCCH && rx_action <= SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH) {
    AssertFatal(false, "RX_PSCCH/SLSCH not handled\n");

  } else if (rx_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH_PSFCH) {
    AssertFatal(false, "RX_PSCCH/SLSCH/PSFCH not handled\n");
  }

  if (rx_config.number_pdus) {
    AssertFatal(sl_ind->slot_type == SIDELINK_SLOT_TYPE_RX || sl_ind->slot_type == SIDELINK_SLOT_TYPE_BOTH,
                "RX action cannot be scheduled in non Sidelink RX slot\n");

    nr_scheduled_response_t scheduled_response = {.sl_rx_config = &rx_config,
                                                  .module_id = sl_ind->module_id,
                                                  .phy_data = sl_ind->phy_data,
                                                  .mac = mac};

    sl_mac->future_ttis[sl_ind->slot_rx].sl_action = 0;

    if ((mac->if_module != NULL) && (mac->if_module->scheduled_response != NULL))
      mac->if_module->scheduled_response(&scheduled_response);
  }
}

static void sl_schedule_tx_actions(nr_sidelink_indication_t *sl_ind, NR_UE_MAC_INST_t *mac)
{

  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  int ue_id = mac->ue_id;

  int tx_action = 0;
  sl_nr_tx_config_request_t tx_config;
  tx_config.number_pdus = 0;
  tx_config.sfn = sl_ind->frame_tx;
  tx_config.slot = sl_ind->slot_tx;

  tx_action = sl_mac->future_ttis[sl_ind->slot_tx].sl_action;

  if (tx_action == SL_NR_CONFIG_TYPE_TX_PSBCH) {
    tx_config.number_pdus = 1;
    tx_config.tx_config_list[0].pdu_type = tx_action;
    tx_config.tx_config_list[0].tx_psbch_config_pdu.tx_slss_id = sl_mac->tx_sl_bch.slss_id;
    tx_config.tx_config_list[0].tx_psbch_config_pdu.psbch_tx_power = 0; // TBD...
    memcpy(tx_config.tx_config_list[0].tx_psbch_config_pdu.psbch_payload, sl_mac->tx_sl_bch.sl_mib, 4);

    LOG_D(NR_MAC, "[UE%d] %d:%d CMD to PHY: TX PSBCH \n", ue_id, sl_ind->frame_tx, sl_ind->slot_tx);

  } else if (tx_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH) {
    AssertFatal(false, "SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH\n");

  } else if (tx_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH) {
    AssertFatal(false, "SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH\n");
  }

  if (tx_config.number_pdus == 1) {
    AssertFatal(sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX || sl_ind->slot_type == SIDELINK_SLOT_TYPE_BOTH,
                "TX action cannot be scheduled in non Sidelink TX slot\n");

    nr_scheduled_response_t scheduled_response = {.sl_tx_config = &tx_config,
                                                  .module_id = sl_ind->module_id,
                                                  .phy_data = sl_ind->phy_data,
                                                  .mac = mac};

    sl_mac->future_ttis[sl_ind->slot_tx].sl_action = 0;

    if ((mac->if_module != NULL) && (mac->if_module->scheduled_response != NULL))
      mac->if_module->scheduled_response(&scheduled_response);
  }
}

NR_SL_ResourcePool_r16_t* get_resource_pool(NR_UE_MAC_INST_t *mac, uint16_t pool_id) {
  return mac->SL_MAC_PARAMS->sl_TxPool[pool_id]->respool;
}

/* Maps an absolute slot index to its position within the physical SL bitmap. */
size_t sl_abs_slot_to_bit_pos(uint64_t abs_slot, size_t phy_map_sz)
{
  return (size_t)(abs_slot % phy_map_sz);
}

/* Return the global ordinal k of a resource-pool slot (TS 38.213 16.3), or -1
 * when abs_slot is not in the pool.  Unlike a bitmap-local index, k continues
 * across bitmap repetitions, so the PSFCH phase is not restarted every cycle. */
static int64_t sl_pool_slot_ordinal(const BIT_STRING_t *phy_sl_bitmap, size_t phy_map_sz, uint64_t abs_slot)
{
  if (phy_map_sz == 0)
    return -1;
  const size_t bit_pos = sl_abs_slot_to_bit_pos(abs_slot, phy_map_sz);
  if (!get_bit_from_map(phy_sl_bitmap->buf, bit_pos))
    return -1;
  size_t slots_per_cycle = 0;
  size_t slots_before = 0;
  for (size_t p = 0; p < phy_map_sz; p++) {
    if (get_bit_from_map(phy_sl_bitmap->buf, p)) {
      slots_per_cycle++;
      if (p < bit_pos)
        slots_before++;
    }
  }
  return (int64_t)((abs_slot / phy_map_sz) * slots_per_cycle + slots_before);
}

/* TS 38.213 16.3 uses the zero-based resource-pool slot index k for the
 * PSFCH-period test: t'^SL_k carries PSFCH when k mod N_PSSCH^PSFCH is zero. */
bool sl_slot_carries_psfch(const BIT_STRING_t *phy_sl_bitmap, size_t phy_map_sz,
                           uint64_t abs_slot, uint8_t psfch_period)
{
  if (psfch_period == 0)
    return false;
  const int64_t k = sl_pool_slot_ordinal(phy_sl_bitmap, phy_map_sz, abs_slot);
  return k >= 0 && (k % psfch_period) == 0;
}

/* Find the first PSFCH occasion at least sl-MinTimeGapPSFCH resource-pool slots
 * after the PSSCH slot, as required by TS 38.213 16.3. */
int64_t get_feedback_abs_slot(const BIT_STRING_t *phy_sl_bitmap, size_t phy_map_sz,
                              uint64_t tx_abs_slot, uint8_t min_time_gap, uint8_t psfch_period)
{
  if (psfch_period == 0 || phy_map_sz == 0)
    return -1;
  size_t slots_per_cycle = 0;
  for (size_t p = 0; p < phy_map_sz; p++)
    slots_per_cycle += get_bit_from_map(phy_sl_bitmap->buf, p) != 0;
  if (slots_per_cycle == 0)
    return -1;
  const size_t cycles = 1 + (min_time_gap + psfch_period) / slots_per_cycle;
  size_t pool_slots_after_pssch = 0;
  for (size_t i = 1; i <= cycles * phy_map_sz; i++) {
    const uint64_t cand = tx_abs_slot + i;
    if (sl_pool_slot_ordinal(phy_sl_bitmap, phy_map_sz, cand) < 0)
      continue;
    pool_slots_after_pssch++;
    if (pool_slots_after_pssch >= min_time_gap
        && sl_slot_carries_psfch(phy_sl_bitmap, phy_map_sz, cand, psfch_period))
      return (int64_t)cand;
  }
  return -1;
}

/* Returns the ordinal position (0-based) of the PSSCH slot within the group of
 * psfch_period consecutive PSSCH slots that share one PSFCH occasion (TS 38.213 s16.3).
 * Counts the SL slots that precede abs_slot in the pool bitmap cycle; the result modulo
 * psfch_period gives the per-slot PRB row index.  Uses pool-cycle SL-slot ordinal, not
 * raw slot-in-frame, so it works for any SL bitmap layout. */
int sl_psfch_pssch_slot_index(const BIT_STRING_t *phy_sl_bitmap, size_t phy_map_sz,
                              uint64_t abs_slot, uint8_t psfch_period)
{
  if (psfch_period == 0 || phy_map_sz == 0)
    return 0;
  const int64_t k = sl_pool_slot_ordinal(phy_sl_bitmap, phy_map_sz, abs_slot);
  return k >= 0 ? (int)(k % psfch_period) : 0;
}

bool slot_has_psfch(NR_UE_MAC_INST_t *mac, BIT_STRING_t *phy_sl_bitmap, uint64_t abs_index_cur_slot, uint8_t psfch_period, size_t phy_sl_map_size, NR_TDD_UL_DL_ConfigCommon_t *conf) {
  (void)mac;
  (void)conf;
  if (psfch_period == 0)
    return false;
  const size_t bit_pos = sl_abs_slot_to_bit_pos(abs_index_cur_slot, phy_sl_map_size);
  const bool has_psfch =
      sl_slot_carries_psfch(phy_sl_bitmap, phy_sl_map_size, abs_index_cur_slot, psfch_period);
  LOG_D(NR_MAC,
        "has_psfch %d abs_slot %lu bit_pos %zu psfch_period %u\n",
        has_psfch,
        abs_index_cur_slot,
        bit_pos,
        psfch_period);
  return has_psfch;
}

void validate_selected_sl_slot(bool tx, bool rx, NR_TDD_UL_DL_ConfigCommon_t *conf, frameslot_t frame_slot)
{
  AssertFatal(tx != rx, "Validate exactly one sidelink direction\n");
  AssertFatal(conf != NULL, "Missing sidelink TDD configuration\n");
  NR_UE_MAC_INST_t *mac = get_mac_inst(0);
  AssertFatal(mac != NULL && mac->SL_MAC_PARAMS != NULL, "Missing sidelink MAC instance\n");
  SL_ResourcePool_params_t *pool = tx ? mac->SL_MAC_PARAMS->sl_TxPool[0] : mac->SL_MAC_PARAMS->sl_RxPool[0];
  AssertFatal(pool != NULL && pool->phy_sl_bitmap.buf != NULL, "Missing physical sidelink pool bitmap\n");
  const size_t map_size = (pool->phy_sl_bitmap.size << 3) - pool->phy_sl_bitmap.bits_unused;
  const uint64_t abs_slot = normalize(&frame_slot, get_softmodem_params()->numerology);
  AssertFatal(is_sl_slot(mac, &pool->phy_sl_bitmap, map_size, abs_slot),
              "Selected slot %u.%u is not assigned to the configured sidelink resource pool\n",
              frame_slot.frame,
              frame_slot.slot);
}

bool is_sl_slot(NR_UE_MAC_INST_t *mac, BIT_STRING_t *phy_sl_bitmap, size_t phy_map_sz, uint64_t abs_slot) {
  (void)mac;
  return phy_map_sz > 0 && get_bit_from_map(phy_sl_bitmap->buf, abs_slot % phy_map_sz);
}

// TODO duplicates nr_update_rlc_buffers_status()
static void nr_store_slsch_buffer(NR_UE_MAC_INST_t *mac, frame_t frame, sub_frame_t slot) {

  NR_SL_UEs_t *UE_info = &mac->sl_info;
  SL_UE_iterator(UE_info->list, UE) {
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    sched_ctrl->num_total_bytes = 0;
    sched_ctrl->sl_pdus_total = 0;

    const int lcid = 4;
    logical_chan_id_t ch = lcid;
    mac_rlc_status_resp_t ret = {0};
    nr_mac_rlc_status_ind(mac->ue_id, frame, 1, &ch, &ret);
    
    sched_ctrl->rlc_status[lcid] = ret;
    if (sched_ctrl->rlc_status[lcid].bytes_in_buffer == 0)
        continue;

    sched_ctrl->sl_pdus_total += sched_ctrl->rlc_status[lcid].pdus_in_buffer;
    sched_ctrl->num_total_bytes += sched_ctrl->rlc_status[lcid].bytes_in_buffer;
    LOG_D(NR_MAC,
          "[%4d.%2d] SLSCH, RLC status for UE: %d bytes in buffer, total DL buffer size = %d bytes, %d total PDU bytes\n",
          frame,
          slot,
          sched_ctrl->rlc_status[lcid].bytes_in_buffer,
          sched_ctrl->num_total_bytes,
          sched_ctrl->sl_pdus_total);
  }
}

void preprocess(NR_UE_MAC_INST_t *mac,
                uint16_t frame,
                uint16_t slot,
                uint16_t rx_frame,
                uint16_t rx_slot,
                int *fb_frame,
                int *fb_slot,
                const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                NR_SetupRelease_SL_PSFCH_Config_r16_t *configured_PSFCH,
                long psfch_period)
{
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  int scs = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  const int nr_slots_frame = mac->frame_structure.numb_slots_frame;
  
  NR_SL_UEs_t *UE_info = &mac->sl_info;
  SL_UE_iterator(UE_info->list, UE) {
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    UE->mac_sl_stats.sl.current_bytes = 0;
    UE->mac_sl_stats.sl.current_rbs = 0;
    /* Advance stale feedback HARQs to retransmission (or abort at max rounds)
     * BEFORE checking available/retrans lists.  Without this, a UE with all 16
     * HARQ processes in feedback_sl_harq and no PSFCH reply (peer gone / DTX)
     * hits the continue below and never calls update_harq_lists() -- leaving
     * every process frozen until an explicit PSFCH is received.
     * Calling it here unconditionally breaks that deadlock: stale entries are
     * promoted to retrans_sl_harq, retransmissions proceed, and after
     * HARQ_ROUND_MAX rounds abort_nr_ue_sl_harq() releases them back to
     * available_sl_harq so new transmissions can resume. */
    if (configured_PSFCH) {
      /* frame/slot identify the look-ahead TX opportunity, while PSFCH is
       * received on the current RX time line.  Expiring against TX time can
       * move a HARQ to retrans_sl_harq before PHY reports feedback from the
       * current RX slot, making a correctly decoded PSFCH look stale. */
      update_harq_lists(mac, rx_frame, rx_slot, UE);
    }
    NR_sched_pssch_t *sched_pssch = &sched_ctrl->sched_pssch;
    sched_pssch->sl_harq_pid = configured_PSFCH ? sched_ctrl->retrans_sl_harq.head : -1;

    /* retransmission */
    if (sched_pssch->sl_harq_pid >= 0) {
      /* A retransmission owns its HARQ process already.  Requiring an
       * additional free process here stalls the entire queue precisely when
       * all processes are waiting for, or recovering from, HARQ feedback. */
    } else {
      if (sched_ctrl->available_sl_harq.head < 0) {
        LOG_W(NR_MAC, "[UE][%4d.%2d] UE has no free SL HARQ process, skipping\n",
              frame,
              slot);
        continue;
      }
      /* PSFCH is scheduled independently below; do not create a padding-only
       * PSSCH (and consume a HARQ process) merely because feedback is due. */
      if (sched_ctrl->num_total_bytes == 0)
          continue;
    }

    /*
    * SLSCH tx computes feedback frame and slot, which will be used by transmitter of PSFCH after receiving SLSCH.
    * Transmitter of SLSCH stores the feedback frame and slot in harq process to use those in retreiving the feedback.
    */
    if (configured_PSFCH) {
      NR_SL_PSFCH_Config_r16_t *sl_psfch_config = mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup;

      int rcv_tx_frame = frame;
      int rcv_tx_slot = slot;
      const uint8_t psfch_time_gaps[] = {2, 3};
      uint8_t min_tg = sl_psfch_config->sl_MinTimeGapPSFCH_r16
                           ? psfch_time_gaps[*sl_psfch_config->sl_MinTimeGapPSFCH_r16] : 2;
      uint64_t rcv_tx_abs = (uint64_t)rcv_tx_frame * nr_slots_frame + rcv_tx_slot;
      SL_ResourcePool_params_t *sl_tx_pool_p = sl_mac->sl_TxPool[0];
      size_t tx_pool_map_sz = (sl_tx_pool_p->phy_sl_bitmap.size << 3) - sl_tx_pool_p->phy_sl_bitmap.bits_unused;
      int64_t fb_abs = get_feedback_abs_slot(&sl_tx_pool_p->phy_sl_bitmap, tx_pool_map_sz,
                                             rcv_tx_abs, min_tg, (uint8_t)psfch_period);
      /* update_harq_lists() was moved to the top of the SL_UE_iterator body so
       * it runs even when available_sl_harq is empty (all HARQs in feedback
       * list). No second call needed here. */
      *fb_frame = (fb_abs >= 0) ? (int)((fb_abs / nr_slots_frame) % 1024) : rcv_tx_frame;
      *fb_slot  = (fb_abs >= 0) ? (int)(fb_abs % nr_slots_frame) : -1;
      LOG_D(NR_MAC, "Tx SLSCH %4d.%2d, Expected Feedback: %4d.%2d in current PSFCH: psfch_period %ld\n",
            frame,
            slot,
            *fb_frame,
            *fb_slot,
            psfch_period);
    }
    int locbw = sl_bwp->sl_BWP_Generic_r16->sl_BWP_r16->locationAndBandwidth;
    sched_pssch->mu = scs;
    sched_pssch->frame = frame;
    sched_pssch->slot = slot;
    sched_pssch->rbSize = NRRIV2BW(locbw, MAX_BWP_SIZE);
    sched_pssch->rbStart = NRRIV2PRBOFFSET(locbw, MAX_BWP_SIZE);
  }
}

bool nr_ue_sl_pssch_scheduler(NR_UE_MAC_INST_t *mac,
                              nr_sidelink_indication_t *sl_ind,
                              const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                              const NR_SL_ResourcePool_r16_t *sl_res_pool,
                              sl_nr_tx_config_request_t *tx_config,
                              sl_resource_info_t *resource,
                              uint8_t *config_type) {

  uint16_t slot = sl_ind->slot_tx;
  uint16_t frame = sl_ind->frame_tx;
  int feedback_frame, feedback_slot;
  int lcid = 4;
  bool is_resource_allocated = false;
  *config_type = 0;

  sl_nr_ue_mac_params_t* sl_mac_params = mac->SL_MAC_PARAMS;
  NR_SetupRelease_SL_PSFCH_Config_r16_t *configured_PSFCH  = mac->sl_tx_res_pool->sl_PSFCH_Config_r16;
  const uint8_t psfch_period_lut[] = {0, 1, 2, 4};
  const long psfch_period = (configured_PSFCH && configured_PSFCH->choice.setup->sl_PSFCH_Period_r16)
                                ? psfch_period_lut[*configured_PSFCH->choice.setup->sl_PSFCH_Period_r16]
                                : 0;
  if ((frame & 127) == 0 && slot == 0) {
    print_meas(&mac->rlc_data_req,"rlc_data_req",NULL,NULL);
  }
  if (sl_ind->slot_type != SIDELINK_SLOT_TYPE_TX) return is_resource_allocated;

  /* The configured TX resource-pool bitmap determines PSSCH eligibility.  Do
   * not impose an additional sync-role-dependent half-frame restriction. */

  LOG_D(NR_MAC,"[UE%d] SL-PSSCH SCHEDULER: Frame:SLOT %d:%d, slot_type:%d\n",
        sl_ind->module_id, frame, slot,sl_ind->slot_type);

  uint16_t slsch_pdu_length_max;
  tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.slsch_payload = mac->slsch_payload;

  NR_SL_UEs_t *UE_info = &mac->sl_info;

  if (*(UE_info->list) == NULL) {
    LOG_D(NR_MAC, "UE list is empty\n");
    return is_resource_allocated;
  }

  /* PSFCH indications run on the RX path and transition HARQ processes out of
   * feedback_sl_harq under this lock.  Keep preprocessing, HARQ selection and
   * grant construction in one transaction so RX cannot release a process
   * between reading its state and removing it from a list. */
  NR_UE_SL_SCHED_LOCK(&mac->sl_sched_lock);
  preprocess(mac,
             frame,
             slot,
             sl_ind->frame_rx,
             sl_ind->slot_rx,
             &feedback_frame,
             &feedback_slot,
             sl_bwp,
             configured_PSFCH,
             psfch_period);

  SL_UE_iterator(UE_info->list, UE) {
    NR_mac_dir_stats_t *sl_mac_stats = &UE->mac_sl_stats.sl;
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    sl_mac_stats->current_bytes = 0;
    sl_mac_stats->current_rbs = 0;
    NR_sched_pssch_t *sched_pssch = &sched_ctrl->sched_pssch;
    int8_t harq_id = sched_pssch->sl_harq_pid;

    if (sched_pssch->rbSize <= 0)
      continue;

    NR_UE_sl_harq_t *cur_harq = NULL;

    LOG_D(NR_MAC,
          "%4d.%2d SL queue: RLC=%u pending-HARQ-bytes=%d available=%d retrans=%d feedback=%d\n",
          frame,
          slot,
          sched_ctrl->num_total_bytes,
          sched_ctrl->sched_sl_bytes,
          sched_ctrl->available_sl_harq.len,
          sched_ctrl->retrans_sl_harq.len,
          sched_ctrl->feedback_sl_harq.len);

    if (harq_id < 0) {
      /* PP has not selected a specific HARQ Process, get a new one */
      harq_id = sched_ctrl->available_sl_harq.head;
      AssertFatal(harq_id >= 0,
                  "no free HARQ process available\n");
      remove_front_nr_list(&sched_ctrl->available_sl_harq);
      sched_pssch->sl_harq_pid = harq_id;
    } else {
      /* PP selected a specific HARQ process. Check whether it will be a new
      * transmission or a retransmission, and remove from the corresponding
      * list */
      if (sched_ctrl->sl_harq_processes[harq_id].round == 0)
        remove_nr_list(&sched_ctrl->available_sl_harq, harq_id);
      else
        remove_nr_list(&sched_ctrl->retrans_sl_harq, harq_id);
    }
    cur_harq = &sched_ctrl->sl_harq_processes[harq_id];
    DevAssert(!cur_harq->is_waiting);

    /* retransmission or bytes to send */
    if (configured_PSFCH && ((cur_harq->round != 0) || (sched_ctrl->num_total_bytes > 0))) {
      if (feedback_slot < 0) {
        LOG_W(NR_MAC,
              "%4d.%2d no PSFCH occasion found for HARQ PID %d; defer the grant instead of stranding it\n",
              frame,
              slot,
              harq_id);
        add_tail_nr_list(cur_harq->round ? &sched_ctrl->retrans_sl_harq : &sched_ctrl->available_sl_harq, harq_id);
        sched_pssch->rbSize = 0;
        continue;
      }
      cur_harq->feedback_slot = feedback_slot;
      cur_harq->feedback_frame = feedback_frame;
      add_tail_nr_list(&sched_ctrl->feedback_sl_harq, harq_id);
      cur_harq->is_waiting = true;
      LOG_D(NR_MAC, "%4d.%2d Sending Data; Expecting feedback at %4d.%2d\n", frame, slot, feedback_frame, feedback_slot);
    }
    else
      add_tail_nr_list(&sched_ctrl->available_sl_harq, harq_id);
    cur_harq->sl_harq_pid = harq_id;
    // TS 38.321 5.22.1.3.1: toggle NDI when a new MAC PDU is obtained.
    // Keep NDI unchanged when the stored MAC PDU is retransmitted.
    if (cur_harq->round == 0) {
    cur_harq->ndi ^= 1;
    }

    // why inside mac->sci1_pdu?
    nr_schedule_slsch(mac, frame, slot, &mac->sci1_pdu, &mac->sci2_pdu, NR_SL_SCI_FORMAT_2A,
                      UE, &slsch_pdu_length_max, cur_harq, &sched_ctrl->rlc_status[lcid], resource);

    *config_type = SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH;
    tx_config->number_pdus = 1;
    tx_config->sfn = frame;
    tx_config->slot = slot;
    tx_config->tx_config_list[0].pdu_type = *config_type;
    fill_pssch_pscch_pdu(sl_mac_params,
                        &tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu,
                        sl_bwp,
                        sl_res_pool,
                        &mac->sci1_pdu,
                        &mac->sci2_pdu,
                        slsch_pdu_length_max,
                        NR_SL_SCI_FORMAT_1A,
                        NR_SL_SCI_FORMAT_2A,
                        slot,
                        resource);
    sl_nr_tx_config_pscch_pssch_pdu_t *pscch_pssch_pdu = &tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu;

    /* TS 38.214 8.1.3.2 / TS 38.321 5.22.1.3.1: for HARQ combining the
     * receiver derives TBS from the SCI grant, so every transmission of the
     * same TB must produce the same TBS.  When psfch_period is 2 or 4, some
     * candidate slots have 3-symbol PSFCH overhead and therefore fewer PSSCH
     * symbols.  If the initial TB were sized using a non-PSFCH slot's larger
     * capacity, no MCS could reproduce that TBS on a PSFCH slot, forcing
     * repeated retransmission deferrals.
     *
     * Fix: on round 0 with variable PSFCH overhead, compute the worst-case
     * (PSFCH-present) TBS first, then find an MCS that produces that same TBS
     * with the actual overhead.  If there is no common TBS, deliberately
     * reserve the three-symbol overhead in this grant.  Retransmissions use
     * the same fallback below, so the receiver always derives the stored TBS
     * from the SCI-1A indication. */
    if (cur_harq->round == 0 && (psfch_period == 2 || psfch_period == 4) && !mac->sci1_pdu.psfch_overhead.val) {
      /* Step 1: compute worst-case TBS (PSFCH overhead present). */
      mac->sci1_pdu.psfch_overhead.val = 1;
      sl_nr_tx_config_pscch_pssch_pdu_t worst_case_pdu = {0};
      fill_pssch_pscch_pdu(sl_mac_params,
                           &worst_case_pdu,
                           sl_bwp,
                           sl_res_pool,
                           &mac->sci1_pdu,
                           &mac->sci2_pdu,
                           slsch_pdu_length_max,
                           NR_SL_SCI_FORMAT_1A,
                           NR_SL_SCI_FORMAT_2A,
                           slot,
                           resource);
      const uint32_t worst_case_tbs = worst_case_pdu.tb_size;
      mac->sci1_pdu.psfch_overhead.val = 0;

      /* Step 2: if the current slot yields a larger TBS, search for an MCS
       * on this slot that matches the worst-case TBS. */
      if (pscch_pssch_pdu->tb_size > worst_case_tbs) {
        const int orig_mcs = mac->sci1_pdu.mcs;
        bool found = false;
        for (int mcs = orig_mcs; mcs >= 0; mcs--) {
          mac->sci1_pdu.mcs = mcs;
          fill_pssch_pscch_pdu(sl_mac_params,
                               pscch_pssch_pdu,
                               sl_bwp,
                               sl_res_pool,
                               &mac->sci1_pdu,
                               &mac->sci2_pdu,
                               slsch_pdu_length_max,
                               NR_SL_SCI_FORMAT_1A,
                               NR_SL_SCI_FORMAT_2A,
                               slot,
                               resource);
          if (pscch_pssch_pdu->tb_size == worst_case_tbs) {
            found = true;
            break;
          }
          if (pscch_pssch_pdu->tb_size < worst_case_tbs)
            break; /* TBS is monotonic in MCS; no point going lower. */
        }
        if (!found) {
          /* Exact match is not available with the full symbol allocation.
           * Signal a three-symbol reservation even though this slot does not
           * carry PSFCH, making the on-air grant and the conservative TBS
           * internally consistent.  Keep the indication set: restoring only
           * the mutable SCI copy would disagree with the already serialized
           * PSCCH payload. */
          mac->sci1_pdu.mcs = orig_mcs;
          mac->sci1_pdu.psfch_overhead.val = 1;
          fill_pssch_pscch_pdu(sl_mac_params,
                               pscch_pssch_pdu,
                               sl_bwp,
                               sl_res_pool,
                               &mac->sci1_pdu,
                               &mac->sci2_pdu,
                               slsch_pdu_length_max,
                               NR_SL_SCI_FORMAT_1A,
                               NR_SL_SCI_FORMAT_2A,
                               slot,
                               resource);
        }
        LOG_D(NR_MAC,
              "%4d.%2d Round 0: sized TB for worst-case PSFCH capacity: MCS %d TBS %u\n",
              frame,
              slot,
              mac->sci1_pdu.mcs,
              pscch_pssch_pdu->tb_size);
      }
    }

    // TS 38.321 5.22.1.3.1a stores the MAC PDU in the HARQ buffer for a new
    // transmission and generates a retransmission from that same HARQ buffer.
    // The receiver derives TBS from the SCI grant. If the current resource and
    // preferred round-0 MCS produce another TBS, select an MCS that produces
    // the stored TBS. Do not transmit when this resource cannot represent it.
    if (cur_harq->round > 0 && pscch_pssch_pdu->tb_size != cur_harq->sched_pssch.tb_size) {
      const uint32_t required_tbs = cur_harq->sched_pssch.tb_size;
      const int max_mcs = min(sched_ctrl->sl_max_mcs, 28);
      const uint8_t actual_psfch_overhead = mac->sci1_pdu.psfch_overhead.val;
      bool matching_tbs = false;
      for (int mcs = 0; mcs <= max_mcs; mcs++) {
        mac->sci1_pdu.mcs = mcs;
        fill_pssch_pscch_pdu(sl_mac_params,
                             pscch_pssch_pdu,
                             sl_bwp,
                             sl_res_pool,
                             &mac->sci1_pdu,
                             &mac->sci2_pdu,
                             slsch_pdu_length_max,
                             NR_SL_SCI_FORMAT_1A,
                             NR_SL_SCI_FORMAT_2A,
                             slot,
                             resource);
        if (pscch_pssch_pdu->tb_size == required_tbs) {
          matching_tbs = true;
          break;
        }
      }
      /* An initial transmission may have deliberately reserved the
       * three-symbol PSFCH overhead because no TBS was common to the 12- and
       * 9-symbol allocations.  Apply the same reservation to a retransmission
       * in a nominally non-PSFCH slot before declaring the resource
       * incompatible.  The final fill serializes overhead=1 and configures
       * the matching reduced PSSCH symbol count. */
      if (!matching_tbs && !actual_psfch_overhead && (psfch_period == 2 || psfch_period == 4)) {
        mac->sci1_pdu.psfch_overhead.val = 1;
        for (int mcs = 0; mcs <= max_mcs; mcs++) {
          mac->sci1_pdu.mcs = mcs;
          fill_pssch_pscch_pdu(sl_mac_params,
                               pscch_pssch_pdu,
                               sl_bwp,
                               sl_res_pool,
                               &mac->sci1_pdu,
                               &mac->sci2_pdu,
                               slsch_pdu_length_max,
                               NR_SL_SCI_FORMAT_1A,
                               NR_SL_SCI_FORMAT_2A,
                               slot,
                               resource);
          if (pscch_pssch_pdu->tb_size == required_tbs) {
            matching_tbs = true;
            LOG_D(NR_MAC,
                  "%4d.%2d HARQ PID %d round %u: reserved PSFCH overhead to preserve TBS %u\n",
                  frame,
                  slot,
                  harq_id,
                  cur_harq->round,
                  required_tbs);
            break;
          }
        }
      }
      if (!matching_tbs) {
        mac->sci1_pdu.psfch_overhead.val = actual_psfch_overhead;
        /* No transmission took place, so this is not a HARQ round.  In
         * particular, a TB created in a 12-symbol slot may not fit a
         * 9-symbol PSFCH-overlap resource.  Defer it until a compatible
         * resource is selected, preserving the stored TB, NDI and round. */
        LOG_W(NR_MAC,
              "%4d.%2d Deferring HARQ PID %d round %u: resource cannot carry stored TBS %u\n",
              frame,
              slot,
              harq_id,
              cur_harq->round,
              required_tbs);
          DevAssert(cur_harq->is_waiting);
          remove_nr_list(&sched_ctrl->feedback_sl_harq, harq_id);
          cur_harq->is_waiting = false;
        cur_harq->feedback_frame = -1;
        cur_harq->feedback_slot = -1;
        add_tail_nr_list(&sched_ctrl->retrans_sl_harq, harq_id);
        sched_pssch->rbSize = 0;
        continue;
      }
    }

    sched_pssch->R = pscch_pssch_pdu->target_coderate;
    sched_pssch->tb_size = pscch_pssch_pdu->tb_size;
    sched_pssch->sl_harq_pid = mac->sci2_pdu.harq_pid;
    sched_pssch->nrOfLayers = pscch_pssch_pdu->num_layers;
    sched_pssch->mcs = pscch_pssch_pdu->mcs;
    sched_pssch->Qm = pscch_pssch_pdu->mod_order;
    /* Retain the exact PSSCH resource and SCI-2 identity used by this
     * transmission.  PSFCH RX is configured later and must not consult the
     * mutable current scheduling decision. */
    sched_pssch->source_id = mac->src_id;
    sched_pssch->dest_id = mac->sci2_pdu.dest_id;
    sched_pssch->cast_type = mac->sci2_pdu.cast_type;
    sched_pssch->harq_feedback = mac->sci2_pdu.harq_feedback;
    sched_pssch->second_stage_sci_format = mac->sci1_pdu.second_stage_sci_format;
    sched_pssch->pssch_start_subchannel = resource->sl_subchan_start;
    sched_pssch->pssch_num_subchannels = resource->sl_subchan_len;
    cur_harq->sched_pssch.source_id = sched_pssch->source_id;
    cur_harq->sched_pssch.dest_id = sched_pssch->dest_id;
    cur_harq->sched_pssch.cast_type = sched_pssch->cast_type;
    cur_harq->sched_pssch.harq_feedback = sched_pssch->harq_feedback;
    cur_harq->sched_pssch.second_stage_sci_format = sched_pssch->second_stage_sci_format;
    cur_harq->sched_pssch.pssch_start_subchannel = sched_pssch->pssch_start_subchannel;
    cur_harq->sched_pssch.pssch_num_subchannels = sched_pssch->pssch_num_subchannels;

    LOG_D(NR_MAC, "PSSCH: %4d.%2d SL sched %4d.%2d start %2d RBS %3d MCS %2d nrOfLayers %2d TBS %4d HARQ PID %2d round %d NDI %d sched %6d\n",
          frame,
          slot,
          sched_pssch->frame,
          sched_pssch->slot,
          sched_pssch->rbStart,
          sched_pssch->rbSize,
          sched_pssch->mcs,
          sched_pssch->nrOfLayers,
          sched_pssch->tb_size,
          sched_pssch->sl_harq_pid,
          cur_harq->round,
          cur_harq->ndi,
          sched_ctrl->sched_sl_bytes);

    /* Statistics */
    AssertFatal(cur_harq->round < sl_mac_params->sl_bler.harq_round_max, "Indexing ulsch_rounds[%d] is out of bounds for max harq round %d\n", cur_harq->round, sl_mac_params->sl_bler.harq_round_max);

    sl_mac_stats->rounds[cur_harq->round]++;
    if (cur_harq->round != 0) { // retransmission
      LOG_D(NR_MAC,
            "PSSCH: %d.%2d SL retransmission sched %d.%2d HARQ PID %d round %d NDI %d\n",
            frame,
            slot,
            sched_pssch->frame,
            sched_pssch->slot,
            sched_pssch->sl_harq_pid,
            cur_harq->round,
            cur_harq->ndi);
      sl_mac_stats->total_rbs_retx += sched_pssch->rbSize;
    } else { // initial transmission

      UE->mac_sl_stats.slsch_total_bytes_scheduled += sched_pssch->tb_size;
      /* save which time allocation and nrOfLayers have been used, to be used on
      * retransmissions */
      cur_harq->sched_pssch.nrOfLayers = sched_pssch->nrOfLayers;
      /* With PSFCH disabled the HARQ process is released immediately, so the
       * TB is not in flight and must not accumulate in sched_sl_bytes. */
      if (configured_PSFCH)
      sched_ctrl->sched_sl_bytes += sched_pssch->tb_size;
      sl_mac_stats->total_rbs += sched_pssch->rbSize;

      const uint32_t buflen = tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.tb_size;

      LOG_D(NR_MAC, "[UE%d] Initial TTI-%d:%d TX PSCCH_PSSCH REQ  TBS %u\n", sl_ind->module_id, frame, slot, buflen);

      uint8_t *const pdu_start = (uint8_t *)cur_harq->transportBlock;
      uint8_t *pdu = pdu_start;
      DevAssert(buflen >= sizeof(NR_SLSCH_MAC_SUBHEADER_FIXED));
      DevAssert(buflen <= sizeof(cur_harq->transportBlock));
      uint8_t *const pdu_end = pdu_start + buflen;

      NR_SLSCH_MAC_SUBHEADER_FIXED *sl_sch_subheader = (NR_SLSCH_MAC_SUBHEADER_FIXED *) pdu;
      sl_sch_subheader->V = 0;
      sl_sch_subheader->R = 0;
      sl_sch_subheader->SRC = mac->sci2_pdu.source_id;
      sl_sch_subheader->DST = mac->sci2_pdu.dest_id;
      pdu += sizeof(NR_SLSCH_MAC_SUBHEADER_FIXED);
      LOG_D(NR_MAC,
            "%4d.%2d Tx V %d, R %d, SRC %d, DST %d\n",
            frame,
            slot,
            sl_sch_subheader->V,
            sl_sch_subheader->R,
            sl_sch_subheader->SRC,
            sl_sch_subheader->DST);
      LOG_D(NR_MAC, "buflen_remain after adding SL_SCH_MAC_SUBHEADER_FIXED %td\n", pdu_end - pdu);
      int num_sdus=0;
      uint32_t sdu_length_total = 0;
            
      if (sched_ctrl->num_total_bytes > 0) {
        if (sched_ctrl->rlc_status[lcid].bytes_in_buffer > 0) {
          while (pdu_end - pdu > sizeof(NR_MAC_SUBHEADER_SHORT)) {
            const ptrdiff_t usable = pdu_end - pdu;
            /* TS 38.321 6.2.1 Table 6.2.1-2: F=0 (short) header encodes L
             * in 8 bits (0..255); F=1 (long) header uses 16 bits.  Use the
             * long form when the payload available after a short header would
             * exceed 255 bytes. */
            const bool use_long_header = sched_ctrl->rlc_status[lcid].bytes_in_buffer > UINT8_MAX
                                         && usable - sizeof(NR_MAC_SUBHEADER_SHORT) > UINT8_MAX;
            const uint8_t sh_size = use_long_header ? sizeof(NR_MAC_SUBHEADER_LONG)
                                                    : sizeof(NR_MAC_SUBHEADER_SHORT);
            const rlc_buffer_occupancy_t ndata =
                min(sched_ctrl->rlc_status[lcid].bytes_in_buffer, usable - sh_size);
            uint8_t *header = pdu;
            pdu += sh_size;

            start_meas(&mac->rlc_data_req);
            const int sdu_length = nr_mac_rlc_data_req(mac->ue_id, mac->ue_id, false, lcid, ndata, (char *)pdu);
            stop_meas(&mac->rlc_data_req);
            AssertFatal(pdu_end - pdu >= sdu_length,
                        "In %s: LCID = 0x%02x RLC has segmented %d bytes but MAC has max %td remaining bytes\n",
                        __FUNCTION__,
                        lcid,
                        sdu_length,
                        pdu_end - pdu);
            if (sdu_length > 0) {
              LOG_D(NR_MAC,
                    "In %s: [UE %d] [%d.%d] SL-DXCH -> SLSCH, Generating SL MAC sub-PDU for SDU %d, length %d bytes, RB with LCID "
                    "0x%02x (buflen (TBS) %u bytes)\n",
                __FUNCTION__,
                0,
                frame,
                slot,
                num_sdus + 1,
                sdu_length,
                lcid,
                buflen);

              if (sh_size == sizeof(NR_MAC_SUBHEADER_SHORT)) {
                *(NR_MAC_SUBHEADER_SHORT *)header =
                    (NR_MAC_SUBHEADER_SHORT){.R = 0, .F = 0, .LCID = lcid, .L = sdu_length};
              } else {
                *(NR_MAC_SUBHEADER_LONG *)header =
                    (NR_MAC_SUBHEADER_LONG){.R = 0, .F = 1, .LCID = lcid, .L = htons(sdu_length)};
              }
              pdu += sdu_length;
              /* Keep the cached byte count in sync with what was actually drained
               * so subsequent iterations compute use_long_header and ndata correctly. */
              sched_ctrl->rlc_status[lcid].bytes_in_buffer -= sdu_length;
              sched_ctrl->num_total_bytes -= sdu_length;
              sdu_length_total += sdu_length;
              LOG_D(NR_PHY,
                    "buflen_remain %td, wrote subheader + SDU %d, SDU total length %u, sdu_length %d\n",
                    pdu_end - pdu,
                    sh_size + sdu_length,
                    sdu_length_total,
                    sdu_length);
              num_sdus++;

            } else {
              pdu -= sh_size;
              break;
            }
          }
          }
        }

      // TS 38.321 6.1.6 and 6.2.4: terminate the SL-SCH MAC PDU with the
      // fixed-size padding subheader (LCID 63), then fill the remainder. Keep
      // the write cursor bounded by the TBS, matching the gNB DL-SCH builder.
      const ptrdiff_t padding_len = pdu_end - pdu;
      if (padding_len > 0) {
        LOG_D(NR_MAC, "In %s filling remainder %td bytes of the SL-SCH PDU\n", __FUNCTION__, padding_len);
        *(NR_MAC_SUBHEADER_FIXED *)pdu = (NR_MAC_SUBHEADER_FIXED){.R = 0, .LCID = SL_SCH_LCID_SL_PADDING};
        pdu++;

        if (get_softmodem_params()->phy_test || get_softmodem_params()->do_ra) {
          uint8_t *buf = pdu;
          for (; buf < pdu_end && ((intptr_t)buf) % 4; buf++)
            *buf = lrand48() & 0xff;
          for (; buf < pdu_end - 3; buf += 4) {
            uint32_t *buf32 = (uint32_t *)buf;
            *buf32 = lrand48();
          } 
          for (; buf < pdu_end; buf++)
            *buf = lrand48() & 0xff;
        } else {
          memset(pdu, 0, pdu_end - pdu);
        }
        pdu = pdu_end;
      } 
      DevAssert(pdu == pdu_end);

      sl_mac_stats->current_bytes = sched_pssch->tb_size;
      sl_mac_stats->current_rbs = sched_pssch->rbSize;
      sl_mac_stats->total_bytes += pscch_pssch_pdu->tb_size;
      sl_mac_stats->num_mac_sdu += num_sdus;
      sl_mac_stats->total_sdu_bytes += sdu_length_total;

      /* Save information on MCS, TBS etc for the current initial transmission
      * so we have access to it when retransmitting */
      cur_harq->sched_pssch = *sched_pssch;
    } // end of initial transmission

    const uint32_t TBS = pscch_pssch_pdu->tb_size;
    DevAssert(cur_harq->round == 0 || TBS == cur_harq->sched_pssch.tb_size);
    memcpy(pscch_pssch_pdu->slsch_payload, cur_harq->transportBlock, TBS);
    pscch_pssch_pdu->slsch_payload_length = TBS;
    // mark UE as scheduled
    sched_pssch->rbSize = 0;
    is_resource_allocated = true;
    /* tx_config contains one PSCCH/PSSCH PDU.  Continuing across peers would
     * drain every peer's RLC view while repeatedly overwriting that single PDU,
     * leaving only the last transport block available to PHY. */
    break;
  }
  NR_UE_SL_SCHED_UNLOCK(&mac->sl_sched_lock);
  return is_resource_allocated;
}

void nr_ue_sl_psfch_scheduler(NR_UE_MAC_INST_t *mac,
                              frame_t frame,
                              uint16_t slot,
                              long psfch_period,
                              nr_sidelink_indication_t *sl_ind,
                              const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                              sl_nr_tx_config_request_t *tx_config,
                              uint8_t *config_type) {
  tx_config->sfn = frame;
  tx_config->slot = slot;
  int num_psfch_symbols = 0;
  if (psfch_period > 0)
    num_psfch_symbols = 3;

  /* Allocate the PDU list large enough to hold every scheduled PSFCH entry.
   * sched_psfch[] has sched_psfch_size entries (nr_slots_frame x num_subch),
   * so at most that many PSFCHs can be scheduled for a single slot.  The
   * previous allocation used psfch_periodxnum_subch (e.g. 2x2=4) which is far
   * smaller and would trigger the AssertFatal below under high HARQ concurrency. */
  const int sched_psfch_size = mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch_size;
  tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.psfch_pdu_list = CALLOC(sched_psfch_size, sizeof(sl_nr_tx_rx_config_psfch_pdu_t));
  sl_nr_tx_rx_config_psfch_pdu_t *psfch_pdu_list = tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.psfch_pdu_list;
  const bool has_pssch = *config_type == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH;
  int k = 0;
  for (int i = 0; i < sched_psfch_size; i++) {
    SL_sched_feedback_t  *sched_psfch = &mac->sl_info.list[0]->UE_sched_ctrl.sched_psfch[i];
    LOG_D(NR_MAC,"frame.slot: feedback %4d.%2d, current (%4d.%2d)\n",
          sched_psfch->feedback_frame, sched_psfch->feedback_slot, frame, slot);
    if (sched_psfch->feedback_slot == slot && sched_psfch->feedback_frame == frame) {
      sl_ind->slot_tx = sched_psfch->feedback_slot;
      sl_ind->frame_tx = sched_psfch->feedback_frame;
      sl_ind->slot_type = SIDELINK_SLOT_TYPE_TX;
      AssertFatal(k < sched_psfch_size, "Number of PSFCH pdus cannot exceed %d\n", sched_psfch_size);
      fill_psfch_pdu(sched_psfch, &psfch_pdu_list[k], num_psfch_symbols);
      *config_type = has_pssch ? SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH : SL_NR_CONFIG_TYPE_TX_PSFCH;
      tx_config->number_pdus = 1;
      tx_config->tx_config_list[0].pdu_type = *config_type;
      LOG_D(NR_MAC,"SL-PSFCH SCHEDULER: frame.slot (%d.%d), slot_type:%d\n",
            frame, slot, sl_ind->slot_type);
      sched_psfch->feedback_slot = -1;
      sched_psfch->feedback_frame = -1;
      sched_psfch->dai_c = 0;
      sched_psfch->harq_feedback = -1;
      k++;
    }
  }
  tx_config->tx_config_list[0].tx_pscch_pssch_config_pdu.num_psfch_pdus = k;
}

void fill_psfch_pdu(SL_sched_feedback_t *mac_psfch_pdu,
                    sl_nr_tx_rx_config_psfch_pdu_t *tx_psfch_pdu,
                    int num_psfch_symbols) {
  tx_psfch_pdu->start_symbol_index = mac_psfch_pdu->start_symbol_index;
  tx_psfch_pdu->hopping_id = mac_psfch_pdu->hopping_id;
  tx_psfch_pdu->prb = mac_psfch_pdu->prb;
  tx_psfch_pdu->sl_bwp_start = mac_psfch_pdu->sl_bwp_start;
  tx_psfch_pdu->initial_cyclic_shift = mac_psfch_pdu->initial_cyclic_shift;
  tx_psfch_pdu->mcs = mac_psfch_pdu->mcs;
  tx_psfch_pdu->freq_hop_flag = mac_psfch_pdu->freq_hop_flag;
  tx_psfch_pdu->second_hop_prb = mac_psfch_pdu->second_hop_prb;
  tx_psfch_pdu->group_hop_flag = mac_psfch_pdu->group_hop_flag;
  tx_psfch_pdu->sequence_hop_flag = mac_psfch_pdu->sequence_hop_flag;
  tx_psfch_pdu->nr_of_symbols = num_psfch_symbols ? num_psfch_symbols - 2 : 0; // (num_psfch_symbols - 2) excludes PSFCH AGC and Guard
  AssertFatal(tx_psfch_pdu->nr_of_symbols >= 0, "Number of PSFCH symbols can not be negative!!!\n");
  tx_psfch_pdu->bit_len_harq = mac_psfch_pdu->bit_len_harq;
  LOG_D(PHY,"%s: nr_symbols %d, start_symbol %d, prb_start %d, second_hop_prb %d, \
        group_hop_flag %d, sequence_hop_flag %d, mcs %d initial_cyclic_shift %d \
        hopping_id %d, sl_bwp_start %d freq_hop_flag %d\n",
        __FUNCTION__,
        tx_psfch_pdu->nr_of_symbols,
        tx_psfch_pdu->start_symbol_index,
        tx_psfch_pdu->prb,
        tx_psfch_pdu->second_hop_prb,
        tx_psfch_pdu->group_hop_flag,
        tx_psfch_pdu->sequence_hop_flag,
        tx_psfch_pdu->mcs,
        tx_psfch_pdu->initial_cyclic_shift,
        tx_psfch_pdu->hopping_id,
        tx_psfch_pdu->sl_bwp_start,
        tx_psfch_pdu->freq_hop_flag
        );
}

void nr_ue_sl_pscch_rx_scheduler(nr_sidelink_indication_t *sl_ind,
                              const NR_SL_BWP_ConfigCommon_r16_t *sl_bwp,
                              const NR_SL_ResourcePool_r16_t *sl_res_pool,
                              sl_nr_rx_config_request_t *rx_config,
                              uint8_t *config_type,
                              bool sl_has_psfch) {

  *config_type = SL_NR_CONFIG_TYPE_RX_PSCCH;
  rx_config->number_pdus = 1;
  rx_config->sfn = sl_ind->frame_rx;
  rx_config->slot = sl_ind->slot_rx;
  rx_config->sl_rx_config_list[0].pdu_type = *config_type;
  config_pscch_pdu_rx(&rx_config->sl_rx_config_list[0].rx_pscch_config_pdu,
                       sl_bwp,
                       sl_res_pool,
                       sl_has_psfch);


   LOG_D(NR_MAC, "[UE%d] TTI-%d:%d RX PSCCH REQ \n", sl_ind->module_id,sl_ind->frame_rx, sl_ind->slot_rx);

}

sl_resource_info_t* get_resource_element(List_t* resource_list, frameslot_t sfn) {
  for (int i = 0; i < resource_list->size; i++) {
    sl_resource_info_t *itr_rsrc = (sl_resource_info_t*)((char*)resource_list->data + i * resource_list->element_size);
    LOG_D(NR_MAC, "%s %4d.%2d, %ld, sl_subchan_len %d, current sfn %4d.%2d\n",
          __FUNCTION__, itr_rsrc->sfn.frame, itr_rsrc->sfn.slot, normalize(&itr_rsrc->sfn, 1), itr_rsrc->sl_subchan_len, sfn.frame, sfn.slot);
    if (itr_rsrc->sfn.frame == sfn.frame && itr_rsrc->sfn.slot == sfn.slot) {
      return itr_rsrc;
    }
  }
  return NULL;
}

static bool has_current_or_future_candidate(const List_t *resource_list, frameslot_t now, uint8_t mu)
{
  const uint64_t now_abs = normalize(&now, mu);
  const uint64_t cycle_slots = (uint64_t)nr_slots_per_frame[mu] * 1024;
  for (int i = 0; i < resource_list->size; i++) {
    const sl_resource_info_t *resource =
        (const sl_resource_info_t *)((const char *)resource_list->data + i * resource_list->element_size);
    frameslot_t resource_sfn = resource->sfn;
    const uint64_t resource_abs = normalize(&resource_sfn, mu);
    const uint64_t slots_ahead = (resource_abs + cycle_slots - now_abs) % cycle_slots;
    /* Candidate windows are far shorter than half an SFN cycle.  The modular
     * comparison therefore distinguishes a wrapped future candidate from an
     * old candidate just behind the current slot. */
    if (slots_ahead < cycle_slots / 2)
      return true;
  }
  return false;
}

static void replace_candidate_resources(NR_UE_MAC_INST_t *mac, List_t *new_resources)
{
  if (mac->sl_candidate_resources != NULL) {
    free_list_mem(mac->sl_candidate_resources);
    free(mac->sl_candidate_resources);
  }
  mac->sl_candidate_resources = new_resources;
}

void nr_ue_sl_mac_free(NR_UE_MAC_INST_t *mac)
{
  /* Release all dynamically-allocated sidelink state so that the MAC instance
   * can be safely freed or re-initialised without leaking memory. */
  replace_candidate_resources(mac, NULL);
  free_list_mem(&mac->sl_sensing_data);
  free_list_mem(&mac->sl_transmit_history);
}

size_t dump_mac_stats_sl(NR_UE_MAC_INST_t *mac, char *output, size_t strlen, bool reset_rsrp)
{
  const char *begin = output;
  const char *end = output + strlen;
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;

  /* this function is called from gNB_dlsch_ulsch_scheduler(), so assumes the
   * scheduler to be locked*/
  // NR_UE_SL_SCHED_ENSURE_LOCKED(&mac->sl_sched_lock);

  NR_UE_SL_SCHED_LOCK(&mac->sl_info.mutex);
  SL_UE_iterator(mac->sl_info.list, UE) {
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    NR_UE_sl_mac_stats_t *stats = &UE->mac_sl_stats;

    output += snprintf(output, end - output, "%"PRIu64, stats->sl.rounds[0]);
    for (int i = 1; i < sl_mac->sl_bler.harq_round_max; i++)
      output += snprintf(output, end - output, "/%"PRIu64, stats->sl.rounds[i]);

    output += snprintf(output,
                       end - output,
                       ", slsch_errors %"PRIu64", BLER %.5f MCS %d\n",
                       stats->sl.errors,
                       sched_ctrl->sl_bler_stats.bler,
                       sched_ctrl->sl_bler_stats.mcs);

    output += snprintf(output,
                       end - output,
                       "UE %04x: slsch_total_bytes %"PRIu64"\n",
                       UE->uid, stats->sl.total_bytes);
    output += snprintf(output,
                       end - output,
                       "UE %04x: slsch_rounds ", UE->uid);
    output += snprintf(output, end - output, "%"PRIu64, stats->sl.rounds[0]);
    for (int i = 1; i < sl_mac->sl_bler.harq_round_max; i++)
      output += snprintf(output, end - output, "/%"PRIu64, stats->sl.rounds[i]);

    output += snprintf(output,
                       end - output,
                       ", slsch_DTX %d, slsch_errors %"PRIu64", BLER %.5f MCS %d\n",
                       stats->slsch_DTX,
                       stats->sl.errors,
                       sched_ctrl->sl_bler_stats.bler,
                       sched_ctrl->sl_bler_stats.mcs);
    output += snprintf(output,
                       end - output,
                       "UE %04x: slsch_total_bytes_scheduled %"PRIu64", slsch_total_bytes_received %"PRIu64"\n",
                       UE->uid,
                       stats->slsch_total_bytes_scheduled, stats->sl.total_bytes);
    output += snprintf(output,
                       end - output,
                       "UE %04x: RLC bytes pulled %"PRIu64", MAC SDUs %u\n",
                       UE->uid,
                       stats->sl.total_sdu_bytes,
                       stats->sl.num_mac_sdu);

    for (int lc_id = 0; lc_id < 63; lc_id++) {
      if (stats->sl.lc_bytes[lc_id] > 0)
        output += snprintf(output,
                           end - output,
                           "UE %04x: LCID %d: %"PRIu64" bytes TX\n",
                           UE->uid,
                           lc_id,
                           stats->sl.lc_bytes[lc_id]);
      if (stats->sl.lc_bytes[lc_id] > 0)
        output += snprintf(output,
                           end - output,
                           "UE %04x: LCID %d: %"PRIu64" bytes RX\n",
                           UE->uid,
                           lc_id,
                           stats->sl.lc_bytes[lc_id]);
    }
  }
  NR_UE_SL_SCHED_UNLOCK(&mac->sl_info.mutex);
  return output - begin;
}

void remove_old_transmit_history(frameslot_t *frame_slot,
                                 uint16_t sensing_window,
                                 List_t* transmit_history,
                                 sl_nr_ue_mac_params_t *sl_mac) {

  int new_size = 0;
  int mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  for (int i = 0; i < transmit_history->size; i++) {
    frameslot_t *tr_his_frame_slot = (frameslot_t*)((char*)transmit_history->data + i * transmit_history->element_size);
    LOG_D(NR_MAC, " i %d, Transmit history data: (%4d.%2d) %ld >=  current (%4d.%2d) %ld\n",
           i,
           tr_his_frame_slot->frame,
           tr_his_frame_slot->slot,
           normalize(tr_his_frame_slot, mu),
           frame_slot->frame,
           frame_slot->slot,
           normalize(frame_slot, mu) - sensing_window);
    /*
      normalize(frame_slot, mu) - sensing_window: This condition is to avoid the two cases, where current absolute slot value can be smaller
      than sensing window size. The first case represents the beginning of simulation, there should be more sensing / transmit history data than
      the sensing window size to check for deletion. e.g. if sensing window size is 100ms (200 slots), for first 200 slots, there will not be any
      old data, which should be removed. In the second case, the frame number starts from 0 after completing a cycle of frame numbers (0..1023).
      In that case, current absolute slot value will also be smaller than the sensing window size. When the above condition is true, it checks if
      the sensed data slot lies within the sensing window (implemented by the internal condition), if sensed data absolute slot lies within the
      sensing window, it stops further iterating over the sensing data.
      In the else part, the sensing data / transmit history list contains data from the last part of the frame number cycle (1013..1023) and beginning
      (0..10). In this case, the older data may belong to the range (1013..1023). The new_size contains the older sensed, which should be removed from
      the sensing data / transmit history list.
    */
    if (normalize(frame_slot, mu) - sensing_window > 0) {
      if (normalize(tr_his_frame_slot, mu) >= normalize(frame_slot, mu) - sensing_window) {
        break;
      }
    } else {
      int transmit_history_size = transmit_history->size;
      int prev_frame_data_size = transmit_history_size - normalize(frame_slot, mu);
      if (prev_frame_data_size > 0) {
        new_size += prev_frame_data_size - (abs(normalize(frame_slot, mu) - sensing_window));
      }
      break;
    }
    new_size ++;
  }
  if (new_size > 0) {
    memmove(transmit_history->data, (char*)transmit_history->data + new_size * transmit_history->element_size, (transmit_history->size - new_size) * transmit_history->element_size);
    LOG_D(NR_MAC, "Subtracting %d from %ld\n", new_size, transmit_history->size);
    transmit_history->size -= new_size;
  }
}

  /*
  // This function will be called only for SIDELINK CAPABLE SLOTS.
  // UPLINK SLOT OR MIXED SLOT which is SIDELINK SLOT

  //Determine if PSBCH SLOT and if PSBCH RX/TX should be done
  // IF NOT PSBCH SLOT continue ahead

  // IF RX RES POOL CONFIGURED
  // Determine if SLOT is a RX RES POOL RESERVED
  // OR RX RES POOL RESOURCE SLOT according to time resource bitmap
  // IF resource slot PSCCH RX action should be done

  // IF TX RES POOL CONFIGURED
  // Determine if SLOT is a TX RES POOL RESERVED
  // OR RX RES POOL RESOURCE SLOT according to time resource bitmap
  // IF resource slot PSCCH TX action should be done in case TX is scheduled
  // ELSE SENSING SHOULD BE DONE

  // IF TX/RX ACTION SHOULD BE DONE in this slot
  // SEND SIDELINK TX/RX CONFIG REQUEST TO PHY
*/
void nr_ue_sidelink_scheduler(nr_sidelink_indication_t *sl_ind, NR_UE_MAC_INST_t *mac)
{
  AssertFatal(sl_ind != NULL, "sl_indication cannot be NULL\n");
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  sl_nr_phy_config_request_t *sl_cfg = &sl_mac->sl_phy_config.sl_config_req;
  uint8_t mu = sl_cfg->sl_bwp_config.sl_scs;
  //uint8_t slots_per_frame = nr_slots_per_frame[mu];

  int ue_id = mac->ue_id;
  frame_t frame     = sl_ind->frame_rx;
  slot_t slot       = sl_ind->slot_rx;

  sl_nr_rx_config_request_t rx_config = {0};
  sl_nr_tx_config_request_t tx_config = {0};

  uint8_t tti_action = 0;
  bool is_psbch_slot = false;

  LOG_D(NR_MAC,
        "[UE%d]SL-SCHEDULER: RX %d-%d- TX %d-%d. slot_type:%d\n",
        ue_id,
        sl_ind->frame_rx,
        sl_ind->slot_rx,
        sl_ind->frame_tx,
        sl_ind->slot_tx,
        sl_ind->slot_type);

  // Adjust indices as new timing is acquired
  if (sl_mac->timing_acquired) {
    sl_actions_after_new_timing(sl_mac, ue_id, sl_ind->frame_tx, sl_ind->slot_tx, mac->frame_structure.numb_slots_frame);
    sl_mac->timing_acquired = false;
  }

  if (sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX || sl_ind->slot_type == SIDELINK_SLOT_TYPE_BOTH) {
    int frame = sl_ind->frame_tx;
    int slot = sl_ind->slot_tx;
    int is_sl_slot = 0;
    is_sl_slot = sl_mac->sl_slot_bitmap & (1 << slot);

    if (is_sl_slot) {
      // Check if PSBCH slot and PSBCH should be transmitted or Received
      const uint8_t psbch_action =
          sl_psbch_scheduler(sl_mac, ue_id, frame, slot, mac->frame_structure.numb_slots_frame);
      is_psbch_slot = psbch_action == SL_NR_CONFIG_TYPE_TX_PSBCH;

#if 0 // To be expanded later
      // TBD .. Check for Actions coming out of TX resource pool
      if (!tti_action && sl_mac->sl_TxPool[0])
        tti_action = sl_tx_scheduler(ue_id, frame, slot, sl_mac, sl_mac->sl_TxPool[0]);

      //TBD .. Check for Actions coming out of RX resource pool
      if (!tti_action && sl_mac->sl_RxPool[0])
        tti_action = sl_rx_scheduler(ue_id, frame, slot, sl_mac, sl_mac->sl_RxPool[0]);
#endif

      LOG_D(NR_MAC, "[UE%d]SL-SCHED: TTI - %d:%d scheduled action:%d\n", ue_id, frame, slot, psbch_action);

    } else {
      AssertFatal(1 == 0, "TX SLOT not a sidelink slot. Should not occur\n");
    }

    // Schedule the Tx actions if any
    sl_schedule_tx_actions(sl_ind, mac);
  }

  if (sl_ind->slot_type == SIDELINK_SLOT_TYPE_RX || sl_ind->slot_type == SIDELINK_SLOT_TYPE_BOTH) {
    is_psbch_slot |= sl_mac->future_ttis[sl_ind->slot_rx].sl_action == SL_NR_CONFIG_TYPE_RX_PSBCH;
    sl_schedule_rx_actions(sl_ind, mac);
  }

  bool rx_allowed = true;

  /* TS 38.214 8.1.4 defines selection relative to the trigger slot n, while
   * this worker prepares a later physical TX slot. Keep both time axes: the
   * RX time anchors sensing/selection and the TX time identifies the resource
   * that can actually be emitted by this invocation. */
  frameslot_t selection_frame_slot = {.frame = sl_ind->frame_rx, .slot = sl_ind->slot_rx};
  frameslot_t tx_frame_slot = {.frame = sl_ind->frame_tx, .slot = sl_ind->slot_tx};

  /* Refresh the RLC view before looking for a PSSCH resource. Polling only
   * from preprocess() creates a circular dependency: preprocess() is entered
   * only after a resource is found, while resource selection is triggered by
   * the backlog exposed by this status update. */
  if (sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX
      || sl_ind->slot_type == SIDELINK_SLOT_TYPE_BOTH)
    nr_store_slsch_buffer(mac, frame, slot);

  bool backlog_pending = false;
  SL_UE_iterator(mac->sl_info.list, UE) {
    NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
    /* Include HARQs waiting for PSFCH feedback: their ACK will free a process
     * and immediately allow a new TX.  Pre-selecting a resource window now
     * prevents a 1-2 TX slot gap between ACK arrival and the next transmission. */
    backlog_pending |= sched_ctrl->num_total_bytes > 0
                       || sched_ctrl->retrans_sl_harq.len > 0
                       || sched_ctrl->feedback_sl_harq.len > 0;
  }
  /* Track whether the main block refreshes the window this slot.  The
   * c1/c4/c5/c7 proactive-reselection block must not issue a second
   * get_candidate_resources() call on the same slot: it would orphan the
   * freshly-computed window and overwrite mac->sl_candidate_resources with an
   * identical (but separate) allocation, causing a memory leak and making the
   * resource pointer obtained below dangle relative to the pointer stored in
   * mac->sl_candidate_resources. */
  bool window_refreshed_this_slot = false;
  if (sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX && backlog_pending
      && (!mac->sl_candidate_resources
          || !has_current_or_future_candidate(mac->sl_candidate_resources, tx_frame_slot, mu))) {
    LOG_D(NR_MAC,
          "%4d.%2d SL queue pending with no usable candidate; refreshing the selection window\n",
          frame,
          slot);
    List_t *new_resources =
        get_candidate_resources(&selection_frame_slot, mac, &mac->sl_sensing_data, &mac->sl_transmit_history);
    replace_candidate_resources(mac, new_resources);
    window_refreshed_this_slot = true;
  }

  nr_sl_transmission_params_t *sl_tx_params = &sl_mac->mac_tx_params;
  uint16_t p_prime_rsvp_tx = time_to_slots(mu, sl_tx_params->resel_counter);
  static int8_t is_rsrc_selected = false;

  if (mac->rsc_selection_method == c1 ||
      mac->rsc_selection_method == c4 ||
      mac->rsc_selection_method == c5 ||
      mac->rsc_selection_method == c7) {
    LOG_D(NR_MAC, "%4d.%2d is_rsrc_selected %d, reselection_timer %d, p_prime_rsvp_tx %d, slot_type %d\n",
          frame, slot, is_rsrc_selected, mac->reselection_timer, p_prime_rsvp_tx, sl_ind->slot_type);
    if(is_rsrc_selected && (sl_ind->slot_type == 2) && (mac->reselection_timer < p_prime_rsvp_tx)) {
      mac->reselection_timer++;
    } else if (sl_ind->slot_type == 2) {
      if (mac->reselection_timer < p_prime_rsvp_tx) {
        if (!window_refreshed_this_slot) {
          /* Proactive reselection: refresh the window to potentially find better
           * resources.  Skip if the main block just computed a fresh window on
           * this same slot -- reusing it avoids a redundant allocation and keeps
           * mac->sl_candidate_resources consistent with the resource pointer
           * resolved below. */
          List_t *new_resources =
              get_candidate_resources(&selection_frame_slot, mac, &mac->sl_sensing_data, &mac->sl_transmit_history);
          if (new_resources) {
            replace_candidate_resources(mac, new_resources);
            LOG_D(NR_MAC, "%4d.%2d Returned resources %p\n", frame, slot, mac->sl_candidate_resources);
            print_candidate_list(mac->sl_candidate_resources, __LINE__);
          }
        }
        is_rsrc_selected = true;
      } else {
        mac->reselection_timer = 0;
        is_rsrc_selected = false;
      }
    }
  }

  /* Resolve the current resource only after every possible window refresh.
   * This keeps the pointer within the live list when proactive reselection
   * replaces the previous allocation. */
  sl_resource_info_t *resource = NULL;
  if (mac->sl_candidate_resources && mac->sl_candidate_resources->size > 0
      && sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX) {
    LOG_D(NR_MAC, "%4d.%2d sl_candidate_resources %p size %ld, capacity %ld slot_type %d\n", frame, slot, mac->sl_candidate_resources, mac->sl_candidate_resources->size, mac->sl_candidate_resources->capacity, sl_ind->slot_type);
    resource = get_resource_element(mac->sl_candidate_resources, tx_frame_slot);
    if (resource) {
      LOG_D(NR_MAC, "SELECTED_RESOURCE %4d.%2d slot_type %d, num_sl_pscch_rbs %d, sl_max_num_per_reserve %d, sl_min_time_gap_psfch %d, sl_pscch_sym_start %d, \
            sl_pscch_sym_len %d, sl_psfch_period %d, sl_pssch_sym_start %d, sl_pssch_sym_len %d, sl_subchan_len %d, sl_subchan_size %d\n",
            resource->sfn.frame, resource->sfn.slot, sl_ind->slot_type,
            resource->num_sl_pscch_rbs,
            resource->sl_max_num_per_reserve,
            resource->sl_min_time_gap_psfch,
            resource->sl_pscch_sym_start,
            resource->sl_pscch_sym_len,
            resource->sl_psfch_period,
            resource->sl_pssch_sym_start,
            resource->sl_pssch_sym_len,
            resource->sl_subchan_len,
            resource->sl_subchan_size);
    }
  }

  if (sl_ind->slot_type==SIDELINK_SLOT_TYPE_TX || sl_ind->phy_data==NULL) rx_allowed=false;
  NR_SL_PSFCH_Config_r16_t *sl_psfch_config = mac->sl_tx_res_pool->sl_PSFCH_Config_r16 ? mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup : NULL;
  const uint8_t psfch_periods[] = {0,1,2,4};
  long psfch_period = (sl_psfch_config && sl_psfch_config->sl_PSFCH_Period_r16)
                      ? psfch_periods[*sl_psfch_config->sl_PSFCH_Period_r16] : 0;

  if ((mac->sl_prev_rx_frame != frame || mac->sl_prev_rx_slot != slot) && rx_allowed && !is_psbch_slot) {
      frameslot_t fs;
      fs.frame = frame;
      fs.slot = slot;
      uint64_t rx_abs_slot = normalize(&fs, mu);
      uint8_t pool_id = 0;
      SL_ResourcePool_params_t *sl_rx_rsrc_pool = sl_mac->sl_RxPool[pool_id];
      size_t phy_map_sz = (sl_rx_rsrc_pool->phy_sl_bitmap.size << 3) - sl_rx_rsrc_pool->phy_sl_bitmap.bits_unused;
      rx_allowed = is_sl_slot(mac, &sl_rx_rsrc_pool->phy_sl_bitmap, phy_map_sz, rx_abs_slot);
      long rx_psfch_period = 0;
      if (mac->sl_rx_res_pool->sl_PSFCH_Config_r16 && mac->sl_rx_res_pool->sl_PSFCH_Config_r16->choice.setup
          && mac->sl_rx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16)
        rx_psfch_period = psfch_periods[*mac->sl_rx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16];
      bool rx_slot_has_psfch = slot_has_psfch(mac,
                                              &sl_rx_rsrc_pool->phy_sl_bitmap,
                                              rx_abs_slot,
                                              rx_psfch_period,
                                              phy_map_sz,
                                              mac->SL_MAC_PARAMS->sl_TDD_config);
      LOG_D(NR_MAC,
            "%4d.%2d RX-pool PSFCH=%d period=%ld; TX-pool feedback period=%ld\n",
            frame,
            slot,
            rx_slot_has_psfch,
            rx_psfch_period,
            psfch_period);
      nr_ue_sl_pscch_rx_scheduler(sl_ind,
                                  mac->sl_bwp,
                                  mac->sl_rx_res_pool,
                                  &rx_config,
                                  &tti_action,
                                  rx_slot_has_psfch);

      SL_ResourcePool_params_t *sl_tx_rsrc_pool = sl_mac->sl_TxPool[pool_id];
      size_t tx_phy_map_sz = (sl_tx_rsrc_pool->phy_sl_bitmap.size << 3) - sl_tx_rsrc_pool->phy_sl_bitmap.bits_unused;
      bool feedback_slot_has_psfch = slot_has_psfch(mac,
                                                    &sl_tx_rsrc_pool->phy_sl_bitmap,
                                                    rx_abs_slot,
                                                    psfch_period,
                                                    tx_phy_map_sz,
                                                    mac->SL_MAC_PARAMS->sl_TDD_config);

      /* 37aba09737: If this slot is a PSFCH occasion for any pending HARQ, arm
       * the slot for standalone PSFCH RX (TX UE listening for ACK/NACK).
       * configure_psfch_params_rx() matches HARQs by feedback_frame/feedback_slot;
       * it must be called here, at the PSFCH occasion slot, not from the SCI-2
       * handler (which runs at PSSCH time and always finds 0 matching HARQs). */
      if (feedback_slot_has_psfch && psfch_period > 0) {
        /* TX scheduling and decoded-PSFCH callbacks use sl_sched_lock too.
         * Protect the feedback-list lookup and RX PDU snapshot as one
         * transaction so the look-ahead TX worker cannot change the matched
         * HARQ while its PSFCH receiver configuration is being constructed. */
        NR_UE_SL_SCHED_LOCK(&mac->sl_sched_lock);
        NR_SL_UEs_t *UE_info = &mac->sl_info;
        SL_UE_iterator(UE_info->list, UE) {
          NR_SL_UE_sched_ctrl_t *sched_ctrl = &UE->UE_sched_ctrl;
          int matched = find_current_slot_harqs(frame, slot, sched_ctrl, NULL);
          if (matched > 0) {
            LOG_D(NR_MAC,
                  "%4d.%2d PSFCH RX: %d pending HARQ(s), retaining PSCCH monitoring and arming PSFCH decode\n",
                  frame,
                  slot,
                  matched);
            configure_psfch_params_rx(mac->ue_id, mac, &rx_config);
            if (rx_config.sl_rx_config_list[0].num_psfch_pdus > 0) {
              /* Keep PSCCH monitoring active.  If SCI is found, PHY advances
               * through SCI-2/SLSCH and retains this PSFCH configuration. */
              tti_action = SL_NR_CONFIG_TYPE_RX_PSCCH_PSFCH;
              rx_config.sl_rx_config_list[0].pdu_type = SL_NR_CONFIG_TYPE_RX_PSCCH_PSFCH;
            }
            break;
          }
        }
        NR_UE_SL_SCHED_UNLOCK(&mac->sl_sched_lock);
      }
      mac->sl_prev_rx_frame = frame;
      mac->sl_prev_rx_slot = slot;
  }

  const bool tx_slot_available = mac->is_synced && !is_psbch_slot && sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX;
  if (resource && tx_slot_available) {
    //Check if reserved slot or a sidelink resource configured in Rx/Tx resource pool timeresource bitmap
    nr_ue_sl_pssch_scheduler(mac, sl_ind, mac->sl_bwp, mac->sl_tx_res_pool, &tx_config, resource, &tti_action);
    }
  // TS 38.213 16.3: transmit HARQ-ACK in the selected PSFCH occasion after
  // PSSCH reception. A simultaneous PSCCH/PSSCH allocation is not required.
  NR_SetupRelease_SL_PSFCH_Config_r16_t *configured_psfch = mac->sl_rx_res_pool->sl_PSFCH_Config_r16;
  const long outgoing_psfch_period = configured_psfch && configured_psfch->choice.setup
                                        && configured_psfch->choice.setup->sl_PSFCH_Period_r16
                                    ? psfch_periods[*configured_psfch->choice.setup->sl_PSFCH_Period_r16]
                                    : 0;
  const bool is_feedback_slot = configured_psfch && configured_psfch->choice.setup && tx_slot_available
                                && is_feedback_scheduled(mac, sl_ind->frame_tx, sl_ind->slot_tx);
  if (is_feedback_slot) {
    nr_ue_sl_psfch_scheduler(mac,
                             sl_ind->frame_tx,
                             sl_ind->slot_tx,
                             outgoing_psfch_period,
                             sl_ind,
                             mac->sl_bwp,
                             &tx_config,
                             &tti_action);
      reset_sched_psfch(mac, sl_ind->frame_tx, sl_ind->slot_tx);
  }

  if (((slot % 20) == 6) && ((frame % 100) == 0)) {
    char stats_output[16000] = {0};
    dump_mac_stats_sl(mac, stats_output, sizeof(stats_output), true);
    LOG_D(NR_MAC, "Frame.Slot %d.%d\n%s\n", frame, slot, stats_output);
  }

  #if 0
  if (tti_action == SL_NR_CONFIG_TYPE_RX_PSBCH || tti_action == SL_NR_CONFIG_TYPE_RX_PSCCH || tti_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SCI ||
      tti_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH) {
    fill_scheduled_response(&scheduled_response, NULL, NULL, NULL,  &rx_config, NULL, mod_id, 0,frame, slot, sl_ind->phy_data);
  }
  if (tti_action == SL_NR_CONFIG_TYPE_TX_PSBCH || tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH || tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH || tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_CSI_RS) {
    fill_scheduled_response(&scheduled_response, NULL, NULL, NULL, NULL, &tx_config, mod_id, 0,frame, slot, sl_ind->phy_data);
  }
  #else
  nr_scheduled_response_t scheduled_response = {.module_id = mac->ue_id,
                                                .phy_data = sl_ind->phy_data,
                                                .mac = mac};

  if (tti_action == SL_NR_CONFIG_TYPE_RX_PSCCH || tti_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SCI ||
      tti_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH || tti_action == SL_NR_CONFIG_TYPE_RX_PSSCH_SLSCH_PSFCH
      || tti_action == SL_NR_CONFIG_TYPE_RX_PSFCH || tti_action == SL_NR_CONFIG_TYPE_RX_PSCCH_PSFCH) {
    scheduled_response.sl_rx_config = &rx_config;
      }
  if (tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH_PSFCH || tti_action == SL_NR_CONFIG_TYPE_TX_PSCCH_PSSCH
      || tti_action == SL_NR_CONFIG_TYPE_TX_PSFCH) {
    scheduled_response.sl_tx_config = &tx_config;
    }
   #endif


  LOG_D(NR_MAC,"[UE%d]SL-SCHEDULER: TTI-RX-%d:%d, TX-%d:%d is_psbch_slot:%d TTIaction:%d\n",
                                                            mac->ue_id,sl_ind->frame_rx, sl_ind->slot_rx,
                                                            sl_ind->frame_tx, sl_ind->slot_tx,
                                                            is_psbch_slot, tti_action);

  if (tti_action) {
    frameslot_t frame_slot;
    frame_slot.frame = frame;
    frame_slot.slot = slot;
    if (mac->sl_transmit_history.size > 1)
      remove_old_transmit_history(&frame_slot, sl_mac->sl_TxPool[0]->t0, &mac->sl_transmit_history, sl_mac);
    if (sl_ind->slot_type == SIDELINK_SLOT_TYPE_TX) {
      LOG_D(NR_MAC, "Inserting transmit history data: %4d.%2d\n", frame_slot.frame, frame_slot.slot);
      push_back(&mac->sl_transmit_history, &frame_slot);
    }
    if ((mac->if_module != NULL) && (mac->if_module->scheduled_response != NULL))
      mac->if_module->scheduled_response(&scheduled_response);
  }
  free(rx_config.sl_rx_config_list[0].rx_psfch_pdu_list);
  //NR_UE_SL_SCHED_UNLOCK(&mac->sl_sched_lock);
}

List_t get_nr_sl_comm_opportunities(NR_UE_MAC_INST_t *mac,
                                    uint64_t abs_idx_cur_slot,
                                    uint8_t bwp_id,
                                    uint16_t mu,
                                    uint16_t pool_id,
                                    uint8_t t1,
                                    uint16_t t2,
                                    uint8_t psfch_period) {
  frameslot_t frame_slot;
  List_t slot_info_list;
  init_list(&slot_info_list, sizeof(slot_info_t), 1);
  SL_ResourcePool_params_t *sl_tx_rsrc_pool = mac->SL_MAC_PARAMS->sl_TxPool[pool_id];
  size_t phy_map_sz = (sl_tx_rsrc_pool->phy_sl_bitmap.size << 3) - sl_tx_rsrc_pool->phy_sl_bitmap.bits_unused;
  LOG_D(NR_MAC, "phy_map_sz %zu\n", phy_map_sz);
  NR_SL_ResourcePool_r16_t* resource_pool = get_resource_pool(mac, pool_id);

  uint64_t first_abs_slot_ind = abs_idx_cur_slot + t1;
  uint64_t last_abs_slot_ind = abs_idx_cur_slot + t2;
  size_t abs_pool_index = first_abs_slot_ind % phy_map_sz;

  frameslot_t fs0;
  de_normalize(abs_idx_cur_slot, mu, &fs0);

  frameslot_t fs1;
  de_normalize(first_abs_slot_ind, mu, &fs1);

  frameslot_t fs2;
  de_normalize(last_abs_slot_ind, mu, &fs2);

  bool sl_has_psfch = false;
  for (uint64_t i = first_abs_slot_ind; i <= last_abs_slot_ind; i++) {
    if (is_sl_slot(mac, &sl_tx_rsrc_pool->phy_sl_bitmap, phy_map_sz, i)) // slot is a sidelink slot
    {
      // PSCCH
      // Number of  RBs used for PSCCH
      uint8_t num_sl_pscch_rbs = pscch_rb_table[*resource_pool->sl_PSCCH_Config_r16->choice.setup->sl_FreqResourcePSCCH_r16];
      // Starting RE of the lowest subchannel in a resource where PSCCH
      // freq domain allocation starts
      uint8_t pscch_startrb = *resource_pool->sl_StartRB_Subchannel_r16;
      // Number of symbols used for PSCCH
      uint16_t num_sl_pscch_sym = pscch_tda[*resource_pool->sl_PSCCH_Config_r16->choice.setup->sl_TimeResourcePSCCH_r16];
      LOG_D(NR_MAC, "pscch_startrb %d, num_sl_pscch_sym %d, pscch_numrbs %d\n",
            pscch_startrb,
            num_sl_pscch_sym,
            num_sl_pscch_rbs);
      uint8_t start_sl_pscch_sym = 1;
      // PSSCH
      uint16_t sl_pssch_sym_start = *mac->sl_bwp->sl_BWP_Generic_r16->sl_StartSymbol_r16;
      sl_has_psfch = slot_has_psfch(mac, &sl_tx_rsrc_pool->phy_sl_bitmap, i, psfch_period, phy_map_sz, mac->SL_MAC_PARAMS->sl_TDD_config);
      int num_psfch_symbols = 0;
      if (sl_has_psfch && resource_pool->sl_PSFCH_Config_r16 && resource_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16
          && *resource_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16 > 0) {
        // As per 38214 8.1.3.2, num_psfch_symbols can be 3 if psfch_overhead_indication.nbits is 1; FYI psfch_overhead_indication.nbits is set to 1 in case of PSFCH period 2 or 4 in sl_determine_sci_1a_len()
        num_psfch_symbols = 3;
      }

      // PSFCH requires an additional 3 symbols
      uint16_t sl_pssch_sym_len = 7 + *mac->sl_bwp->sl_BWP_Generic_r16->sl_LengthSymbols_r16 - num_psfch_symbols - 2;
      LOG_D(NR_MAC, "Tx sl_has_psfch %d, %4d.%2d sl_pssch_sym_len %d\n", sl_has_psfch, frame_slot.frame, frame_slot.slot, sl_pssch_sym_len);

      uint16_t sl_subchannel_size = sl_get_subchannel_size(resource_pool);
      uint16_t sl_max_num_reserve = *resource_pool->sl_UE_SelectedConfigRP_r16->sl_MaxNumPerReserve_r16;
      uint64_t abs_slot_idx = i;
      uint64_t st_offset = (i - abs_idx_cur_slot);

      slot_info_t slot_info = {.sl_pscch_sym_start = start_sl_pscch_sym,
                               .sl_pscch_sym_len   = num_sl_pscch_sym,
                               .num_sl_pscch_rbs   = num_sl_pscch_rbs,
                               .sl_pssch_sym_start = sl_pssch_sym_start,
                               .sl_pssch_sym_len   = sl_pssch_sym_len,
                               .slot_offset        = st_offset,
                               .abs_slot_index     = abs_slot_idx,
                               .sl_max_num_per_reserve = sl_max_num_reserve,
                               .sl_sub_chan_size       = sl_subchannel_size,
                               .sl_has_psfch           = sl_has_psfch};
      de_normalize(slot_info.abs_slot_index, mu, &frame_slot);
      LOG_D(NR_MAC, "Pushing %4d.%2d\n", frame_slot.frame, frame_slot.slot);
      validate_selected_sl_slot(true , false, mac->SL_MAC_PARAMS->sl_TDD_config, frame_slot);
      push_back(&slot_info_list, &slot_info);
    }
    abs_pool_index = (abs_pool_index + 1) % phy_map_sz;
  }

  LOG_D(NR_MAC, "Total number of slots available for Sidelink in the selection window = %ld\n", slot_info_list.size);

#ifdef SLOT_INFO_DEBUG
  for (size_t i = 0; i < slot_info_list.size; i++) {
    slot_info_t *slot_inf = (slot_info_t*)((char*)slot_info_list.data + i * slot_info_list.element_size);
    LOG_D(NR_MAC, "sidelink pscch (sym_start %d, sym_len %d, pscch_rbs %d), slot_offset %d, abs_slot_index %ld, max_num_per_reserve %d, sub_chan_size %d\n",
          slot_inf->sl_pscch_sym_start,
          slot_inf->sl_pscch_sym_len,
          slot_inf->num_sl_pscch_rbs,
          slot_inf->slot_offset,
          slot_inf->abs_slot_index,
          slot_inf->sl_max_num_per_reserve,
          slot_inf->sl_sub_chan_size);
  }
#endif
  return slot_info_list;
}

static bool sl_bch_has_ssb_in_slot(const sl_bch_params_t *sl_bch, uint64_t abs_slot, int slots_per_frame)
{
  if (!sl_bch->status || sl_bch->ssb_time_alloc.sl_NumSSB_WithinPeriod == 0)
    return false;

  const uint64_t slot_in_16_frames = abs_slot % (SL_NR_SSB_REPETITION_IN_FRAMES * slots_per_frame);
  for (uint16_t i = 0; i < sl_bch->ssb_time_alloc.sl_NumSSB_WithinPeriod; i++) {
    const uint64_t ssb_slot = sl_bch->ssb_time_alloc.sl_TimeOffsetSSB + i * sl_bch->ssb_time_alloc.sl_TimeInterval;
    if (slot_in_16_frames == ssb_slot)
      return true;
  }
  return false;
}

int get_physical_sl_pool(NR_UE_MAC_INST_t *mac, BIT_STRING_t *sl_time_rsrc, BIT_STRING_t *phy_sl_bitmap)
{
  /* Construct the physical resource-pool slot set over the complete 1024-frame
   * DFN/SFN cycle according to TS 38.214 clause 8. */
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  uint8_t mu = get_softmodem_params()->numerology;
  int n_slots_frame = nr_slots_per_frame[mu];
  AssertFatal(sl_mac->sl_TDD_config != NULL, "Cannot construct an SL resource pool without a TDD configuration\n");
  NR_TDD_UL_DL_Pattern_t *tdd = &sl_mac->sl_TDD_config->pattern1;
  const int nr_slots_period = n_slots_frame / get_nb_periods_per_frame(tdd->dl_UL_TransmissionPeriodicity);
  const int sl_bitmap_num_bits = (sl_time_rsrc->size << 3) - sl_time_rsrc->bits_unused;
  const int phy_sl_bits = SL_FRAME_NUMBER_CYCLE * n_slots_frame;
  const int phy_sl_capacity = (phy_sl_bitmap->size << 3) - phy_sl_bitmap->bits_unused;
  AssertFatal(sl_bitmap_num_bits > 0, "sl-TimeResource cannot be empty\n");
  AssertFatal(phy_sl_capacity == phy_sl_bits,
              "Physical SL bitmap capacity %d does not cover the 1024-frame cycle (%d slots)\n",
              phy_sl_capacity,
              phy_sl_bits);

  const size_t bitmap_bytes = (phy_sl_bits + 7) / 8;
  uint8_t *eligible_bitmap = calloc(bitmap_bytes, sizeof(*eligible_bitmap));
  AssertFatal(eligible_bitmap != NULL, "Unable to allocate SL eligibility bitmap\n");
  memset(phy_sl_bitmap->buf, 0, phy_sl_bitmap->size);

  size_t eligible_slots = 0;
  for (uint64_t abs_slot = 0; abs_slot < phy_sl_bits; abs_slot++) {
    const bool symbols_are_ul = nr_sl_tdd_slot_supports_symbols(abs_slot % nr_slots_period,
                                                                nr_slots_period,
                                                                tdd->nrofUplinkSlots,
                                                                tdd->nrofUplinkSymbols,
                                                                sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_start_symbol,
                                                                sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_num_symbols);
    const bool is_ssb = sl_bch_has_ssb_in_slot(&sl_mac->rx_sl_bch, abs_slot, n_slots_frame)
                        || sl_bch_has_ssb_in_slot(&sl_mac->tx_sl_bch, abs_slot, n_slots_frame);
    if (symbols_are_ul && !is_ssb) {
      nr_sl_bitmap_set(eligible_bitmap, abs_slot);
      eligible_slots++;
    }
  }

  const size_t reserved_slots = eligible_slots % sl_bitmap_num_bits;
  const size_t pool_slots = nr_sl_build_physical_pool_bitmap(
      sl_time_rsrc->buf, sl_bitmap_num_bits, eligible_bitmap, phy_sl_bits, phy_sl_bitmap->buf);
  free(eligible_bitmap);

  LOG_I(NR_MAC,
        "SL pool bitmap: physical=%d, TDD/S-SSB eligible=%zu, reserved=%zu, assigned=%zu, L_bitmap=%d\n",
        phy_sl_bits,
        eligible_slots,
        reserved_slots,
        pool_slots,
        sl_bitmap_num_bits);

#ifdef BITMAP_DEBUG
  for (int i = 0; i < (phy_sl_bits + 7) >> 3; i++)
    LOG_D(NR_MAC, "phy_sl_bitmap[%d] %2x\n", i, phy_sl_bitmap->buf[i]);
#endif

  return phy_sl_bits;
}

List_t* get_candidate_resources_from_slots(frameslot_t *sfn,
                                           uint8_t psfch_period,
                                           uint8_t min_time_gap_psfch,
                                           uint16_t l_subch,
                                           uint16_t total_subch,
                                           List_t* slot_info,
                                           uint8_t mu) {
  LOG_D(NR_MAC, "%4d.%2d, psfch_period %d,  min_time_gap_psfch %d\n",
        sfn->frame, sfn->slot, psfch_period,  min_time_gap_psfch);

  List_t *nr_resource_list = (List_t *)malloc16_clear(sizeof(*nr_resource_list));
  init_list(nr_resource_list, sizeof(sl_resource_info_t), 1);
  for (int s = 0; s < slot_info->size; s++) {
    for (uint16_t i = 0; i + l_subch <= total_subch; i++) {
        slot_info_t *s_info = (slot_info_t*)((char*)slot_info->data + s * slot_info->element_size);
        frameslot_t frame_slot;
        de_normalize(normalize(sfn, mu) + s_info->slot_offset, mu, &frame_slot);
        sl_resource_info_t rsrc_info = {.num_sl_pscch_rbs = s_info->num_sl_pscch_rbs,
                                        .sl_pscch_sym_start = s_info->sl_pscch_sym_start,
                                        .sl_pscch_sym_len = s_info->sl_pscch_sym_len,
                                        .sl_pssch_sym_start = s_info->sl_pssch_sym_start,
                                        .sl_pssch_sym_len = s_info->sl_pssch_sym_len,
                                        .sl_subchan_size = s_info->sl_sub_chan_size,
                                        .sl_subchan_start = i,
                                        .sl_subchan_len = l_subch,
                                        .sl_max_num_per_reserve = s_info->sl_max_num_per_reserve,
                                        .sfn = frame_slot,
                                        .sl_psfch_period = psfch_period,
                                        .sl_min_time_gap_psfch = min_time_gap_psfch};
        LOG_D(NR_MAC, "abs slot %ld, capacity %ld size %ld subchan: %d/%d slot %d/%ld frame_slot %4d.%2d\n",
              normalize(sfn, mu) + s_info->slot_offset,
              nr_resource_list->capacity, nr_resource_list->size, i, total_subch, s, slot_info->size, rsrc_info.sfn.frame, rsrc_info.sfn.slot);
        push_back(nr_resource_list, &rsrc_info);
    }
  }
  return nr_resource_list;
}

void exclude_resources_based_on_history(frameslot_t frame_slot,
                                        List_t* transmit_history,
                                        List_t* candidate_resources,
                                        List_t* sl_rsrc_rsrv_period_list,
                                        uint8_t mu) {

  LOG_D(NR_MAC, "abs_slot %ld, size (transmit_history: %ld, candidate_resources: %ld, sl_rsrc_rsrv_period: %ld)\n",
        normalize(&frame_slot, 1), transmit_history->size, candidate_resources->size, sl_rsrc_rsrv_period_list->size);

  List_t sfn_to_exclude; // SFN slot numbers (normalized) to exclude
  init_list(&sfn_to_exclude, sizeof(uint64_t), 1);
  sl_resource_info_t* sl_rsrc_info = (sl_resource_info_t*) get_front(candidate_resources);
  uint64_t first_sfn_norm = normalize(&sl_rsrc_info->sfn, mu); // lowest candidate SFN slot number

  sl_rsrc_info = (sl_resource_info_t*) get_back(candidate_resources);
  uint64_t last_sfn_norm = normalize(&sl_rsrc_info->sfn, mu); // highest candidate SFN slot number
  LOG_D(NR_MAC, "Excluding resources between SFNs (%lu, %lu)\n", first_sfn_norm, last_sfn_norm);

  // Iterate the resource reserve period list and the transmit history to
  // find all slot numbers such that multiples of the reserve period, when
  // added to the history's slot number, are within the candidate resource
  // slots lowest and highest numbers
  for (int k = 0; k < sl_rsrc_rsrv_period_list->size; k++) {
    uint16_t *rsrv_period = (uint16_t*)((char*)sl_rsrc_rsrv_period_list->data + k * sl_rsrc_rsrv_period_list->element_size);
    if (*rsrv_period == 0) {
        continue; // 0ms value is ignored
    }
    *rsrv_period = *rsrv_period * (1 << mu); // Convert from ms to slots
    for (int j = 0; j < transmit_history->size; j++) {
      uint16_t i = 1;
      frameslot_t *sfn = (frameslot_t*)((char*)transmit_history->data + j * transmit_history->element_size);
      uint64_t sfn_to_check = normalize(sfn, mu) + (*rsrv_period);
      while (sfn_to_check <= last_sfn_norm) {
        if (sfn_to_check >= first_sfn_norm) {
          push_back(&sfn_to_exclude, &sfn_to_check);
        }
        i++;
        sfn_to_check = normalize(sfn, mu) + (i) * (*rsrv_period);
      }
    }
  }

  // sfn_to_exclude is a set of SFN normalized slot numbers for which we need
  // to exclude (erase) any candidate resources that match
  for (int k = 0; k < sfn_to_exclude.size; k++) {
    uint64_t *norm_sfn = (uint64_t*)((char*)sfn_to_exclude.data + k * sfn_to_exclude.element_size);
    for (int j = 0; j < candidate_resources->size; j++) {
      sl_resource_info_t *rsrc_info = (sl_resource_info_t*)((char*)candidate_resources->data + j * candidate_resources->element_size);
      uint64_t norm_rsrc_info_sfn = normalize(&rsrc_info->sfn, mu);
      if (norm_rsrc_info_sfn == *norm_sfn)
      {
        LOG_D(NR_MAC, "Erasing candidate resource at %lu\n", *norm_sfn);
        delete_at(candidate_resources, j);
      }
    }
  }
}

List_t exclude_reserved_resources(sensing_data_t *sensed_data,
                                  float slot_period_ms,
                                  uint16_t resv_period_slots,
                                  uint16_t t1,
                                  uint16_t t2,
                                  uint8_t mu) {

  LOG_D(NR_MAC, "sfn %ld, %4d.%2d slot_period %f, resv_period_slots %d, gap_re_tx1 %d, gap_re_tx2 %d\n",
        normalize(&sensed_data->frame_slot, mu),
        sensed_data->frame_slot.frame,
        sensed_data->frame_slot.slot,
        slot_period_ms,
        resv_period_slots,
        sensed_data->gap_re_tx1,
        sensed_data->gap_re_tx2);

  List_t resource_list;
  init_list(&resource_list, sizeof(reserved_resource_t), 1);
  AssertFatal(slot_period_ms <= 1, "Slot length can not exceed 1 ms\n");
  // slot range is [n + T1, n + T2] (both endpoints included)
  uint16_t window_slots = (t2 - t1) + 1; // selection window length in physical slots
  double t_scal_ms = window_slots * slot_period_ms; // Parameter T_scal in the algorithm
  double p_rsvp_ms = (double)(sensed_data->rsvp); // Parameter Pprime_rsvp_rx in algorithm
  uint16_t q = 0;                                        // Parameter Q in the algorithm

  if (sensed_data->rsvp != 0) {
    if (p_rsvp_ms < t_scal_ms) {
      q = (uint16_t)(ceil(t_scal_ms / p_rsvp_ms));
    } else {
      q = 1;
    }
    LOG_D(NR_MAC, "t_scal_ms: %lf, p_rsvp_ms: %lf\n", t_scal_ms, p_rsvp_ms);
  }

  uint16_t p_prime_rsvp_rx = resv_period_slots;
  for (uint16_t i = 1; i <= q; i++) {
    reserved_resource_t resource = {.sfn = sensed_data->frame_slot,
                                    .rsvp = sensed_data->rsvp,
                                    .sb_ch_length = sensed_data->subch_len,
                                    .sb_ch_start = sensed_data->subch_start,
                                    .prio = sensed_data->prio,
                                    .sl_rsrp = sensed_data->sl_rsrp
                                    };
    resource.sfn = add_to_sfn(&resource.sfn, p_prime_rsvp_rx, mu);
    push_back(&resource_list, &resource);
    if (sensed_data->gap_re_tx1 != 0 && sensed_data->gap_re_tx1 != 0xFF) {
      reserved_resource_t re_tx1_slot = resource;
      re_tx1_slot.sfn = add_to_sfn(&re_tx1_slot.sfn, sensed_data->gap_re_tx1, mu);
      re_tx1_slot.sb_ch_length = sensed_data->subch_len;
      re_tx1_slot.sb_ch_start = sensed_data->subch_startre_tx1;
      push_back(&resource_list, &re_tx1_slot);
    }
    if (sensed_data->gap_re_tx1 != 0 && sensed_data->gap_re_tx2 != 0xFF) {
      reserved_resource_t re_tx2_slot = resource;
      re_tx2_slot.sfn = add_to_sfn(&re_tx2_slot.sfn, sensed_data->gap_re_tx2, mu);
      re_tx2_slot.sb_ch_length = sensed_data->subch_len;
      re_tx2_slot.sb_ch_start = sensed_data->subch_startre_tx2;
      push_back(&resource_list, &re_tx2_slot);
    }
  }
  LOG_D(NR_MAC, "q: %d,  Size of resource_list: %ld\n", q, resource_list.size);
  return resource_list;
}

bool overlapped_resource(uint8_t first_start,
                         uint8_t first_length,
                         uint8_t second_start,
                         uint8_t second_length) {
  AssertFatal(first_length && second_length, "Length should not be zero\n");
  return (max(first_start, second_start) < min(first_start + first_length, second_start + second_length));
}

uint8_t get_lower_bound_resel_counter(uint16_t p_rsrv) {
    AssertFatal(p_rsrv < 100, "Resource reservation must be less than 100 ms");
    uint8_t l_bound = (5 * ceil(100 / (max(20, p_rsrv))));
    return l_bound;
}

uint8_t get_upper_bound_resel_counter(uint16_t p_rsrv) {
    AssertFatal(p_rsrv < 100, "Resource reservation must be less than 100 ms");
    uint8_t u_bound = (15 * ceil(100 / (max((20), p_rsrv))));
    return u_bound;
}

uint8_t get_random_reselection_counter(uint16_t rri) {
    uint8_t min_res_cntr = 0;
    uint8_t max_res_cntr = 0;

    switch (rri)
    {
    case 100:
    case 150:
    case 200:
    case 250:
    case 300:
    case 350:
    case 400:
    case 450:
    case 500:
    case 550:
    case 600:
    case 700:
    case 750:
    case 800:
    case 850:
    case 900:
    case 950:
    case 1000:
        min_res_cntr = 5;
        max_res_cntr = 15;
        break;
    default:
        if (rri < 100) {
          min_res_cntr = get_lower_bound_resel_counter(rri);
          max_res_cntr = get_upper_bound_resel_counter(rri);
        } else {
            LOG_E(NR_MAC, "Value not supported!");
        }
        break;
    }

    LOG_D(NR_MAC, "Range to choose random reselection counter. min: %d max: %d\n", min_res_cntr, max_res_cntr);
    return min_res_cntr;
}

List_t* get_candidate_resources(frameslot_t *frame_slot, NR_UE_MAC_INST_t *mac, List_t *sensing_data, List_t *transmit_history) {

  uint16_t pool_id = 0;
  uint8_t bwp_id = 0;
  sl_nr_ue_mac_params_t *sl_mac = mac->SL_MAC_PARAMS;
  uint8_t mu = sl_mac->sl_phy_config.sl_config_req.sl_bwp_config.sl_scs;
  nr_sl_transmission_params_t *sl_tx_params = &sl_mac->mac_tx_params;
  uint8_t t1 = sl_mac->sl_TxPool[pool_id]->t1;
  uint8_t tproc1 = sl_mac->sl_TxPool[pool_id]->tproc1;
  uint16_t t2 = get_t2(pool_id, mu, sl_tx_params, sl_mac);

  AssertFatal(check_t1_within_tproc1(mu, t1), "Configured t1 %d is greater than tproc1 %d for this numerology", t1, tproc1);

  LOG_D(NR_MAC, "Transmit  size: %ld; sensing data size: %ld\n", transmit_history->size, sensing_data->size);

  List_t candidate_slots;
  List_t *candidate_resources;
  uint64_t abs_slot_ind = normalize(frame_slot, mu);

  // Check the validity of the resource selection window configuration (t1 and t2)
  // and the following parameters: numerology and reservation period.
  uint16_t num_slots_mul_s_dur_ms = (t2 - t1 + 1) * (1 / pow(2, mu)); // number of slots multiplied by the slot duration in ms

  uint16_t rsvpMs = sl_tx_params->rri;
  LOG_D(NR_MAC, "abs_slot_ind %ld, %4d.%2d rsvpMs %hu, num_slots_mul_s_dur %d, t2 %d, t1 %d, (t2 - t1 + 1) %d, tproc1 %d\n",
        abs_slot_ind, frame_slot->frame, frame_slot->slot,
        rsvpMs, num_slots_mul_s_dur_ms, t2, t1, (t2 - t1 + 1),
        tproc1);
  AssertFatal(rsvpMs != 0 && num_slots_mul_s_dur_ms <= rsvpMs, "An error may be generated due to the fact that the resource selection window" \
                    "size is higher than the resource reservation period value. Make sure that " \
                    "(T2-T1+1) x (1/(2^numerology)) < reservation period. Modify the values of T1, " \
                    "T2, numerology, and reservation period accordingly.");

  uint16_t total_subch = *mac->sl_tx_res_pool->sl_NumSubchannel_r16;
  uint16_t l_subch = sl_tx_params->l_subch;
  AssertFatal(l_subch > 0 && l_subch <= total_subch,
              "PSSCH L_subCH must be in [1, %u], configured %u\n",
              total_subch,
              l_subch);
  uint8_t psfch_time_gaps[] = {2, 3};
  uint8_t min_time_gap_psfch = mac->sl_tx_res_pool->sl_PSFCH_Config_r16 ? psfch_time_gaps[*mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_MinTimeGapPSFCH_r16] : 0;

  uint8_t psfch_period = 0;
  const uint8_t psfch_periods[] = {0,1,2,4};
  psfch_period = (mac->sl_tx_res_pool->sl_PSFCH_Config_r16 &&
                  mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16)
                  ? psfch_periods[*mac->sl_tx_res_pool->sl_PSFCH_Config_r16->choice.setup->sl_PSFCH_Period_r16] : 0;

  // step 4 as per TS 38.214 sec 8.1.4
  // Find sidelink slots from tx_phy_sl_bitmap
  candidate_slots = get_nr_sl_comm_opportunities(mac,
                                                 abs_slot_ind,
                                                 bwp_id,
                                                 mu,
                                                 pool_id,
                                                 t1,
                                                 t2,
                                                 psfch_period);

  if (candidate_slots.size == 0 ) {
    /* TS 38.214 8.1.4 restricts candidate resources to [n+T1,n+T2], with T2
     * bounded by the packet delay budget when it is known.  Expanding an empty
     * window to the reservation period can select a resource after that bound.
     * Return no resource; the next trigger will construct a new bounded window. */
    LOG_W(NR_MAC,
          "%4d.%2d No SL TX slots in selection window [T1=%u T2=%u, PDB=%u ms]; deferring selection\n",
          frame_slot->frame,
          frame_slot->slot,
          t1,
          t2,
          sl_tx_params->packet_delay_budget_ms);
      free_list_mem(&candidate_slots);
    return NULL;
  }

  // Get candidate resources from sidelink slots
  candidate_resources = get_candidate_resources_from_slots(frame_slot,
                                                           psfch_period,
                                                           min_time_gap_psfch,
                                                           l_subch,
                                                           total_subch,
                                                           &candidate_slots,
                                                           mu);
  free_list_mem(&candidate_slots);
  print_candidate_list(candidate_resources, __LINE__);

  uint64_t m_total = candidate_resources->size; // total number of candidate single-slot resources

  // This is an optimization to skip further null processing below
  if ((sensing_data->size == 0) && (transmit_history->size == 0))
  {
    LOG_D(NR_MAC, "No sensing or data found: Total slots selected %ld\n", m_total);
    return candidate_resources;
  }

  // Copy the buffer so we can trim the buffer as per Tproc0.
  // Note, we do not need to delete the latest measurement
  // from the original buffer because it will be deleted
  // by RemoveOldSensingData method once it is outdated.

  List_t *updated_sensing_data = sensing_data;

  // latest sensing data is at the end of the list
  // now remove the sensing data as per the value of Tproc0. This would
  // keep the size of the buffer equal to [n – T0 , n – Tproc0)

  update_sensing_data(updated_sensing_data, frame_slot, sl_mac, pool_id);

  // Perform a similar operation on the transmit history.
  // latest is at the end of the list
  // keep the size of the buffer equal to [n – T0 , n – Tproc0)
  List_t *updated_history = transmit_history;

  update_transmit_history(updated_history, frame_slot, sl_mac, pool_id);

  // step 5: filter candidateResources based on transmit history, if threshold
  // defined in step 5a) is met
  List_t *remaining_candidates = candidate_resources;
  LOG_D(NR_MAC, "size: (candidate_resources %ld, remaining_candidates %ld, updated_history %ld)\n",
        candidate_resources->size, remaining_candidates->size, updated_history->size);

  // Exclude resources function may not be effective if updated history is empty
  List_t *rsrc_rsrvation_period_list = malloc16_clear(sizeof(*rsrc_rsrvation_period_list));
  init_list(rsrc_rsrvation_period_list, sizeof(long), 1);
  push_back(rsrc_rsrvation_period_list, &sl_tx_params->rri);
  exclude_resources_based_on_history(*frame_slot, updated_history, remaining_candidates, rsrc_rsrvation_period_list, mu);

  LOG_D(NR_MAC, "sl_res_ratio %f, %lf\n",
        sl_tx_params->sl_res_ratio, (sl_tx_params->sl_res_ratio * m_total));
  if (remaining_candidates->size >= (sl_tx_params->sl_res_ratio * m_total)) {
    LOG_D(NR_MAC, "Step 5a check allows step 5 to pass: original: %ld  remaining: %ld X: %lf\n",
          candidate_resources->size, remaining_candidates->size, sl_tx_params->sl_res_ratio);
  } else {
    LOG_D(NR_MAC, "Step 5a fails-- too few remaining candidates: original: %ld  updated: %ld  X: %lf", candidate_resources->size, remaining_candidates->size, sl_tx_params->sl_res_ratio);
    remaining_candidates = candidate_resources;
  }

  // step 6

  // calculate all possible transmissions based on sensed SCIs,
  // with past transmissions projected into the selection window.
  // Using a vector of ReservedResource, since we need to check all the SCIs
  // and their possible future transmission that are received during the
  // above trimmed sensing window. Each element of the vector holds a
  // list that holds the info of each received SCI and its possible
  // future transmissions.

  vec_of_list_t sensing_data_projections;
  init_vector(&sensing_data_projections, 1);
  add_list(&sensing_data_projections, sizeof(reserved_resource_t), 1);
  uint8_t nr_slots_per_subframe = pow(2, mu);
  float slot_duraton_ms = (1.0 / nr_slots_per_subframe);
  print_sensing_data_list(updated_sensing_data, __LINE__);
  for (int k = 0; k < updated_sensing_data->size; k++) {
    sensing_data_t *itr_sdata = (sensing_data_t*)((char*)updated_sensing_data->data + k * updated_sensing_data->element_size);
    uint16_t resv_period_slots = time_to_slots(mu, itr_sdata->rsvp);
    LOG_D(NR_MAC, "sfn %ld, %4d.%2d slot_period %f, resv_period_slots %d, gap_re_tx1 %d, gap_re_tx2 %d\n",
          normalize(&itr_sdata->frame_slot, mu),
          itr_sdata->frame_slot.frame,
          itr_sdata->frame_slot.slot,
          slot_duraton_ms,
          resv_period_slots,
          itr_sdata->gap_re_tx1,
          itr_sdata->gap_re_tx2);
    itr_sdata->gap_re_tx1 = 0;
    itr_sdata->gap_re_tx2 = 0;
    List_t temp_rsrc_list = exclude_reserved_resources(itr_sdata,
                                                       slot_duraton_ms,
                                                       resv_period_slots,
                                                       t1,
                                                       t2,
                                                       mu);
    LOG_D(NR_MAC, "k %d, Inserting list of size %ld\n", k, temp_rsrc_list.size);
    push_back_list(&sensing_data_projections, &temp_rsrc_list);
  }

  int rsrp_threshold = sl_tx_params->sl_thresh_rsrp;
  List_t* candidate_resources_after_step5 = remaining_candidates;
  int counter_c = 0;
  do
  {
    // following assignment is needed since we might have to perform
    // multiple do-while over the same list by increasing the rsrpThreshold
    remaining_candidates = candidate_resources_after_step5;
    LOG_D(NR_MAC, "Step 6 loop iteration checking %ld resources against threshold %d resel counter %d counter_c %d\n",
          remaining_candidates->size, rsrp_threshold, sl_tx_params->resel_counter, counter_c);

    // itr_rsrc is the candidate single-slot resource R_x, y
    // k increment is conditional based on delete action
    int k = 0;
    while ( k < remaining_candidates->size) {
      sl_resource_info_t *itr_rsrc = (sl_resource_info_t*)((char*)remaining_candidates->data + k * remaining_candidates->element_size);
      bool erased = false;
      itr_rsrc->slot_busy = false;
      // calculate all proposed transmissions of current candidate resource within selection
      // window
      List_t *resource_info_list = calloc(1, sizeof(*resource_info_list));
      init_list(resource_info_list, sizeof(sl_resource_info_t), 1);
      uint16_t p_prime_rsvp_tx = time_to_slots(mu, sl_tx_params->rri);
      for (uint16_t i = 0; i < sl_tx_params->resel_counter; i++) {
        sl_resource_info_t sl_resource_info;
        sl_resource_info.sfn = itr_rsrc->sfn;
        frameslot_t fs = sl_resource_info.sfn;
        sl_resource_info.sfn = add_to_sfn(&fs, p_prime_rsvp_tx, mu);
        LOG_D(NR_MAC, "sfn %4d.%2d, %4d.%2d i * p_prime_rsvp_tx %d\n", itr_rsrc->sfn.frame, itr_rsrc->sfn.slot, fs.frame, fs.slot, i * p_prime_rsvp_tx);
        push_back(resource_info_list, &sl_resource_info);
      }

      // Traverse over all the possible transmissions derived from each sensed SCI
      for (int i = 0; i < sensing_data_projections.size; i++) {
        List_t proj_reserved_rsc_list = sensing_data_projections.lists[i];
        print_reserved_list(&proj_reserved_rsc_list, __LINE__);
        // for all proposed transmissions of current candidate resource
        for (int j = 0; j < resource_info_list->size; j++) {
          sl_resource_info_t *future_cand_info = (sl_resource_info_t*)((char*)resource_info_list->data + j * resource_info_list->element_size);

          // Traverse the list of future projected transmissions for the given sensed SCI
          for (int l = 0; l < proj_reserved_rsc_list.size; l++) {
            reserved_resource_t *rsrvd_rsc = (reserved_resource_t*)((char*)proj_reserved_rsc_list.data + l * proj_reserved_rsc_list.element_size);
            LOG_D(NR_MAC, "future candidate %ld rsrvd_rsc candidate %ld\n", normalize(&future_cand_info->sfn, mu), normalize(&rsrvd_rsc->sfn, mu));
            // If overlapped in time ...
            if (normalize(&future_cand_info->sfn, mu) == normalize(&rsrvd_rsc->sfn, mu)) {
              LOG_D(NR_MAC, "%4d.%2d rsrvd_rsc->sl_rsrp %lf, rsrp_threshold %d\n", rsrvd_rsc->sfn.frame, rsrvd_rsc->sfn.slot, rsrvd_rsc->sl_rsrp, rsrp_threshold);
              // And above the current threshold ...
              if (rsrvd_rsc->sl_rsrp > rsrp_threshold) {
                // And overlapped in frequency ...
                if (overlapped_resource(rsrvd_rsc->sb_ch_start,
                                        rsrvd_rsc->sb_ch_length,
                                        itr_rsrc->sl_subchan_start,
                                        itr_rsrc->sl_subchan_len)) {
                  LOG_D(NR_MAC, "%4d.%2d Overlapped resource %ld occupied %d subchannels index %d\n",
                        rsrvd_rsc->sfn.frame, rsrvd_rsc->sfn.slot,
                        normalize(&itr_rsrc->sfn, mu), rsrvd_rsc->sb_ch_length, rsrvd_rsc->sb_ch_start);
                  delete_at(remaining_candidates, k);
                  LOG_D(NR_MAC, "Resource %ld %4d.%2d : [%d,%d] erased. Its rsrp : %lf  Threshold : %d\n",
                        normalize(&itr_rsrc->sfn, mu),
                        itr_rsrc->sfn.frame,
                        itr_rsrc->sfn.slot,
                        itr_rsrc->sl_subchan_start,
                        (itr_rsrc->sl_subchan_start + itr_rsrc->sl_subchan_len - 1),
                        rsrvd_rsc->sl_rsrp,
                        rsrp_threshold);
                  erased = true; // Used to break out of outer for loop of sensed
                                 // data projections
                  break; // Stop further evaluation because candidate is erased
                } else {
                  // Although not overlapping in frequency, overlapped in time
                  future_cand_info->slot_busy = true;
                }
              }
            }
          }
          /* Candidate k no longer exists.  Stop using itr_rsrc and do not
           * delete the element shifted into the same index for another
           * projected transmission of the removed candidate. */
          if (erased)
            break;
        }
        if (erased) {
          break; // break for proj_reserved_rsc_list
        }
      }
      if (!erased) {
        // Only need to increment if not erased above; if erased, the erase()
        // action will point itCandidate to the next item
        k++;
      }
    } //end of while

    // step 7. If the following while will not break, start over do-while
    // loop with rsrpThreshold increased by 3dB
    rsrp_threshold += 3;
    if (rsrp_threshold > 0) {
      // 0 dBm is the maximum RSRP threshold level so if we reach
      // it, that means all the available slots are overlapping
      // in time and frequency with the sensed slots, and the
      // RSRP of the sensed slots is very high.
      LOG_D(NR_MAC, "Reached maximum RSRP threshold, unable to select resources\n");
      for (int z = 0; z < remaining_candidates->size; z++) {
        delete_at(remaining_candidates, z);
      }
      break; // break do while
    }
    counter_c++;
  } while (remaining_candidates->size < (sl_tx_params->sl_res_ratio * m_total));

  LOG_D(NR_MAC, "%ld resources selected after sensing resource selection from %ld slots\n", remaining_candidates->size, m_total);
  return remaining_candidates;
}
