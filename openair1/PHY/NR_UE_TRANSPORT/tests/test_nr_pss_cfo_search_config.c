/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "../nr_pss_cfo_search_config.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                                                                         \
  do {                                                                                                                           \
    if (!(condition)) {                                                                                                          \
      fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);                                                      \
      failures++;                                                                                                                \
    }                                                                                                                            \
  } while (0)

static nr_pss_cfo_search_config_t default_config(void)
{
  return (nr_pss_cfo_search_config_t){
      .enabled = true,
      .coarse_span_hz = NR_PSS_CFO_SEARCH_DEFAULT_COARSE_SPAN_HZ,
      .coarse_step_hz = NR_PSS_CFO_SEARCH_DEFAULT_COARSE_STEP_HZ,
      .fine_span_hz = NR_PSS_CFO_SEARCH_DEFAULT_FINE_SPAN_HZ,
      .fine_step_hz = NR_PSS_CFO_SEARCH_DEFAULT_FINE_STEP_HZ,
  };
}

static void expect_invalid_grid(nr_pss_cfo_search_config_t config)
{
  int coarse_bins = -1;
  int fine_bins = -1;
  CHECK(nr_pss_cfo_search_validate_config(&config, &coarse_bins, &fine_bins) == NR_PSS_CFO_SEARCH_INVALID_GRID);
  CHECK(coarse_bins == 0);
  CHECK(fine_bins == 0);
}

static void test_default_blind_grid(void)
{
  const nr_pss_cfo_search_config_t config = default_config();
  int coarse_bins = 0;
  int fine_bins = 0;
  CHECK(nr_pss_cfo_search_validate_config(&config, &coarse_bins, &fine_bins) == NR_PSS_CFO_SEARCH_OK);
  CHECK(coarse_bins == 25);
  CHECK(fine_bins == 31);
}

static void test_assisted_grid(void)
{
  nr_pss_cfo_search_config_t config = default_config();
  config.coarse_span_hz = 15000;
  int coarse_bins = 0;
  int fine_bins = 0;
  CHECK(nr_pss_cfo_search_validate_config(&config, &coarse_bins, &fine_bins) == NR_PSS_CFO_SEARCH_OK);
  CHECK(coarse_bins == 7);
  CHECK(fine_bins == 31);
}

static void test_disabled_grid_is_not_evaluated(void)
{
  const nr_pss_cfo_search_config_t config = {.enabled = false, .coarse_span_hz = -1, .coarse_step_hz = 0};
  int coarse_bins = -1;
  int fine_bins = -1;
  CHECK(nr_pss_cfo_search_validate_config(&config, &coarse_bins, &fine_bins) == NR_PSS_CFO_SEARCH_OK);
  CHECK(coarse_bins == 0);
  CHECK(fine_bins == 0);
}

static void test_null_config(void)
{
  int coarse_bins = -1;
  int fine_bins = -1;
  CHECK(nr_pss_cfo_search_validate_config(NULL, &coarse_bins, &fine_bins) == NR_PSS_CFO_SEARCH_INVALID_ARGUMENT);
  CHECK(coarse_bins == 0);
  CHECK(fine_bins == 0);
}

static void test_invalid_signs_and_steps(void)
{
  nr_pss_cfo_search_config_t config = default_config();
  config.coarse_span_hz = -1;
  expect_invalid_grid(config);

  config = default_config();
  config.fine_span_hz = -1;
  expect_invalid_grid(config);

  config = default_config();
  config.coarse_step_hz = 0;
  expect_invalid_grid(config);

  config = default_config();
  config.fine_step_hz = -400;
  expect_invalid_grid(config);
}

static void test_zero_must_belong_to_grid(void)
{
  nr_pss_cfo_search_config_t config = default_config();
  config.coarse_step_hz = 7000;
  expect_invalid_grid(config);

  config = default_config();
  config.fine_step_hz = 700;
  expect_invalid_grid(config);
}

static void test_grid_size_limit_and_overflow(void)
{
  nr_pss_cfo_search_config_t config = default_config();
  config.coarse_step_hz = 400; /* 301 bins over +/-60 kHz. */
  expect_invalid_grid(config);

  config = default_config();
  config.coarse_span_hz = INT_MAX;
  config.coarse_step_hz = 1;
  expect_invalid_grid(config);
}

static void test_zero_span_and_optional_outputs(void)
{
  nr_pss_cfo_search_config_t config = default_config();
  config.coarse_span_hz = 0;
  config.fine_span_hz = 0;
  int coarse_bins = 0;
  int fine_bins = 0;
  CHECK(nr_pss_cfo_search_validate_config(&config, &coarse_bins, &fine_bins) == NR_PSS_CFO_SEARCH_OK);
  CHECK(coarse_bins == 1);
  CHECK(fine_bins == 1);
  CHECK(nr_pss_cfo_search_validate_config(&config, NULL, NULL) == NR_PSS_CFO_SEARCH_OK);
}

static void test_status_names(void)
{
  CHECK(strcmp(nr_pss_cfo_search_status_name(NR_PSS_CFO_SEARCH_OK), "ok") == 0);
  CHECK(strcmp(nr_pss_cfo_search_status_name(NR_PSS_CFO_SEARCH_INVALID_ARGUMENT), "invalid_argument") == 0);
  CHECK(strcmp(nr_pss_cfo_search_status_name(NR_PSS_CFO_SEARCH_INVALID_GRID), "invalid_grid") == 0);
  CHECK(strcmp(nr_pss_cfo_search_status_name(NR_PSS_CFO_SEARCH_NO_CANDIDATE), "no_candidate") == 0);
  CHECK(strcmp(nr_pss_cfo_search_status_name((nr_pss_cfo_search_status_t)99), "unknown") == 0);
}

int main(void)
{
  test_default_blind_grid();
  test_assisted_grid();
  test_disabled_grid_is_not_evaluated();
  test_null_config();
  test_invalid_signs_and_steps();
  test_zero_must_belong_to_grid();
  test_grid_size_limit_and_overflow();
  test_zero_span_and_optional_outputs();
  test_status_names();

  if (failures != 0) {
    fprintf(stderr, "%d CFO search configuration test(s) failed\n", failures);
    return 1;
  }

  printf("All CFO search configuration tests passed\n");
  return 0;
}
