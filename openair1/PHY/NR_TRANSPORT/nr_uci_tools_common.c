/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_dci.h"

void nr_group_sequence_hopping(pucch_GroupHopping_t PUCCH_GroupHopping,
                               uint32_t n_id,
                               int n_hop,
                               int nr_slot_tx,
                               uint8_t *u,
                               uint8_t *v)
{
  /*
   * Implements TS 38.211 subclause 6.3.2.2.1 Group and sequence hopping
   * The following variables are set by higher layers:
   *    - PUCCH_GroupHopping:
   *    - n_id: higher-layer parameter hoppingId
   *    - n_hop: frequency hopping index
   *             if intra-slot frequency hopping is disabled by the higher-layer parameter PUCCH-frequency-hopping
   *                n_hop=0
   *             if frequency hopping is enabled by the higher-layer parameter PUCCH-frequency-hopping
   *                n_hop=0 for the first hop
   *                n_hop=1 for the second hop
   */
  // depending on the value of the PUCCH_GroupHopping, we will obtain different values for u,v
  // n_id defined as per TS 38.211 subclause 6.3.2.2.1 (is given by the higher-layer parameter hoppingId)
  // it is hoppingId from PUCCH-ConfigCommon:
  // Cell-Specific scrambling ID for group hoppping and sequence hopping if enabled
  // Corresponds to L1 parameter 'HoppingID' (see 38.211, section 6.3.2.2) BIT STRING (SIZE (10))
#ifdef DEBUG_NR_PUCCH_TX
  printf("\t\t [nr_group_sequence_hopping] PUCCH_GroupHopping=%u, n_id=%u \n",PUCCH_GroupHopping,n_id);
#endif
  uint8_t f_ss=0,f_gh=0;
  *u=0;
  *v = 0;
#ifdef DEBUG_NR_PUCCH_TX
  printf("\t\t [nr_group_sequence_hopping] calculating u,v -> ");
#endif

  if (PUCCH_GroupHopping == neither) { // PUCCH_GroupHopping 'neither'
    f_ss = n_id%30;
  }

  if (PUCCH_GroupHopping == enable) { // PUCCH_GroupHopping 'enabled'
    uint32_t c_init = floor(n_id / 30); // we initialize c_init to calculate u,v according to 6.3.2.2.1 of 38.211
    int l = 32, minShift = (2 * nr_slot_tx + n_hop) << 3;
    uint32_t *seq = gold_cache(c_init, (minShift + 31) / 32 + 8); // TS 38.211 Subclause 5.2.1
    int goldIdx = 0;
    for (int m=0; m<8; m++) {
      while(minShift >= l) {
        goldIdx++;
        l = l+32;
      }
      AssertFatal(goldIdx < 8 + (minShift + 31) / 32, "impossible: %d >= %d", goldIdx, 8 + (minShift + 31) / 32);
      int tmpShift = (minShift & ((1 << 5) - 1)); // minShift%32;
      f_gh = f_gh + ((1 << m) * ((uint8_t)((seq[goldIdx] >> tmpShift) & 1)));
      minShift ++;
    }

    f_gh = f_gh%30;
    f_ss = n_id % 30;
  }

  if (PUCCH_GroupHopping == disable) { // PUCCH_GroupHopping 'disabled'
    uint32_t c_init = (1 << 5) * floor(n_id / 30) + (n_id % 30); // we initialize c_init to calculate u,v
    int goldIdx = 0;
    f_ss = n_id%30;
    int l = 32, minShift = (2 * nr_slot_tx + n_hop);

    while(minShift >= l) {
      goldIdx++;
      l = l+32;
    }
    // TS 38.211 Subclause 5.2.1
    uint32_t *seq = gold_cache(c_init, goldIdx + 1);
    int tmpShift = (minShift & ((1 << 5) - 1)); // minShift%32;
    *v = (uint8_t)((seq[goldIdx] >> tmpShift) & 1);
    //    *v = (uint8_t)((s>>(2*nr_slot_tx+n_hop))&1); // Not sure we have to use nr_slot_tx FIXME!!!
  }

  *u = (f_gh+f_ss)%30;
#ifdef DEBUG_NR_PUCCH_TX
  printf("%d,%d\n",*u,*v);
#endif
}

double nr_cyclic_shift_hopping(uint32_t n_id,
                               uint8_t m0,
                               uint8_t mcs,
                               uint8_t lnormal,
                               uint8_t lprime,
                               int nr_slot_tx) {
  /*
   * Implements TS 38.211 subclause 6.3.2.2.2 Cyclic shift hopping
   *     - n_id: higher-layer parameter hoppingId
   *     - m0: provided by higher layer parameter PUCCH-F0-F1-initial-cyclic-shift of PUCCH-F0-resource-config
   *     - mcs: mcs=0 except for PUCCH format 0 when it depends on information to be transmitted according to TS 38.213 subclause 9.2
   *     - lnormal: lnormal is the OFDM symbol number in the PUCCH transmission where l=0 corresponds to the first OFDM symbol of the PUCCH transmission
   *     - lprime: lprime is the index of the OFDM symbol in the slot that corresponds to the first OFDM symbol of the PUCCH transmission in the slot given by [5, TS 38.213]
   */
  // alpha_init initialized to 2*PI/12=0.5235987756
  double alpha = 0.5235987756;
  uint32_t c_init = n_id; // we initialize c_init again to calculate n_cs

  uint8_t n_cs=0;
  int l = 32, minShift = (14 * 8 * nr_slot_tx) + 8 * (lnormal + lprime);
#ifdef DEBUG_NR_PUCCH_TX
  printf("\t\t [nr_cyclic_shift_hopping] calculating alpha (cyclic shift) using c_init=%u -> \n",c_init);
#endif
  uint32_t *seq = gold_cache(c_init, 8 + (minShift + 31) / 32); // TS 38.211 Subclause 5.2.1
  int goldIdx = 0;
  for (int m=0; m<8; m++) {
    while(minShift >= l) {
      goldIdx++;
      l = l+32;
    }
    AssertFatal(goldIdx < 8 + (minShift + 31) / 32, "impossible: %d >= %d", goldIdx, 8 + (minShift + 31) / 32);
    int tmpShift = (minShift & ((1 << 5) - 1)); // minShift%32;
    minShift ++;
    n_cs += (1 << m) * (uint8_t)((seq[goldIdx] >> tmpShift) & 1);
    // calculating n_cs (Not sure we have to use nr_slot_tx FIXME!!!)
    // n_cs = n_cs+((1<<m)*((uint8_t)((s>>((14*8*nr_slot_tx) + 8*(lnormal+lprime) + m))&1)));
  }

  alpha = alpha * (double)((m0 + mcs + n_cs) % 12);
#ifdef DEBUG_NR_PUCCH_TX
  printf("n_cs=%d -> %lf\n",n_cs,alpha);
#endif
  return(alpha);
}
