/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <bits.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <string.h>

/* ── test harness ── */
static int g_failures = 0;
#define ARRSIZE 9
#define BITS_PER_WORD 32
#define TOTAL_BITS (ARRSIZE * BITS_PER_WORD)

static inline void set_bit_in_array(uint32_t *arr, int bit_pos)
{
  arr[bit_pos / BITS_PER_WORD] |= 1u << (bit_pos % BITS_PER_WORD);
}

static void run_test(const char *name, int result, int expected)
{
  if (result != expected) {
    fprintf(stderr, "FAIL [%s]: got %d, want %d\n", name, result, expected);
    g_failures++;
  } else {
    printf("PASS [%s]\n", name);
  }
}

static void test_get_first_bit_index_mask(void)
{
  /* --- no bits set --- */
  uint32_t zeros[ARRSIZE] = {0};
  run_test("firstbit_all_zeros", get_first_bit_index_mask(zeros, ARRSIZE, 0, TOTAL_BITS), -1);

  uint32_t arr[ARRSIZE];
  /* --- random single hit at a random position --- */
  int num_trials = 5;
  for (int t = 0; t < num_trials; t++) {
    int bit_pos = rand() % (TOTAL_BITS);  /* random bit in [0, 128) */
    char test_name[64];
    snprintf(test_name, sizeof(test_name), "firstbit_single_hit_bit%d", bit_pos);
    memset(arr, 0, sizeof(arr));
    set_bit_in_array(arr, bit_pos);
    run_test(test_name, get_first_bit_index_mask(arr, ARRSIZE, 0, TOTAL_BITS), bit_pos);
  }

  /* --- random multi-hit with random window --- */
  int multi_trials = 5;
  for (int t = 0; t < multi_trials; t++) {
    memset(arr, 0, sizeof(arr));
    int from_bit = rand() % (TOTAL_BITS);
    int num_bits = 1 + rand() % (TOTAL_BITS - from_bit);  /* at least 1 bit wide */
    int window_end = from_bit + num_bits;
    int min_pos = -1;
    int num_bits_set = 5 + rand() % 20;
    for (int b = 0; b < num_bits_set; b++) {
      int bit_pos = from_bit + rand() % num_bits;  /* confined to the window */
      set_bit_in_array(arr, bit_pos);
      if (min_pos == -1 || bit_pos < min_pos)
        min_pos = bit_pos;
    }
    /* put some bits outside the window to test they are ignored in some trial */
    if (from_bit > 0 && t % 2) {
      int bit_pos = rand() % from_bit;  /* before the window */
      set_bit_in_array(arr, bit_pos);
    }
    if (window_end < TOTAL_BITS  && t % 2) {
      int bit_pos = window_end + rand() % (TOTAL_BITS - window_end);  /* after */
      set_bit_in_array(arr, bit_pos);
    }

    char test_name[64];
    snprintf(test_name, sizeof(test_name),  "firstbit_multi_hit_trial%d_from%d_num%d_min%d", t, from_bit, num_bits, min_pos);
    run_test(test_name, get_first_bit_index_mask(arr, ARRSIZE, from_bit, num_bits), min_pos);
  }
}

static void test_get_last_bit_index(void)
{
  /* --- no bits set --- */
  uint32_t zeros[ARRSIZE] = {0};
  run_test("lastbit_all_zeros", get_last_bit_index(zeros, ARRSIZE), -1);

  uint32_t arr[ARRSIZE];

  /* --- random single hit at a random position --- */
  int num_trials = 5;
  for (int t = 0; t < num_trials; t++) {
    int bit_pos = rand() % (TOTAL_BITS);
    char test_name[64];
    snprintf(test_name, sizeof(test_name), "lastbit_single_hit_bit%d", bit_pos);
    memset(arr, 0, sizeof(arr));
    set_bit_in_array(arr, bit_pos);
    run_test(test_name, get_last_bit_index(arr, ARRSIZE), bit_pos);
  }

  /* --- random multi-hit: scattered bits, must return the maximum --- */
  int multi_trials = 5;
  for (int t = 0; t < multi_trials; t++) {
    memset(arr, 0, sizeof(arr));
    int max_pos = -1;
    int num_bits_set = 5 + rand() % 20;
    for (int b = 0; b < num_bits_set; b++) {
      int bit_pos = rand() % (TOTAL_BITS);
      set_bit_in_array(arr, bit_pos);
      if (bit_pos > max_pos)
        max_pos = bit_pos;
    }
    char test_name[64];
    snprintf(test_name, sizeof(test_name), "lastbit_multi_hit_trial%d_max%d", t, max_pos);
    run_test(test_name, get_last_bit_index(arr, ARRSIZE), max_pos);
  }
}

