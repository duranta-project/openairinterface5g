# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nfapi_common
            src/debug.c
            src/nfapi.c)
target_include_directories(nfapi_common PUBLIC public_inc/)

if (ENABLE_TESTS)
  # Copies of nfapi_common with different FAPI_TARGET_BYTE_ORDER for unit tests
  add_library(nfapi_common_le src/nfapi.c src/debug.c)
  target_include_directories(nfapi_common_le PUBLIC public_inc/)
  target_compile_definitions(nfapi_common_le PRIVATE FAPI_TARGET_BYTE_ORDER=__ORDER_LITTLE_ENDIAN__)

  add_library(nfapi_common_be src/nfapi.c src/debug.c)
  target_include_directories(nfapi_common_be PUBLIC public_inc/)
  target_compile_definitions(nfapi_common_be PRIVATE FAPI_TARGET_BYTE_ORDER=__ORDER_BIG_ENDIAN__)
endif()
