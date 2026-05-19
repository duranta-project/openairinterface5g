/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "openair1/PHY/gold.h"
#include "sl_refsig_defs.h"
#include "openair1/PHY/LTE_TRANSPORT/transport_proto.h" 
#include "common/utils/LOG/log.h"

void nr_init_pscch_dmrs(NR_DL_FRAME_PARMS *fp, uint32_t ***nr_gold, uint16_t nid)
{
  unsigned int n = 0, x1 = 0, x2 = 0, x2tmp0 = 0;
  uint8_t reset;
  int pdcch_dmrs_init_length = (((fp->N_RB_UL << 1) * 3) >> 5) + 1;

  for (int ns = 0; ns < fp->slots_per_frame; ns++) {
    for (int l = 0; l < fp->symbols_per_slot; l++) {
      reset = 1;
      x2tmp0 = ((fp->symbols_per_slot * ns + l + 1) * ((nid << 1) + 1));
      x2tmp0 <<= 17;
      x2 = (x2tmp0 + (nid << 1)) % (1U << 31);  //cinit
      for (n=0; n<pdcch_dmrs_init_length; n++) {
        nr_gold[ns][l][n] = gold_generic(&x1, &x2, reset);
        reset = 0;
      }
    }
  }
}

void nr_init_pssch_dmrs_oneshot(NR_DL_FRAME_PARMS *fp,
                                uint16_t N_id,
                                uint32_t *pssch_dmrs,
                                int slot,
                                int symb)
{
  uint32_t x1 = 0, x2 = 0, n = 0;
  int pusch_dmrs_init_length = ((fp->N_RB_UL * 12) >> 5) + 1;

  int reset = 1;
  x2 = ((1U << 17) * (fp->symbols_per_slot*slot + symb + 1) * ((N_id << 1) + 1) + (N_id << 1));
  LOG_D(PHY,"PSSCH DMRS slot %d, symb %d x2 %x\n", slot, symb, x2);
  for (n=0; n<pusch_dmrs_init_length; n++) {
    pssch_dmrs[n] = gold_generic(&x1, &x2, reset);
    reset = 0;
  }
}

void sl_init_psbch_dmrs_gold_sequences(PHY_VARS_NR_UE *UE)
{
  unsigned int x1, x2;
  uint16_t slss_id;
  uint8_t reset;

  for (slss_id = 0; slss_id < SL_NR_NUM_SLSS_IDs; slss_id++) {
    reset = 1;
    x2 = slss_id;

#ifdef SL_DEBUG_INIT
    printf("\nPSBCH DMRS GOLD SEQ for SLSSID :%d  :\n", slss_id);
#endif

    for (uint8_t n = 0; n < SL_NR_NUM_PSBCH_DMRS_RE_DWORD; n++) {
      UE->SL_UE_PHY_PARAMS.init_params.psbch_dmrs_gold_sequences[slss_id][n] = gold_generic(&x1, &x2, reset);
      reset = 0;

#ifdef SL_DEBUG_INIT_DATA
      printf("%x\n", SL_UE_INIT_PARAMS.sl_psbch_dmrs_gold_sequences[slss_id][n]);
#endif
    }
  }
}
