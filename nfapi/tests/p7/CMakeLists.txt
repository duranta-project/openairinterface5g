# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(Test_Labels fapi p7)
set(_fapi_p7_messages "slot_indication;rx_data_indication;crc_indication;uci_indication;rach_indication;srs_toa_vendor_ext_indication")
set(_fapi_p7_no_aerial_messages "dl_tti_request;ul_tti_request;ul_dci_request;tx_data_request;srs_indication")
foreach (fapi_p7_message IN LISTS _fapi_p7_messages)
    add_nr_fapi_test(nr_fapi_${fapi_p7_message} nr_fapi_p7 "${Test_Labels}")
endforeach ()
foreach (fapi_p7_message IN LISTS _fapi_p7_no_aerial_messages)
    add_nr_fapi_test(nr_fapi_${fapi_p7_message} nr_fapi_p7 "${Test_Labels}" NO_AERIAL)
endforeach ()
add_nr_fapi_test(nr_fapi_p7_phy_id nr_fapi_p7 "${Test_Labels}")

add_executable(nr_fapi_p7_bounds_test nr_fapi_p7_bounds_test.c)
target_link_libraries(nr_fapi_p7_bounds_test PUBLIC nr_fapi_p7)
target_link_libraries(nr_fapi_p7_bounds_test PRIVATE pthread UTIL ${T_LIB} minimal_lib)
add_dependencies(tests nr_fapi_p7_bounds_test)

add_test(nr_fapi_p7_bounds_test nr_fapi_p7_bounds_test)
set_tests_properties(nr_fapi_p7_bounds_test PROPERTIES LABELS "${Test_Labels}")

# Add the dci label for the dci payload test
add_nr_fapi_test(nr_fapi_dci_inversion nr_fapi_p7 "dci;${Test_Labels}")
# Add the FAPI_BYTE_ORDERING_BIG_ENDIAN preprocessor define in order to test the invert/uninvert dci payload functions
target_compile_definitions(nfapi_common PRIVATE FAPI_BYTE_ORDERING_BIG_ENDIAN)
