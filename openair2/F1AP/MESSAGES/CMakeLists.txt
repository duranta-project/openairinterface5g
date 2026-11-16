# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(F1AP_VERSION 17 0 0)
make_version(F1AP_cc ${F1AP_VERSION})
string(REPLACE ";" "." F1AP_RELEASE "${F1AP_VERSION}")

if(F1AP_RELEASE VERSION_EQUAL "16.3.1")
  include(ASN1/f1ap-16.3.1.cmake)
elseif(F1AP_RELEASE VERSION_EQUAL "16.21.0")
  include(ASN1/f1ap-16.21.0.cmake)
elseif(F1AP_RELEASE VERSION_EQUAL "17.0.0")
  include(ASN1/f1ap-17.0.0.cmake)
else()
  message(FATAL_ERROR "unknown F1AP_RELEASE ${F1AP_RELEASE}")
endif()

run_asn1c(${CMAKE_CURRENT_SOURCE_DIR}/${F1AP_GRAMMAR}
          "F1AP_"
          OUTPUT ${f1ap_source} ${f1ap_headers}
          OPTIONS -gen-APER -no-gen-BER -no-gen-JER -no-gen-OER -gen-UPER -fcompound-names -no-gen-example -findirect-choice -fno-include-deps
          COMMENT "F1AP source files"
)

add_library(asn1_f1ap ${f1ap_source})
target_include_directories(asn1_f1ap PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(asn1_f1ap PRIVATE -DASN_DISABLE_OER_SUPPORT -w)
