# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nr_sl_slot_bitmap nr_sl_slot_bitmap.c)
add_library(nr_ue_power_procedures nr_ue_power_procedures.c)
target_link_libraries(nr_ue_power_procedures PUBLIC asn1_nr_rrc log_headers MAC_NR_COMMON nr_common)
add_library(nr_ue_ra_procedures nr_ra_procedures.c)
target_link_libraries(nr_ue_ra_procedures PUBLIC asn1_nr_rrc asn1_lte_rrc_hdrs log_headers MAC_NR_COMMON nr_common)
if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
