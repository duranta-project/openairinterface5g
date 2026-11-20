# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(nfapi_common
            src/debug.c
            src/nfapi.c)
target_include_directories(nfapi_common PUBLIC public_inc/)
