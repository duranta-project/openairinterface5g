/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef COMMON_UTILS_DBT_H
#define COMMON_UTILS_DBT_H

#include <complex.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int num_beams;
  int num_weights_per_beam;
  uint16_t *beam_ids;
  double complex **weights;
} dbt_table_t;

bool dbt_parse_complex_token(const char *token, double complex *weight);
dbt_table_t dbt_read_csv(const char *filename);
void dbt_table_free(dbt_table_t *table);

#ifdef __cplusplus
}
#endif

#endif /* COMMON_UTILS_DBT_H */
