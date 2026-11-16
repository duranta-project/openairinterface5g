/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* TEMPORARY test adapter for MBS PTM transmissions, to be removed when the MBS control plane and user plane are
 * available. It stands in for:
 * - Multicast Context Setup (TS 38.473 §8.14.6): creates one MBS group, with the PTM RLC entity of its MRB;
 * - the setup of the MRB in the gNB-CU-UP: creates its PDCP entity;
 * - the UE join (Multicast MBS Session List in UE Context Setup/Modification): every UE of the cell with a PDU session
 *   joins the group;
 * - the shared N3 tunnel of the MBS session (TS 23.247 §7.2.1.4): the DL traffic of the PDU session
 *   shared_delivery_pdu_session of the UE shared_delivery_ue_id (gNB-CU UE ID) is delivered on the MRB, and/or
 *   numbered test SDUs are sent every sdu_interval_ms.
 * It only uses the MAC, RLC and PDCP MBS interfaces that the real procedures will use, and does nothing unless the
 * optional MBS_TEST section is present in the configuration file. */

#ifndef NR_MBS_TEST_ADAPTER_H
#define NR_MBS_TEST_ADAPTER_H

#include "nr_mac_gNB.h"

#define CONFIG_STRING_MBS_TEST "MBS_TEST"
#define CONFIG_STRING_MBS_TEST_DCI_FORMAT "dci_format"
#define CONFIG_STRING_MBS_TEST_LCID "lcid"
#define CONFIG_STRING_MBS_TEST_MRB_ID "mrb_id"
#define CONFIG_STRING_MBS_TEST_NUM_HARQ "num_ptm_harq"
#define CONFIG_STRING_MBS_TEST_HARQ_FEEDBACK "harq_feedback"
#define CONFIG_STRING_MBS_TEST_SDU_SIZE "sdu_size"
#define CONFIG_STRING_MBS_TEST_SDU_INTERVAL "sdu_interval_ms"
#define CONFIG_STRING_MBS_TEST_SHARED_DELIVERY_UE "shared_delivery_ue_id"
#define CONFIG_STRING_MBS_TEST_SHARED_DELIVERY_PDU_SESSION "shared_delivery_pdu_session"

// clang-format off
#define MBS_TEST_PARAMS_DESC { \
  {CONFIG_STRING_MBS_TEST_DCI_FORMAT, "PTM DCI format (41 or 42)\n", 0, .iptr = NULL, .defintval = 41, TYPE_INT, 0}, \
  {CONFIG_STRING_MBS_TEST_LCID,                        "PTM MTCH LCID\n",       0, .iptr = NULL, .defintval = 32,   TYPE_INT, 0}, \
  {CONFIG_STRING_MBS_TEST_MRB_ID,                      "MRB-Identity\n",        0, .iptr = NULL, .defintval = 1,    TYPE_INT, 0}, \
  {CONFIG_STRING_MBS_TEST_NUM_HARQ,                    "PTM HARQ processes\n",  0, .iptr = NULL, .defintval = 4,    TYPE_INT, 0}, \
  {CONFIG_STRING_MBS_TEST_HARQ_FEEDBACK,               "ACK/NACK feedback\n",   0, .iptr = NULL, .defintval = 1,    TYPE_INT, 0}, \
  {CONFIG_STRING_MBS_TEST_SDU_SIZE,                    "test SDU bytes\n",      0, .iptr = NULL, .defintval = 1000, TYPE_INT, 0}, \
  {CONFIG_STRING_MBS_TEST_SDU_INTERVAL,                "test SDU period ms\n",  0, .iptr = NULL, .defintval = 0,    TYPE_INT, 0}, \
  {CONFIG_STRING_MBS_TEST_SHARED_DELIVERY_UE,          "shared delivery UE\n",  0, .iptr = NULL, .defintval = 0,    TYPE_INT, 0}, \
  {CONFIG_STRING_MBS_TEST_SHARED_DELIVERY_PDU_SESSION, "its PDU session\n",     0, .iptr = NULL, .defintval = 1,    TYPE_INT, 0}, \
}
// clang-format on

void nr_mbs_test_adapter_init(gNB_MAC_INST *mac, nr_cell_sched_t *cell);

#endif /* NR_MBS_TEST_ADAPTER_H */
