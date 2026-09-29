/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*! 
 * \brief Defines the constant variables for polar coding of the UCI from 38-212, V15.1.1 2018-04.
 */

#ifndef __NR_POLAR_UCI_DEFS__H__
#define __NR_POLAR_UCI_DEFS__H__

#define NR_POLAR_UCI_PUCCH_MESSAGE_TYPE 2 // int8_t
#define NR_POLAR_UCI_PUSCH_MESSAGE_TYPE 4
#define NR_POLAR_PUCCH_CRC_ERROR_CORRECTION_BITS 3

// Ref. 38-212, Section 6.3.1.3.1
#define NR_POLAR_UCI_N_MAX 10
#define NR_POLAR_UCI_I_IL 0

// Ref. 38-212, Section 6.3.1.4.1
#define NR_POLAR_UCI_I_BIL 1

#endif
