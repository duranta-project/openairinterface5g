/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include "nr_fapi_p5_utils.h"

bool eq_param_request(const nfapi_nr_param_request_scf_t *unpacked_req, const nfapi_nr_param_request_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  return true;
}

bool eq_param_response(const nfapi_nr_param_response_scf_t *unpacked_req, const nfapi_nr_param_response_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  EQ(unpacked_req->num_tlv, req->num_tlv);
  EQ(unpacked_req->error_code, req->error_code);

  EQ_TLV(unpacked_req->cell_param.release_capability, req->cell_param.release_capability);

  EQ_TLV(unpacked_req->cell_param.phy_state, req->cell_param.phy_state);

  EQ_TLV(unpacked_req->cell_param.skip_blank_dl_config, req->cell_param.skip_blank_dl_config);

  EQ_TLV(unpacked_req->cell_param.skip_blank_ul_config, req->cell_param.skip_blank_ul_config);

  EQ_TLV(unpacked_req->cell_param.num_config_tlvs_to_report, req->cell_param.num_config_tlvs_to_report);

  for (int i = 0; i < unpacked_req->cell_param.num_config_tlvs_to_report.value; ++i) {
    EQ_TLV(unpacked_req->cell_param.config_tlvs_to_report_list[i], req->cell_param.config_tlvs_to_report_list[i]);
  }

  EQ_TLV(unpacked_req->carrier_param.cyclic_prefix, req->carrier_param.cyclic_prefix);

  EQ_TLV(unpacked_req->carrier_param.supported_subcarrier_spacings_dl, req->carrier_param.supported_subcarrier_spacings_dl);

  EQ_TLV(unpacked_req->carrier_param.supported_bandwidth_dl, req->carrier_param.supported_bandwidth_dl);

  EQ_TLV(unpacked_req->carrier_param.supported_subcarrier_spacings_ul, req->carrier_param.supported_subcarrier_spacings_ul);

  EQ_TLV(unpacked_req->carrier_param.supported_bandwidth_ul, req->carrier_param.supported_bandwidth_ul);

  EQ_TLV(unpacked_req->pdcch_param.cce_mapping_type, req->pdcch_param.cce_mapping_type);

  EQ_TLV(unpacked_req->pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot,
         req->pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot);

  EQ_TLV(unpacked_req->pdcch_param.coreset_precoder_granularity_coreset, req->pdcch_param.coreset_precoder_granularity_coreset);

  EQ_TLV(unpacked_req->pdcch_param.pdcch_mu_mimo, req->pdcch_param.pdcch_mu_mimo);

  EQ_TLV(unpacked_req->pdcch_param.pdcch_precoder_cycling, req->pdcch_param.pdcch_precoder_cycling);

  EQ_TLV(unpacked_req->pdcch_param.max_pdcch_per_slot, req->pdcch_param.max_pdcch_per_slot);

  EQ_TLV(unpacked_req->pucch_param.pucch_formats, req->pucch_param.pucch_formats);

  EQ_TLV(unpacked_req->pucch_param.max_pucchs_per_slot, req->pucch_param.max_pucchs_per_slot);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_mapping_type, req->pdsch_param.pdsch_mapping_type);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_allocation_types, req->pdsch_param.pdsch_allocation_types);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_vrb_to_prb_mapping, req->pdsch_param.pdsch_vrb_to_prb_mapping);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_cbg, req->pdsch_param.pdsch_cbg);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_dmrs_config_types, req->pdsch_param.pdsch_dmrs_config_types);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_dmrs_max_length, req->pdsch_param.pdsch_dmrs_max_length);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_dmrs_additional_pos, req->pdsch_param.pdsch_dmrs_additional_pos);

  EQ_TLV(unpacked_req->pdsch_param.max_pdsch_tbs_per_slot, req->pdsch_param.max_pdsch_tbs_per_slot);

  EQ_TLV(unpacked_req->pdsch_param.max_number_mimo_layers_pdsch, req->pdsch_param.max_number_mimo_layers_pdsch);

  EQ_TLV(unpacked_req->pdsch_param.supported_max_modulation_order_dl, req->pdsch_param.supported_max_modulation_order_dl);

  EQ_TLV(unpacked_req->pdsch_param.max_mu_mimo_users_dl, req->pdsch_param.max_mu_mimo_users_dl);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_data_in_dmrs_symbols, req->pdsch_param.pdsch_data_in_dmrs_symbols);

  EQ_TLV(unpacked_req->pdsch_param.premption_support, req->pdsch_param.premption_support);

  EQ_TLV(unpacked_req->pdsch_param.pdsch_non_slot_support, req->pdsch_param.pdsch_non_slot_support);

  EQ_TLV(unpacked_req->pusch_param.uci_mux_ulsch_in_pusch, req->pusch_param.uci_mux_ulsch_in_pusch);

  EQ_TLV(unpacked_req->pusch_param.uci_only_pusch, req->pusch_param.uci_only_pusch);

  EQ_TLV(unpacked_req->pusch_param.pusch_frequency_hopping, req->pusch_param.pusch_frequency_hopping);

  EQ_TLV(unpacked_req->pusch_param.pusch_dmrs_config_types, req->pusch_param.pusch_dmrs_config_types);

  EQ_TLV(unpacked_req->pusch_param.pusch_dmrs_max_len, req->pusch_param.pusch_dmrs_max_len);

  EQ_TLV(unpacked_req->pusch_param.pusch_dmrs_additional_pos, req->pusch_param.pusch_dmrs_additional_pos);

  EQ_TLV(unpacked_req->pusch_param.pusch_cbg, req->pusch_param.pusch_cbg);

  EQ_TLV(unpacked_req->pusch_param.pusch_mapping_type, req->pusch_param.pusch_mapping_type);

  EQ_TLV(unpacked_req->pusch_param.pusch_allocation_types, req->pusch_param.pusch_allocation_types);

  EQ_TLV(unpacked_req->pusch_param.pusch_vrb_to_prb_mapping, req->pusch_param.pusch_vrb_to_prb_mapping);

  EQ_TLV(unpacked_req->pusch_param.pusch_max_ptrs_ports, req->pusch_param.pusch_max_ptrs_ports);

  EQ_TLV(unpacked_req->pusch_param.max_pduschs_tbs_per_slot, req->pusch_param.max_pduschs_tbs_per_slot);

  EQ_TLV(unpacked_req->pusch_param.max_number_mimo_layers_non_cb_pusch, req->pusch_param.max_number_mimo_layers_non_cb_pusch);

  EQ_TLV(unpacked_req->pusch_param.supported_modulation_order_ul, req->pusch_param.supported_modulation_order_ul);

  EQ_TLV(unpacked_req->pusch_param.max_mu_mimo_users_ul, req->pusch_param.max_mu_mimo_users_ul);

  EQ_TLV(unpacked_req->pusch_param.dfts_ofdm_support, req->pusch_param.dfts_ofdm_support);

  EQ_TLV(unpacked_req->pusch_param.pusch_aggregation_factor, req->pusch_param.pusch_aggregation_factor);

  EQ_TLV(unpacked_req->prach_param.prach_long_formats, req->prach_param.prach_long_formats);

  EQ_TLV(unpacked_req->prach_param.prach_short_formats, req->prach_param.prach_short_formats);

  EQ_TLV(unpacked_req->prach_param.prach_restricted_sets, req->prach_param.prach_restricted_sets);

  EQ_TLV(unpacked_req->prach_param.max_prach_fd_occasions_in_a_slot, req->prach_param.max_prach_fd_occasions_in_a_slot);

  EQ_TLV(unpacked_req->measurement_param.rssi_measurement_support, req->measurement_param.rssi_measurement_support);

  return true;
}

