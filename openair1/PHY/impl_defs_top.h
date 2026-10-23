/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief More defines and structure definitions
 */

#ifndef __PHY_IMPLEMENTATION_DEFS_H__
#define __PHY_IMPLEMENTATION_DEFS_H__

/** @defgroup _ref_implementation_ OpenAirInterface LTE Implementation
 * @{

 * @defgroup _PHY_RF_INTERFACE_ PHY - RF Interface
 * @{
 * @ingroup _PHY_RF_INTERFACE_
 * @{
 * @defgroup _GENERIC_PHY_RF_INTERFACE_ Generic PHY - RF Interface
 * @defgroup _USRP_PHY_RF_INTERFACE_    PHY - USRP RF Interface
 * @defgroup _BLADERF_PHY_RF_INTERFACE_    PHY - BLADERF RF Interface
 * @defgroup _LMSSDR_PHY_RF_INTERFACE_    PHY - LMSSDR RF Interface
 * @}
 * @}
 * @ingroup _ref_implementation_
 * @{
 * This module is responsible for defining the generic interface between PHY and RF Target
 * @}

 * @defgroup _openair1_ openair1 Reference Implementation
 * @ingroup _ref_implementation_
 * @{


 * @defgroup _physical_layer_ref_implementation_ Physical Layer Reference Implementation
 * @ingroup _openair1_
 * @{


 * @defgroup _PHY_STRUCTURES_ Basic Structures and Memory Initialization
 * @ingroup _physical_layer_ref_implementation_
 * @{
 * This module is responsible for defining and initializing the PHY variables during static configuration of OpenAirInterface.
 * @}

 * @defgroup _PHY_DSP_TOOLS_ DSP Tools
 * @ingroup _physical_layer_ref_implementation_
 * @{
 * This module is responsible for basic signal processing related to inner-MODEM processing.
 * @}

 * @defgroup _PHY_MODULATION_ Modulation and Demodulation
 * @ingroup _physical_layer_ref_implementation_
 * @{
 * This module is responsible for procedures related to OFDMA modulation and demodulation.
 * @}

 * @defgroup _PHY_PARAMETER_ESTIMATION_BLOCKS_ Parameter Estimation
 * @ingroup _physical_layer_ref_implementation_
 * @{
 * This module is responsible for procedures related to OFDMA frequency-domain channel estimation for LTE Downlink Channels.
 * @}

 * @defgroup _PHY_CODING_BLOCKS_ Channel Coding/Decoding Functions
 * @ingroup _physical_layer_ref_implementation_
 * @{
 * This module is responsible for procedures related to channel coding/decoding, rate-matching, segementation and interleaving.
 * @}

 * @defgroup _PHY_TRANSPORT_ Transport/Physical Channel Processing
 * @ingroup _physical_layer_ref_implementation_
 * @{
 * This module is responsible for defining and processing the PHY procedures (TX/RX) related to transport and physical channels.
 * @}

 * @defgroup _PHY_PROCEDURES_ Physical Layer Procedures
 * @ingroup _physical_layer_ref_implementation_
 * @{
 * This module is responsible for defining and processing the PHY procedures (TX/RX) related to transport and physical channels.
 * @}

 * @}
 * @}
 * @}
 */

#include <common/utils/nr/nr_common.h>
#include <common/utils/utils.h>
#include "types.h"

/** @addtogroup _PHY_STRUCTURES_
 * @{
*/
#define NUMBER_OF_OFDM_CARRIERS (frame_parms->ofdm_symbol_size)
#define NUMBER_OF_SYMBOLS_PER_FRAME (frame_parms->symbols_per_tti*LTE_NUMBER_OF_SUBFRAMES_PER_FRAME)
#define NUMBER_OF_USEFUL_CARRIERS (12*frame_parms->N_RB_DL)
#define NUMBER_OF_ZERO_CARRIERS (NUMBER_OF_OFDM_CARRIERS-NUMBER_OF_USEFUL_CARRIERS)
#define NUMBER_OF_USEFUL_CARRIERS_BYTES (NUMBER_OF_USEFUL_CARRIERS>>2)
#define HALF_NUMBER_OF_USEFUL_CARRIERS (NUMBER_OF_USEFUL_CARRIERS>>1)
#define HALF_NUMBER_OF_USEFUL_CARRIERS_BYTES (HALF_NUMBER_OF_USEFUL_CARRIERS>>2)
#define FIRST_CARRIER_OFFSET (HALF_NUMBER_OF_USEFUL_CARRIERS+NUMBER_OF_ZERO_CARRIERS)
#define NUMBER_OF_OFDM_SYMBOLS_PER_SLOT (NUMBER_OF_SYMBOLS_PER_FRAME/LTE_SLOTS_PER_FRAME)

