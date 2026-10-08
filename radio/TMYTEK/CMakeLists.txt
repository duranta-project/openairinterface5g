# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_library(tmytek_spi_config INTERFACE)
target_include_directories(tmytek_spi_config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_boolean_option(OAI_TMYTEK_TLKCORE OFF "Build the Ethernet-only TMYTEK TLKCore controller" OFF)
if(OAI_TMYTEK_TLKCORE)
  add_subdirectory(tlkcore)
endif()

if (ENABLE_TESTS)
  add_subdirectory(tests)
endif()
