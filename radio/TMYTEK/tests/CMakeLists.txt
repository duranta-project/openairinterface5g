# SPDX-License-Identifier: LicenseRef-CSSL-1.0

add_executable(test_tmytek_spi_config test_tmytek_spi_config.cpp)
target_link_libraries(test_tmytek_spi_config PRIVATE GTest::gtest tmytek_spi_config)
add_dependencies(tests test_tmytek_spi_config)
add_test(NAME test_tmytek_spi_config
  COMMAND ./test_tmytek_spi_config)
