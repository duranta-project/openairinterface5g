add_library(MAC_NR_COMMON
            nr_mac_common.c
            nr_mac_common_tdd.c
            ue_capability_handler.c
)
target_link_libraries(MAC_NR_COMMON PRIVATE asn1_nr_rrc_hdrs asn1_lte_rrc_hdrs)

# the TBS computation alone, without the MAC: the PHY sizes its buffers with it
add_library(nr_compute_tbs nr_compute_tbs_common.c)
target_link_libraries(nr_compute_tbs PUBLIC nr_common)
target_link_libraries(MAC_NR_COMMON PUBLIC nr_compute_tbs)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
