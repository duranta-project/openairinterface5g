# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nfapi_common
            src/debug.c
            src/nfapi.c)
target_include_directories(nfapi_common PUBLIC public_inc/)
target_link_libraries(nfapi_common PRIVATE log_headers)
if (OAI_AERIAL)
    target_compile_definitions(nfapi_common PRIVATE FAPI_BYTE_ORDERING_BIG_ENDIAN)
endif()