bool eq_config_request(const nfapi_nr_config_request_scf_t *unpacked_req, const nfapi_nr_config_request_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  EQ(unpacked_req->num_tlv, req->num_tlv);

  EQ_TLV(unpacked_req->carrier_config.dl_bandwidth, req->carrier_config.dl_bandwidth);

  EQ_TLV(unpacked_req->carrier_config.dl_frequency, req->carrier_config.dl_frequency);

  for (int i = 0; i < 5; ++i) {
    EQ_TLV(unpacked_req->carrier_config.dl_k0[i], req->carrier_config.dl_k0[i]);

    EQ_TLV(unpacked_req->carrier_config.dl_grid_size[i], req->carrier_config.dl_grid_size[i]);
  }

  EQ_TLV(unpacked_req->carrier_config.num_tx_ant, req->carrier_config.num_tx_ant);

  EQ_TLV(unpacked_req->carrier_config.uplink_bandwidth, req->carrier_config.uplink_bandwidth);

  EQ_TLV(unpacked_req->carrier_config.uplink_frequency, req->carrier_config.uplink_frequency);

  for (int i = 0; i < 5; ++i) {
    EQ_TLV(unpacked_req->carrier_config.ul_k0[i], req->carrier_config.ul_k0[i]);

    EQ_TLV(unpacked_req->carrier_config.ul_grid_size[i], req->carrier_config.ul_grid_size[i]);
  }

  EQ_TLV(unpacked_req->carrier_config.num_rx_ant, req->carrier_config.num_rx_ant);

  EQ_TLV(unpacked_req->carrier_config.frequency_shift_7p5khz, req->carrier_config.frequency_shift_7p5khz);

  EQ_TLV(unpacked_req->cell_config.phy_cell_id, req->cell_config.phy_cell_id);

  EQ_TLV(unpacked_req->cell_config.frame_duplex_type, req->cell_config.frame_duplex_type);

  EQ_TLV(unpacked_req->ssb_config.ss_pbch_power, req->ssb_config.ss_pbch_power);

  EQ_TLV(unpacked_req->ssb_config.bch_payload, req->ssb_config.bch_payload);

  EQ_TLV(unpacked_req->ssb_config.scs_common, req->ssb_config.scs_common);

  EQ_TLV(unpacked_req->prach_config.prach_sequence_length, req->prach_config.prach_sequence_length);

  EQ_TLV(unpacked_req->prach_config.prach_sub_c_spacing, req->prach_config.prach_sub_c_spacing);

  EQ_TLV(unpacked_req->prach_config.restricted_set_config, req->prach_config.restricted_set_config);

  EQ_TLV(unpacked_req->prach_config.num_prach_fd_occasions, req->prach_config.num_prach_fd_occasions);

  EQ_TLV(unpacked_req->prach_config.prach_ConfigurationIndex, req->prach_config.prach_ConfigurationIndex);

  for (int i = 0; i < unpacked_req->prach_config.num_prach_fd_occasions.value; i++) {
    nfapi_nr_num_prach_fd_occasions_t unpacked_prach_fd_occasion = unpacked_req->prach_config.num_prach_fd_occasions_list[i];
    nfapi_nr_num_prach_fd_occasions_t req_prach_fd_occasion = req->prach_config.num_prach_fd_occasions_list[i];

    EQ_TLV(unpacked_prach_fd_occasion.prach_root_sequence_index, req_prach_fd_occasion.prach_root_sequence_index);

    EQ_TLV(unpacked_prach_fd_occasion.num_root_sequences, req_prach_fd_occasion.num_root_sequences);

    EQ_TLV(unpacked_prach_fd_occasion.k1, req_prach_fd_occasion.k1);

    EQ_TLV(unpacked_prach_fd_occasion.prach_zero_corr_conf, req_prach_fd_occasion.prach_zero_corr_conf);

    EQ_TLV(unpacked_prach_fd_occasion.num_unused_root_sequences, req_prach_fd_occasion.num_unused_root_sequences);
    for (int k = 0; k < unpacked_prach_fd_occasion.num_unused_root_sequences.value; k++) {
      EQ_TLV(unpacked_prach_fd_occasion.unused_root_sequences_list[k], req_prach_fd_occasion.unused_root_sequences_list[k]);
    }
  }

  EQ_TLV(unpacked_req->prach_config.ssb_per_rach, req->prach_config.ssb_per_rach);

  EQ_TLV(unpacked_req->prach_config.prach_multiple_carriers_in_a_band, req->prach_config.prach_multiple_carriers_in_a_band);

  EQ_TLV(unpacked_req->ssb_table.ssb_offset_point_a, req->ssb_table.ssb_offset_point_a);

  EQ_TLV(unpacked_req->ssb_table.beta_pss, req->ssb_table.beta_pss);

  EQ_TLV(unpacked_req->ssb_table.ssb_period, req->ssb_table.ssb_period);

  EQ_TLV(unpacked_req->ssb_table.ssb_subcarrier_offset, req->ssb_table.ssb_subcarrier_offset);

  EQ_TLV(unpacked_req->ssb_table.MIB, req->ssb_table.MIB);

  EQ_TLV(unpacked_req->ssb_table.ssb_mask_list[0].ssb_mask, req->ssb_table.ssb_mask_list[0].ssb_mask);

  EQ_TLV(unpacked_req->ssb_table.ssb_mask_list[1].ssb_mask, req->ssb_table.ssb_mask_list[1].ssb_mask);

  for (int i = 0; i < 64; i++) {
    EQ_TLV(unpacked_req->ssb_table.ssb_beam_id_list[i].beam_id, req->ssb_table.ssb_beam_id_list[i].beam_id);
  }

  EQ_TLV(unpacked_req->ssb_table.case_v3, req->ssb_table.case_v3);

  if (req->cell_config.frame_duplex_type.value == 1 /* TDD */) {
    EQ_TLV(unpacked_req->tdd_table.tdd_period, req->tdd_table.tdd_period);

    const uint8_t slotsperframe[5] = {10, 20, 40, 80, 160};
    // Assuming always CP_Normal, because Cyclic prefix is not included in CONFIG.request 10.02, but is present in 10.04
    uint8_t cyclicprefix = 1;
    // 3GPP 38.211 Table 4.3.2.1 & Table 4.3.2.2
    uint8_t number_of_symbols_per_slot = cyclicprefix ? 14 : 12;

    for (int i = 0; i < slotsperframe[unpacked_req->ssb_config.scs_common.value]; i++) {
      for (int k = 0; k < number_of_symbols_per_slot; k++) {
        EQ_TLV(unpacked_req->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config,
               req->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config);
      }
    }
  }

  EQ_TLV(unpacked_req->measurement_config.rssi_measurement, req->measurement_config.rssi_measurement);

  EQ(unpacked_req->dbt_config.num_dig_beams, req->dbt_config.num_dig_beams);

  EQ(unpacked_req->dbt_config.num_txrus, req->dbt_config.num_txrus);

  for (int i = 0; i < req->dbt_config.num_dig_beams; i++) {
    const nfapi_nr_dig_beam_t *unpacked_dig_beam = &unpacked_req->dbt_config.dig_beam_list[i];
    const nfapi_nr_dig_beam_t *req_dig_beam = &req->dbt_config.dig_beam_list[i];
    EQ(unpacked_dig_beam->beam_idx, req_dig_beam->beam_idx);
    for (int k = 0; k < req->dbt_config.num_txrus; ++k) {
      const nfapi_nr_txru_t *unpacked_tx_ru = &unpacked_dig_beam->txru_list[k];
      const nfapi_nr_txru_t *req_tx_ru = &req_dig_beam->txru_list[k];
      EQ(unpacked_tx_ru->r, req_tx_ru->r);
      EQ(unpacked_tx_ru->i, req_tx_ru->i);
    }
  }

  EQ(unpacked_req->pmi_list.num_pm_idx, req->pmi_list.num_pm_idx);

  for (int i = 0; i < req->pmi_list.num_pm_idx; i++) {
    const nfapi_nr_pm_pdu_t *req_pmi_pdu = &req->pmi_list.pmi_pdu[i];
    const nfapi_nr_pm_pdu_t *unpacked_pmi_pdu = &unpacked_req->pmi_list.pmi_pdu[i];
    EQ(unpacked_pmi_pdu->pm_idx, req_pmi_pdu->pm_idx);
    EQ(unpacked_pmi_pdu->numLayers, req_pmi_pdu->numLayers);
    EQ(unpacked_pmi_pdu->num_ant_ports, req_pmi_pdu->num_ant_ports);
    for (int j = 0; j < req_pmi_pdu->num_ant_ports; ++j) {
      for (int k = 0; k < req_pmi_pdu->num_ant_ports; ++k) {
        const nfapi_nr_pm_weights_t* req_pm_weight = &req_pmi_pdu->weights[j][k];
        const nfapi_nr_pm_weights_t* unpacked_pm_weight = &unpacked_pmi_pdu->weights[j][k];
        EQ(unpacked_pm_weight->r, req_pm_weight->r);
        EQ(unpacked_pm_weight->i, req_pm_weight->i);
      }
    }
  }

  EQ(unpacked_req->nfapi_config.p7_vnf_address_ipv4.tl.tag, req->nfapi_config.p7_vnf_address_ipv4.tl.tag);
  for (int i = 0; i < NFAPI_IPV4_ADDRESS_LENGTH; ++i) {
    EQ(unpacked_req->nfapi_config.p7_vnf_address_ipv4.address[i], req->nfapi_config.p7_vnf_address_ipv4.address[i]);
  }

  EQ(unpacked_req->nfapi_config.p7_vnf_address_ipv6.tl.tag, req->nfapi_config.p7_vnf_address_ipv6.tl.tag);
  for (int i = 0; i < NFAPI_IPV6_ADDRESS_LENGTH; ++i) {
    EQ(unpacked_req->nfapi_config.p7_vnf_address_ipv6.address[i], req->nfapi_config.p7_vnf_address_ipv6.address[i]);
  }

  EQ_TLV(unpacked_req->nfapi_config.p7_vnf_port, req->nfapi_config.p7_vnf_port);

  EQ_TLV(unpacked_req->nfapi_config.timing_window, req->nfapi_config.timing_window);

  EQ_TLV(unpacked_req->nfapi_config.timing_info_mode, req->nfapi_config.timing_info_mode);

  EQ_TLV(unpacked_req->nfapi_config.timing_info_period, req->nfapi_config.timing_info_period);

  EQ_TLV(unpacked_req->analog_beamforming_ve.num_beams_period_vendor_ext, req->analog_beamforming_ve.num_beams_period_vendor_ext);

  EQ_TLV(unpacked_req->analog_beamforming_ve.analog_bf_vendor_ext, req->analog_beamforming_ve.analog_bf_vendor_ext);

  return true;
}

static bool eq_config_response_tlv_lists(nfapi_nr_generic_tlv_scf_t *unpacked_list,
                                         nfapi_nr_generic_tlv_scf_t *req_list,
                                         uint8_t size)
{
  for (int i = 0; i < size; ++i) {
    nfapi_nr_generic_tlv_scf_t *unpacked_element = &(unpacked_list[i]);
    nfapi_nr_generic_tlv_scf_t *req_element = &(req_list[i]);

    EQ(unpacked_element->tl.tag, req_element->tl.tag);
    EQ(unpacked_element->tl.length, req_element->tl.length);
    // check according to value type
    switch (unpacked_element->tl.length) {
      case UINT_8:
        EQ(unpacked_element->value.u8, req_element->value.u8);
        break;
      case UINT_16:
        EQ(unpacked_element->value.u16, req_element->value.u16);
        break;
      case UINT_32:
        EQ(unpacked_element->value.u32, req_element->value.u32);
        break;
      case ARRAY_UINT_16:
        for (int j = 0; j < 5; ++j) {
          EQ(unpacked_element->value.array_u16[j], req_element->value.array_u16[j]);
        }
        break;
      default:
        printf("unknown length %d\n", unpacked_element->tl.length);
        DevAssert(1 == 0);
        break;
    }
  }
  return true;
}

bool eq_config_response(const nfapi_nr_config_response_scf_t *unpacked_req, const nfapi_nr_config_response_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);

  EQ(unpacked_req->error_code, req->error_code);
  EQ(unpacked_req->num_invalid_tlvs, req->num_invalid_tlvs);
  EQ(unpacked_req->num_invalid_tlvs_configured_in_idle, req->num_invalid_tlvs_configured_in_idle);
  EQ(unpacked_req->num_invalid_tlvs_configured_in_running, req->num_invalid_tlvs_configured_in_running);
  EQ(unpacked_req->num_missing_tlvs, req->num_missing_tlvs);
  // compare the list elements
  EQ(eq_config_response_tlv_lists(unpacked_req->invalid_tlvs_list, req->invalid_tlvs_list, req->num_invalid_tlvs), true);
  EQ(eq_config_response_tlv_lists(unpacked_req->invalid_tlvs_configured_in_idle_list,
                                  req->invalid_tlvs_configured_in_idle_list,
                                  req->num_invalid_tlvs_configured_in_idle),
     true);
  EQ(eq_config_response_tlv_lists(unpacked_req->invalid_tlvs_configured_in_running_list,
                                  req->invalid_tlvs_configured_in_running_list,
                                  req->num_invalid_tlvs_configured_in_running),
     true);
  EQ(eq_config_response_tlv_lists(unpacked_req->missing_tlvs_list, req->missing_tlvs_list, req->num_missing_tlvs), true);
  return true;
}

