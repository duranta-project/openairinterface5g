# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(SPECTRUM_SM_VERSION 1 0 0)
make_version(SPECTRUM_SM_cc ${SPECTRUM_SM_VERSION})
string(REPLACE ";" "." SPECTRUM_SM_RELEASE "${SPECTRUM_SM_VERSION}")

if(SPECTRUM_SM_RELEASE VERSION_EQUAL "1.0.0")
  include(ASN1/V1/e3sm_spectrum.cmake)
else()
  message(FATAL_ERROR "unknown SPECTRUM_SM_RELEASE ${SPECTRUM_SM_RELEASE}")
endif()

message(STATUS "Selected SPECTRUM_SM_VERSION: ${SPECTRUM_SM_RELEASE}")
run_asn1c(${CMAKE_CURRENT_SOURCE_DIR}/${SPECTRUM_SM_GRAMMAR}
          ""
          OUTPUT ${spectrum_sm_source} ${spectrum_sm_headers}
          OPTIONS -pdu=all -gen-APER -gen-UPER -no-gen-JER -no-gen-BER -no-gen-OER -fno-include-deps -fcompound-names -findirect-choice -no-gen-example
          COMMENT "Spectrum SM source files"
)


add_library(spectrum_sm_asn1 ${spectrum_sm_source})
target_include_directories(spectrum_sm_asn1 PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(spectrum_sm_asn1 PRIVATE -DASN_DISABLE_OER_SUPPORT -w)

# Export variables to parent scope
set(SPECTRUM_SM_ASN1_LIB spectrum_sm_asn1 PARENT_SCOPE)
set(SPECTRUM_SM_ASN1_INCLUDE_DIR "${CMAKE_CURRENT_BINARY_DIR}" PARENT_SCOPE)

# Protobuf-c SM payload codec (the first-class 'protobuf' encoding). Optional:
# when libprotobuf-c / protoc-c are absent the SM builds without it and the
# 'protobuf' encoding fails cleanly at runtime instead of breaking the build.
pkg_check_modules(PROTOBUFC libprotobuf-c)
find_program(PROTOC_C_EXECUTABLE protoc-c)
if(PROTOBUFC_FOUND AND PROTOC_C_EXECUTABLE)
  set(SPECTRUM_PROTO ${CMAKE_CURRENT_SOURCE_DIR}/PROTO/V1/e3sm_spectrum.proto)
  set(SPECTRUM_PB_C ${CMAKE_CURRENT_BINARY_DIR}/e3sm_spectrum.pb-c.c)
  set(SPECTRUM_PB_H ${CMAKE_CURRENT_BINARY_DIR}/e3sm_spectrum.pb-c.h)
  add_custom_command(
    OUTPUT ${SPECTRUM_PB_C} ${SPECTRUM_PB_H}
    COMMAND ${PROTOC_C_EXECUTABLE} --c_out=${CMAKE_CURRENT_BINARY_DIR}
            --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/PROTO/V1 ${SPECTRUM_PROTO}
    DEPENDS ${SPECTRUM_PROTO}
    COMMENT "protoc-c: Spectrum SM protobuf"
  )
  add_library(spectrum_sm_pb STATIC ${SPECTRUM_PB_C})
  target_include_directories(spectrum_sm_pb PUBLIC "${CMAKE_CURRENT_BINARY_DIR}" ${PROTOBUFC_INCLUDE_DIRS})
  target_link_libraries(spectrum_sm_pb ${PROTOBUFC_LIBRARIES})
  target_compile_options(spectrum_sm_pb PRIVATE -w)
  set(SPECTRUM_SM_PB_LIB spectrum_sm_pb PARENT_SCOPE)
  set(SPECTRUM_SM_HAVE_PROTOBUF ON PARENT_SCOPE)
  set(PROTOBUFC_INCLUDE_DIRS "${PROTOBUFC_INCLUDE_DIRS}" PARENT_SCOPE)
else()
  message(WARNING "libprotobuf-c/protoc-c not found: Spectrum SM 'protobuf' encoding unavailable")
  set(SPECTRUM_SM_HAVE_PROTOBUF OFF PARENT_SCOPE)
endif()