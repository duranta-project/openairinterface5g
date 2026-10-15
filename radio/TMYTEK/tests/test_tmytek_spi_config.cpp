/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
// How to run the test:
// build the test: cmake --build build --target test_tmytek_spi_config
// run the test: ./build/radio/TMYTEK/tests/test_tmytek_spi_config
// This test does not require a USRP or TMYTEK hardware; 
// it tests the pure SPI payload and pin-map logic.

#include <gtest/gtest.h>
#include "tmytek_spi_config.h"

//! beam index field of a register write
#define BEAM_INDEX_MASK 0x3f

TEST(tmytek_spi_config, beam_index_is_zero_based_on_the_wire)
{
  for (int id = TMYTEK_BEAM_ID_MIN; id <= TMYTEK_BEAM_ID_MAX; id++) {
    EXPECT_EQ(tmytek_beam_payload(TMYTEK_MODE_TX, id) & BEAM_INDEX_MASK, (uint32_t)(id - 1));
    EXPECT_EQ(tmytek_beam_payload(TMYTEK_MODE_RX, id) & BEAM_INDEX_MASK, (uint32_t)(id - 1));
  }
}

TEST(tmytek_spi_config, tx_and_rx_address_different_registers)
{
  for (int id = TMYTEK_BEAM_ID_MIN; id <= TMYTEK_BEAM_ID_MAX; id++) {
    const uint32_t tx = tmytek_beam_payload(TMYTEK_MODE_TX, id);
    const uint32_t rx = tmytek_beam_payload(TMYTEK_MODE_RX, id);
    EXPECT_NE(tx >> 6, rx >> 6);
  }
}

TEST(tmytek_spi_config, whole_codebook_fits_the_index_field)
{
  EXPECT_GE(TMYTEK_BEAM_ID_MIN, 1);
  EXPECT_LE(TMYTEK_BEAM_ID_MAX - 1, BEAM_INDEX_MASK);
}

TEST(tmytek_spi_config, beam_payload_fits_the_spi_transfer)
{
  for (int id = TMYTEK_BEAM_ID_MIN; id <= TMYTEK_BEAM_ID_MAX; id++) {
    EXPECT_LT(tmytek_beam_payload(TMYTEK_MODE_TX, id), 1u << TMYTEK_SPI_PAYLOAD_BITS);
    EXPECT_LT(tmytek_beam_payload(TMYTEK_MODE_RX, id), 1u << TMYTEK_SPI_PAYLOAD_BITS);
  }
}

TEST(tmytek_spi_config, beam_id_range)
{
  EXPECT_FALSE(tmytek_beam_id_valid(TMYTEK_BEAM_ID_MIN - 1));
  EXPECT_TRUE(tmytek_beam_id_valid(TMYTEK_BEAM_ID_MIN));
  EXPECT_TRUE(tmytek_beam_id_valid(TMYTEK_BEAM_ID_MAX));
  EXPECT_FALSE(tmytek_beam_id_valid(TMYTEK_BEAM_ID_MAX + 1));
}

TEST(tmytek_spi_config, pins_are_distinct)
{
  const tmytek_pin_map_t p = tmytek_default_pin_map();
  const uint8_t pins[] = {p.clk, p.sdi, p.sdo, p.cs, p.gpio_sdi, p.ldb, p.tx_en, p.rx_en};
  uint32_t mask = 0;
  for (uint8_t pin : pins) {
    EXPECT_LT(pin, 12) << "pin outside the 12 bit GPIO bank";
    EXPECT_EQ(mask & (1u << pin), 0u) << "pin " << (int)pin << " assigned twice";
    mask |= 1u << pin;
  }
}

int main(int argc, char **argv)
{
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