bool eq_start_request(const nfapi_nr_start_request_scf_t *unpacked_req, const nfapi_nr_start_request_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  return true;
}

bool eq_start_response(const nfapi_nr_start_response_scf_t *unpacked_req, const nfapi_nr_start_response_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  EQ(unpacked_req->error_code, req->error_code);
  return true;
}

bool eq_stop_request(const nfapi_nr_stop_request_scf_t *unpacked_req, const nfapi_nr_stop_request_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  return true;
}

bool eq_stop_indication(const nfapi_nr_stop_indication_scf_t *unpacked_req, const nfapi_nr_stop_indication_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  return true;
}

bool eq_error_indication(const nfapi_nr_error_indication_scf_t *unpacked_req, const nfapi_nr_error_indication_scf_t *req)
{
  EQ(unpacked_req->header.message_id, req->header.message_id);
  EQ(unpacked_req->header.message_length, req->header.message_length);
  EQ(unpacked_req->sfn, req->sfn);
  EQ(unpacked_req->slot, req->slot);
  EQ(unpacked_req->message_id, req->message_id);
  EQ(unpacked_req->error_code, req->error_code);
  return true;
}

void free_param_request(nfapi_nr_param_request_scf_t *msg)
{
  free(msg->vendor_extension);
}

void free_param_response(nfapi_nr_param_response_scf_t *msg)
{
  free(msg->vendor_extension);

  free(msg->cell_param.config_tlvs_to_report_list);

}

void free_config_request(nfapi_nr_config_request_scf_t *msg)
{
  free(msg->vendor_extension);

  if (msg->prach_config.num_prach_fd_occasions_list) {
    for (int i = 0; i < msg->prach_config.num_prach_fd_occasions.value; i++) {
      nfapi_nr_num_prach_fd_occasions_t *prach_fd_occasion = &(msg->prach_config.num_prach_fd_occasions_list[i]);
      if (prach_fd_occasion->unused_root_sequences_list) {
        free(prach_fd_occasion->unused_root_sequences_list);
      }
    }
    free(msg->prach_config.num_prach_fd_occasions_list);
  }
  const uint8_t slotsperframe[5] = {10, 20, 40, 80, 160};
  if (msg->tdd_table.max_tdd_periodicity_list) {
    for (int i = 0; i < slotsperframe[msg->ssb_config.scs_common.value]; i++) {
      free(msg->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list);
    }
    free(msg->tdd_table.max_tdd_periodicity_list);
  }

  for (int i = 0; i < msg->dbt_config.num_dig_beams; i++) {
    free(msg->dbt_config.dig_beam_list[i].txru_list);
  }

  free(msg->dbt_config.dig_beam_list);

  free(msg->pmi_list.pmi_pdu);
}

void free_config_response(nfapi_nr_config_response_scf_t *msg)
{
  free(msg->vendor_extension);

  free(msg->invalid_tlvs_list);

  free(msg->invalid_tlvs_configured_in_idle_list);

  free(msg->invalid_tlvs_configured_in_running_list);

  free(msg->missing_tlvs_list);
}

void free_start_request(nfapi_nr_start_request_scf_t *msg)
{
  free(msg->vendor_extension);
}

void free_start_response(nfapi_nr_start_response_scf_t *msg)
{
  free(msg->vendor_extension);
}

void free_stop_request(nfapi_nr_stop_request_scf_t *msg)
{
  free(msg->vendor_extension);
}

void free_stop_indication(nfapi_nr_stop_indication_scf_t *msg)
{
  free(msg->vendor_extension);
}

void free_error_indication(nfapi_nr_error_indication_scf_t *msg)
{
  free(msg->vendor_extension);
}

void copy_param_request(const nfapi_nr_param_request_scf_t *src, nfapi_nr_param_request_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }
}

void copy_param_response(const nfapi_nr_param_response_scf_t *src, nfapi_nr_param_response_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }

  dst->error_code = src->error_code;
  dst->num_tlv = src->num_tlv;

  COPY_TLV(dst->cell_param.release_capability, src->cell_param.release_capability);

  COPY_TLV(dst->cell_param.phy_state, src->cell_param.phy_state);

  COPY_TLV(dst->cell_param.skip_blank_dl_config, src->cell_param.skip_blank_dl_config);

  COPY_TLV(dst->cell_param.skip_blank_ul_config, src->cell_param.skip_blank_ul_config);

  COPY_TLV(dst->cell_param.num_config_tlvs_to_report, src->cell_param.num_config_tlvs_to_report);

  if (src->cell_param.config_tlvs_to_report_list) {
    dst->cell_param.config_tlvs_to_report_list =
        calloc(src->cell_param.num_config_tlvs_to_report.value, sizeof(nfapi_uint8_tlv_t *));
    for (int i = 0; i < src->cell_param.num_config_tlvs_to_report.value; ++i) {
      COPY_TLV(dst->cell_param.config_tlvs_to_report_list[i], src->cell_param.config_tlvs_to_report_list[i]);
    }
  }

  COPY_TLV(dst->carrier_param.cyclic_prefix, src->carrier_param.cyclic_prefix);

  COPY_TLV(dst->carrier_param.supported_subcarrier_spacings_dl, src->carrier_param.supported_subcarrier_spacings_dl);

  COPY_TLV(dst->carrier_param.supported_bandwidth_dl, src->carrier_param.supported_bandwidth_dl);

  COPY_TLV(dst->carrier_param.supported_subcarrier_spacings_ul, src->carrier_param.supported_subcarrier_spacings_ul);

  COPY_TLV(dst->carrier_param.supported_bandwidth_ul, src->carrier_param.supported_bandwidth_ul);

  COPY_TLV(dst->pdcch_param.cce_mapping_type, src->pdcch_param.cce_mapping_type);

  COPY_TLV(dst->pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot,
           src->pdcch_param.coreset_outside_first_3_of_ofdm_syms_of_slot);

  COPY_TLV(dst->pdcch_param.coreset_precoder_granularity_coreset, src->pdcch_param.coreset_precoder_granularity_coreset);

  COPY_TLV(dst->pdcch_param.pdcch_mu_mimo, src->pdcch_param.pdcch_mu_mimo);

  COPY_TLV(dst->pdcch_param.pdcch_precoder_cycling, src->pdcch_param.pdcch_precoder_cycling);

  COPY_TLV(dst->pdcch_param.max_pdcch_per_slot, src->pdcch_param.max_pdcch_per_slot);

  COPY_TLV(dst->pucch_param.pucch_formats, src->pucch_param.pucch_formats);

  COPY_TLV(dst->pucch_param.max_pucchs_per_slot, src->pucch_param.max_pucchs_per_slot);

  COPY_TLV(dst->pdsch_param.pdsch_mapping_type, src->pdsch_param.pdsch_mapping_type);

  COPY_TLV(dst->pdsch_param.pdsch_allocation_types, src->pdsch_param.pdsch_allocation_types);

  COPY_TLV(dst->pdsch_param.pdsch_vrb_to_prb_mapping, src->pdsch_param.pdsch_vrb_to_prb_mapping);

  COPY_TLV(dst->pdsch_param.pdsch_cbg, src->pdsch_param.pdsch_cbg);

  COPY_TLV(dst->pdsch_param.pdsch_dmrs_config_types, src->pdsch_param.pdsch_dmrs_config_types);

  COPY_TLV(dst->pdsch_param.pdsch_dmrs_max_length, src->pdsch_param.pdsch_dmrs_max_length);

  COPY_TLV(dst->pdsch_param.pdsch_dmrs_additional_pos, src->pdsch_param.pdsch_dmrs_additional_pos);

  COPY_TLV(dst->pdsch_param.max_pdsch_tbs_per_slot, src->pdsch_param.max_pdsch_tbs_per_slot);

  COPY_TLV(dst->pdsch_param.max_number_mimo_layers_pdsch, src->pdsch_param.max_number_mimo_layers_pdsch);

  COPY_TLV(dst->pdsch_param.supported_max_modulation_order_dl, src->pdsch_param.supported_max_modulation_order_dl);

  COPY_TLV(dst->pdsch_param.max_mu_mimo_users_dl, src->pdsch_param.max_mu_mimo_users_dl);

  COPY_TLV(dst->pdsch_param.pdsch_data_in_dmrs_symbols, src->pdsch_param.pdsch_data_in_dmrs_symbols);

  COPY_TLV(dst->pdsch_param.premption_support, src->pdsch_param.premption_support);

  COPY_TLV(dst->pdsch_param.pdsch_non_slot_support, src->pdsch_param.pdsch_non_slot_support);

  COPY_TLV(dst->pusch_param.uci_mux_ulsch_in_pusch, src->pusch_param.uci_mux_ulsch_in_pusch);

  COPY_TLV(dst->pusch_param.uci_only_pusch, src->pusch_param.uci_only_pusch);

  COPY_TLV(dst->pusch_param.pusch_frequency_hopping, src->pusch_param.pusch_frequency_hopping);

  COPY_TLV(dst->pusch_param.pusch_dmrs_config_types, src->pusch_param.pusch_dmrs_config_types);

  COPY_TLV(dst->pusch_param.pusch_dmrs_max_len, src->pusch_param.pusch_dmrs_max_len);

  COPY_TLV(dst->pusch_param.pusch_dmrs_additional_pos, src->pusch_param.pusch_dmrs_additional_pos);

  COPY_TLV(dst->pusch_param.pusch_cbg, src->pusch_param.pusch_cbg);

  COPY_TLV(dst->pusch_param.pusch_mapping_type, src->pusch_param.pusch_mapping_type);

  COPY_TLV(dst->pusch_param.pusch_allocation_types, src->pusch_param.pusch_allocation_types);

  COPY_TLV(dst->pusch_param.pusch_vrb_to_prb_mapping, src->pusch_param.pusch_vrb_to_prb_mapping);

  COPY_TLV(dst->pusch_param.pusch_max_ptrs_ports, src->pusch_param.pusch_max_ptrs_ports);

  COPY_TLV(dst->pusch_param.max_pduschs_tbs_per_slot, src->pusch_param.max_pduschs_tbs_per_slot);

  COPY_TLV(dst->pusch_param.max_number_mimo_layers_non_cb_pusch, src->pusch_param.max_number_mimo_layers_non_cb_pusch);

  COPY_TLV(dst->pusch_param.supported_modulation_order_ul, src->pusch_param.supported_modulation_order_ul);

  COPY_TLV(dst->pusch_param.max_mu_mimo_users_ul, src->pusch_param.max_mu_mimo_users_ul);

  COPY_TLV(dst->pusch_param.dfts_ofdm_support, src->pusch_param.dfts_ofdm_support);

  COPY_TLV(dst->pusch_param.pusch_aggregation_factor, src->pusch_param.pusch_aggregation_factor);

  COPY_TLV(dst->prach_param.prach_long_formats, src->prach_param.prach_long_formats);

  COPY_TLV(dst->prach_param.prach_short_formats, src->prach_param.prach_short_formats);

  COPY_TLV(dst->prach_param.prach_restricted_sets, src->prach_param.prach_restricted_sets);

  COPY_TLV(dst->prach_param.max_prach_fd_occasions_in_a_slot, src->prach_param.max_prach_fd_occasions_in_a_slot);

  COPY_TLV(dst->measurement_param.rssi_measurement_support, src->measurement_param.rssi_measurement_support);

  COPY_TL(dst->nfapi_config.p7_vnf_address_ipv4.tl, src->nfapi_config.p7_vnf_address_ipv4.tl);
  memcpy(dst->nfapi_config.p7_vnf_address_ipv4.address,
         src->nfapi_config.p7_vnf_address_ipv4.address,
         sizeof(dst->nfapi_config.p7_vnf_address_ipv4.address));

  COPY_TL(dst->nfapi_config.p7_vnf_address_ipv6.tl, src->nfapi_config.p7_vnf_address_ipv6.tl);
  memcpy(dst->nfapi_config.p7_vnf_address_ipv6.address,
         src->nfapi_config.p7_vnf_address_ipv6.address,
         sizeof(dst->nfapi_config.p7_vnf_address_ipv6.address));

  COPY_TLV(dst->nfapi_config.p7_vnf_port, src->nfapi_config.p7_vnf_port);

  COPY_TL(dst->nfapi_config.p7_pnf_address_ipv4.tl, src->nfapi_config.p7_pnf_address_ipv4.tl);
  memcpy(dst->nfapi_config.p7_pnf_address_ipv4.address,
         src->nfapi_config.p7_pnf_address_ipv4.address,
         sizeof(dst->nfapi_config.p7_pnf_address_ipv4.address));

  COPY_TL(dst->nfapi_config.p7_pnf_address_ipv6.tl, src->nfapi_config.p7_pnf_address_ipv6.tl);
  memcpy(dst->nfapi_config.p7_pnf_address_ipv6.address,
         src->nfapi_config.p7_pnf_address_ipv6.address,
         sizeof(dst->nfapi_config.p7_pnf_address_ipv6.address));

  COPY_TLV(dst->nfapi_config.p7_pnf_port, src->nfapi_config.p7_pnf_port);

  COPY_TLV(dst->nfapi_config.timing_window, src->nfapi_config.timing_window);

  COPY_TLV(dst->nfapi_config.timing_info_mode, src->nfapi_config.timing_info_mode);

  COPY_TLV(dst->nfapi_config.timing_info_period, src->nfapi_config.timing_info_period);

  COPY_TLV(dst->nfapi_config.dl_tti_timing_offset, src->nfapi_config.dl_tti_timing_offset);

  COPY_TLV(dst->nfapi_config.ul_tti_timing_offset, src->nfapi_config.ul_tti_timing_offset);

  COPY_TLV(dst->nfapi_config.ul_dci_timing_offset, src->nfapi_config.ul_dci_timing_offset);

  COPY_TLV(dst->nfapi_config.tx_data_timing_offset, src->nfapi_config.tx_data_timing_offset);
}

