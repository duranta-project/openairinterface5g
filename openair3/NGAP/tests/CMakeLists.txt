# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(ngap_lib_test ngap_lib_test.c)
add_dependencies(tests ngap_lib_test)
add_test(NAME ngap_lib_test COMMAND ngap_lib_test)
target_link_libraries(ngap_lib_test PRIVATE ngap asn1_ngap asn1_nr_rrc asn1_lte_rrc_hdrs UTIL ds)
target_include_directories(ngap_lib_test PRIVATE ${CMAKE_SOURCE_DIR})

