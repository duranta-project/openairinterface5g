# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(Test_Labels fapi p5)
set(_fapi_p5_messages "param_request;param_response;config_request;config_response;start_request;start_response;stop_request;stop_indication;error_indication")

foreach (fapi_p5_message IN LISTS _fapi_p5_messages)
    add_executable(nr_fapi_${fapi_p5_message}_test nr_fapi_${fapi_p5_message}_test.c)
    target_link_libraries(nr_fapi_${fapi_p5_message}_test PUBLIC nr_fapi_p5)
    target_link_libraries(nr_fapi_${fapi_p5_message}_test PRIVATE pthread UTIL ${T_LIB} minimal_lib)
    add_dependencies(tests nr_fapi_${fapi_p5_message}_test)

    add_test(nr_fapi_${fapi_p5_message}_test nr_fapi_${fapi_p5_message}_test)
    set_tests_properties(nr_fapi_${fapi_p5_message}_test PROPERTIES LABELS "${Test_Labels}")
endforeach ()

add_executable(nr_fapi_p5_phy_id_test nr_fapi_p5_phy_id_test.c)
target_link_libraries(nr_fapi_p5_phy_id_test PUBLIC nr_fapi_p5)
target_link_libraries(nr_fapi_p5_phy_id_test PRIVATE pthread UTIL ${T_LIB} minimal_lib)
add_dependencies(tests nr_fapi_p5_phy_id_test)

add_test(nr_fapi_p5_phy_id_test nr_fapi_p5_phy_id_test)
set_tests_properties(nr_fapi_p5_phy_id_test PROPERTIES LABELS "${Test_Labels}")

add_executable(nr_fapi_config_request_bounds_test nr_fapi_config_request_bounds_test.c)
target_link_libraries(nr_fapi_config_request_bounds_test PUBLIC nr_fapi_p5)
target_link_libraries(nr_fapi_config_request_bounds_test PRIVATE pthread UTIL ${T_LIB} minimal_lib)
# same TLV variants as the library under test
if (OAI_AERIAL)
  target_compile_definitions(nr_fapi_config_request_bounds_test PRIVATE ENABLE_AERIAL ENABLE_10_04)
endif ()
add_dependencies(tests nr_fapi_config_request_bounds_test)

add_test(nr_fapi_config_request_bounds_test nr_fapi_config_request_bounds_test)
set_tests_properties(nr_fapi_config_request_bounds_test PROPERTIES LABELS "${Test_Labels}")
