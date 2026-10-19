# SPDX-License-Identifier: LicenseRef-CSSL-1.0

# UE Low-PHY: timebase, client port, device.
add_library(ue_low_phy STATIC wait.c timebase.c port.c device.c)
target_link_libraries(ue_low_phy PUBLIC radio_common)

# UE anchor service: cell search on the device's RX ring.
add_library(ue_low_phy_sync STATIC sync.c)
target_link_libraries(ue_low_phy_sync PUBLIC ue_low_phy)