void copy_config_request(const nfapi_nr_config_request_scf_t *src, nfapi_nr_config_request_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }

  dst->num_tlv = src->num_tlv;

  COPY_TLV(dst->carrier_config.dl_bandwidth, src->carrier_config.dl_bandwidth);

  COPY_TLV(dst->carrier_config.dl_frequency, src->carrier_config.dl_frequency);

  for (int i = 0; i < 5; ++i) {
    COPY_TLV(dst->carrier_config.dl_k0[i], src->carrier_config.dl_k0[i]);

    COPY_TLV(dst->carrier_config.dl_grid_size[i], src->carrier_config.dl_grid_size[i]);
  }

  COPY_TLV(dst->carrier_config.num_tx_ant, src->carrier_config.num_tx_ant);

  COPY_TLV(dst->carrier_config.uplink_bandwidth, src->carrier_config.uplink_bandwidth);

  COPY_TLV(dst->carrier_config.uplink_frequency, src->carrier_config.uplink_frequency);

  COPY_TLV(dst->carrier_config.uplink_frequency, src->carrier_config.uplink_frequency);

  for (int i = 0; i < 5; ++i) {
    COPY_TLV(dst->carrier_config.ul_k0[i], src->carrier_config.ul_k0[i]);

    COPY_TLV(dst->carrier_config.ul_grid_size[i], src->carrier_config.ul_grid_size[i]);
  }

  COPY_TLV(dst->carrier_config.num_rx_ant, src->carrier_config.num_rx_ant);

  COPY_TLV(dst->carrier_config.frequency_shift_7p5khz, src->carrier_config.frequency_shift_7p5khz);

  COPY_TLV(dst->cell_config.phy_cell_id, src->cell_config.phy_cell_id);

  COPY_TLV(dst->cell_config.frame_duplex_type, src->cell_config.frame_duplex_type);

  COPY_TLV(dst->ssb_config.ss_pbch_power, src->ssb_config.ss_pbch_power);

  COPY_TLV(dst->ssb_config.bch_payload, src->ssb_config.bch_payload);

  COPY_TLV(dst->ssb_config.scs_common, src->ssb_config.scs_common);

  COPY_TLV(dst->prach_config.prach_sequence_length, src->prach_config.prach_sequence_length);

  COPY_TLV(dst->prach_config.prach_sub_c_spacing, src->prach_config.prach_sub_c_spacing);

  COPY_TLV(dst->prach_config.restricted_set_config, src->prach_config.restricted_set_config);

  COPY_TLV(dst->prach_config.num_prach_fd_occasions, src->prach_config.num_prach_fd_occasions);

  COPY_TLV(dst->prach_config.prach_ConfigurationIndex, src->prach_config.prach_ConfigurationIndex);

  COPY_TLV(dst->prach_config.prach_ConfigurationIndex, src->prach_config.prach_ConfigurationIndex);

  dst->prach_config.num_prach_fd_occasions_list = (nfapi_nr_num_prach_fd_occasions_t *)malloc(
      dst->prach_config.num_prach_fd_occasions.value * sizeof(nfapi_nr_num_prach_fd_occasions_t));
  for (int i = 0; i < dst->prach_config.num_prach_fd_occasions.value; i++) {
    nfapi_nr_num_prach_fd_occasions_t *src_prach_fd_occasion = &(src->prach_config.num_prach_fd_occasions_list[i]);
    nfapi_nr_num_prach_fd_occasions_t *dst_prach_fd_occasion = &(dst->prach_config.num_prach_fd_occasions_list[i]);

    COPY_TLV(dst_prach_fd_occasion->prach_root_sequence_index, src_prach_fd_occasion->prach_root_sequence_index);

    COPY_TLV(dst_prach_fd_occasion->num_root_sequences, src_prach_fd_occasion->num_root_sequences);

    COPY_TLV(dst_prach_fd_occasion->k1, src_prach_fd_occasion->k1);

    COPY_TLV(dst_prach_fd_occasion->prach_zero_corr_conf, src_prach_fd_occasion->prach_zero_corr_conf);

    COPY_TLV(dst_prach_fd_occasion->num_unused_root_sequences, src_prach_fd_occasion->num_unused_root_sequences);

    dst_prach_fd_occasion->unused_root_sequences_list =
        (nfapi_uint8_tlv_t *)malloc(dst_prach_fd_occasion->num_unused_root_sequences.value * sizeof(nfapi_uint8_tlv_t));
    for (int k = 0; k < dst_prach_fd_occasion->num_unused_root_sequences.value; k++) {
      COPY_TLV(dst_prach_fd_occasion->unused_root_sequences_list[k], src_prach_fd_occasion->unused_root_sequences_list[k]);
    }
  }

  COPY_TLV(dst->prach_config.ssb_per_rach, src->prach_config.ssb_per_rach);

  COPY_TLV(dst->prach_config.prach_multiple_carriers_in_a_band, src->prach_config.prach_multiple_carriers_in_a_band);

  COPY_TLV(dst->ssb_table.ssb_offset_point_a, src->ssb_table.ssb_offset_point_a);

  COPY_TLV(dst->ssb_table.beta_pss, src->ssb_table.beta_pss);

  COPY_TLV(dst->ssb_table.ssb_period, src->ssb_table.ssb_period);

  COPY_TLV(dst->ssb_table.ssb_subcarrier_offset, src->ssb_table.ssb_subcarrier_offset);

  COPY_TLV(dst->ssb_table.MIB, src->ssb_table.MIB);

  COPY_TLV(dst->ssb_table.ssb_mask_list[0].ssb_mask, src->ssb_table.ssb_mask_list[0].ssb_mask);

  COPY_TLV(dst->ssb_table.ssb_mask_list[1].ssb_mask, src->ssb_table.ssb_mask_list[1].ssb_mask);

  for (int i = 0; i < 64; i++) {
    COPY_TLV(dst->ssb_table.ssb_beam_id_list[i].beam_id, src->ssb_table.ssb_beam_id_list[i].beam_id);
  }

  COPY_TLV(dst->ssb_table.case_v3, src->ssb_table.case_v3);

  if (src->cell_config.frame_duplex_type.value == 1 /* TDD */) {
    COPY_TLV(dst->tdd_table.tdd_period, src->tdd_table.tdd_period);

    const uint8_t slotsperframe[5] = {10, 20, 40, 80, 160};
    // Assuming always CP_Normal, because Cyclic prefix is not included in CONFIG.request 10.02, but is present in 10.04
    uint8_t cyclicprefix = 1;
    // 3GPP 38.211 Table 4.3.2.1 & Table 4.3.2.2
    uint8_t number_of_symbols_per_slot = cyclicprefix ? 14 : 12;
    dst->tdd_table.max_tdd_periodicity_list = (nfapi_nr_max_tdd_periodicity_t *)malloc(slotsperframe[dst->ssb_config.scs_common.value]
                                                                                       * sizeof(nfapi_nr_max_tdd_periodicity_t));

    for (int i = 0; i < slotsperframe[dst->ssb_config.scs_common.value]; i++) {
      dst->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list =
          (nfapi_nr_max_num_of_symbol_per_slot_t *)malloc(number_of_symbols_per_slot * sizeof(nfapi_nr_max_num_of_symbol_per_slot_t));
    }
    for (int i = 0; i < slotsperframe[dst->ssb_config.scs_common.value]; i++) { // TODO check right number of slots
      for (int k = 0; k < number_of_symbols_per_slot; k++) { // TODO can change?
        COPY_TLV(dst->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config,
                 src->tdd_table.max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config);
      }
    }
  }

  COPY_TLV(dst->measurement_config.rssi_measurement, src->measurement_config.rssi_measurement);

  dst->dbt_config.num_dig_beams = src->dbt_config.num_dig_beams;
  dst->dbt_config.num_txrus = src->dbt_config.num_txrus;
  dst->dbt_config.dig_beam_list = calloc(dst->dbt_config.num_dig_beams, sizeof(*dst->dbt_config.dig_beam_list));
  for (int i = 0; i < dst->dbt_config.num_dig_beams; i++) {
    nfapi_nr_dig_beam_t *dst_dig_beam = &dst->dbt_config.dig_beam_list[i];
    const nfapi_nr_dig_beam_t *src_dig_beam = &src->dbt_config.dig_beam_list[i];
    dst_dig_beam->beam_idx = src_dig_beam->beam_idx;
    dst_dig_beam->txru_list = calloc(dst->dbt_config.num_txrus , sizeof(*dst_dig_beam->txru_list ));
    for (int k = 0; k < dst->dbt_config.num_txrus; ++k) {
      nfapi_nr_txru_t *dst_tx_ru = &dst_dig_beam->txru_list[k];
      const nfapi_nr_txru_t *src_tx_ru = &src_dig_beam->txru_list[k];
      dst_tx_ru->r = src_tx_ru->r;
      dst_tx_ru->i = src_tx_ru->i;
    }
  }

  dst->pmi_list.num_pm_idx = src->pmi_list.num_pm_idx;
  if (dst->pmi_list.num_pm_idx > 0) {
    dst->pmi_list.pmi_pdu = calloc(dst->pmi_list.num_pm_idx , sizeof(*dst->pmi_list.pmi_pdu));
    for (int i = 0; i < dst->pmi_list.num_pm_idx; i++) {
      nfapi_nr_pm_pdu_t * dst_pmi_pdu = &dst->pmi_list.pmi_pdu[i];
      const nfapi_nr_pm_pdu_t * src_pmi_pdu = &src->pmi_list.pmi_pdu[i];
      dst_pmi_pdu->pm_idx = src_pmi_pdu->pm_idx;
      dst_pmi_pdu->numLayers = src_pmi_pdu->numLayers;
      dst_pmi_pdu->num_ant_ports = src_pmi_pdu->num_ant_ports;
      for (int j = 0; j < dst_pmi_pdu->numLayers; ++j) {
        for (int k = 0; k < dst_pmi_pdu->num_ant_ports; ++k) {
          dst_pmi_pdu->weights[j][k] = src_pmi_pdu->weights[j][k];
        }
      }
    }
  }

  COPY_TL(dst->nfapi_config.p7_vnf_address_ipv4.tl, src->nfapi_config.p7_vnf_address_ipv4.tl);
  memcpy(dst->nfapi_config.p7_vnf_address_ipv4.address,
         src->nfapi_config.p7_vnf_address_ipv4.address,
         sizeof(dst->nfapi_config.p7_vnf_address_ipv4.address));

  COPY_TL(dst->nfapi_config.p7_vnf_address_ipv6.tl, src->nfapi_config.p7_vnf_address_ipv6.tl);
  memcpy(dst->nfapi_config.p7_vnf_address_ipv6.address,
         src->nfapi_config.p7_vnf_address_ipv6.address,
         sizeof(dst->nfapi_config.p7_vnf_address_ipv6.address));

  COPY_TLV(dst->nfapi_config.p7_vnf_port, src->nfapi_config.p7_vnf_port);

  COPY_TL(dst->nfapi_config.p7_pnf_address_ipv4.tl, src->nfapi_config.p7_pnf_address_ipv4.tl);
  memcpy(dst->nfapi_config.p7_pnf_address_ipv4.address,
         src->nfapi_config.p7_pnf_address_ipv4.address,
         sizeof(dst->nfapi_config.p7_pnf_address_ipv4.address));

  COPY_TL(dst->nfapi_config.p7_pnf_address_ipv6.tl, src->nfapi_config.p7_pnf_address_ipv6.tl);
  memcpy(dst->nfapi_config.p7_pnf_address_ipv6.address,
         src->nfapi_config.p7_pnf_address_ipv6.address,
         sizeof(dst->nfapi_config.p7_pnf_address_ipv6.address));

  COPY_TLV(dst->nfapi_config.p7_pnf_port, src->nfapi_config.p7_pnf_port);

  COPY_TLV(dst->nfapi_config.timing_window, src->nfapi_config.timing_window);

  COPY_TLV(dst->nfapi_config.timing_info_mode, src->nfapi_config.timing_info_mode);

  COPY_TLV(dst->nfapi_config.timing_info_period, src->nfapi_config.timing_info_period);

  COPY_TLV(dst->nfapi_config.dl_tti_timing_offset, src->nfapi_config.dl_tti_timing_offset);

  COPY_TLV(dst->nfapi_config.ul_tti_timing_offset, src->nfapi_config.ul_tti_timing_offset);

  COPY_TLV(dst->nfapi_config.ul_dci_timing_offset, src->nfapi_config.ul_dci_timing_offset);

  COPY_TLV(dst->nfapi_config.tx_data_timing_offset, src->nfapi_config.tx_data_timing_offset);

  COPY_TLV(dst->analog_beamforming_ve.num_beams_period_vendor_ext, src->analog_beamforming_ve.num_beams_period_vendor_ext);

  COPY_TLV(dst->analog_beamforming_ve.analog_bf_vendor_ext, src->analog_beamforming_ve.analog_bf_vendor_ext);
}

