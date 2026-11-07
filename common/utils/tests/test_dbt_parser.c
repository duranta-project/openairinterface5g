/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#define _POSIX_C_SOURCE 200809L

#include "common/utils/dbt.h"

#include <complex.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define CHECK(condition)                                                                            \
  do {                                                                                              \
    if (!(condition)) {                                                                             \
      fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #condition);               \
      return 1;                                                                                     \
    }                                                                                               \
  } while (0)

int main(void)
{
  char filename[] = "oai-dbt-XXXXXX";
  int fd = mkstemp(filename);
  CHECK(fd >= 0);
  FILE *fp = fdopen(fd, "w");
  CHECK(fp != NULL);
  fputs("beam_id,txru0,txru1,txru2\n"
        "7,0.5,0.5-0.5i,-0.5j\n"
        "42,1,0.25+0.75i,-1\n",
        fp);
  CHECK(fclose(fp) == 0);

  dbt_table_t table = dbt_read_csv(filename);
  CHECK(table.num_beams == 2);
  CHECK(table.num_weights_per_beam == 3);
  CHECK(table.beam_ids[0] == 7);
  CHECK(table.beam_ids[1] == 42);
  CHECK(creal(table.weights[0][0]) == 0.5);
  CHECK(cimag(table.weights[0][0]) == 0.0);
  CHECK(creal(table.weights[0][1]) == 0.5);
  CHECK(cimag(table.weights[0][1]) == -0.5);
  CHECK(creal(table.weights[0][2]) == 0.0);
  CHECK(cimag(table.weights[0][2]) == -0.5);
  CHECK(creal(table.weights[1][1]) == 0.25);
  CHECK(cimag(table.weights[1][1]) == 0.75);

  dbt_table_free(&table);
  CHECK(table.weights == NULL);
  CHECK(unlink(filename) == 0);
  return 0;
}
