/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_MAC_FAPI_H_
#define NR_MAC_FAPI_H_

#include "nfapi/open-nFAPI/nfapi/public_inc/nfapi_nr_interface_scf.h"

struct NR_Sched_Rsp;
void reset_sched_response(struct NR_Sched_Rsp *sched_response, int frame, int slot, int module_id, int phy_id);
void run_scheduler_monolithic(const nfapi_nr_slot_indication_scf_t *ind, struct NR_Sched_Rsp *rsp);

#endif /* NR_MAC_FAPI_H_ */
