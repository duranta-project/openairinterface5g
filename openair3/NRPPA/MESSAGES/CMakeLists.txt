# SPDX-License-Identifier: LicenseRef-CSSL-1.0

#set(NRPPA_VERSION 16 78 0)
#make_version(NRPPA_cc ${NRPPA_VERSION})
#add_definitions(-DNRPPA_VERSION=${NRPPA_cc})
#string(REPLACE ";" "." NRPPA_RELEASE "${NRPPA_VERSION}")

#if(NRPPA_RELEASE VERSION_EQUAL "16.78.0")
include(ASN1/38455.cmake)
#else()
#  message(FATAL_ERROR "unknown NRPPA_RELEASE ${NRPPA_RELEASE}")
#endif()

run_asn1c(${CMAKE_CURRENT_SOURCE_DIR}/${NRPPA_GRAMMAR}
          "NRPPA_"
          OUTPUT ${nrppa_source} ${nrppa_headers}
          OPTIONS -gen-APER -gen-UPER -no-gen-BER -no-gen-OER -no-gen-JER -fcompound-names -no-gen-example -findirect-choice -fno-include-deps
          COMMENT "NRPPA source files"
)

add_library(asn1_nrppa ${nrppa_source})
target_include_directories(asn1_nrppa PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(asn1_nrppa PRIVATE -DASN_DISABLE_OER_SUPPORT -w)


