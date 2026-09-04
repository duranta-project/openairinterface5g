/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef RRC_GNB_MEASUREMENTS_H_
#define RRC_GNB_MEASUREMENTS_H_

/* void *ue is actually gNB_RRC_UE_t *ue but this cannot be used because
 * including "openair2/RRC/NR/nr_rrc_defs.h" breaks the compilation of
 * nr-uesoftmodem
 * to be fixed if it's seen as a problem
 */

int allocate_measurement_object_id(void *ue);
void free_measurement_object_id(void *ue, int mo_id);

int allocate_measurement_id(void *ue);
void free_measurement_id(void *ue, int mo_id);

int allocate_report_config_id(void *ue);
void free_report_config_id(void *ue, int mo_id);

void reset_all_measurement_ids(void *ue);

#endif /* RRC_GNB_MEASUREMENTS_H_ */
