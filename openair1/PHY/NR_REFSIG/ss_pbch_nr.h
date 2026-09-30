/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**********************************************************************
*
* FILENAME    :  ss_pbch_nr.h
*
* MODULE      : this file contains define only
*
* DESCRIPTION : define elements related to SS/PBCH block ie synchronisation (pss/sss) and pbch
*
*               see TS 38.211  7.4.2 Synchronisation Signals
*               see TS 38.213  4 Synchronisation procedures
*
************************************************************************/

#ifndef SS_PBCH_NR_H
#define SS_PBCH_NR_H

#include <stdbool.h>

#include "PHY/defs_nr_sl_UE.h"

/* PSS parameters */
#define  NUMBER_PSS_SEQUENCE          (3)
#define  NUMBER_PSS_SEQUENCE_SL       (2)
#define  PSS_SSS_SUB_CARRIER_START    (56)
#define PSS_SSS_SUB_CARRIER_START_SL (2)
#define LENGTH_PSS_NR (127)
// We scale down the reference PSS signal in freq to not saturate inside the idft when we generate the corresponding time domain
// signal
#define SCALING_PSS_NR (12)

/* define ofdm symbol offset in the SS/PBCH block of NR synchronisation */
#ifdef NR_UNIT_TEST
#define OFFSET_SS_PBCH                (0)
#else
#define OFFSET_SS_PBCH                (4)
#endif

#define  PSS_SYMBOL_NB                ((0) + OFFSET_SS_PBCH)   /* symbol numbers for each element */
#define  PBCH_SYMBOL_NB               ((1) + OFFSET_SS_PBCH)
#define  SSS_SYMBOL_NB                ((2) + OFFSET_SS_PBCH)
#define  PBCH_LAST_SYMBOL_NB          ((3) + OFFSET_SS_PBCH)

/* symbol numbers inside the sidelink SS/PSBCH block, see TS 38.211 8.4.3.1
   symbol 0 and symbols 5 to 12 carry PSBCH */
#define PSS0_SL_SYMBOL_NB (1)
#define PSS1_SL_SYMBOL_NB (2)
#define SSS0_SL_SYMBOL_NB (3)
#define SSS1_SL_SYMBOL_NB (4)

/* Mapping between the cell identity and the two sequence indices carried by PSS (N_ID_2)
   and SSS (N_ID_1). Downlink: N_cell_ID = 3 * N_ID_1 + N_ID_2 (TS 38.211 7.4.2.1).
   Sidelink: NID_SL = N_ID_1 + 336 * N_ID_2 (TS 38.211 8.4.2.1). */
static inline int nr_cell_id(bool sidelink, int nid1, int nid2)
{
  return sidelink ? nid1 + SL_NR_NUM_IDs_IN_SSS * nid2 : nid2 + NUMBER_PSS_SEQUENCE * nid1;
}

static inline int nr_cell_id_to_nid1(bool sidelink, int cell_id)
{
  return sidelink ? cell_id % SL_NR_NUM_IDs_IN_SSS : cell_id / NUMBER_PSS_SEQUENCE;
}

static inline int nr_cell_id_to_nid2(bool sidelink, int cell_id)
{
  return sidelink ? cell_id / SL_NR_NUM_IDs_IN_SSS : cell_id % NUMBER_PSS_SEQUENCE;
}

/* SS/PBCH parameters */
#define  N_RB_SS_PBCH_BLOCK           (20)
#define  NB_SYMBOLS_PBCH              (3)
/* number of symbols of an SS/PBCH block initial sync has to demodulate: PSS, PBCH, SSS,
   PBCH for the downlink, the whole SS/PSBCH block for the sidelink */
#define  NR_N_SYMBOLS_SSB             (4)
#define SL_N_SYMBOLS_SSB SL_NR_NUM_SYMBOLS_SSB_NORMAL_CP

#define IQ_SIZE sizeof(c16_t) /* I and Q are alternatively stored into buffers */

/* SS/PBCH parameters :  see from TS 38.211 table 7.4.3.1-1: Resources within an SS/PBCH block for PSS... */
#define DMRS_PBCH_PER_RB (NR_NB_SC_PER_RB >> 4) /* at 0+v, 4+v, 8+v for a resource block with v = NcellID modulo 4 */
#define  DMRS_END_FIRST_PART          (44)
#define  DMRS_START_SECOND_PART       (192)
#define  DMRS_END_SECOND_PART         (236)
#define  DMRS_PBCH_NUMBER             (NB_SYMBOLS_PBCH*(N_RB_SS_PBCH_BLOCK * DMRS_PBCH_PER_RB))    /* there are both PBCH and SSS/(Set to 0) at the second OFDM symbol of SS/PBCH so size is increased */

/* see TS 38211 7.4.1.4 Demodulation reference signals for PBCH */
#define  DMRS_PBCH_I_SSB              (8)         /* maximum index value for SSB/PBCH which can have alength of L=4 or L=8 */
#define  DMRS_PBCH_N_HF               (2)         /* half frame indication - 0 for first part of frame and 1 for second part of frame */

#endif /* SS_PBCH_NR_H */


