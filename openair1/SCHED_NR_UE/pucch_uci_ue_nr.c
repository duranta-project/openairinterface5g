/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/************************************************************************
*
* MODULE      :  PUCCH Packed Uplink Control Channel for UE NR
*                PUCCH is used to transmit Uplink Control Information UCI
*                which is composed of:
*                - SR Scheduling Request
*                - HARQ ACK/NACK
*                - CSI Channel State Information
*                UCI can also be transmitted on a PUSCH if it schedules.
*
* DESCRIPTION :  functions related to PUCCH UCI management
*                TS 38.213 9  UE procedure for reporting control information
*
**************************************************************************/

#include "executables/softmodem-common.h"
#include "PHY/NR_REFSIG/ss_pbch_nr.h"
#include "PHY/defs_nr_UE.h"
#include <openair1/SCHED/sched_common.h>
#include <openair1/PHY/NR_UE_TRANSPORT/pucch_nr.h>
#include "openair1/PHY/NR_UE_ESTIMATION/nr_estimation.h"
#include <openair1/PHY/impl_defs_nr.h>
#include <common/utils/nr/nr_common.h>
#include "SCHED_NR_UE/defs.h"
#include "SCHED_NR_UE/harq_nr.h"

#include "SCHED_NR_UE/pucch_uci_ue_nr.h"

long
binary_search_float_nr(
  float elements[],
  long numElem,
  float value
)
//-----------------------------------------------------------------------------
{
  long first, last, middle;
  first = 0;
  last = numElem-1;
  middle = (first+last)/2;

  if(value < elements[0]) {
    return first;
  }

  if(value >= elements[last]) {
    return last;
  }

  while (last - first > 1) {
    if (elements[middle] > value) {
      last = middle;
    } else {
      first = middle;
    }

    middle = (first+last)/2;
  }

  if (first < 0 || first >= numElem) {
    LOG_E(RRC,"\n Error in binary search float!");
  }

  return first;
}

/*******************************************************************
*
* NAME :         pucch_procedures_ue_nr
*
* PARAMETERS :   ue context
*                processing slots of reception/transmission
*                gNB_id identifier
*
* RETURN :       bool TRUE  PUCCH will be transmitted
*                     FALSE No PUCCH to transmit
*
* DESCRIPTION :  determines UCI (uplink Control Information) payload
*                and PUCCH format and its parameters.
*                PUCCH is no transmitted if:
*                - there is no valid data to transmit
*                - Pucch parameters are not valid
*
* Below information is scanned in order to know what information should be transmitted to network.
*
* (SR Scheduling Request)   (HARQ ACK/NACK)    (CSI Channel State Information)
*          |                        |               - CQI Channel Quality Indicator
*          |                        |                - RI  Rank Indicator
*          |                        |                - PMI Primary Matrux Indicator
*          |                        |                - LI Layer Indicator
*          |                        |                - L1-RSRP
*          |                        |                - CSI-RS resource idicator
*          |                        V                    |
*          +-------------------- -> + <------------------
*                                   |
*                   +--------------------------------+
*                   | UCI Uplink Control Information |
*                   +--------------------------------+
*                                   V                                            PUCCH Configuration
*               +----------------------------------------+                   +--------------------------+
*               | Determine PUCCH  payload and its       |                   |     PUCCH Resource Set   |
*               +----------------------------------------+                   |     PUCCH Resource       |
*                                   V                                        |     Format parameters    |
*               +-----------------------------------------+                  |                          |
*               | Select PUCCH format with its parameters | <----------------+--------------------------+
*               +-----------------------------------------+
*                                   V
*                          +-----------------+
*                          |  Generate PUCCH |
*                          +-----------------+
*
* TS 38.213 9  UE procedure for reporting control information
*
*********************************************************************/
void pucch_procedures_ue_nr(PHY_VARS_NR_UE *ue,
                            const UE_nr_rxtx_proc_t *proc,
                            nr_phy_data_tx_t *phy_data,
                            c16_t **txdataF,
                            bool was_symbol_used[NR_SYMBOLS_PER_SLOT])
{
  const int nr_slot_tx = proc->nr_slot_tx;
  NR_UE_PUCCH *pucch_vars = &phy_data->pucch_vars;

  for (int i=0; i<2; i++) {
    if(pucch_vars->active[i]) {
      const fapi_nr_ul_config_pucch_pdu *pucch_pdu = &pucch_vars->pucch_pdu[i];
      for (int symb_idx = 0; symb_idx < pucch_pdu->nr_of_symbols; symb_idx++) {
        int symb = pucch_pdu->start_symbol_index + symb_idx;
        was_symbol_used[symb] = true;
      }
      uint16_t nb_of_prbs = pucch_pdu->prb_size;
      /* Generate PUCCH signal according to its format and parameters */

      int16_t pucch_tx_power = pucch_pdu->pucch_tx_power;

      if (pucch_tx_power > ue->tx_power_max_dBm)
        pucch_tx_power = ue->tx_power_max_dBm;

      /* set tx power */
      ue->tx_power_dBm[nr_slot_tx] = pucch_tx_power;
      ue->tx_total_RE[nr_slot_tx] = nb_of_prbs * NR_NB_SC_PER_RB;

      int tx_amp;

      /*
      tx_amp = nr_get_tx_amp(pucch_tx_power,
                             ue->tx_power_max_dBm,
                             ue->frame_parms.N_RB_UL,
                             nb_of_prbs);
      if (tx_amp == 0)*/
      // FIXME temporarly using fixed amplitude before pucch power control implementation revised
      tx_amp = AMP;


      LOG_D(PHY,"Generation of PUCCH format %d at frame.slot %d.%d\n",pucch_pdu->format_type,proc->frame_tx,nr_slot_tx);

      switch(pucch_pdu->format_type) {
        case 0:
          nr_generate_pucch0(txdataF, &ue->frame_parms, tx_amp, nr_slot_tx, pucch_pdu);
          break;
        case 1:
          nr_generate_pucch1(txdataF, &ue->frame_parms, tx_amp, nr_slot_tx, pucch_pdu);
          break;
        case 2:
          nr_generate_pucch2(txdataF, &ue->frame_parms, tx_amp, nr_slot_tx, pucch_pdu);
          break;
        case 3:
        case 4:
          nr_generate_pucch3_4(txdataF, &ue->frame_parms, tx_amp, nr_slot_tx, pucch_pdu);
          break;
      }
    }
    pucch_vars->active[i] = false;
  }
}
