# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(softmodem_common STATIC softmodem-common.c)
target_link_libraries(softmodem_common PUBLIC shlib_loader)

add_subdirectory(ue_low_phy)

# O-RU south side simulating FD-UEs (nr-oru-ue.h)
add_library(oru_ue STATIC nr-oru-ue.c)
target_link_libraries(oru_ue PUBLIC ue_low_phy_port cdl_channel PHY_NR_COMMON UTIL)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
