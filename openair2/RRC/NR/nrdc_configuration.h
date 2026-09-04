/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NRDC_CONFIGURATION_H_
#define _NRDC_CONFIGURATION_H_

#include <stdint.h>

typedef struct {
  uint64_t mcg_cell_id;
  uint64_t scg_cell_id;
} nrdc_combination_t;

typedef struct {
  int combination_count;
  nrdc_combination_t *combinations;
} nrdc_configuration_t;

#endif /* _NRDC_CONFIGURATION_H_ */
