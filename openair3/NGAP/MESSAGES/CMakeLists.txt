# SPDX-License-Identifier: LicenseRef-CSSL-1.0

set(NGAP_VERSION 17 10 0)
make_version(NGAP_cc ${NGAP_VERSION})
#add_definitions(-DNGAP_VERSION=${NGAP_cc})
string(REPLACE ";" "." NGAP_RELEASE "${NGAP_VERSION}")

if(NGAP_RELEASE VERSION_EQUAL "15.8.0")
  include(ASN1/ngap-15.8.0.cmake)
elseif(NGAP_RELEASE VERSION_EQUAL "17.10.0")
  include(ASN1/ngap-17.10.0.cmake)
else()
  message(FATAL_ERROR "unknown NGAP_RELEASE ${NGAP_RELEASE}")
endif()

run_asn1c(${CMAKE_CURRENT_SOURCE_DIR}/${NGAP_GRAMMAR}
          "NGAP_"
          OUTPUT ${ngap_source} ${ngap_headers}
          OPTIONS -pdu=all -fcompound-names -gen-APER -no-gen-BER -no-gen-JER -no-gen-OER -gen-UPER -no-gen-example -fno-include-deps -findirect-choice
          COMMENT "NGAP source files"
)

add_library(asn1_ngap ${ngap_source})
target_include_directories(asn1_ngap PUBLIC "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_options(asn1_ngap PRIVATE -DASN_DISABLE_OER_SUPPORT -w)
