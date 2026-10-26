# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(snow3g_tests snow3g_tests.c)
target_link_libraries(snow3g_tests PRIVATE nr_pdcp_snow3g minimal_lib SECURITY)
add_test(NAME snow3g_tests
         COMMAND snow3g_tests)
add_dependencies(tests snow3g_tests)

add_executable(test_nr_pdcp_rx
               test_nr_pdcp_rx.c
               ../nr_pdcp_entity.c
               ../nr_pdcp_sdu.c
               ../nr_pdcp_security_nea2.c
               ../nr_pdcp_integrity_nia2.c)
target_link_libraries(test_nr_pdcp_rx PRIVATE nr_pdcp_snow3g minimal_lib SECURITY)
add_test(NAME test_nr_pdcp_rx
         COMMAND test_nr_pdcp_rx)
add_dependencies(tests test_nr_pdcp_rx)
