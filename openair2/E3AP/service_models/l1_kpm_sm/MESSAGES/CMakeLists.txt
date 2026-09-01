# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(L1_KPM_SM_VERSION 1 0 0)
make_version(L1_KPM_SM_cc ${L1_KPM_SM_VERSION})
string(REPLACE ";" "." L1_KPM_SM_RELEASE "${L1_KPM_SM_VERSION}")

if(L1_KPM_SM_RELEASE VERSION_EQUAL "1.0.0")
  include(ASN1/V1/e3sm_l1_kpm.cmake)
else()
  message(FATAL_ERROR "unknown L1_KPM_SM_RELEASE ${L1_KPM_SM_RELEASE}")
endif()

message(STATUS "Selected L1_KPM_SM_VERSION: ${L1_KPM_SM_RELEASE}")
run_asn1c(${CMAKE_CURRENT_SOURCE_DIR}/${L1_KPM_SM_GRAMMAR}
          ""
          OUTPUT ${l1_kpm_sm_source} ${l1_kpm_sm_headers}
          OPTIONS -pdu=all -gen-APER -gen-UPER -no-gen-JER -no-gen-BER -no-gen-OER -fno-include-deps -fcompound-names -findirect-choice -no-gen-example
          COMMENT "L1-KPM SM source files"
)

add_library(l1_kpm_sm_asn1 ${l1_kpm_sm_source})
target_include_directories(l1_kpm_sm_asn1 PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(l1_kpm_sm_asn1 PRIVATE -DASN_DISABLE_OER_SUPPORT -w)

set(L1_KPM_SM_ASN1_LIB l1_kpm_sm_asn1 PARENT_SCOPE)
set(L1_KPM_SM_ASN1_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)

# Protobuf-c payload codec (optional; the first-class 'protobuf' encoding).
pkg_check_modules(PROTOBUFC libprotobuf-c)
find_program(PROTOC_C_EXECUTABLE protoc-c)
if(PROTOBUFC_FOUND AND PROTOC_C_EXECUTABLE)
  set(L1_KPM_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/PROTO/V1/e3sm_l1_kpm.proto)
  set(L1_KPM_PB_C ${CMAKE_CURRENT_BINARY_DIR}/e3sm_l1_kpm.pb-c.c)
  set(L1_KPM_PB_H ${CMAKE_CURRENT_BINARY_DIR}/e3sm_l1_kpm.pb-c.h)
  add_custom_command(
    OUTPUT ${L1_KPM_PB_C} ${L1_KPM_PB_H}
    COMMAND ${PROTOC_C_EXECUTABLE} --c_out=${CMAKE_CURRENT_BINARY_DIR}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/PROTO/V1 ${L1_KPM_PROTO}
    DEPENDS ${L1_KPM_PROTO}
    COMMENT "protoc-c: L1-KPM SM protobuf"
  )
  add_library(l1_kpm_sm_pb STATIC ${L1_KPM_PB_C})
  target_include_directories(l1_kpm_sm_pb PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" ${PROTOBUFC_INCLUDE_DIRS})
  target_link_libraries(l1_kpm_sm_pb ${PROTOBUFC_LIBRARIES})
  target_compile_options(l1_kpm_sm_pb PRIVATE -w)
  set(L1_KPM_SM_PB_LIB l1_kpm_sm_pb PARENT_SCOPE)
  set(L1_KPM_SM_HAVE_PROTOBUF ON PARENT_SCOPE)
  set(PROTOBUFC_INCLUDE_DIRS "${PROTOBUFC_INCLUDE_DIRS}" PARENT_SCOPE)
else()
  message(WARNING "libprotobuf-c/protoc-c not found: L1-KPM SM 'protobuf' encoding unavailable")
  set(L1_KPM_SM_HAVE_PROTOBUF OFF PARENT_SCOPE)
endif()
