# SPDX-License-Identifier: LicenseRef-CSSL-1.0

cmake_minimum_required(VERSION 3.10)

add_subdirectory(MESSAGES)

set(L1_KPM_SM_SOURCES
    l1_kpm_sm.c
    l1_kpm_enc.c
    e3_ran_buffers.c
)

# ASN.1 is always compiled in; the JSON and protobuf codecs below are added
# only if their library is present. The active one is selected at runtime from
# the config file (E3Configuration.encoding).
add_library(l1_kpm_sm STATIC ${L1_KPM_SM_SOURCES})

# USRP front ends need a larger IQ->FP16 scale than the cuPHY baseline; see
# E3_RB_FP16_BETA in e3_ran_buffers.c.
if(OAI_USRP)
    target_compile_definitions(l1_kpm_sm PRIVATE E3_FP16_BETA_USRP)
endif()


target_include_directories(l1_kpm_sm PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common/utils/LOG
    ${CMAKE_SOURCE_DIR}/openair1
    ${CMAKE_SOURCE_DIR}/openair2/E3AP
    ${CLIBE3_INCLUDE_DIRS}
)

# The SM calls back into the agent for e3_get_encoding(), while the agent links
# the SM to register it: a genuine cycle between two static libraries. Declaring
# it lets CMake repeat them on the link line instead of relying on the executable
# to wrap them in --start-group, which the phy simulators do not do.
target_link_libraries(l1_kpm_sm e3ap)
target_link_directories(l1_kpm_sm PRIVATE ${CLIBE3_LIBRARY_DIRS})
target_link_libraries(l1_kpm_sm
    rt
    pthread
    LOG
)

target_include_directories(l1_kpm_sm PUBLIC ${L1_KPM_SM_ASN1_INCLUDE_DIR})
target_link_libraries(l1_kpm_sm ${L1_KPM_SM_ASN1_LIB})
add_dependencies(l1_kpm_sm ${L1_KPM_SM_ASN1_LIB})

# JSON payload codec (optional, like the protobuf one below): the 'json'
# encoding needs json-c. Without it the SM still builds with ASN.1 only.
pkg_check_modules(JSON_C json-c)
if(JSON_C_FOUND)
  target_include_directories(l1_kpm_sm PUBLIC ${JSON_C_INCLUDE_DIRS})
  target_link_libraries(l1_kpm_sm ${JSON_C_LIBRARIES})
  target_compile_definitions(l1_kpm_sm PRIVATE E3_SM_HAVE_JSON)
else()
  message(WARNING "json-c not found: the L1-KPM SM will not support the 'json' encoding")
endif()


if(L1_KPM_SM_HAVE_PROTOBUF)
  target_include_directories(l1_kpm_sm PUBLIC ${CMAKE_CURRENT_BINARY_DIR}/MESSAGES ${PROTOBUFC_INCLUDE_DIRS})
  target_link_libraries(l1_kpm_sm l1_kpm_sm_pb)
  target_compile_definitions(l1_kpm_sm PRIVATE E3_SM_HAVE_PROTOBUF)
endif()

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()

set(L1_KPM_SM_LIBRARY l1_kpm_sm PARENT_SCOPE)