void copy_config_response(const nfapi_nr_config_response_scf_t *src, nfapi_nr_config_response_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }

  dst->error_code = src->error_code;
  dst->num_invalid_tlvs = src->num_invalid_tlvs;
  dst->num_invalid_tlvs_configured_in_idle = src->num_invalid_tlvs_configured_in_idle;
  dst->num_invalid_tlvs_configured_in_running = src->num_invalid_tlvs_configured_in_running;
  dst->num_missing_tlvs = src->num_missing_tlvs;

  dst->invalid_tlvs_list = (nfapi_nr_generic_tlv_scf_t *)malloc(dst->num_invalid_tlvs * sizeof(nfapi_nr_generic_tlv_scf_t));
  dst->invalid_tlvs_configured_in_idle_list =
      (nfapi_nr_generic_tlv_scf_t *)malloc(dst->num_invalid_tlvs_configured_in_idle * sizeof(nfapi_nr_generic_tlv_scf_t));
  dst->invalid_tlvs_configured_in_running_list =
      (nfapi_nr_generic_tlv_scf_t *)malloc(dst->num_invalid_tlvs_configured_in_running * sizeof(nfapi_nr_generic_tlv_scf_t));
  dst->missing_tlvs_list = (nfapi_nr_generic_tlv_scf_t *)malloc(dst->num_missing_tlvs * sizeof(nfapi_nr_generic_tlv_scf_t));

  for (int i = 0; i < dst->num_invalid_tlvs; ++i) {
    COPY_TLV(dst->invalid_tlvs_list[i], src->invalid_tlvs_list[i]);
  }

  for (int i = 0; i < dst->num_invalid_tlvs_configured_in_idle; ++i) {
    COPY_TLV(dst->invalid_tlvs_configured_in_idle_list[i], src->invalid_tlvs_configured_in_idle_list[i]);
  }

  for (int i = 0; i < dst->num_invalid_tlvs_configured_in_running; ++i) {
    COPY_TLV(dst->invalid_tlvs_configured_in_running_list[i], src->invalid_tlvs_configured_in_running_list[i]);
  }

  for (int i = 0; i < dst->num_missing_tlvs; ++i) {
    COPY_TLV(dst->missing_tlvs_list[i], src->missing_tlvs_list[i]);
  }
}

void copy_start_request(const nfapi_nr_start_request_scf_t *src, nfapi_nr_start_request_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }
}

void copy_start_response(const nfapi_nr_start_response_scf_t *src, nfapi_nr_start_response_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }
  dst->error_code = src->error_code;
}

void copy_stop_request(const nfapi_nr_stop_request_scf_t *src, nfapi_nr_stop_request_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }
}

void copy_stop_indication(const nfapi_nr_stop_indication_scf_t *src, nfapi_nr_stop_indication_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }
}

void copy_error_indication(const nfapi_nr_error_indication_scf_t *src, nfapi_nr_error_indication_scf_t *dst)
{
  dst->header.message_id = src->header.message_id;
  dst->header.message_length = src->header.message_length;
  if (src->vendor_extension) {
    dst->vendor_extension = calloc(1, sizeof(nfapi_vendor_extension_tlv_t));
    dst->vendor_extension->tag = src->vendor_extension->tag;
    dst->vendor_extension->length = src->vendor_extension->length;
    copy_vendor_extension_value(&dst->vendor_extension, &src->vendor_extension);
  }

  dst->sfn = src->sfn;
  dst->slot = src->slot;
  dst->message_id = src->message_id;
  dst->error_code = src->error_code;
}

static void dump_p5_message_header(const nfapi_nr_p4_p5_message_header_t *hdr, int depth)
{
  INDENTED_PRINTF("Message ID = 0x%02x\n", hdr->message_id);
  INDENTED_PRINTF("Message Length = 0x%02x\n", hdr->message_length);
}

void dump_param_request(const nfapi_nr_param_request_scf_t *msg)
{
  int depth = 0;
  dump_p5_message_header(&msg->header,depth);
}

