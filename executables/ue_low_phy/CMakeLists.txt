# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# UE Low-PHY port (process or shared memory) and its frequency-domain host.
add_library(ue_low_phy_port STATIC wait.c port.c fd_host.c)
target_link_libraries(ue_low_phy_port PUBLIC log_headers)

# UE Low-PHY device: timebase, radio, OFDM.
add_library(ue_low_phy STATIC timebase.c device.c)
target_link_libraries(ue_low_phy PUBLIC ue_low_phy_port radio_common)

# UE anchor service: cell search on the device's RX ring.
add_library(ue_low_phy_sync STATIC sync.c)
target_link_libraries(ue_low_phy_sync PUBLIC ue_low_phy)

if(ENABLE_TESTS)
  add_subdirectory(tests)
endif()
