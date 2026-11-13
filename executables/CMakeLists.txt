# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(softmodem_common STATIC softmodem-common.c)
target_link_libraries(softmodem_common PUBLIC shlib_loader)

add_subdirectory(ue_low_phy)