#define EXTENSION_TYPE (PHY_config->PHY_framing.Extension_type)

#define CYCLIC_PREFIX_LENGTH (frame_parms->nb_prefix_samples)
#define CYCLIC_PREFIX_LENGTH_SAMPLES (CYCLIC_PREFIX_LENGTH*2)
#define CYCLIC_PREFIX_LENGTH_BYTES (CYCLIC_PREFIX_LENGTH*4)
#define CYCLIC_PREFIX_LENGTH0 (frame_parms->nb_prefix_samples0)
#define CYCLIC_PREFIX_LENGTH_SAMPLES0 (CYCLIC_PREFIX_LENGTH0*2)
#define CYCLIC_PREFIX_LENGTH_BYTES0 (CYCLIC_PREFIX_LENGTH0*4)

#define OFDM_SYMBOL_SIZE_SAMPLES ((NUMBER_OF_OFDM_CARRIERS + CYCLIC_PREFIX_LENGTH)*2)   // 16-bit units (i.e. real samples)
#define OFDM_SYMBOL_SIZE_SAMPLES0 ((NUMBER_OF_OFDM_CARRIERS + CYCLIC_PREFIX_LENGTH0)*2)   // 16-bit units (i.e. real samples)
#define OFDM_SYMBOL_SIZE_SAMPLES_MAX 4096   // 16-bit units (i.e. real samples)
#define OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES (OFDM_SYMBOL_SIZE_SAMPLES/2)                   // 32-bit units (i.e. complex samples)
#define OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES0 (OFDM_SYMBOL_SIZE_SAMPLES0/2)                   // 32-bit units (i.e. complex samples)
#define OFDM_SYMBOL_SIZE_SAMPLES_NO_PREFIX ((NUMBER_OF_OFDM_CARRIERS)*2)
#define OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES_NO_PREFIX (OFDM_SYMBOL_SIZE_SAMPLES_NO_PREFIX/2)
#define OFDM_SYMBOL_SIZE_BYTES (OFDM_SYMBOL_SIZE_SAMPLES*2)
#define OFDM_SYMBOL_SIZE_BYTES0 (OFDM_SYMBOL_SIZE_SAMPLES0*2)
#define OFDM_SYMBOL_SIZE_BYTES_NO_PREFIX (OFDM_SYMBOL_SIZE_SAMPLES_NO_PREFIX*2)

#define SLOT_LENGTH_BYTES (frame_parms->samples_per_slot) // 4 bytes * samples_per_tti/2
#define SLOT_LENGTH_BYTES_NO_PREFIX (OFDM_SYMBOL_SIZE_BYTES_NO_PREFIX * NUMBER_OF_OFDM_SYMBOLS_PER_SLOT)

#define FRAME_LENGTH_COMPLEX_SAMPLES (frame_parms->samples_per_subframe*LTE_NUMBER_OF_SUBFRAMES_PER_FRAME)
#define FRAME_LENGTH_SAMPLES (FRAME_LENGTH_COMPLEX_SAMPLES*2)
#define FRAME_LENGTH_SAMPLES_NO_PREFIX (NUMBER_OF_SYMBOLS_PER_FRAME*OFDM_SYMBOL_SIZE_SAMPLES_NO_PREFIX)
#define FRAME_LENGTH_COMPLEX_SAMPLES_NO_PREFIX (FRAME_LENGTH_SAMPLES_NO_PREFIX/2)

#define NUMBER_OF_CARRIERS_PER_GROUP (NUMBER_OF_USEFUL_CARRIERS/NUMBER_OF_FREQUENCY_GROUPS)

#define RX_PRECISION (16)
#define LOG2_RX_PRECISION (4)
#define RX_OUTPUT_SHIFT (4)


