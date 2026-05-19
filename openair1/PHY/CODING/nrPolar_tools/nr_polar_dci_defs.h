/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief Defines the constant variables for polar coding of the DCI from 38-212, V15.1.1 2018-04.
 */

#ifndef __NR_POLAR_DCI_DEFS__H__
#define __NR_POLAR_DCI_DEFS__H__

#define NR_POLAR_DCI_MESSAGE_TYPE 1 // int8_t
#define NR_POLAR_SCI_MESSAGE_TYPE (NR_POLAR_PSBCH_MESSAGE_TYPE + 1) //int8_t
#define NR_POLAR_SCI2_MESSAGE_TYPE (NR_POLAR_SCI_MESSAGE_TYPE + 1) //int8_t
#define NR_POLAR_DCI_CRC_PARITY_BITS 24
#define NR_POLAR_DCI_CRC_ERROR_CORRECTION_BITS 3

// Sec. 7.3.3: Channel Coding
#define NR_POLAR_DCI_N_MAX 9 // uint8_t
#define NR_POLAR_DCI_I_IL 1 // uint8_t
#define NR_POLAR_DCI_I_SEG 0 // uint8_t
#define NR_POLAR_DCI_N_PC 0 // uint8_t
#define NR_POLAR_DCI_N_PC_WM 0 // uint8_t

// Sec. 7.3.4: Rate Matching
#define NR_POLAR_DCI_I_BIL 0 // uint8_t

#endif
