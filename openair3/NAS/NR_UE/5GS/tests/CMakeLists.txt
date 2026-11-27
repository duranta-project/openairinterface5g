# SPDX-License-Identifier: LicenseRef-CSSL-1.0

message(STATUS "Add test nas_lib_test")

add_executable(nas_lib_test nas_lib_test.c)

add_dependencies(tests nas_lib_test)
add_test(NAME nas_lib_test COMMAND nas_lib_test)

target_link_libraries(nas_lib_test PRIVATE
    nr_nas
)

target_include_directories(nas_lib_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_executable(nr_sdap_qos_rule_test nr_sdap_qos_rule_test.c
    ${OPENAIR2_DIR}/SDAP/nr_sdap/nr_sdap_qos_rule.c)
target_link_libraries(nr_sdap_qos_rule_test PRIVATE nr_nas alg)
add_dependencies(tests nr_sdap_qos_rule_test)
add_test(NAME nr_sdap_qos_rule_test COMMAND nr_sdap_qos_rule_test)
