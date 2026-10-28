# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(Test_Labels fapi p5)
set(_fapi_p5_messages "param_request;param_response;config_response;start_request;start_response;stop_request;stop_indication;error_indication")

foreach (fapi_p5_message IN LISTS _fapi_p5_messages)
    add_nr_fapi_test(nr_fapi_${fapi_p5_message} nr_fapi_p5 "${Test_Labels}")
endforeach ()
add_nr_fapi_test(nr_fapi_config_request nr_fapi_p5 "${Test_Labels}" NO_AERIAL)

add_nr_fapi_test(nr_fapi_p5_phy_id nr_fapi_p5 "${Test_Labels}")