void dump_param_response(const nfapi_nr_param_response_scf_t *msg)
{
  int depth = 0;
  dump_p5_message_header(&msg->header, depth);
  depth++;
  INDENTED_PRINTF("Error Code = 0x%02x\n", msg->error_code);
  INDENTED_PRINTF("Number of TLVs = 0x%02x\n", msg->num_tlv);
  /* dump all TLVs from each table */
  /* Cell Param */
  const nfapi_nr_cell_param_t *cell_param = &msg->cell_param;
  depth++;
  INDENTED_TLV_PRINT("Release Capability", cell_param->release_capability);
  INDENTED_TLV_PRINT("PHY State", cell_param->phy_state);
  INDENTED_TLV_PRINT("Skip Blank Dl Config", cell_param->skip_blank_dl_config);
  INDENTED_TLV_PRINT("Skip Blank UL Config", cell_param->skip_blank_ul_config);
  INDENTED_TLV_PRINT("NumConfigTLVsToReport", cell_param->num_config_tlvs_to_report);
  depth++;
  for (int i = 0; i < cell_param->num_config_tlvs_to_report.value; i++) {
    const nfapi_uint8_tlv_t *reportedTLV = &cell_param->config_tlvs_to_report_list[i];
    INDENTED_PRINTF("(0x%04x) Reported CONFIG TLV #%d = 0x%02x\n", reportedTLV->tl.tag, i + 1, reportedTLV->value);
  }
  depth--;
  /* Carrier Param */
  const nfapi_nr_carrier_param_t *carrier_param = &msg->carrier_param;
  INDENTED_TLV_PRINT("Cyclic Prefix", carrier_param->cyclic_prefix);
  INDENTED_TLV_PRINT("Supported SCS DL", carrier_param->supported_subcarrier_spacings_dl);
  INDENTED_TLV_PRINT("Supported BW DL", carrier_param->supported_bandwidth_dl);
  INDENTED_TLV_PRINT("Supported SCS UL", carrier_param->supported_subcarrier_spacings_ul);
  INDENTED_TLV_PRINT("Supported BW UL", carrier_param->supported_bandwidth_ul);
  /* PDCCH Param */
  const nfapi_nr_pdcch_param_t *pdcch_param = &msg->pdcch_param;
  INDENTED_TLV_PRINT("CCE Mapping Type", pdcch_param->cce_mapping_type);
  INDENTED_TLV_PRINT("CSet Outside First 3 OFDM Symbols of Slot", pdcch_param->coreset_outside_first_3_of_ofdm_syms_of_slot);
  INDENTED_TLV_PRINT("Precoder Granularity Coreset", pdcch_param->coreset_precoder_granularity_coreset);
  INDENTED_TLV_PRINT("PDCCH Mu MIMO", pdcch_param->pdcch_mu_mimo);
  INDENTED_TLV_PRINT("PDCCH Precoder Cycling", pdcch_param->pdcch_precoder_cycling);
  INDENTED_TLV_PRINT("Max PDCCH per Slot", pdcch_param->max_pdcch_per_slot);
  /* PUCHH Params */
  const nfapi_nr_pucch_param_t *pucch_param = &msg->pucch_param;
  INDENTED_TLV_PRINT("PUCCH Formats", pucch_param->pucch_formats);
  INDENTED_TLV_PRINT("Max PUCCH per Slot", pucch_param->max_pucchs_per_slot);
  /* PDSCH Params */
  const nfapi_nr_pdsch_param_t *pdsch_param = &msg->pdsch_param;
  INDENTED_TLV_PRINT("PDSCH Mapping Type", pdsch_param->pdsch_mapping_type);
  INDENTED_TLV_PRINT("PDSCH Allocation Types", pdsch_param->pdsch_allocation_types);
  INDENTED_TLV_PRINT("PDSCH VRB to PRB Mapping", pdsch_param->pdsch_vrb_to_prb_mapping);
  INDENTED_TLV_PRINT("PDSCH CBG", pdsch_param->pdsch_cbg);
  INDENTED_TLV_PRINT("PDSCH DMRS Config Types", pdsch_param->pdsch_dmrs_config_types);
  INDENTED_TLV_PRINT("PDSCH DMRS Max Length", pdsch_param->pdsch_dmrs_max_length);
  INDENTED_TLV_PRINT("PDSCH DMRS Additional Position", pdsch_param->pdsch_dmrs_additional_pos);
  INDENTED_TLV_PRINT("Max PDSCH TBs per Slot", pdsch_param->max_pdsch_tbs_per_slot);
  INDENTED_TLV_PRINT("Max number MIMO Layers PDSCH", pdsch_param->max_number_mimo_layers_pdsch);
  INDENTED_TLV_PRINT("Supported Max Modulation Order DL", pdsch_param->supported_max_modulation_order_dl);
  INDENTED_TLV_PRINT("Max Mu MIMO Users DL", pdsch_param->max_mu_mimo_users_dl);
  INDENTED_TLV_PRINT("PDSCH Data in DMRS Symbols", pdsch_param->pdsch_data_in_dmrs_symbols);
  INDENTED_TLV_PRINT("Preemption Support", pdsch_param->premption_support);
  INDENTED_TLV_PRINT("PDSCH Non Slot Support", pdsch_param->pdsch_non_slot_support);
  /* PUSCH Params */
  const nfapi_nr_pusch_param_t *pusch_param = &msg->pusch_param;
  INDENTED_TLV_PRINT("UCI Mux ULSCH in PUSCH", pusch_param->uci_mux_ulsch_in_pusch);
  INDENTED_TLV_PRINT("UCI Only PUSCH", pusch_param->uci_only_pusch);
  INDENTED_TLV_PRINT("PUSCH Frequency Hopping", pusch_param->pusch_frequency_hopping);
  INDENTED_TLV_PRINT("PUSCH DMRS Config Types", pusch_param->pusch_dmrs_config_types);
  INDENTED_TLV_PRINT("PUSCH DMRS Max Len", pusch_param->pusch_dmrs_max_len);
  INDENTED_TLV_PRINT("PUSCH DMRS Additional Pos", pusch_param->pusch_dmrs_additional_pos);
  INDENTED_TLV_PRINT("PUSCH CBG", pusch_param->pusch_cbg);
  INDENTED_TLV_PRINT("PUSCH Mapping Type", pusch_param->pusch_mapping_type);
  INDENTED_TLV_PRINT("PUSCH Allocation Types", pusch_param->pusch_allocation_types);
  INDENTED_TLV_PRINT("PUSCH VRB to PRB Mapping", pusch_param->pusch_vrb_to_prb_mapping);
  INDENTED_TLV_PRINT("PUSCH Max PTRS Ports", pusch_param->pusch_max_ptrs_ports);
  INDENTED_TLV_PRINT("Max PUSCH TBs per Slot", pusch_param->max_pduschs_tbs_per_slot);
  INDENTED_TLV_PRINT("Max Number MIMO Layers non CB PUSCH", pusch_param->max_number_mimo_layers_non_cb_pusch);
  INDENTED_TLV_PRINT("Supported Modulation Order UL", pusch_param->supported_modulation_order_ul);
  INDENTED_TLV_PRINT("Max MU MIMO Users UL", pusch_param->max_mu_mimo_users_ul);
  INDENTED_TLV_PRINT("DTFS OFDM Support", pusch_param->dfts_ofdm_support);
  INDENTED_TLV_PRINT("PUSCH Aggregation Factor", pusch_param->pusch_aggregation_factor);
  /* PRACH Params */
  const nfapi_nr_prach_param_t *prach_param = &msg->prach_param;
  INDENTED_TLV_PRINT("PRACH Long Formats", prach_param->prach_long_formats);
  INDENTED_TLV_PRINT("PRACH Short Formats", prach_param->prach_short_formats);
  INDENTED_TLV_PRINT("PRACH Restricted Sets", prach_param->prach_restricted_sets);
  INDENTED_TLV_PRINT("Max PRACH Occasions in a Slot", prach_param->max_prach_fd_occasions_in_a_slot);
  /* Measurement Params */
  const nfapi_nr_measurement_param_t *measurement_param = &msg->measurement_param;
  INDENTED_TLV_PRINT("RSSI Measurement Support", measurement_param->rssi_measurement_support);
  /* nFAPI Config */
  const nfapi_nr_nfapi_t *nfapi_config = &msg->nfapi_config;
  if (nfapi_config->p7_vnf_address_ipv4.tl.tag) {
    char ip_str[INET_ADDRSTRLEN];
    INDENTED_GENERIC_PRINT("(0x%04x) P7 VNF Address IPv4",
                           "%s",
                           nfapi_config->p7_vnf_address_ipv4.tl.tag,
                           inet_ntop(AF_INET, nfapi_config->p7_vnf_address_ipv4.address, ip_str, sizeof(ip_str)));
  }
  if (nfapi_config->p7_vnf_address_ipv6.tl.tag) {
    char addr6[INET6_ADDRSTRLEN];
    INDENTED_GENERIC_PRINT("(0x%04x) P7 VNF Address IPv6",
                           "%s",
                           nfapi_config->p7_vnf_address_ipv6.tl.tag,
                           inet_ntop(AF_INET6, nfapi_config->p7_vnf_address_ipv6.address, addr6, sizeof(addr6)));
  }
  INDENTED_TLV_FORMAT_PRINT("P7 VNF Port", "%d", nfapi_config->p7_vnf_port);

  if (nfapi_config->p7_pnf_address_ipv4.tl.tag) {
    char ip_str[INET_ADDRSTRLEN];
    INDENTED_GENERIC_PRINT("(0x%04x) P7 PNF Address IPv4",
                           "%s",
                           nfapi_config->p7_pnf_address_ipv4.tl.tag,
                           inet_ntop(AF_INET, nfapi_config->p7_pnf_address_ipv4.address, ip_str, sizeof(ip_str)));
  }
  if (nfapi_config->p7_pnf_address_ipv6.tl.tag) {
    char addr6[INET6_ADDRSTRLEN];
    INDENTED_GENERIC_PRINT("(0x%04x) P7 PNF Address IPv6",
                           "%s",
                           nfapi_config->p7_pnf_address_ipv6.tl.tag,
                           inet_ntop(AF_INET6, nfapi_config->p7_pnf_address_ipv6.address, addr6, sizeof(addr6)));
  }
  INDENTED_TLV_FORMAT_PRINT("P7 PNF Port", "%d", nfapi_config->p7_pnf_port);
  INDENTED_TLV_FORMAT_PRINT("Timing Window", "%d", nfapi_config->timing_window);
  INDENTED_TLV_FORMAT_PRINT("Timing Info Mode", "%d", nfapi_config->timing_info_mode);
  INDENTED_TLV_FORMAT_PRINT("Timing Info Period", "%d", nfapi_config->timing_info_period);
  INDENTED_TLV_FORMAT_PRINT("DL_TTI Timing Offset", "%d", nfapi_config->dl_tti_timing_offset);
  INDENTED_TLV_FORMAT_PRINT("UL_TTI Timing Offset", "%d", nfapi_config->ul_tti_timing_offset);
  INDENTED_TLV_FORMAT_PRINT("UL_DCI Timing Offset", "%d", nfapi_config->ul_dci_timing_offset);
  INDENTED_TLV_FORMAT_PRINT("TX_DATA Timing Offset", "%d", nfapi_config->tx_data_timing_offset);
  depth--;
}

