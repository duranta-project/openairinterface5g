/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! \file tmytek_spi_config.h
 * \brief SPI register protocol of the TMYTEK BBox beamformer, independent of the radio driving it.
 *
 * Beam steering at runtime only writes the phase register of the active RF mode;
 * the gain registers are programmed once when the array is initialised.
 */

#ifndef TMYTEK_SPI_CONFIG_H
#define TMYTEK_SPI_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*! \brief RF mode of the beamformer, selects the register bank to write */
typedef enum {
  TMYTEK_MODE_TX = 0,
  TMYTEK_MODE_RX = 1,
} tmytek_rf_mode_t;

/*! \brief GPIO pin assignment of the beamformer control interface.
 * SDI/SDO are named from the point of view of the USRP. */
typedef struct {
  uint8_t clk;
  //! data from the beamformer, periph_sdi of the UHD SPI peripheral
  uint8_t sdi;
  //! data to the beamformer, periph_sdo of the UHD SPI peripheral
  uint8_t sdo;
  uint8_t cs;
  //! IC_SDI in the TMYTEK pinout, held low
  uint8_t gpio_sdi;
  //! pulsed low to apply a register write, LDB in the TMYTEK pinout
  uint8_t ldb;
  uint8_t tx_en;
  uint8_t rx_en;
} tmytek_pin_map_t;

//! SPI transfer length of a register write
#define TMYTEK_SPI_PAYLOAD_BITS 13
//! SPI clock divider the beamformer is specified for
#define TMYTEK_SPI_CLK_DIVIDER 4

#define TMYTEK_BEAM_ID_MIN 1
#define TMYTEK_BEAM_ID_MAX 64

//! phase register, the beam index is written here
#define TMYTEK_REG_PHASE_TX 0x78
#define TMYTEK_REG_PHASE_RX 0x70

/*! \brief Pin assignment defined by TMYTEK for the USRP X410 GPIO connector */
static inline tmytek_pin_map_t tmytek_default_pin_map(void)
{
  tmytek_pin_map_t p;
  p.clk = 3;
  p.sdi = 7;
  p.sdo = 4;
  p.cs = 0;
  p.gpio_sdi = 5;
  p.ldb = 6;
  p.tx_en = 2;
  p.rx_en = 9;
  return p;
}

static inline bool tmytek_beam_id_valid(int beam_id)
{
  return beam_id >= TMYTEK_BEAM_ID_MIN && beam_id <= TMYTEK_BEAM_ID_MAX;
}

/*! \brief Build the SPI payload selecting a beam from the codebook
 * \param mode current RF mode
 * \param beam_id beam index, 1-based as defined by TMYTEK
 * \return payload of TMYTEK_SPI_PAYLOAD_BITS bits
 */
static inline uint32_t tmytek_beam_payload(tmytek_rf_mode_t mode, int beam_id)
{
  const uint32_t phase_reg = mode == TMYTEK_MODE_TX ? TMYTEK_REG_PHASE_TX : TMYTEK_REG_PHASE_RX;
  return (phase_reg << 6) | ((uint32_t)(beam_id - 1) & 0x3f);
}

#ifdef __cplusplus
}
#endif

#endif /* TMYTEK_SPI_CONFIG_H */
