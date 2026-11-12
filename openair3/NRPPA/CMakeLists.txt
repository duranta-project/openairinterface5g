# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_subdirectory(MESSAGES)

add_library(nrppa
  nrppa_common.c
  nrppa_gNB.c
  nrppa_gNB_config.c
  nrppa_gNB_handlers.c
  nrppa_gNB_decoder.c
  nrppa_gNB_ue_context.c
  nrppa_gNB_location_information_transfer.c
  nrppa_gNB_encoder.c
  nrppa_gNB_measurement_information_transfer.c
  )

target_link_libraries(nrppa PUBLIC asn1_nrppa)
target_link_libraries(nrppa PRIVATE asn1_lte_rrc_hdrs asn1_nr_rrc_hdrs log_headers)