void dump_config_request(const nfapi_nr_config_request_scf_t *msg)
{
  int depth = 0;
  dump_p5_message_header(&msg->header, depth);
  depth++;
  INDENTED_PRINTF("Number of TLVs = 0x%02x\n", msg->num_tlv);
  /* dump all TLVs from each table */
  /* Carrier Config */
  const nfapi_nr_carrier_config_t *carrier_config = &msg->carrier_config;
  INDENTED_TLV_PRINT("DL Bandwidth", carrier_config->dl_bandwidth);
  INDENTED_TLV_PRINT("DL Frequency", carrier_config->dl_frequency);
  INDENTED_PRINTF("(0x%04x) DL K0 = ", NFAPI_NR_CONFIG_DL_K0_TAG);
  for (int i = 0; i < 5; i++) {
    printf("%d ", carrier_config->dl_k0[i].value);
  }
  printf("\n");
  INDENTED_PRINTF("(0x%04x) DL Grid Size = ", NFAPI_NR_CONFIG_DL_GRID_SIZE_TAG);
  for (int i = 0; i < 5; i++) {
    printf("%d ", carrier_config->dl_grid_size[i].value);
  }
  printf("\n");
  INDENTED_TLV_PRINT("Num TX Antennas", carrier_config->num_tx_ant);
  INDENTED_TLV_PRINT("UL Bandwidth", carrier_config->uplink_bandwidth);
  INDENTED_TLV_PRINT("UL Frequency", carrier_config->uplink_frequency);
  INDENTED_PRINTF("(0x%04x) UL K0 = ", NFAPI_NR_CONFIG_UL_K0_TAG);
  for (int i = 0; i < 5; i++) {
    printf("%d ", carrier_config->ul_k0[i].value);
  }
  printf("\n");
  INDENTED_PRINTF("(0x%04x) UL Grid Size = ", NFAPI_NR_CONFIG_UL_GRID_SIZE_TAG);
  for (int i = 0; i < 5; i++) {
    printf("%d ", carrier_config->ul_grid_size[i].value);
  }
  printf("\n");
  INDENTED_TLV_PRINT("Num RX Antennas", carrier_config->num_rx_ant);
  INDENTED_TLV_PRINT("Frequency Shift 7p5KHz", carrier_config->frequency_shift_7p5khz);
  /* Cell Config */
  const nfapi_nr_cell_config_t *cell_config = &msg->cell_config;
  INDENTED_TLV_PRINT("Physical Cell ID", cell_config->phy_cell_id);
  INDENTED_TLV_PRINT("Frame Duplex Type", cell_config->frame_duplex_type);
  /* SSB Config */
  const nfapi_nr_ssb_config_t *ssb_config = &msg->ssb_config;
  INDENTED_TLV_PRINT("SSB Block Power", ssb_config->ss_pbch_power);
  INDENTED_TLV_PRINT("BCH Payload", ssb_config->bch_payload);
  INDENTED_TLV_PRINT("SCS Common", ssb_config->scs_common);
  /* PRACH Config */
  const nfapi_nr_prach_config_t *prach_config = &msg->prach_config;
  INDENTED_TLV_PRINT("PRACH Sequence Length", prach_config->prach_sequence_length);
  INDENTED_TLV_PRINT("PRACH Subcarrier Spacing", prach_config->prach_sub_c_spacing);
  INDENTED_TLV_PRINT("Restricted Set Config", prach_config->restricted_set_config);
  INDENTED_TLV_PRINT("Num PRACH FD Occasions", prach_config->num_prach_fd_occasions);
  INDENTED_TLV_PRINT("PRACH Config Index", prach_config->prach_ConfigurationIndex);
  depth++;
  for (int i = 0; i < prach_config->num_prach_fd_occasions.value; i++) {
    const nfapi_nr_num_prach_fd_occasions_t *prach_fd_occasion = &(prach_config->num_prach_fd_occasions_list[i]);
    INDENTED_TLV_PRINT("PRACH Root Sequence Index", prach_fd_occasion->prach_root_sequence_index);
    INDENTED_TLV_PRINT("Num Root Sequences", prach_fd_occasion->num_root_sequences);
    INDENTED_TLV_PRINT("K1", prach_fd_occasion->k1);
    INDENTED_TLV_PRINT("PRACH Zero CorrelationZone Config", prach_fd_occasion->prach_zero_corr_conf);
    INDENTED_TLV_PRINT("Num Unused Root Sequences", prach_fd_occasion->num_unused_root_sequences);
    depth++;
    for (int k = 0; k < prach_fd_occasion->num_unused_root_sequences.value; k++) {
      INDENTED_TLV_PRINT("Unused Root Sequence", prach_fd_occasion->unused_root_sequences_list[k]);
    }
    depth--;
  }
  depth--;
  INDENTED_TLV_PRINT("SSB Per RACH", prach_config->ssb_per_rach);
  INDENTED_TLV_PRINT("PRACH Multiple Carriers in a Band", prach_config->prach_multiple_carriers_in_a_band);
  /* SSB Table */
  const nfapi_nr_ssb_table_t *ssb_table = &msg->ssb_table;
  INDENTED_TLV_PRINT("SSB Offset PointA", ssb_table->ssb_offset_point_a);
  INDENTED_TLV_PRINT("Beta PSS", ssb_table->beta_pss);
  INDENTED_TLV_PRINT("SSB Period", ssb_table->ssb_period);
  INDENTED_TLV_PRINT("SSB Subcarrier Offset", ssb_table->ssb_subcarrier_offset);
  INDENTED_TLV_PRINT("MIB", ssb_table->MIB);

  INDENTED_PRINTF("SSB Mask:\n");
  depth++;
  for (int i = 0; i < 2; i++) {
    INDENTED_TLV_PRINT("SSB Mask", ssb_table->ssb_mask_list[i].ssb_mask);
  }
  depth--;
  INDENTED_PRINTF("Beam ID:\n");
  depth++;
  for (int i = 0; i < 64; i++) {
    INDENTED_TLV_PRINT("Beam ID", ssb_table->ssb_beam_id_list[i].beam_id);
  }
  depth--;
  INDENTED_TLV_PRINT("ssPBCH Multiple Carriers in a Band", ssb_table->ss_pbch_multiple_carriers_in_a_band);
  INDENTED_TLV_PRINT("Multiple Cells ssPBCH in a Carrier", ssb_table->multiple_cells_ss_pbch_in_a_carrier);
  // Only print if it's filled
  if (ssb_table->case_v3.tl.tag == NFAPI_NR_FAPI_SSB_CASE_VENDOR_EXTENSION_TAG) {
    INDENTED_TLV_PRINT("SSB Case V3", ssb_table->case_v3);
  }
  /* TDD Table */
  const nfapi_nr_tdd_table_t *tdd_table = &msg->tdd_table;
  if (cell_config->frame_duplex_type.value == 1 /* TDD */) {
    INDENTED_TLV_PRINT("TDD Period", tdd_table->tdd_period);
    const uint8_t slotsperframe[5] = {10, 20, 40, 80, 160};
    // Assuming always CP_Normal, because Cyclic prefix is not included in CONFIG.request 10.02, but is present in 10.04
    uint8_t cyclicprefix = 1;
    // 3GPP 38.211 Table 4.3.2.1 & Table 4.3.2.2
    uint8_t number_of_symbols_per_slot = cyclicprefix ? 14 : 12;
    INDENTED_PRINTF("Slot Config:\n");
    depth++;
    for (int i = 0; i < slotsperframe[ssb_config->scs_common.value]; i++) {
      for (int k = 0; k < number_of_symbols_per_slot; k++) {
        const uint8_t slot_type = tdd_table->max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config.value;
        const char *slot_dir = slot_type == 0 ? "DL" : slot_type == 1 ? "UL" : "S";
        INDENTED_GENERIC_PRINT("(0x%04x) Slot Config",
                               "%d (%s)",
                               tdd_table->max_tdd_periodicity_list[i].max_num_of_symbol_per_slot_list[k].slot_config.tl.tag,
                               slot_type,
                               slot_dir);
      }
    }
    depth--;
  }
  /* Measurement Config */
  const nfapi_nr_measurement_config_t *measurement_config = &msg->measurement_config;
  INDENTED_TLV_PRINT("RSSI Measurement", measurement_config->rssi_measurement);
  /* Beamforming Table */
  const nfapi_nr_dbt_pdu_t *dbt_config = &msg->dbt_config;
  INDENTED_PRINTF("Digital Beam Table\n");
  depth++;
  INDENTED_GENERIC_PRINT("Num Digital Beams", "%d", dbt_config->num_dig_beams);
  INDENTED_GENERIC_PRINT("Num TX RUs", "%d", dbt_config->num_txrus);
  depth++;
  for (int i = 0; i < dbt_config->num_dig_beams; i++) {
    const nfapi_nr_dig_beam_t *dig_beam = &dbt_config->dig_beam_list[i];
    INDENTED_GENERIC_PRINT("Beam IDX", "%d", dig_beam->beam_idx);
    depth++;
    for (int k = 0; k < dbt_config->num_txrus; k++) {
      const nfapi_nr_txru_t *tx_ru = &dig_beam->txru_list[k];
      INDENTED_GENERIC_PRINT("Dig Beam Weight Real", "0x%02x", tx_ru->r);
      INDENTED_GENERIC_PRINT("Dig Beam Weight Imaginary", "0x%02x", tx_ru->i);
    }
    depth--;
  }
  depth--;
  depth--;

  /* Precoding Table */
  const nfapi_nr_pm_list_t *pmi_list = &msg->pmi_list;
  INDENTED_PRINTF("Precoding Matrix Table\n");
  depth++;
  INDENTED_GENERIC_PRINT("Num Precoding Tables", "%d", pmi_list->num_pm_idx);
  depth++;
  for (int i = 0; i < pmi_list->num_pm_idx; i++) {
    const nfapi_nr_pm_pdu_t *pm_pdu = &pmi_list->pmi_pdu[i];

    INDENTED_GENERIC_PRINT("Precoding Matrix IDX", "0x%02x", pm_pdu->pm_idx);
    INDENTED_GENERIC_PRINT("Number of Layers", "%d", pm_pdu->numLayers);
    INDENTED_GENERIC_PRINT("Number of Antenna Ports", "%d", pm_pdu->num_ant_ports);
    depth++;
    for (int k = 0; k < pm_pdu->numLayers; k++) {
      for (int l = 0; l < pm_pdu->num_ant_ports; l++) {
        const nfapi_nr_pm_weights_t *pm_weights = &pm_pdu->weights[k][l];
        INDENTED_GENERIC_PRINT("Dig Beam Weight Real", "0x%02x", pm_weights->r);
        INDENTED_GENERIC_PRINT("Dig Beam Weight Imaginary", "0x%02x", pm_weights->i);
      }
    }
    depth--;
  }
  depth--;
  depth--;
  /* nFAPI Config */
  const nfapi_nr_nfapi_t *nfapi_config = &msg->nfapi_config;
  if (nfapi_config->p7_vnf_address_ipv4.tl.tag) {
    char ip_str[INET_ADDRSTRLEN];
    INDENTED_GENERIC_PRINT("(0x%04x) P7 VNF Address IPv4",
                           "%s",
                           nfapi_config->p7_vnf_address_ipv4.tl.tag,
                           inet_ntop(AF_INET, nfapi_config->p7_vnf_address_ipv4.address, ip_str, sizeof(ip_str)));
  }
  if (nfapi_config->p7_vnf_address_ipv6.tl.tag) {
    char addr6[INET6_ADDRSTRLEN];
    INDENTED_GENERIC_PRINT("(0x%04x) P7 VNF Address IPv6",
                           "%s",
                           nfapi_config->p7_vnf_address_ipv6.tl.tag,
                           inet_ntop(AF_INET6, nfapi_config->p7_vnf_address_ipv6.address, addr6, sizeof(addr6)));
  }
  INDENTED_TLV_FORMAT_PRINT("P7 VNF Port", "%d", nfapi_config->p7_vnf_port);

  if (nfapi_config->p7_pnf_address_ipv4.tl.tag) {
    char ip_str[INET_ADDRSTRLEN];
    INDENTED_GENERIC_PRINT("(0x%04x) P7 PNF Address IPv4",
                           "%s",
                           nfapi_config->p7_pnf_address_ipv4.tl.tag,
                           inet_ntop(AF_INET, nfapi_config->p7_pnf_address_ipv4.address, ip_str, sizeof(ip_str)));
  }
  if (nfapi_config->p7_pnf_address_ipv6.tl.tag) {
    char addr6[INET6_ADDRSTRLEN];
    INDENTED_GENERIC_PRINT("(0x%04x) P7 PNF Address IPv6",
                           "%s",
                           nfapi_config->p7_pnf_address_ipv6.tl.tag,
                           inet_ntop(AF_INET6, nfapi_config->p7_pnf_address_ipv6.address, addr6, sizeof(addr6)));
  }
  INDENTED_TLV_FORMAT_PRINT("P7 PNF Port", "%d", nfapi_config->p7_pnf_port);
  INDENTED_TLV_FORMAT_PRINT("Timing Window", "%d", nfapi_config->timing_window);
  INDENTED_TLV_FORMAT_PRINT("Timing Info Mode", "%d", nfapi_config->timing_info_mode);
  INDENTED_TLV_FORMAT_PRINT("Timing Info Period", "%d", nfapi_config->timing_info_period);
  INDENTED_TLV_FORMAT_PRINT("DL_TTI Timing Offset", "%d", nfapi_config->dl_tti_timing_offset);
  INDENTED_TLV_FORMAT_PRINT("UL_TTI Timing Offset", "%d", nfapi_config->ul_tti_timing_offset);
  INDENTED_TLV_FORMAT_PRINT("UL_DCI Timing Offset", "%d", nfapi_config->ul_dci_timing_offset);
  INDENTED_TLV_FORMAT_PRINT("TX_DATA Timing Offset", "%d", nfapi_config->tx_data_timing_offset);
  /* Beamforming VE */
  const nfapi_nr_analog_beamforming_ve_t *analog_beamforming_ve = &msg->analog_beamforming_ve;
  INDENTED_TLV_FORMAT_PRINT("Num Beams per Period", "%d", analog_beamforming_ve->num_beams_period_vendor_ext);
  INDENTED_TLV_PRINT("Analog Beamforming VE", analog_beamforming_ve->analog_bf_vendor_ext);
  /* Vendor Extension */
  if (msg->vendor_extension) {
    const nfapi_tl_t *vendor_extension = (nfapi_tl_t *)&msg->vendor_extension;
    switch (vendor_extension->tag) {
      case VENDOR_EXT_TLV_1_TAG: {
        vendor_ext_tlv_1 *ve = (vendor_ext_tlv_1 *)vendor_extension;
        INDENTED_PRINTF("(0x%04x) Vendor extension = %d 0x%02x\n", ve->tl.tag, ve->tl.length, ve->dummy);
      } break;
      case VENDOR_EXT_TLV_2_TAG: {
        vendor_ext_tlv_2 *ve = (vendor_ext_tlv_2 *)vendor_extension;
        INDENTED_PRINTF("(0x%04x) Vendor extension = %d 0x%02x\n", ve->tl.tag, ve->tl.length, ve->dummy);
      } break;
      default:
        break;
    }
  }

  depth--;
}

