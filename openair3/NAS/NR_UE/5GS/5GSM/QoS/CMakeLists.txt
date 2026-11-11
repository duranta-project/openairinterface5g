# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(packet_filter_lib OBJECT PacketFilter.c)
target_link_libraries(packet_filter_lib PUBLIC log_headers)
target_include_directories(packet_filter_lib PUBLIC . PRIVATE ../../)
