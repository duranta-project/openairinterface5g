/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/**********************************************************************
*
* FILENAME    :  pss_nr.h
*
* MODULE      :  primary synchronisation signal
*
* DESCRIPTION :  elements related to pss
*
************************************************************************/

#ifndef PSS_NR_H
#define PSS_NR_H

#include "PHY/defs_nr_common.h"
#include "PHY/NR_REFSIG/ss_pbch_nr.h"
#include "PHY/defs_nr_UE.h"

/* PSS configuration */
#define PSS_STEP 4 // number of samples per correlation step
#define PSS_THRESHOLD 5 // ratio signal versus signal multiply by PSS sequence

typedef struct {
  c16_t **rxdata;
  int nb_antennas_rx;
  int rxdata_length;
  int ofdm_symbol_size;
  int nb_prefix_samples;
  int subcarrier_spacing;
  bool fo_flag;
  bool sidelink; // search the sidelink PSS (2 sequences) instead of the downlink one (3)
  int target_Nid_cell; // -1 for a blind search, else the PCI (downlink) or SLSS id (sidelink)
  c16_t *pssTime;
} pss_search_t;

typedef struct {
  pss_detection_result_t pss_elem_info[NUMBER_PSS_SEQUENCE];
} nr_pss_info_t;

nr_pss_info_t pss_search_time_nr(const pss_search_t *p);

void generate_pss_nr_time(int ofdm_symbol_size,
                          int first_carrier_offset,
                          const int N_ID_2,
                          int ssbFirstSCS,
                          bool sidelink,
                          c16_t pssTime[ofdm_symbol_size]);
void generate_pss_nr(const int N_ID_2, bool sidelink, int16_t *pss);
/* Number of N_ID_2 hypotheses to search: 3 in the downlink, 2 in the sidelink. */
static inline int nr_num_pss_sequences(bool sidelink)
{
  return sidelink ? NUMBER_PSS_SEQUENCE_SL : NUMBER_PSS_SEQUENCE;
}
#endif /* PSS_NR_H */


