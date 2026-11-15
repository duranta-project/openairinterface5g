/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "common/utils/dbt.h"
#include "common/utils/utils.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool dbt_parse_complex_token(const char *token, double complex *weight)
{
  // Check the suffix explicitly: sscanf's assignment count does not require trailing literals to match.
  double re = 0.0;
  double im = 0.0;
  int n = 0;
  if (sscanf(token, "%lf%lf%n", &re, &im, &n) == 2 && (token[n] == 'i' || token[n] == 'j') && token[n + 1] == '\0') {
    *weight = re + I * im;
    return true;
  }

  n = 0;
  if (sscanf(token, "%lf%n", &im, &n) == 1 && (token[n] == 'i' || token[n] == 'j') && token[n + 1] == '\0') {
    *weight = I * im;
    return true;
  }

  n = 0;
  if (sscanf(token, "%lf%n", &re, &n) == 1 && token[n] == '\0') {
    *weight = re;
    return true;
  }

  return false;
}

dbt_table_t dbt_read_csv(const char *filename)
{
  FILE *fp = fopen(filename, "r");
  AssertFatal(fp != NULL, "Failed to open DBT CSV file '%s'\n", filename);

  char line[16384];
  AssertFatal(fgets(line, sizeof(line), fp) != NULL, "Failed to read DBT CSV header from '%s'\n", filename);

  int weights = 0;
  for (char *p = line; *p; ++p)
    if (*p == ',')
      weights++;
  AssertFatal(weights > 0, "DBT CSV '%s' has no weight columns\n", filename);

  int beams = 0;
  while (fgets(line, sizeof(line), fp))
    beams++;
  AssertFatal(beams > 0, "No DBT beam rows found in CSV file '%s'\n", filename);

  rewind(fp);
  AssertFatal(fgets(line, sizeof(line), fp) != NULL, "Failed to reread DBT CSV header from '%s'\n", filename);

  dbt_table_t table = {
      .num_beams = beams,
      .num_weights_per_beam = weights,
  };
  table.beam_ids = calloc_or_fail(beams, sizeof(*table.beam_ids));
  table.weights = calloc_or_fail(beams, sizeof(*table.weights));

  int b = 0;
  while (fgets(line, sizeof(line), fp)) {
    line[strcspn(line, "\r\n")] = '\0';
    char *saveptr = NULL;
    char *tok = strtok_r(line, ",", &saveptr);
    AssertFatal(tok != NULL, "Malformed DBT row %d in '%s'\n", b + 1, filename);

    errno = 0;
    char *endptr = NULL;
    long beam_id = strtol(tok, &endptr, 10);
    AssertFatal(endptr != tok && errno != ERANGE && *endptr == '\0' && beam_id >= 0 && beam_id <= UINT16_MAX,
                "Invalid DBT beam id '%s' in file '%s', beam row %d\n",
                tok,
                filename,
                b + 1);
    table.beam_ids[b] = (uint16_t)beam_id;

    table.weights[b] = calloc_or_fail(weights, sizeof(*table.weights[b]));
    for (int w = 0; w < weights; ++w) {
      tok = strtok_r(NULL, ",", &saveptr);
      AssertFatal(tok != NULL, "Missing DBT weight in file '%s', beam row %d column %d\n", filename, b + 1, w + 1);
      AssertFatal(dbt_parse_complex_token(tok, &table.weights[b][w]),
                  "Invalid DBT complex weight '%s' in file '%s', beam row %d column %d\n",
                  tok,
                  filename,
                  b + 1,
                  w + 1);
    }
    AssertFatal(strtok_r(NULL, ",", &saveptr) == NULL,
                "Too many DBT columns in file '%s', beam row %d\n",
                filename,
                b + 1);
    b++;
  }
  AssertFatal(b == beams, "Parsed %d DBT rows but counted %d in file '%s'\n", b, beams, filename);

  fclose(fp);
  return table;
}

void dbt_table_free(dbt_table_t *table)
{
  if (table == NULL)
    return;
  for (int b = 0; b < table->num_beams; ++b)
    free(table->weights[b]);
  free(table->weights);
  free(table->beam_ids);
  memset(table, 0, sizeof(*table));
}
