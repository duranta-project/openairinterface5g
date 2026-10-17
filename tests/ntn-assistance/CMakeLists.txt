# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(NTN_MAC_DIR ${CMAKE_SOURCE_DIR}/openair2/LAYER2/NR_MAC_gNB)

# These C fixtures call production functions. Section collection lets a fixture
# isolate those entrypoints without linking an entire running softmodem.
function(add_ntn_test name)
  add_executable(${name} ${ARGN})
  target_include_directories(${name} PRIVATE
    ${NTN_MAC_DIR} ${NTN_JANSSON_INCLUDE_DIR}
    $<TARGET_PROPERTY:L2_NR,INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:nr-softmodem,INCLUDE_DIRECTORIES>)
  target_compile_definitions(${name} PRIVATE
    $<TARGET_PROPERTY:L2_NR,COMPILE_DEFINITIONS>)
  target_compile_options(${name} PRIVATE -UNDEBUG -UT_TRACER -ffunction-sections -fdata-sections)
  # The top-level softmodem -rdynamic keeps every exported section alive. These
  # isolated fixtures load no plugins and retain only directly exercised code.
  target_link_options(${name} PRIVATE -Wl,--no-export-dynamic -Wl,--gc-sections)
  target_link_libraries(${name} PRIVATE asn1_nr_rrc ${NTN_JANSSON_LIBRARY} m pthread)
  add_dependencies(tests ${name})
  add_test(NAME ${name} COMMAND ${name})
endfunction()

add_ntn_test(test_ntn_assistance_codec codec_test.c ${NTN_MAC_DIR}/ntn_assistance.c)
add_ntn_test(test_ntn_assistance_server server_test.c)
add_ntn_test(test_ntn_assistance_publisher publisher_test.c
  ${NTN_MAC_DIR}/config.c ${NTN_MAC_DIR}/nr_radio_config.c)
target_link_options(test_ntn_assistance_publisher PRIVATE -Wl,--wrap=calloc)
add_ntn_test(test_ntn_radio_time radio_time_test.c ${CMAKE_SOURCE_DIR}/openair1/PHY/INIT/nr_parms.c)
target_link_options(test_ntn_radio_time PRIVATE -Wl,--wrap=clock_gettime)