static void dump_generic_tlv(const nfapi_nr_generic_tlv_scf_t* tlv, int depth)
{
  INDENTED_PRINTF("TLV tag 0x%02x , Length 0x%02x , Value ", tlv->tl.tag, tlv->tl.length);
  switch (tlv->tl.length) {
    case UINT_8:
      printf("0x%x\n", tlv->value.u8);
      break;
    case UINT_16:
      printf("0x%02x\n", tlv->value.u16);
      break;
    case UINT_32:
      printf("0x%04x\n", tlv->value.u32);
      break;
    case ARRAY_UINT_16:
      for (int i = 0; i< 5 ; i++) {
        printf("0x%04x ", tlv->value.array_u16[i]);
      }
      printf("\n");
      break;
    default:
      printf("unknown length %d\n", tlv->tl.length);
      DevAssert(1 == 0);
      break;
  }
}

void dump_config_response(const nfapi_nr_config_response_scf_t *msg)
{
  int depth = 0;
  dump_p5_message_header(&msg->header,depth);
  depth++;
  INDENTED_PRINTF("Error Code = 0x%02x\n", msg->error_code);
  INDENTED_PRINTF("Number of Invalid or Unsupported TLVs = 0x%02x\n", msg->num_invalid_tlvs);
  INDENTED_PRINTF("Number of Invalid TLVs that can be configured in IDLE = 0x%02x\n", msg->num_invalid_tlvs_configured_in_idle);
  INDENTED_PRINTF("Number of Invalid TLVs that can be configured in RUNNING = 0x%02x\n",
                  msg->num_invalid_tlvs_configured_in_running);
  INDENTED_PRINTF("Number of Missing TLVs = 0x%02x\n", msg->num_missing_tlvs);
  /* dump all TLVs from each table */
  INDENTED_PRINTF("Invalid or Unsupported TLVs (%d) :\n",msg->num_invalid_tlvs);
  for (int i = 0; i < msg->num_invalid_tlvs; ++i) {
    dump_generic_tlv(&msg->invalid_tlvs_list[i], depth+1);
  }

  INDENTED_PRINTF("TLVs that can only be configured in IDLE (%d):\n", msg->num_invalid_tlvs_configured_in_idle);
  for (int i = 0; i < msg->num_invalid_tlvs; ++i) {
    dump_generic_tlv(&msg->invalid_tlvs_list[i], depth+1);
  }

  INDENTED_PRINTF("TLVs that can only be configured in RUNNING (%d):\n",msg->num_invalid_tlvs_configured_in_running);
  for (int i = 0; i < msg->num_invalid_tlvs; ++i) {
    dump_generic_tlv(&msg->invalid_tlvs_list[i], depth+1);
  }

  INDENTED_PRINTF("Missing TLVs (%d):\n",msg->num_missing_tlvs);
  for (int i = 0; i < msg->num_invalid_tlvs; ++i) {
    dump_generic_tlv(&msg->invalid_tlvs_list[i], depth+1);
  }
  depth--;
}

void dump_start_request(const nfapi_nr_start_request_scf_t *msg)
{
  int depth = 0;
  dump_p5_message_header(&msg->header,depth);
}

void dump_start_response(const nfapi_nr_start_response_scf_t *msg)
{
  int depth = 0;
  dump_p5_message_header(&msg->header,depth);
}

void dump_stop_request(const nfapi_nr_stop_request_scf_t *msg)
{
  int depth = 0;
  dump_p5_message_header(&msg->header,depth);
}

void dump_stop_indication(const nfapi_nr_stop_indication_scf_t *msg)
{
  int depth = 0;
  dump_p5_message_header(&msg->header,depth);
}

char* error_ind_code_to_str(nfapi_nr_phy_notifications_errors_e error_code)
{
  switch (error_code) {
#define X(name, value) case name: return #name;
    NFAPI_PHY_ERROR_LIST
#undef X
    default: return "UNKNOWN_ERROR";
  }
}

void dump_error_indication(const nfapi_nr_error_indication_scf_t *msg)
{
  int depth = 0;
  dump_p5_message_header(&msg->header,depth);
  depth++;
  INDENTED_PRINTF("SFN = 0x%02x (%d)\n", msg->sfn, msg->sfn);
  INDENTED_PRINTF("Slot = 0x%02x (%d)\n", msg->slot, msg->slot);
  INDENTED_PRINTF("Message ID = 0x%02x\n", msg->message_id);
  INDENTED_PRINTF("Error Code = 0x%02x (%s) \n", msg->error_code, error_ind_code_to_str(msg->error_code));
  depth--;
}