static void fisher_yates_shuffle(int *positions, int total, int n)
{
  /* partial shuffle: pick n distinct random positions from [0, total)   */
  /* see https://en.wikipedia.org/wiki/Fisher%E2%80%93Yates_shuffle#The_modern_algorithm */
  for (int i = 0; i < n; i++) {
    int j = i + rand() % (total - i);
    int tmp = positions[i];
    positions[i] = positions[j];
    positions[j] = tmp;
  }
}

static void test_count_bits(void)
{
  uint32_t arr[ARRSIZE];

  /* --- no bits set --- */
  uint32_t zeros[ARRSIZE] = {0};
  run_test("countbits_all_zeros", count_bits(zeros, ARRSIZE), 0);

  /* --- all bits set --- */
  uint32_t all_set[ARRSIZE];
  memset(all_set, 0xFF, sizeof(all_set));
  run_test("countbits_all_set", count_bits(all_set, ARRSIZE), TOTAL_BITS);

  /* --- random trials: set exactly N random bits, expect count N --- */
  int positions[TOTAL_BITS];
  for (int i = 0; i < TOTAL_BITS; i++)
    positions[i] = i;

  int num_trials = 5;
  for (int t = 0; t < num_trials; t++) {
    memset(arr, 0, sizeof(arr));
    int n = 1 + rand() % TOTAL_BITS;
    fisher_yates_shuffle(positions, TOTAL_BITS, n);
    for (int i = 0; i < n; i++)
      set_bit_in_array(arr, positions[i]);
    char test_name[64];
    snprintf(test_name, sizeof(test_name), "countbits_trial%d_n%d", t, n);
    run_test(test_name, count_bits(arr, ARRSIZE), n);
  }
}

void test_count_bits64_with_mask(void)
{
  /* --- zero value --- */
  run_test("countbits64_zero_value", count_bits64_with_mask(0ULL, 0, 64), 0);

  /* --- all bits set, full window --- */
  run_test("countbits64_all_set_full_window", count_bits64_with_mask(0xFFFFFFFFFFFFFFFFULL, 0, 64), 64);

  /* --- all bits set, window filters correctly --- */
  run_test("countbits64_all_set_window_16_from_8", count_bits64_with_mask(0xFFFFFFFFFFFFFFFFULL, 8, 16), 16);

  /* --- random trials: set exactly N bits within window, expect count N --- */
  int num_trials = 5;
  for (int t = 0; t < num_trials; t++) {
    int start = rand() % 64;
    int num = 1 + rand() % (64 - start);
    int window_end = start + num;

    /* initialise positions to the window [start, start+num) */
    int positions[64];
    for (int i = 0; i < num; i++)
      positions[i] = start + i;

    int n = 1 + rand() % num;
    fisher_yates_shuffle(positions, num, n);

    uint64_t v = 0;
    for (int i = 0; i < n; i++)
      v |= 1ULL << positions[i];

    /* put bits outside the window on odd trials */
    if (t % 2) {
      if (start > 0)
        v |= 1ULL << (rand() % start);
      if (window_end < 64)
        v |= 1ULL << (window_end + rand() % (64 - window_end));
    }

    char test_name[64];
    snprintf(test_name, sizeof(test_name), "countbits64_trial%d_start%d_num%d_n%d", t, start, num, n);
    run_test(test_name, count_bits64_with_mask(v, start, num), n);
  }
}

int main(void)
{
  srand((unsigned)time(NULL));
  test_get_first_bit_index_mask();
  test_get_last_bit_index();
  test_count_bits();
  test_count_bits64_with_mask();

  if (g_failures > 0) {
    fprintf(stderr, "\n%d test(s) FAILED.\n", g_failures);
    return 1;
  }
  printf("\nAll tests passed.\n");
  return 0;
}