#define SAMPLE_SIZE_BYTES    2                                           // 2 bytes/real sample

#define FRAME_LENGTH_BYTES   (FRAME_LENGTH_SAMPLES * SAMPLE_SIZE_BYTES)  // frame size in bytes
#define FRAME_LENGTH_BYTES_NO_PREFIX   (FRAME_LENGTH_SAMPLES_NO_PREFIX * SAMPLE_SIZE_BYTES)  // frame size in bytes


#define FFT_SCALE_FACTOR     8                                           // Internal Scaling for FFT
#define DMA_BLKS_PER_SLOT    (SLOT_LENGTH_BYTES/2048)                    // Number of DMA blocks per slot
#define SLOT_TIME_NS         (SLOT_LENGTH_SAMPLES*(1e3)/7.68)            // slot time in ns

#define NB_ANTENNA_PORTS_ENB  6                                         // total number of eNB antenna ports

#define TARGET_RX_POWER 50    // Target digital power for the AGC
#define TARGET_RX_POWER_MAX 65    // Maximum digital power, such that signal does not saturate (value found by simulation)
#define TARGET_RX_POWER_MIN 35    // Minimum digital power, anything below will be discarded (value found by simulation)

// Increase USRP rx gain in steps of 3dB during Initial search
#define INCREASE_IN_RXGAIN 3

//the min and max gains have to match the calibrated gain table
//#define MAX_RF_GAIN 160
//#define MIN_RF_GAIN 96
#define MAX_RF_GAIN 200
#define MIN_RF_GAIN 80

#define PHY_SYNCH_OFFSET ((OFDM_SYMBOL_SIZE_COMPLEX_SAMPLES)-1)  // OFFSET of BEACON SYNCH



#define ONE_OVER_SQRT2_Q15 23170
#define ONE_OVER_2_Q15 16384

// QAM amplitude definitions

/// Amplitude for QPSK (\f$ 2^15 \times 1/\sqrt{2}\f$)
#define QPSK 23170

/// First Amplitude for QAM16 (\f$ 2^{15} \times 2/\sqrt{10}\f$)
#define QAM16_n1 20724

/// Second Amplitude for QAM16 (\f$ 2^{15} \times 1/\sqrt{10}\f$)
#define QAM16_n2 10362

///First Amplitude for QAM64 (\f$ 2^{15} \times 4/\sqrt{42}\f$)
#define QAM64_n1 20225
///Second Amplitude for QAM64 (\f$ 2^{15} \times 2/\sqrt{42}\f$)
#define QAM64_n2 10112
///Third Amplitude for QAM64 (\f$ 2^{15} \times 1/\sqrt{42}\f$)
#define QAM64_n3 5056

///First Amplitude for QAM256 (\f$ 2^{15} \times 8/\sqrt{170}\f$)
#define QAM256_n1 20106
///Second Amplitude for QAM256 (\f$ 2^{15} \times 4/\sqrt{170}\f$)
#define QAM256_n2 10053
///Third Amplitude for QAM256 (\f$ 2^{15} \times 2/\sqrt{170}\f$)
#define QAM256_n3 5026

/// First Amplitude for QAM1024 (\f$ 2^{15} \times 16/\sqrt{682}\f$)
#define QAM1024_n1 20076
/// Second Amplitude for QAM1024 (\f$ 2^{15} \times 8/\sqrt{682}\f$)
#define QAM1024_n2 10038
/// Third Amplitude for QAM1024 (\f$ 2^{15} \times 4/\sqrt{682}\f$)
#define QAM1024_n3 5019

#ifdef BIT8_TX
#define AMP_SHIFT 7
#else
#define AMP_SHIFT 9
#endif

#define AMP ((1)<<AMP_SHIFT)

#define AMP_OVER_SQRT2 ((AMP*ONE_OVER_SQRT2_Q15)>>15)
#define AMP_OVER_2 (AMP>>1)

/// Measurement Variables

//#define NUMBER_OF_SUBBANDS_MAX 13
#define NUMBER_OF_HARQ_PID_MAX 8

#define MAX_FRAME_NUMBER 0x400
#include "common/openairinterface5g_limits.h"
#include "assertions.h"

#endif //__PHY_IMPLEMENTATION_DEFS_H__
/**@}
*/
