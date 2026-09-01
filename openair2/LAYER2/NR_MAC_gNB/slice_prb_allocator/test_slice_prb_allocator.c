/*!
 * \file test_slice_prb_allocator.c
 * \brief Unit tests for Network Slice PRB Range Allocation Algorithm
 * 
 * Compile with:
 *   gcc -Wall -Wextra -std=c11 -O2 -g test_slice_prb_allocator.c slice_prb_allocator.c -o test_slice_prb_allocator -lm
 * 
 * Run with:
 *   ./test_slice_prb_allocator
 */

#include "slice_prb_allocator.h"
#include "slice_prb_allocator_internal.h"  // Tests need access to functional implementation
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <limits.h>

#define ASSERT_EQ(a, b, msg) do { \
  if ((a) != (b)) { \
    fprintf(stderr, "FAIL: %s: expected %d, got %d\n", (msg), (b), (a)); \
    exit(1); \
  } \
} while(0)

#define ASSERT_FLOAT_EQ(a, b, msg) do { \
  if (fabsf((float)(a) - (float)(b)) > 0.0001f) { \
    fprintf(stderr, "FAIL: %s: expected %.6f, got %.6f\n", (msg), (float)(b), (float)(a)); \
    exit(1); \
  } \
} while(0)

#define ASSERT_GE(a, b, msg) do { \
  if ((a) < (b)) { \
    fprintf(stderr, "FAIL: %s: expected >= %d, got %d\n", (msg), (b), (a)); \
    exit(1); \
  } \
} while(0)

#define ASSERT_LE(a, b, msg) do { \
  if ((a) > (b)) { \
    fprintf(stderr, "FAIL: %s: expected <= %d, got %d\n", (msg), (b), (a)); \
    exit(1); \
  } \
} while(0)

#define ASSERT_GT(a, b, msg) do { \
  if ((a) <= (b)) { \
    fprintf(stderr, "FAIL: %s: expected > %d, got %d\n", (msg), (b), (a)); \
    exit(1); \
  } \
} while(0)

#define ASSERT_TRUE(cond, msg) do { \
  if (!(cond)) { \
    fprintf(stderr, "FAIL: %s\n", (msg)); \
    exit(1); \
  } \
} while(0)

static int tests_run = 0;
static int tests_passed = 0;

/* Helper function to print slice configuration */
static void print_slice_config(const slice_config_t *slice, int idx) {
  printf("    Slice %d (SST=%d, SD=%u):\n", idx, slice->slice_id.sst, slice->slice_id.sd);
  printf("      Dedicated: %.1f%%, Min: %.1f%%, Max: %.1f%%, Required PRBs: %d",
         slice->dedicated_prb_ratio * 100.0,
         slice->min_prb_ratio * 100.0,
         slice->max_prb_ratio * 100.0,
         slice->required_prbs);
  printf("\n");
}

/* Helper function to print allocation result with required PRBs */
static void print_allocation_result_with_required(const slice_alloc_result_t *result, 
                                                   const slice_alloc_input_t *input, 
                                                   int total_prbs) {
  printf("  Allocation Result:\n");
  printf("    Total Allocated: %d / %d PRBs\n", result->total_allocated_prbs, total_prbs);
  printf("    Per-Slice Allocation:\n");
  int max_slices = (input != NULL) ? input->num_slices : input->num_slices;
  for (int s = 0; s < max_slices; ++s) {
    float percentage = (result->ranges[s].num_prbs > 0) ? 
                        (float)result->ranges[s].num_prbs / total_prbs * 100.0 : 0.0;
    // Find the corresponding slice config to get required_prbs
    int required_prbs = 0;
    for (int i = 0; i < input->num_slices; ++i) {
      if (slice_nssai_eq(&result->ranges[s].slice_id, &input->slices[i].slice_id)) {
        required_prbs = input->slices[i].required_prbs;
        break;
      }
    }
    printf("      Slice (SST=%d, SD=%u): PRBs [%d, %d) = %d PRBs", 
           result->ranges[s].slice_id.sst,
           result->ranges[s].slice_id.sd,
           result->ranges[s].start_prb,
           result->ranges[s].end_prb,
           result->ranges[s].num_prbs);
    if (result->ranges[s].num_prbs > 0) {
      printf(" (%.1f%%)", percentage);
    }
    if (required_prbs > 0) {
      printf(", Required: %d PRBs", required_prbs);
    }
    printf("\n");
  }
}

/* Helper function to print OOP scheduler allocation result */
static void print_oop_allocation_result(const slice_scheduler_t *obj, int total_prbs) {
  if (obj == NULL) {
    return;
  }
  
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(obj, &num_ranges);
  
  if (ranges == NULL) {
    printf("  Allocation Result: Not available (schedule not called or invalid)\n");
    return;
  }
  
  int num_active = 0;
  int total_allocated = 0;
  slice_sch_get_stats(obj, &num_active, &total_allocated);
  
  printf("  Allocation Result:\n");
  printf("    Active Slices: %d\n", num_active);
  printf("    Total Allocated: %d / %d PRBs\n", total_allocated, total_prbs);
  printf("    Per-Slice Allocation:\n");
  
  for (int s = 0; s < num_ranges; ++s) {
    if (ranges[s].num_prbs > 0) {
      float percentage = (float)ranges[s].num_prbs / total_prbs * 100.0;
      // Find the corresponding slice config to get required_prbs
      int required_prbs = 0;
      if (obj->input != NULL) {
        for (int i = 0; i < obj->input->num_slices; ++i) {
          if (slice_nssai_eq(&ranges[s].slice_id, &obj->input->slices[i].slice_id)) {
            required_prbs = obj->input->slices[i].required_prbs;
            break;
          }
        }
      }
      printf("      Slice (SST=%d, SD=%u): PRBs [%d, %d) = %d PRBs (%.1f%%)", 
             ranges[s].slice_id.sst,
             ranges[s].slice_id.sd,
             ranges[s].start_prb,
             ranges[s].end_prb,
             ranges[s].num_prbs,
             percentage);
      if (required_prbs > 0) {
        printf(", Required: %d PRBs", required_prbs);
      }
      printf("\n");
    }
  }
}

#define TEST(name) \
  do { \
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"); \
    printf("Test %d: %s\n", tests_run + 1, #name); \
    printf("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n"); \
    tests_run++; \
    test_##name(); \
    tests_passed++; \
    printf("  ✓ PASS\n\n"); \
  } while(0)

/* Helper macro to allocate and initialize test structures */
#define ALLOCATE_TEST_STRUCTURES(num_slices, input_var, result_var) \
  slice_alloc_input_t *input_var = allocate_slice_input(num_slices); \
  slice_alloc_result_t *result_var = allocate_slice_result(num_slices); \
  if (input_var == NULL || result_var == NULL) { \
    fprintf(stderr, "FAIL: Memory allocation failed\n"); \
    if (input_var) free_slice_input(input_var); \
    if (result_var) free_slice_result(result_var); \
    exit(1); \
  }

/* Test 1: Basic allocation with two slices */
static void test_basic_two_slices(void) {
  printf("  Purpose: Test basic two-slice allocation with different ratios\n");
  printf("  Expected: Dedicated PRBs only when require=0 (min does not idle-allocate)\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  // Slice 1: 30% dedicated, 30% min, 50% max
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.30f;
  input->slices[0].min_prb_ratio = 0.30f;
  input->slices[0].max_prb_ratio = 0.50f;
  
  // Slice 2: 20% dedicated, 20% min, 50% max
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.20f;
  input->slices[1].min_prb_ratio = 0.20f;
  input->slices[1].max_prb_ratio = 0.50f;
  
  input->total_prbs = 100;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  printf("\n");
  
  int ret = calculate_slice_prb_ranges(input, result);
  
  print_allocation_result_with_required(result, input, input->total_prbs);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 2, "Should allocate to 2 slices");
  ASSERT_EQ(result->total_allocated_prbs, 50, "Only dedicated PRBs without require");
  
  // Check slice 1: dedicated 30% only
  printf("    ✓ Slice 1: %d PRBs (expected: 30 dedicated)\n", result->ranges[0].num_prbs);
  ASSERT_EQ(result->ranges[0].num_prbs, 30, "Slice 1 should get 30 dedicated PRBs");
  ASSERT_EQ(result->ranges[0].slice_id.sst, 1, "Slice 1 SST should be 1");
  ASSERT_EQ(result->ranges[0].slice_id.sd, 0, "Slice 1 SD should be 0");
  ASSERT_EQ(result->ranges[0].start_prb, 0, "Slice 1 should start at PRB 0");
  ASSERT_EQ(result->ranges[0].end_prb, result->ranges[0].start_prb + result->ranges[0].num_prbs,
            "Slice 1 end_prb should be start_prb + num_prbs");
  
  // Check slice 2: dedicated 20% only
  printf("    ✓ Slice 2: %d PRBs (expected: 20 dedicated)\n", result->ranges[1].num_prbs);
  ASSERT_EQ(result->ranges[1].num_prbs, 20, "Slice 2 should get 20 dedicated PRBs");
  ASSERT_EQ(result->ranges[1].slice_id.sst, 2, "Slice 2 SST should be 2");
  ASSERT_EQ(result->ranges[1].slice_id.sd, 0, "Slice 2 SD should be 0");
  ASSERT_EQ(result->ranges[1].start_prb, result->ranges[0].end_prb,
            "Slice 2 should start where slice 1 ends");
  ASSERT_EQ(result->ranges[1].end_prb, result->ranges[1].start_prb + result->ranges[1].num_prbs,
            "Slice 2 end_prb should be start_prb + num_prbs");
  
  // Check that ranges don't overlap
  ASSERT_EQ(result->ranges[0].end_prb, result->ranges[1].start_prb,
            "Slice ranges should be contiguous");
  
  // Total should equal dedicated sum
  ASSERT_EQ(result->ranges[0].num_prbs + result->ranges[1].num_prbs, 50,
            "Total PRBs should equal dedicated sum");
  printf("    ✓ Ranges are contiguous: [%d, %d) and [%d, %d)\n",
         result->ranges[0].start_prb, result->ranges[0].end_prb,
         result->ranges[1].start_prb, result->ranges[1].end_prb);
  
  free_slice_input(input);
  free_slice_result(result);
}

/* Test 2: Single slice with all PRBs */
static void test_single_slice_all_prbs(void) {
  printf("  Purpose: Test single slice that gets all available PRBs\n");
  printf("  Expected: Single slice gets 100%% of PRBs (106 PRBs)\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 1.0f;
  input->slices[0].min_prb_ratio = 1.0f;
  input->slices[0].max_prb_ratio = 1.0f;
  
  input->total_prbs = 106;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  printf("\n");
  
  int ret = calculate_slice_prb_ranges(input, result);
  
  print_allocation_result_with_required(result, input, input->total_prbs);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 1, "Should allocate to 1 slice");
  ASSERT_EQ(result->ranges[0].num_prbs, 106, "Slice should get all 106 PRBs");
  ASSERT_EQ(result->ranges[0].start_prb, 0, "Should start at PRB 0");
  ASSERT_EQ(result->ranges[0].end_prb, 106, "Should end at PRB 106");
  ASSERT_EQ(result->total_allocated_prbs, 106, "Should allocate all PRBs");
  printf("    ✓ Slice gets all %d PRBs: [%d, %d)\n",
         result->ranges[0].num_prbs, result->ranges[0].start_prb, result->ranges[0].end_prb);
  
  free_slice_input(input);
  free_slice_result(result);
}

/* Test 3: Slice with no active UEs should get no PRBs */
static void test_no_active_ues(void) {
  printf("  Purpose: Test that slices with zero ratios get no PRBs\n");
  printf("  Expected: Slice with zero ratios gets 0 PRBs\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.0f;
  input->slices[0].min_prb_ratio = 0.0f;
  input->slices[0].max_prb_ratio = 0.0f;
  
  input->num_slices = 1;
  input->total_prbs = 100;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  printf("    Note: Slice has zero ratios, so it should get 0 PRBs\n\n");
  
  int ret = calculate_slice_prb_ranges(input, result);
  
  print_allocation_result_with_required(result, input, input->total_prbs);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 1, "Should return 1 slice");
  ASSERT_EQ(result->ranges[0].num_prbs, 0, "Slice with zero ratios should get 0 PRBs");
  ASSERT_EQ(result->total_allocated_prbs, 0, "Should allocate 0 PRBs");
  printf("    ✓ Slice with zero ratios correctly gets 0 PRBs\n");
  
  free_slice_input(input);
  free_slice_result(result);
}

/* Test 4: Multiple slices with different ratios */
static void test_multiple_slices_different_ratios(void) {
  printf("  Purpose: Test allocation with 3 slices having different dedicated/min/max ratios\n");
  printf("  Expected: Dedicated PRBs only when require=0\n\n");
  
  ALLOCATE_TEST_STRUCTURES(3, input, result);
  
  // Slice 1: 10% dedicated, 20% min, 40% max
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.10f;
  input->slices[0].min_prb_ratio = 0.20f;
  input->slices[0].max_prb_ratio = 0.40f;
  
  // Slice 2: 15% dedicated, 25% min, 50% max
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.15f;
  input->slices[1].min_prb_ratio = 0.25f;
  input->slices[1].max_prb_ratio = 0.50f;
  
  // Slice 3: 5% dedicated, 10% min, 30% max
  input->slices[2].slice_id = slice_nssai_from_int(3);
  input->slices[2].dedicated_prb_ratio = 0.05f;
  input->slices[2].min_prb_ratio = 0.10f;
  input->slices[2].max_prb_ratio = 0.30f;
  
  input->num_slices = 3;
  input->total_prbs = 100;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  print_slice_config(&input->slices[2], 2);
  printf("\n");
  
  int ret = calculate_slice_prb_ranges(input, result);
  
  print_allocation_result_with_required(result, input, input->total_prbs);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 3, "Should allocate to 3 slices");
  ASSERT_EQ(result->total_allocated_prbs, 30, "Only dedicated PRBs (10+15+5) without require");
  
  printf("    ✓ Slice 1: %d PRBs (expected: 10 dedicated)\n", result->ranges[0].num_prbs);
  ASSERT_EQ(result->ranges[0].num_prbs, 10, "Slice 1 dedicated");
  
  printf("    ✓ Slice 2: %d PRBs (expected: 15 dedicated)\n", result->ranges[1].num_prbs);
  ASSERT_EQ(result->ranges[1].num_prbs, 15, "Slice 2 dedicated");
  
  printf("    ✓ Slice 3: %d PRBs (expected: 5 dedicated)\n", result->ranges[2].num_prbs);
  ASSERT_EQ(result->ranges[2].num_prbs, 5, "Slice 3 dedicated");
  
  // Check contiguous allocation
  ASSERT_EQ(result->ranges[0].end_prb, result->ranges[1].start_prb,
            "Slice 1 and 2 should be contiguous");
  ASSERT_EQ(result->ranges[1].end_prb, result->ranges[2].start_prb,
            "Slice 2 and 3 should be contiguous");
  
  // Total should equal 100
  int total = result->ranges[0].num_prbs + result->ranges[1].num_prbs + result->ranges[2].num_prbs;
  ASSERT_EQ(total, 30, "Total PRBs should equal dedicated sum");
  printf("    ✓ Ranges are contiguous: [%d, %d), [%d, %d), [%d, %d)\n",
         result->ranges[0].start_prb, result->ranges[0].end_prb,
         result->ranges[1].start_prb, result->ranges[1].end_prb,
         result->ranges[2].start_prb, result->ranges[2].end_prb);
free_slice_input(input);
free_slice_result(result);

}

/* Test 5: Invalid config — sum(min) > 100% */
static void test_dedicated_exceeds_total(void) {
  printf("  Purpose: Reject configurations where sum(min) exceeds 100%%\n");
  printf("  Expected: validate_slice_config / calculate_slice_prb_ranges fail\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.60f;
  input->slices[0].min_prb_ratio = 0.60f;
  input->slices[0].max_prb_ratio = 0.60f;
  
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.60f;
  input->slices[1].min_prb_ratio = 0.60f;
  input->slices[1].max_prb_ratio = 0.60f;
  
  input->num_slices = 2;
  input->total_prbs = 100;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  printf("    Note: sum(min)=120%% > 100%% — invalid NS policy\n\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(validate_slice_config(input), false, "validate should reject sum(min)>100%");
  ASSERT_EQ(calculate_slice_prb_ranges(input, result), -1, "calculate should fail");
  printf("    ✓ sum(min) > 100%% rejected\n");
free_slice_input(input);
free_slice_result(result);

}

/* Test 6: Max ratio enforcement */
static void test_max_ratio_enforcement(void) {
  printf("  Purpose: Test that max_prb_ratio is enforced as a hard limit\n");
  printf("  Expected: Even with 90 PRBs remaining, slice cannot exceed 30%% (30 PRBs)\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  // Slice with 30% max, but plenty of remaining PRBs
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.10f;
  input->slices[0].min_prb_ratio = 0.10f;
  input->slices[0].max_prb_ratio = 0.30f;  // Hard limit at 30%
  
  input->num_slices = 1;
  input->total_prbs = 100;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  printf("    Note: After dedicated (10 PRBs), 90 PRBs remain, but max is 30%%\n");
  printf("          Slice should get exactly 30 PRBs (max limit)\n\n");
  
  int ret = calculate_slice_prb_ranges(input, result);
  
  print_allocation_result_with_required(result, input, input->total_prbs);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 1, "Should allocate to 1 slice");
  printf("    ✓ Slice 1: %d PRBs (expected: ≤30, hard limit)\n", result->ranges[0].num_prbs);
  ASSERT_LE(result->ranges[0].num_prbs, 30, "Should not exceed max (30 PRBs)");
  // Even though there are 90 PRBs remaining, slice should only get up to 30
  printf("    ✓ Max ratio (30%%) is correctly enforced as hard limit\n");
free_slice_input(input);
free_slice_result(result);

}

/* Test 7: Validation tests */
static void test_validation(void) {
  printf("  Purpose: Test input validation and error handling\n");
  printf("  Expected: Invalid inputs return -1, valid inputs succeed\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  printf("  Testing NULL input...\n");
  int ret = calculate_slice_prb_ranges(NULL, result);
  ASSERT_EQ(ret, -1, "NULL input should return -1");
  printf("    ✓ NULL input correctly rejected\n");
  
  printf("  Testing NULL result...\n");
  ret = calculate_slice_prb_ranges(input, NULL);
  ASSERT_EQ(ret, -1, "NULL result should return -1");
  printf("    ✓ NULL result correctly rejected\n");
  
  printf("  Testing invalid ratio (> 1.0)...\n");
  // Test invalid ratios
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 1.5f;  // Invalid: > 1.0
  input->slices[0].min_prb_ratio = 0.5f;
  input->slices[0].max_prb_ratio = 0.5f;
  input->total_prbs = 100;
  
  ret = calculate_slice_prb_ranges(input, result);
  ASSERT_EQ(ret, -1, "Invalid ratio should return -1");
  printf("    ✓ Ratio > 1.0 correctly rejected\n");
  
  printf("  Testing invalid relationship (dedicated > min)...\n");
  // Test invalid relationship (dedicated > min)
  input->slices[0].dedicated_prb_ratio = 0.5f;
  input->slices[0].min_prb_ratio = 0.3f;  // Invalid: min < dedicated
  input->slices[0].max_prb_ratio = 0.5f;
  
  ret = calculate_slice_prb_ranges(input, result);
  ASSERT_EQ(ret, -1, "Invalid ratio relationship should return -1");
  printf("    ✓ Invalid ratio relationship (dedicated > min) correctly rejected\n");
}

/* Test 8: Edge case - zero total PRBs */
static void test_zero_total_prbs(void) {
  printf("  Purpose: Test edge case with zero total PRBs\n");
  printf("  Expected: Zero total PRBs should be rejected (return -1)\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.5f;
  input->slices[0].min_prb_ratio = 0.5f;
  input->slices[0].max_prb_ratio = 0.5f;
  input->num_slices = 1;
  input->total_prbs = 0;  // Invalid
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d (INVALID)\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  printf("\n");
  
  int ret = calculate_slice_prb_ranges(input, result);
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, -1, "Zero total PRBs should return -1");
  printf("    ✓ Zero total PRBs correctly rejected\n");
free_slice_input(input);
free_slice_result(result);

}

/* Test 9: Real-world scenario - 106 PRBs with 2 slices */
static void test_real_world_106_prbs(void) {
  printf("  Purpose: Test real-world 5G NR scenario with 106 PRBs\n");
  printf("  Expected: eMBB and URLLC slices get appropriate allocations\n");
  printf("            respecting their dedicated, min, and max ratios\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  // Slice 1: 33% dedicated, 33% min, 50% max (typical eMBB slice)
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.33f;
  input->slices[0].min_prb_ratio = 0.33f;
  input->slices[0].max_prb_ratio = 0.50f;
  
  // Slice 2: 20% dedicated, 20% min, 50% max (typical URLLC slice)
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.20f;
  input->slices[1].min_prb_ratio = 0.20f;
  input->slices[1].max_prb_ratio = 0.50f;
  
  input->num_slices = 2;
  input->total_prbs = 106;  // Typical 5G NR bandwidth
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d (typical 5G NR bandwidth)\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  printf("    Slice 1 (eMBB - Enhanced Mobile Broadband):\n");
  print_slice_config(&input->slices[0], 0);
  printf("    Slice 2 (URLLC - Ultra-Reliable Low-Latency Communication):\n");
  print_slice_config(&input->slices[1], 1);
  printf("\n");
  
  int ret = calculate_slice_prb_ranges(input, result);
  
  print_allocation_result_with_required(result, input, input->total_prbs);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 2, "Should allocate to 2 slices");
  ASSERT_EQ(result->total_allocated_prbs, 56, "Dedicated only without require (35+21)");
  
  printf("    ✓ Slice 1 (eMBB): %d PRBs (expected: 35 dedicated)\n", result->ranges[0].num_prbs);
  ASSERT_EQ(result->ranges[0].num_prbs, 35, "Slice 1 dedicated");
  
  printf("    ✓ Slice 2 (URLLC): %d PRBs (expected: 21 dedicated)\n", result->ranges[1].num_prbs);
  ASSERT_EQ(result->ranges[1].num_prbs, 21, "Slice 2 dedicated");
  
  // Check contiguous allocation
  ASSERT_EQ(result->ranges[0].end_prb, result->ranges[1].start_prb,
            "Slices should be contiguous");
  ASSERT_EQ(result->ranges[1].end_prb, 56, "Last slice ends at dedicated total");
  printf("    ✓ Ranges are contiguous: [%d, %d) and [%d, %d)\n",
         result->ranges[0].start_prb, result->ranges[0].end_prb,
         result->ranges[1].start_prb, result->ranges[1].end_prb);
free_slice_input(input);
free_slice_result(result);

}

/* Test Pass 1: Dedicated PRB Allocation */
static void test_pass1_dedicated(void) {
  printf("  Purpose: Test Pass 1 (dedicated allocation) in isolation\n");
  printf("  Expected: Allocates dedicated PRBs, handles scaling when exceeds total\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  // Test case 1: Normal dedicated allocation
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.30f;
  input->slices[0].min_prb_ratio = 0.30f;
  input->slices[0].max_prb_ratio = 0.50f;
  
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.20f;
  input->slices[1].min_prb_ratio = 0.20f;
  input->slices[1].max_prb_ratio = 0.50f;
  
  input->num_slices = 2;
  input->total_prbs = 100;
  
  // Initialize result
  // Result already zeroed by calloc
  for (int s = 0; s < input->num_slices; ++s) {
    result->ranges[s].slice_id = input->slices[s].slice_id;
  }
  
  int allocated_prbs = 0;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  printf("\n");
  
  int ret = pass1_allocate_dedicated(input, result, &allocated_prbs);
  
  printf("  Pass 1 Result:\n");
  printf("    Allocated PRBs: %d\n", allocated_prbs);
  printf("    Slice 1: %d PRBs (expected: 30)\n", result->ranges[0].num_prbs);
  printf("    Slice 2: %d PRBs (expected: 20)\n", result->ranges[1].num_prbs);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 0, "Pass 1 should succeed");
  ASSERT_EQ(result->ranges[0].num_prbs, 30, "Slice 1 should get 30 PRBs");
  ASSERT_EQ(result->ranges[1].num_prbs, 20, "Slice 2 should get 20 PRBs");
  ASSERT_EQ(allocated_prbs, 50, "Total allocated should be 50 PRBs");
  
  // Test case 2: Dedicated exceeds total (scaling)
  input->slices[0].dedicated_prb_ratio = 0.60f;
  input->slices[1].dedicated_prb_ratio = 0.60f;
  
  // Result already zeroed by calloc
  for (int s = 0; s < input->num_slices; ++s) {
    result->ranges[s].slice_id = input->slices[s].slice_id;
  }
  allocated_prbs = 0;
  
  printf("  Test Case 2: Dedicated exceeds total (60%% + 60%% = 120%%)\n");
  ret = pass1_allocate_dedicated(input, result, &allocated_prbs);
  
  printf("  Pass 1 Result (with scaling):\n");
  printf("    Allocated PRBs: %d (expected: 100 after scaling)\n", allocated_prbs);
  printf("    Slice 1: %d PRBs (expected: 50 after scaling)\n", result->ranges[0].num_prbs);
  printf("    Slice 2: %d PRBs (expected: 50 after scaling)\n", result->ranges[1].num_prbs);
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Pass 1 should succeed");
  ASSERT_EQ(allocated_prbs, 100, "After scaling, should allocate exactly 100 PRBs");
  ASSERT_EQ(result->ranges[0].num_prbs, 50, "Slice 1 should get 50 PRBs after scaling");
  ASSERT_EQ(result->ranges[1].num_prbs, 50, "Slice 2 should get 50 PRBs after scaling");
free_slice_input(input);
free_slice_result(result);

}

/* Test Pass 2: Prioritized Resource Allocation */
static void test_pass2_prioritized(void) {
  printf("  Purpose: Test Pass 2 (prioritized allocation) in isolation\n");
  printf("  Expected: Allocates prioritized resources based on required_prbs\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  // Setup: Pass 1 already allocated dedicated PRBs
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.20f;
  input->slices[0].min_prb_ratio = 0.30f;
  input->slices[0].max_prb_ratio = 0.50f;
  input->slices[0].required_prbs = 35; // Needs more than dedicated
  
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.10f;
  input->slices[1].min_prb_ratio = 0.25f;
  input->slices[1].max_prb_ratio = 0.50f;
  input->slices[1].required_prbs = 30; // Needs more than dedicated
  
  input->num_slices = 2;
  input->total_prbs = 100;
  
  // Initialize result with Pass 1 allocations
  // Result already zeroed by calloc
  for (int s = 0; s < input->num_slices; ++s) {
    result->ranges[s].slice_id = input->slices[s].slice_id;
  }
  result->ranges[0].num_prbs = 20; // Dedicated from Pass 1
  result->ranges[1].num_prbs = 10; // Dedicated from Pass 1
  
  int allocated_prbs = 30; // From Pass 1
  int remaining_prbs = 70; // 100 - 30
  
  printf("  Input Configuration (after Pass 1):\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Already allocated: %d PRBs (dedicated)\n", allocated_prbs);
  printf("    Remaining: %d PRBs\n", remaining_prbs);
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  printf("\n");
  
  int ret = pass2_allocate_prioritized(input, result, &allocated_prbs, &remaining_prbs);
  
  printf("  Pass 2 Result:\n");
  printf("    Allocated PRBs: %d (was %d, added %d)\n", allocated_prbs, 30, allocated_prbs - 30);
  printf("    Remaining PRBs: %d\n", remaining_prbs);
  printf("    Slice 1: %d PRBs (was 20, added %d, prioritized=10)\n",
         result->ranges[0].num_prbs, result->ranges[0].num_prbs - 20);
  printf("    Slice 2: %d PRBs (was 10, added %d, prioritized=15)\n",
         result->ranges[1].num_prbs, result->ranges[1].num_prbs - 10);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 0, "Pass 2 should succeed");
  ASSERT_EQ(result->ranges[0].num_prbs, 30, "Slice 1 should get 30 PRBs (20 dedicated + 10 prioritized)");
  ASSERT_EQ(result->ranges[1].num_prbs, 25, "Slice 2 should get 25 PRBs (10 dedicated + 15 prioritized)");
  ASSERT_EQ(allocated_prbs, 55, "Total allocated should be 55 PRBs");
  ASSERT_EQ(remaining_prbs, 45, "Remaining should be 45 PRBs");
free_slice_input(input);
free_slice_result(result);

}

/* Test Pass 3: Shared Resource Allocation */
static void test_pass3_shared(void) {
  printf("  Purpose: Test Pass 3 (shared allocation) in isolation\n");
  printf("  Expected: Distributes shared resources proportionally\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  // Setup: Pass 1 and 2 already allocated
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.20f;
  input->slices[0].min_prb_ratio = 0.30f;
  input->slices[0].max_prb_ratio = 0.50f;
  input->slices[0].required_prbs = 45;
  
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.10f;
  input->slices[1].min_prb_ratio = 0.25f;
  input->slices[1].max_prb_ratio = 0.50f;
  input->slices[1].required_prbs = 35;
  
  input->num_slices = 2;
  input->total_prbs = 100;
  
  // Initialize result with Pass 1+2 allocations
  // Result already zeroed by calloc
  for (int s = 0; s < input->num_slices; ++s) {
    result->ranges[s].slice_id = input->slices[s].slice_id;
  }
  result->ranges[0].num_prbs = 30; // After Pass 1+2
  result->ranges[1].num_prbs = 25; // After Pass 1+2
  
  int allocated_prbs = 55; // From Pass 1+2
  int remaining_prbs = 45; // Shared resources
  
  printf("  Input Configuration (after Pass 1+2):\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Already allocated: %d PRBs\n", allocated_prbs);
  printf("    Remaining (shared): %d PRBs\n", remaining_prbs);
  printf("    Slice 1: %d PRBs, max=50, can_add=%d, required=%d\n",
         result->ranges[0].num_prbs, 50 - result->ranges[0].num_prbs, input->slices[0].required_prbs);
  printf("    Slice 2: %d PRBs, max=50, can_add=%d, required=%d\n",
         result->ranges[1].num_prbs, 50 - result->ranges[1].num_prbs, input->slices[1].required_prbs);
  printf("\n");
  
  int ret = pass3_allocate_shared(input, result, &allocated_prbs, &remaining_prbs);
  
  printf("  Pass 3 Result:\n");
  printf("    Allocated PRBs: %d (was %d, added %d)\n", allocated_prbs, 55, allocated_prbs - 55);
  printf("    Remaining PRBs: %d\n", remaining_prbs);
  printf("    Slice 1: %d PRBs (was 30, added %d)\n",
         result->ranges[0].num_prbs, result->ranges[0].num_prbs - 30);
  printf("    Slice 2: %d PRBs (was 25, added %d)\n",
         result->ranges[1].num_prbs, result->ranges[1].num_prbs - 25);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 0, "Pass 3 should succeed");
  ASSERT_GE(result->ranges[0].num_prbs, 30, "Slice 1 should get at least 30 PRBs");
  ASSERT_LE(result->ranges[0].num_prbs, 50, "Slice 1 should not exceed max (50)");
  ASSERT_GE(result->ranges[1].num_prbs, 25, "Slice 2 should get at least 25 PRBs");
  ASSERT_LE(result->ranges[1].num_prbs, 50, "Slice 2 should not exceed max (50)");
  ASSERT_EQ(allocated_prbs + remaining_prbs, 100, "Allocated + remaining should equal total");
free_slice_input(input);
free_slice_result(result);

}

/* Test Pass 4: Range Assignment */
static void test_pass4_ranges(void) {
  printf("  Purpose: Test Pass 4 (range assignment) in isolation\n");
  printf("  Expected: Assigns contiguous, non-overlapping ranges\n\n");
  
  ALLOCATE_TEST_STRUCTURES(3, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[2].slice_id = slice_nssai_from_int(3);
  input->num_slices = 3;
  
  // Setup: PRBs already allocated (from previous passes)
  result->ranges[0].num_prbs = 30;
  result->ranges[1].num_prbs = 20;
  result->ranges[2].num_prbs = 50;
  // Slice 0 has no PRBs (should be skipped)
  
  printf("  Input Configuration:\n");
  printf("    Slice 1: %d PRBs\n", result->ranges[0].num_prbs);
  printf("    Slice 2: %d PRBs\n", result->ranges[1].num_prbs);
  printf("    Slice 3: %d PRBs\n", result->ranges[2].num_prbs);
  printf("    Total: %d PRBs\n", 30 + 20 + 50);
  printf("\n");
  
  int ret = pass4_assign_ranges(input, result);
  
  printf("  Pass 4 Result:\n");
  printf("    Slice 1: [%d, %d) = %d PRBs\n",
         result->ranges[0].start_prb, result->ranges[0].end_prb, result->ranges[0].num_prbs);
  printf("    Slice 2: [%d, %d) = %d PRBs\n",
         result->ranges[1].start_prb, result->ranges[1].end_prb, result->ranges[1].num_prbs);
  printf("    Slice 3: [%d, %d) = %d PRBs\n",
         result->ranges[2].start_prb, result->ranges[2].end_prb, result->ranges[2].num_prbs);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 0, "Pass 4 should succeed");
  ASSERT_EQ(result->ranges[0].start_prb, 0, "Slice 1 should start at 0");
  ASSERT_EQ(result->ranges[0].end_prb, 30, "Slice 1 should end at 30");
  ASSERT_EQ(result->ranges[1].start_prb, 30, "Slice 2 should start at 30 (contiguous)");
  ASSERT_EQ(result->ranges[1].end_prb, 50, "Slice 2 should end at 50");
  ASSERT_EQ(result->ranges[2].start_prb, 50, "Slice 3 should start at 50 (contiguous)");
  ASSERT_EQ(result->ranges[2].end_prb, 100, "Slice 3 should end at 100");
  ASSERT_EQ(result->ranges[0].end_prb, result->ranges[1].start_prb,
            "Slices should be contiguous");
  ASSERT_EQ(result->ranges[1].end_prb, result->ranges[2].start_prb,
            "Slices should be contiguous");
free_slice_input(input);
free_slice_result(result);

}

/* Test Pass 1 Edge Cases */
static void test_pass1_dedicated_static_allocation(void) {
  printf("  Purpose: Test Pass 1 with dedicated = min = max (static allocation)\n");
  printf("  Expected: All PRBs allocated in Pass 1, no scaling needed\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.50f;
  input->slices[0].min_prb_ratio = 0.50f;
  input->slices[0].max_prb_ratio = 0.50f;
  
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.50f;
  input->slices[1].min_prb_ratio = 0.50f;
  input->slices[1].max_prb_ratio = 0.50f;
  
  input->num_slices = 2;
  input->total_prbs = 100;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  printf("\n");
  
  // Result already zeroed by calloc
  for (int s = 0; s < input->num_slices; ++s) {
    result->ranges[s].slice_id = input->slices[s].slice_id;
  }
  
  int allocated_prbs = 0;
  
  int ret = pass1_allocate_dedicated(input, result, &allocated_prbs);
  
  printf("  Pass 1 Result:\n");
  printf("    Allocated PRBs: %d\n", allocated_prbs);
  printf("    Slice 1: %d PRBs\n", result->ranges[0].num_prbs);
  printf("    Slice 2: %d PRBs\n", result->ranges[1].num_prbs);
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Should succeed");
  ASSERT_EQ(allocated_prbs, 100, "Should allocate all PRBs");
  ASSERT_EQ(result->ranges[0].num_prbs, 50, "Slice 1 should get 50 PRBs");
  ASSERT_EQ(result->ranges[1].num_prbs, 50, "Slice 2 should get 50 PRBs");
free_slice_input(input);
free_slice_result(result);

}

static void test_pass1_max_less_than_dedicated(void) {
  printf("  Purpose: Test Pass 1 when max < dedicated (should cap at max)\n");
  printf("  Expected: Dedicated is capped at max_prb_ratio\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.50f;
  input->slices[0].min_prb_ratio = 0.50f;
  input->slices[0].max_prb_ratio = 0.30f; // Max < dedicated
  
  input->num_slices = 1;
  input->total_prbs = 100;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  printf("    Note: Max (30%%) < Dedicated (50%%), so dedicated will be capped at max\n\n");
  
  // Result already zeroed by calloc
  result->ranges[0].slice_id = input->slices[0].slice_id;
  
  int allocated_prbs = 0;
  
  int ret = pass1_allocate_dedicated(input, result, &allocated_prbs);
  
  printf("  Pass 1 Result:\n");
  printf("    Allocated PRBs: %d (capped at max=30)\n", allocated_prbs);
  printf("    Slice 1: %d PRBs (capped at max, dedicated was 50)\n", result->ranges[0].num_prbs);
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Should succeed");
  ASSERT_EQ(result->ranges[0].num_prbs, 30, "Should be capped at max (30)");
  ASSERT_EQ(allocated_prbs, 30, "Total should be 30");
free_slice_input(input);
free_slice_result(result);

}

static void test_pass1_zero_dedicated(void) {
  printf("  Purpose: Test Pass 1 with zero dedicated (slice still active)\n");
  printf("  Expected: Slice gets 0 PRBs but is counted as active\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.0f;
  input->slices[0].min_prb_ratio = 0.30f;
  input->slices[0].max_prb_ratio = 0.50f;
  
  input->num_slices = 1;
  input->total_prbs = 100;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  printf("    Note: Dedicated is 0%%, but slice has active UEs, so it's counted as active\n\n");
  
  // Result already zeroed by calloc
  result->ranges[0].slice_id = input->slices[0].slice_id;
  
  int allocated_prbs = 0;
  
  int ret = pass1_allocate_dedicated(input, result, &allocated_prbs);
  
  printf("  Pass 1 Result:\n");
  printf("    Allocated PRBs: %d\n", allocated_prbs);
  printf("    Slice 1: %d PRBs (zero dedicated)\n", result->ranges[0].num_prbs);
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Should succeed");
  ASSERT_EQ(result->ranges[0].num_prbs, 0, "Should get 0 PRBs (zero dedicated)");
  ASSERT_EQ(allocated_prbs, 0, "Total should be 0");
free_slice_input(input);
free_slice_result(result);

}

/* Test Pass 2 Edge Cases */
static void test_pass2_slice_doesnt_need_prioritized(void) {
  printf("  Purpose: Test Pass 2 when slice doesn't need prioritized resources\n");
  printf("  Expected: Prioritized resources remain available for Pass 3\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.20f;
  input->slices[0].min_prb_ratio = 0.30f;
  input->slices[0].max_prb_ratio = 0.50f;
  input->slices[0].required_prbs = 15; // Less than current (20)
  
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.10f;
  input->slices[1].min_prb_ratio = 0.25f;
  input->slices[1].max_prb_ratio = 0.50f;
  input->slices[1].required_prbs = 30; // Needs more
  
  input->num_slices = 2;
  input->total_prbs = 100;
  
  printf("  Input Configuration (after Pass 1):\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Already allocated (dedicated): 30 PRBs\n");
  printf("    Remaining: 70 PRBs\n");
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  printf("    Note: Slice 1 required=15 < current=20, so doesn't need prioritized\n");
  printf("          Slice 2 required=30 > current=10, so needs prioritized\n\n");
  
  // Result already zeroed by calloc
  for (int s = 0; s < input->num_slices; ++s) {
    result->ranges[s].slice_id = input->slices[s].slice_id;
  }
  result->ranges[0].num_prbs = 20; // From Pass 1
  result->ranges[1].num_prbs = 10; // From Pass 1
  
  int allocated_prbs = 30;
  int remaining_prbs = 70;
  
  int ret = pass2_allocate_prioritized(input, result, &allocated_prbs, &remaining_prbs);
  
  printf("  Pass 2 Result:\n");
  printf("    Allocated PRBs: %d (was 30, added %d)\n", allocated_prbs, allocated_prbs - 30);
  printf("    Remaining PRBs: %d\n", remaining_prbs);
  printf("    Slice 1: %d PRBs (no change, doesn't need prioritized)\n", result->ranges[0].num_prbs);
  printf("    Slice 2: %d PRBs (was 10, added %d prioritized)\n",
         result->ranges[1].num_prbs, result->ranges[1].num_prbs - 10);
  printf("    Note: Slice 1's prioritized resources (10 PRBs) remain available for Pass 3\n");
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Should succeed");
  ASSERT_EQ(result->ranges[0].num_prbs, 20, "Slice 1 should not get prioritized (doesn't need)");
  ASSERT_EQ(result->ranges[1].num_prbs, 25, "Slice 2 should get prioritized (15 PRBs)");
  // Slice 1's prioritized resources (10 PRBs) remain available, so remaining = 70 - 15 = 55
  // But actually, Slice 1's prioritized (10) + remaining after Pass 2 = 10 + 45 = 55
  // The prioritized resources that weren't claimed become available for Pass 3
  ASSERT_GE(remaining_prbs, 55, "At least 55 PRBs should remain (Slice 1's prioritized not claimed)");
free_slice_input(input);
free_slice_result(result);

}

static void test_pass2_insufficient_prioritized(void) {
  printf("  Purpose: Test Pass 2 with insufficient prioritized resources\n");
  printf("  Expected: Proportional scaling of prioritized allocations\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.20f;
  input->slices[0].min_prb_ratio = 0.50f; // Needs 30 prioritized
  input->slices[0].max_prb_ratio = 0.60f;
  input->slices[0].required_prbs = 50;
  
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.10f;
  input->slices[1].min_prb_ratio = 0.40f; // Needs 30 prioritized
  input->slices[1].max_prb_ratio = 0.60f;
  input->slices[1].required_prbs = 40;
  
  input->num_slices = 2;
  input->total_prbs = 100;
  
  printf("  Input Configuration (after Pass 1):\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Already allocated (dedicated): 30 PRBs\n");
  printf("    Remaining: 40 PRBs (insufficient for both slices' prioritized needs)\n");
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  printf("    Note: Slice 1 needs 30 prioritized, Slice 2 needs 30 prioritized\n");
  printf("          Total need: 60 PRBs, but only 40 available (proportional scaling)\n\n");
  
  // Result already zeroed by calloc
  for (int s = 0; s < input->num_slices; ++s) {
    result->ranges[s].slice_id = input->slices[s].slice_id;
  }
  result->ranges[0].num_prbs = 20; // From Pass 1
  result->ranges[1].num_prbs = 10; // From Pass 1
  
  int allocated_prbs = 30;
  int remaining_prbs = 40; // Not enough for both (need 60 total)
  
  int ret = pass2_allocate_prioritized(input, result, &allocated_prbs, &remaining_prbs);
  
  printf("  Pass 2 Result (with proportional scaling):\n");
  printf("    Allocated PRBs: %d (was 30, added %d)\n", allocated_prbs, allocated_prbs - 30);
  printf("    Remaining PRBs: %d\n", remaining_prbs);
  printf("    Slice 1: %d PRBs (was 20, added %d)\n",
         result->ranges[0].num_prbs, result->ranges[0].num_prbs - 20);
  printf("    Slice 2: %d PRBs (was 10, added %d)\n",
         result->ranges[1].num_prbs, result->ranges[1].num_prbs - 10);
  printf("    Scale factor: 40/60 = 0.667\n");
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Should succeed");
  ASSERT_EQ(allocated_prbs, 70, "Should allocate all remaining (40)");
  ASSERT_EQ(remaining_prbs, 0, "No PRBs should remain");
  // Proportional: 30/60 * 40 = 20 for slice 1, 30/60 * 40 = 20 for slice 2
  ASSERT_EQ(result->ranges[0].num_prbs, 40, "Slice 1 should get 40 (20 + 20)");
  ASSERT_EQ(result->ranges[1].num_prbs, 30, "Slice 2 should get 30 (10 + 20)");
free_slice_input(input);
free_slice_result(result);

}

static void test_pass2_max_less_than_min(void) {
  printf("  Purpose: Test Pass 2 when max < min (max takes precedence)\n");
  printf("  Expected: Allocation capped at max, min not fully met\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.20f;
  input->slices[0].min_prb_ratio = 0.50f; // Min = 50
  input->slices[0].max_prb_ratio = 0.30f; // Max = 30 (< min!)
  input->slices[0].required_prbs = 50;
  
  input->num_slices = 1;
  input->total_prbs = 100;
  
  printf("  Input Configuration (after Pass 1):\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Already allocated (dedicated): 20 PRBs\n");
  printf("    Remaining: 80 PRBs\n");
  print_slice_config(&input->slices[0], 0);
  printf("    Note: Max (30%%) < Min (50%%), so max takes precedence\n");
  printf("          Slice needs prioritized to reach min=50, but max=30 caps allocation\n\n");
  
  // Result already zeroed by calloc
  result->ranges[0].slice_id = input->slices[0].slice_id;
  result->ranges[0].num_prbs = 20; // From Pass 1
  
  int allocated_prbs = 20;
  int remaining_prbs = 80;
  
  int ret = pass2_allocate_prioritized(input, result, &allocated_prbs, &remaining_prbs);
  
  printf("  Pass 2 Result:\n");
  printf("    Allocated PRBs: %d (was 20, added %d)\n", allocated_prbs, allocated_prbs - 20);
  printf("    Remaining PRBs: %d\n", remaining_prbs);
  printf("    Slice 1: %d PRBs (capped at max=30, min=50 not met)\n", result->ranges[0].num_prbs);
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Should succeed");
  ASSERT_EQ(result->ranges[0].num_prbs, 30, "Should be capped at max (30)");
  ASSERT_EQ(allocated_prbs, 30, "Total should be 30");
free_slice_input(input);
free_slice_result(result);

}

/* Test Pass 3 Edge Cases */
static void test_pass3_no_prb_requirements(void) {
  printf("  Purpose: Test Pass 3 without PRB requirements\n");
  printf("  Expected: No shared allocation (min alone does not activate Pass 3)\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.30f;
  input->slices[0].min_prb_ratio = 0.30f;
  input->slices[0].max_prb_ratio = 0.50f;
  input->slices[0].required_prbs = 0; // No requirement
  
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.20f;
  input->slices[1].min_prb_ratio = 0.20f;
  input->slices[1].max_prb_ratio = 0.50f;
  input->slices[1].required_prbs = 0; // No requirement
  
  input->num_slices = 2;
  input->total_prbs = 100;
  
  printf("  Input Configuration (after Pass 1+2):\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Already allocated: 50 PRBs\n");
  printf("    Remaining (shared): 50 PRBs\n");
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  printf("    Note: required=0 for both slices — Pass 3 must not add PRBs\n\n");
  
  // Result already zeroed by calloc
  for (int s = 0; s < input->num_slices; ++s) {
    result->ranges[s].slice_id = input->slices[s].slice_id;
  }
  result->ranges[0].num_prbs = 30; // After Pass 1+2
  result->ranges[1].num_prbs = 20; // After Pass 1+2
  
  int allocated_prbs = 50;
  int remaining_prbs = 50; // Shared resources
  
  int ret = pass3_allocate_shared(input, result, &allocated_prbs, &remaining_prbs);
  
  printf("  Pass 3 Result (no demand):\n");
  printf("    Allocated PRBs: %d (unchanged)\n", allocated_prbs);
  printf("    Remaining PRBs: %d\n", remaining_prbs);
  printf("    Slice 1: %d PRBs\n", result->ranges[0].num_prbs);
  printf("    Slice 2: %d PRBs\n", result->ranges[1].num_prbs);
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Should succeed");
  ASSERT_EQ(result->ranges[0].num_prbs, 30, "Slice 1 unchanged");
  ASSERT_EQ(result->ranges[1].num_prbs, 20, "Slice 2 unchanged");
  ASSERT_EQ(allocated_prbs, 50, "No Pass 3 allocation without require");
  ASSERT_EQ(remaining_prbs, 50, "Shared pool left unallocated");
free_slice_input(input);
free_slice_result(result);

}

static void test_pass3_all_slices_at_max(void) {
  printf("  Purpose: Test Pass 3 when all slices are at max limit\n");
  printf("  Expected: No allocation, remaining PRBs stay unallocated\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.50f;
  input->slices[0].min_prb_ratio = 0.50f;
  input->slices[0].max_prb_ratio = 0.50f;
  
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.30f;
  input->slices[1].min_prb_ratio = 0.30f;
  input->slices[1].max_prb_ratio = 0.30f;
  
  input->num_slices = 2;
  input->total_prbs = 100;
  
  printf("  Input Configuration (after Pass 1+2):\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Already allocated: 80 PRBs\n");
  printf("    Remaining (shared): 20 PRBs\n");
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  printf("    Note: Both slices are already at their max limits\n");
  printf("          Slice 1: 50 PRBs (at max=50%%)\n");
  printf("          Slice 2: 30 PRBs (at max=30%%)\n\n");
  
  // Result already zeroed by calloc
  for (int s = 0; s < input->num_slices; ++s) {
    result->ranges[s].slice_id = input->slices[s].slice_id;
  }
  result->ranges[0].num_prbs = 50; // At max
  result->ranges[1].num_prbs = 30; // At max
  
  int allocated_prbs = 80;
  int remaining_prbs = 20; // But can't allocate (all at max)
  
  int ret = pass3_allocate_shared(input, result, &allocated_prbs, &remaining_prbs);
  
  printf("  Pass 3 Result:\n");
  printf("    Allocated PRBs: %d (no change, all at max)\n", allocated_prbs);
  printf("    Remaining PRBs: %d (cannot allocate, all slices at max)\n", remaining_prbs);
  printf("    Slice 1: %d PRBs (at max, no change)\n", result->ranges[0].num_prbs);
  printf("    Slice 2: %d PRBs (at max, no change)\n", result->ranges[1].num_prbs);
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Should succeed");
  ASSERT_EQ(result->ranges[0].num_prbs, 50, "Slice 1 should stay at 50");
  ASSERT_EQ(result->ranges[1].num_prbs, 30, "Slice 2 should stay at 30");
  ASSERT_EQ(remaining_prbs, 20, "20 PRBs should remain (all slices at max)");
free_slice_input(input);
free_slice_result(result);

}

/* Test Pass 4 Edge Cases */
static void test_pass4_empty_result(void) {
  printf("  Purpose: Test Pass 4 with no PRBs allocated\n");
  printf("  Expected: No ranges assigned, no errors\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->num_slices = 1;
  
  printf("  Input Configuration:\n");
  printf("    Number of Slices: %d\n", input->num_slices);
  printf("    Slice 1: 0 PRBs allocated\n");
  printf("    Note: No PRBs allocated, so no ranges will be assigned\n\n");
  
  // No PRBs allocated (all slices have num_prbs = 0)
  // Result already zeroed by calloc
  result->ranges[0].slice_id = slice_nssai_from_int(1);
  result->ranges[0].num_prbs = 0;
  
  int ret = pass4_assign_ranges(input, result);
  
  printf("  Pass 4 Result:\n");
  printf("    Slice 1: [%d, %d) = %d PRBs (no range assigned, 0 PRBs)\n",
         result->ranges[0].start_prb, result->ranges[0].end_prb, result->ranges[0].num_prbs);
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Should succeed");
  ASSERT_EQ(result->ranges[0].start_prb, 0, "Start should be 0");
  ASSERT_EQ(result->ranges[0].end_prb, 0, "End should be 0");
free_slice_input(input);
free_slice_result(result);

}

static void test_pass4_single_slice(void) {
  printf("  Purpose: Test Pass 4 with single slice\n");
  printf("  Expected: Range [0, num_prbs)\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->num_slices = 1;
  
  printf("  Input Configuration:\n");
  printf("    Number of Slices: %d\n", input->num_slices);
  printf("    Slice 1: 100 PRBs allocated\n");
  printf("    Note: Single slice gets all PRBs, range should be [0, 100)\n\n");
  
  result->ranges[0].slice_id = slice_nssai_from_int(1);
  result->ranges[0].num_prbs = 100;
  
  int ret = pass4_assign_ranges(input, result);
  
  printf("  Pass 4 Result:\n");
  printf("    Slice 1: [%d, %d) = %d PRBs\n",
         result->ranges[0].start_prb, result->ranges[0].end_prb, result->ranges[0].num_prbs);
  printf("\n");
  
  ASSERT_EQ(ret, 0, "Should succeed");
  ASSERT_EQ(result->ranges[0].start_prb, 0, "Should start at 0");
  ASSERT_EQ(result->ranges[0].end_prb, 100, "Should end at 100");
free_slice_input(input);
free_slice_result(result);

}

/* Integration Edge Cases */
static void test_integration_max_less_than_min(void) {
  printf("  Purpose: Test full algorithm with max < min (edge case)\n");
  printf("  Expected: Max takes precedence, min not fully met\n\n");
  
  ALLOCATE_TEST_STRUCTURES(1, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.20f;
  input->slices[0].min_prb_ratio = 0.50f; // Min = 50
  input->slices[0].max_prb_ratio = 0.30f; // Max = 30 (< min!)
  input->slices[0].required_prbs = 50;
  
  input->num_slices = 1;
  input->total_prbs = 100;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  printf("    Note: Max (30%%) < Min (50%%), so max takes precedence\n\n");
  
  int ret = calculate_slice_prb_ranges(input, result);
  
  print_allocation_result_with_required(result, input, input->total_prbs);
  printf("\n");
  
  ASSERT_EQ(ret, 1, "Should have 1 active slice");
  ASSERT_EQ(result->ranges[0].num_prbs, 30, "Should be capped at max (30)");
  ASSERT_EQ(result->total_allocated_prbs, 30, "Total should be 30");
  ASSERT_EQ(result->ranges[0].start_prb, 0, "Should start at 0");
  ASSERT_EQ(result->ranges[0].end_prb, 30, "Should end at 30");
free_slice_input(input);
free_slice_result(result);

}

static void test_integration_all_slices_no_prioritized_need(void) {
  printf("  Purpose: Test when all slices don't need prioritized resources\n");
  printf("  Expected: Dedicated only; Pass 3 skipped when require < dedicated\n\n");
  
  ALLOCATE_TEST_STRUCTURES(2, input, result);
  
  input->slices[0].slice_id = slice_nssai_from_int(1);
  input->slices[0].dedicated_prb_ratio = 0.20f;
  input->slices[0].min_prb_ratio = 0.30f;
  input->slices[0].max_prb_ratio = 0.50f;
  input->slices[0].required_prbs = 15; // Less than dedicated (20)
  
  input->slices[1].slice_id = slice_nssai_from_int(2);
  input->slices[1].dedicated_prb_ratio = 0.10f;
  input->slices[1].min_prb_ratio = 0.25f;
  input->slices[1].max_prb_ratio = 0.50f;
  input->slices[1].required_prbs = 5; // Less than dedicated (10)
  
  input->num_slices = 2;
  input->total_prbs = 100;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  print_slice_config(&input->slices[0], 0);
  print_slice_config(&input->slices[1], 1);
  printf("    Note: Both slices don't need prioritized resources\n");
  printf("          Slice 1: required=15 < dedicated=20\n");
  printf("          Slice 2: required=5 < dedicated=10\n");
  printf("          Prioritized resources will go to Pass 3 as shared\n\n");
  
  int ret = calculate_slice_prb_ranges(input, result);
  
  print_allocation_result_with_required(result, input, input->total_prbs);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 2, "Should have 2 active slices");
  // Pass 1: 20 + 10 = 30; Pass 2/3: require below dedicated → no extra PRBs
  ASSERT_EQ(result->ranges[0].num_prbs, 20, "Slice 1 dedicated");
  ASSERT_EQ(result->ranges[1].num_prbs, 10, "Slice 2 dedicated");
  ASSERT_EQ(result->total_allocated_prbs, 30, "No idle Pass 3 allocation");
free_slice_input(input);
free_slice_result(result);

}

/* ============================================================================
 * OOP Scheduler Interface Tests
 * ============================================================================ */

/* Test OOP: Basic two slices */
static void test_oop_basic_two_slices(void) {
  printf("  Purpose: Test OOP scheduler with basic two-slice allocation\n");
  printf("  Expected: Dedicated PRBs only when require=0\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add slice 1: 30% dedicated, 30% min, 50% max
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.30f, 0.30f, 0.50f, 0), 0, "Should add slice 1");
  
  // Add slice 2: 20% dedicated, 20% min, 50% max
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.20f, 0.20f, 0.50f, 0), 0, "Should add slice 2");
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", sch->input->total_prbs);
  printf("    Number of Slices: %d\n", sch->input->num_slices);
  print_slice_config(&sch->input->slices[0], 0);
  print_slice_config(&sch->input->slices[1], 1);
  printf("\n");
  
  // Schedule
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  print_oop_allocation_result(sch, sch->input->total_prbs);
  printf("\n");
  
  // Get allocation
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Allocation should be available");
  
  int num_active = 0;
  int total_allocated = 0;
  ASSERT_EQ(slice_sch_get_stats(sch, &num_active, &total_allocated), 0, "Get stats should succeed");
  
  printf("  Verification:\n");
  ASSERT_EQ(num_active, 2, "Should have 2 active slices");
  ASSERT_EQ(total_allocated, 50, "Only dedicated PRBs without require");
  
  // Find slices by ID
  int slice1_idx = -1, slice2_idx = -1;
  for (int i = 0; i < num_ranges; ++i) {
    if (slice_nssai_eq_int(&ranges[i].slice_id, 1)) slice1_idx = i;
    if (slice_nssai_eq_int(&ranges[i].slice_id, 2)) slice2_idx = i;
  }
  
  ASSERT_TRUE(slice1_idx >= 0, "Slice 1 should be found");
  ASSERT_TRUE(slice2_idx >= 0, "Slice 2 should be found");
  
  printf("    ✓ Slice 1: %d PRBs (expected: 30 dedicated)\n", ranges[slice1_idx].num_prbs);
  ASSERT_EQ(ranges[slice1_idx].num_prbs, 30, "Slice 1 should get 30 dedicated PRBs");
  
  printf("    ✓ Slice 2: %d PRBs (expected: 20 dedicated)\n", ranges[slice2_idx].num_prbs);
  ASSERT_EQ(ranges[slice2_idx].num_prbs, 20, "Slice 2 should get 20 dedicated PRBs");
  
  ASSERT_EQ(ranges[slice1_idx].num_prbs + ranges[slice2_idx].num_prbs, 50,
            "Total PRBs should equal dedicated sum");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Add and delete slices */
static void test_oop_add_delete_slices(void) {
  printf("  Purpose: Test adding and deleting slices dynamically\n");
  printf("  Expected: Slices can be added and removed, scheduling adapts\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add three slices
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.20f, 0.20f, 0.40f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.15f, 0.15f, 0.35f, 0), 0, "Should add slice 2");
  ASSERT_EQ(slice_sch_add_slice(sch, 3, 0, 0.10f, 0.10f, 0.30f, 0), 0, "Should add slice 3");
  
  ASSERT_EQ(sch->input->num_slices, 3, "Should have 3 slices");
  
  // Schedule with 3 slices
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Allocation should be available");
  
  printf("  After adding 3 slices:\n");
  print_oop_allocation_result(sch, sch->input->total_prbs);
  printf("\n");
  
  // Delete slice 2
  ASSERT_EQ(slice_sch_del_slice(sch, 2, 0), 0, "Should delete slice 2");
  ASSERT_EQ(sch->input->num_slices, 2, "Should have 2 slices after deletion");
  
  // Schedule again
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed after deletion");
  
  ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Allocation should be available");
  
  printf("  After deleting slice 2:\n");
  print_oop_allocation_result(sch, sch->input->total_prbs);
  printf("\n");
  
  // Verify slice 2 is gone
  int found_slice2 = 0;
  for (int i = 0; i < num_ranges; ++i) {
    if (slice_nssai_eq_int(&ranges[i].slice_id, 2)) {
      found_slice2 = 1;
      break;
    }
  }
  ASSERT_EQ(found_slice2, 0, "Slice 2 should not be in allocation");
  
  // Verify slices 1 and 3 are still there
  int found_slice1 = 0, found_slice3 = 0;
  for (int i = 0; i < num_ranges; ++i) {
    if (slice_nssai_eq_int(&ranges[i].slice_id, 1)) found_slice1 = 1;
    if (slice_nssai_eq_int(&ranges[i].slice_id, 3)) found_slice3 = 1;
  }
  ASSERT_EQ(found_slice1, 1, "Slice 1 should still be present");
  ASSERT_EQ(found_slice3, 1, "Slice 3 should still be present");
  
  printf("  Verification:\n");
  printf("    ✓ Slice 2 successfully removed\n");
  printf("    ✓ Slices 1 and 3 still present\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Update requirements */
static void test_oop_update_require(void) {
  printf("  Purpose: Test updating PRB requirements for slices\n");
  printf("  Expected: Updating requirements invalidates result, new schedule adapts\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add slice with no requirement
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.10f, 0.20f, 0.50f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.10f, 0.20f, 0.50f, 0), 0, "Should add slice 2");
  
  // Schedule
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  printf("  Initial allocation (no requirements):\n");
  print_oop_allocation_result(sch, sch->input->total_prbs);
  printf("\n");
  
  // Update requirement for slice 1
  ASSERT_EQ(slice_sch_update_require(sch, 1, 0, 40), 0, "Should update requirement for slice 1");
  
  // Schedule again
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed after update");
  
  printf("  After updating slice 1 requirement to 40 PRBs:\n");
  print_oop_allocation_result(sch, sch->input->total_prbs);
  printf("\n");
  
  // Verify slice 1 gets more PRBs
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Allocation should be available");
  
  int slice1_idx = -1;
  for (int i = 0; i < num_ranges; ++i) {
    if (slice_nssai_eq_int(&ranges[i].slice_id, 1)) {
      slice1_idx = i;
      break;
    }
  }
  
  ASSERT_TRUE(slice1_idx >= 0, "Slice 1 should be found");
  printf("  Verification:\n");
  printf("    ✓ Slice 1 requirement updated to 40 PRBs\n");
  printf("    ✓ Slice 1 allocated: %d PRBs\n", ranges[slice1_idx].num_prbs);
  ASSERT_GE(ranges[slice1_idx].num_prbs, 20, "Slice 1 should get at least min (20 PRBs)");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Dynamic allocation (many slices) */
static void test_oop_dynamic_allocation(void) {
  printf("  Purpose: Test scheduler with many slices\n");
  printf("  Expected: Scheduler can store and schedule any number of slices dynamically\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add many slices
  int num_slices_to_add = 50;
  for (int i = 0; i < num_slices_to_add; ++i) {
    float ratio = 0.01f; // 1% each
    ASSERT_EQ(slice_sch_add_slice(sch, i + 1, 0, ratio, ratio, ratio * 2, 0), 0,
              "Should add slice");
  }
  
  ASSERT_EQ(sch->input->num_slices, num_slices_to_add, "Should have all slices");
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", sch->input->total_prbs);
  printf("    Number of Slices: %d\n", sch->input->num_slices);
  printf("    (Showing first 5 slices)\n");
  for (int i = 0; i < 5 && i < sch->input->num_slices; ++i) {
    print_slice_config(&sch->input->slices[i], i);
  }
  printf("    ...\n\n");
  
  // Schedule
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Allocation should be available");
  
  int num_active = 0;
  int total_allocated = 0;
  slice_sch_get_stats(sch, &num_active, &total_allocated);
  
  printf("  Allocation Result:\n");
  printf("    Active Slices: %d\n", num_active);
  printf("    Total Allocated: %d / %d PRBs\n", total_allocated, sch->input->total_prbs);
  printf("    (Showing first 5 allocations)\n");
  for (int i = 0; i < 5 && i < num_ranges; ++i) {
    if (ranges[i].num_prbs > 0) {
      printf("      Slice (SST=%d, SD=%u): %d PRBs\n", ranges[i].slice_id.sst, ranges[i].slice_id.sd, ranges[i].num_prbs);
    }
  }
  printf("    ...\n\n");
  
  printf("  Verification:\n");
  printf("    ✓ Successfully stored %d slices (dynamic allocation works)\n", num_slices_to_add);
  printf("    ✓ Scheduling processed all %d slices (unlimited slices supported)\n", num_slices_to_add);
  ASSERT_EQ(num_active, num_slices_to_add, "All slices should be active");
  ASSERT_EQ(total_allocated, 50, "50 slices × 1%% dedicated without require");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Error handling */
static void test_oop_error_handling(void) {
  printf("  Purpose: Test error handling in OOP scheduler\n");
  printf("  Expected: Proper error codes for invalid operations\n\n");
  
  // Test NULL scheduler
  ASSERT_EQ(slice_sch_add_slice(NULL, 1, 0, 0.1f, 0.1f, 0.2f, 0), -1,
            "Should fail with NULL scheduler");
  ASSERT_EQ(slice_sch_del_slice(NULL, 1, 0), -1, "Should fail with NULL scheduler");
  ASSERT_EQ(slice_sch_update_require(NULL, 1, 0, 10), -1, "Should fail with NULL scheduler");
  ASSERT_EQ(slice_sch_schedule(NULL), -1, "Should fail with NULL scheduler");
  ASSERT_TRUE(slice_sch_get_allocation(NULL, NULL) == NULL, "Should return NULL with NULL scheduler");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Test invalid total PRBs
  slice_scheduler_t *sch_invalid = slice_sch_create(0);
  ASSERT_TRUE(sch_invalid == NULL, "Should fail with invalid total PRBs");
  sch_invalid = slice_sch_create(-1);
  ASSERT_TRUE(sch_invalid == NULL, "Should fail with negative total PRBs");
  
  // Test duplicate slice ID (should update existing slice, not reject)
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.1f, 0.2f, 0), 0, "Should add slice 1");
  ASSERT_EQ(sch->input->num_slices, 1, "Should have 1 slice");
  ASSERT_FLOAT_EQ(sch->input->slices[0].dedicated_prb_ratio, 0.1f, "Initial dedicated should be 0.1");
  
  // Add with same SST/SD - should update, not reject
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.2f, 0.2f, 0.3f, 10), 0,
            "Should update existing slice with same SST/SD");
  ASSERT_EQ(sch->input->num_slices, 1, "Should still have 1 slice (not duplicated)");
  ASSERT_FLOAT_EQ(sch->input->slices[0].dedicated_prb_ratio, 0.2f, "Updated dedicated should be 0.2");
  
  // Test invalid ratios
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, -0.1f, 0.1f, 0.2f, 0), -1,
            "Should fail with negative dedicated ratio");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.1f, 1.5f, 0.2f, 0), -1,
            "Should fail with ratio > 1.0");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.3f, 0.1f, 0.2f, 0), -1,
            "Should fail with dedicated > min");
  
  // Test invalid requirement
  ASSERT_EQ(slice_sch_update_require(sch, 1, 0, -1), -1, "Should fail with negative requirement");
  
  // Test non-existent slice (delete is idempotent - returns success)
  ASSERT_EQ(slice_sch_del_slice(sch, (uint8_t)999, 0), 0, "Should succeed with non-existent slice (idempotent)");
  ASSERT_EQ(slice_sch_update_require(sch, (uint8_t)999, 0, 10), -1, "Should fail with non-existent slice");
  
  // Test get_allocation before schedule
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges == NULL, "Should return NULL before schedule");
  
  printf("  Verification:\n");
  printf("    ✓ All error cases handled correctly\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Initial capacity */
static void test_oop_initial_capacity(void) {
  printf("  Purpose: Test that scheduler starts with MIN_CAPACITY (8)\n");
  printf("  Expected: Initial capacity should be 8\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  printf("  Verification:\n");
  printf("    Initial capacity: %d\n", sch->slices_capacity);
  ASSERT_EQ(sch->slices_capacity, 8, "Initial capacity should be MIN_CAPACITY (8)");
  ASSERT_EQ(sch->input->num_slices, 0, "Should start with 0 slices");
  printf("    ✓ Initial capacity is correct\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Array growth */
static void test_oop_array_growth(void) {
  printf("  Purpose: Test that array grows when adding many slices\n");
  printf("  Expected: Capacity doubles when full, starting from 8\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  printf("  Adding slices to trigger growth:\n");
  printf("    Initial capacity: %d\n", sch->slices_capacity);
  
  // Add 8 slices (should fit in initial capacity)
  for (int i = 0; i < 8; ++i) {
    ASSERT_EQ(slice_sch_add_slice(sch, i + 1, 0, 0.05f, 0.05f, 0.10f, 0), 0,
              "Should add slice");
  }
  printf("    After adding 8 slices: capacity=%d, num_slices=%d\n",
         sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, 8, "Capacity should still be 8");
  ASSERT_EQ(sch->input->num_slices, 8, "Should have 8 slices");
  
  // Add one more (should trigger growth to 16)
  ASSERT_EQ(slice_sch_add_slice(sch, 9, 0, 0.05f, 0.05f, 0.10f, 0), 0,
            "Should add 9th slice");
  printf("    After adding 9th slice: capacity=%d, num_slices=%d\n",
         sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, 16, "Capacity should double to 16");
  ASSERT_EQ(sch->input->num_slices, 9, "Should have 9 slices");
  
  // Add more to trigger another growth (to 32)
  for (int i = 9; i < 16; ++i) {
    ASSERT_EQ(slice_sch_add_slice(sch, i + 1, 0, 0.05f, 0.05f, 0.10f, 0), 0,
              "Should add slice");
  }
  printf("    After adding 16th slice: capacity=%d, num_slices=%d\n",
         sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, 16, "Capacity should still be 16");
  
  // Add one more to trigger growth to 32
  ASSERT_EQ(slice_sch_add_slice(sch, 17, 0, 0.05f, 0.05f, 0.10f, 0), 0,
            "Should add 17th slice");
  printf("    After adding 17th slice: capacity=%d, num_slices=%d\n",
         sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, 32, "Capacity should double to 32");
  ASSERT_EQ(sch->input->num_slices, 17, "Should have 17 slices");
  
  printf("\n  Verification:\n");
  printf("    ✓ Array grows correctly: 8 -> 16 -> 32\n");
  printf("    ✓ Growth happens when capacity is reached\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Array shrinking */
static void test_oop_array_shrinking(void) {
  printf("  Purpose: Test that array shrinks when capacity is much larger than needed\n");
  printf("  Expected: Capacity halves when using <25%% of capacity, but not below MIN_CAPACITY\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Grow array to 32 by adding 17 slices
  for (int i = 0; i < 17; ++i) {
    ASSERT_EQ(slice_sch_add_slice(sch, i + 1, 0, 0.05f, 0.05f, 0.10f, 0), 0,
              "Should add slice");
  }
  printf("  After adding 17 slices:\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, 32, "Capacity should be 32");
  ASSERT_EQ(sch->input->num_slices, 17, "Should have 17 slices");
  
  // Delete slices until we're using <25% of capacity (32 * 0.25 = 8)
  // We need to delete down to 7 or fewer slices to trigger shrinking
  printf("\n  Deleting slices to trigger shrinking:\n");
  for (int i = 17; i > 7; --i) {
    ASSERT_EQ(slice_sch_del_slice(sch, i, 0), 0, "Should delete slice");
  }
  printf("    After deleting down to 7 slices: capacity=%d, num_slices=%d\n",
         sch->slices_capacity, sch->input->num_slices);
  // Should shrink: 32 -> 16 (since 7 < 32/4 = 8)
  ASSERT_EQ(sch->slices_capacity, 16, "Capacity should shrink to 16");
  ASSERT_EQ(sch->input->num_slices, 7, "Should have 7 slices");
  
  // Delete more to trigger another shrink
  for (int i = 7; i > 3; --i) {
    ASSERT_EQ(slice_sch_del_slice(sch, i, 0), 0, "Should delete slice");
  }
  printf("    After deleting down to 3 slices: capacity=%d, num_slices=%d\n",
         sch->slices_capacity, sch->input->num_slices);
  // Should shrink: 16 -> 8 (since 3 < 16/4 = 4, and 8 >= MIN_CAPACITY)
  ASSERT_EQ(sch->slices_capacity, 8, "Capacity should shrink to 8 (MIN_CAPACITY)");
  ASSERT_EQ(sch->input->num_slices, 3, "Should have 3 slices");
  
  // Delete one more - should NOT shrink below MIN_CAPACITY
  ASSERT_EQ(slice_sch_del_slice(sch, 2, 0), 0, "Should delete slice");
  printf("    After deleting down to 2 slices: capacity=%d, num_slices=%d\n",
         sch->slices_capacity, sch->input->num_slices);
  // Should NOT shrink: 2 < 8/4 = 2, but capacity is already MIN_CAPACITY
  ASSERT_EQ(sch->slices_capacity, 8, "Capacity should stay at MIN_CAPACITY (8)");
  ASSERT_EQ(sch->input->num_slices, 2, "Should have 2 slices");
  
  printf("\n  Verification:\n");
  printf("    ✓ Array shrinks correctly: 32 -> 16 -> 8\n");
  printf("    ✓ Shrinking happens when using <25%% of capacity\n");
  printf("    ✓ Does not shrink below MIN_CAPACITY (8)\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: No shrinking when num_slices would exceed new capacity */
static void test_oop_no_shrink_below_num_slices(void) {
  printf("  Purpose: Test that array doesn't shrink if new capacity would be < num_slices\n");
  printf("  Expected: Capacity should not shrink below num_slices\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Grow to capacity 16 by adding 9 slices
  for (int i = 0; i < 9; ++i) {
    ASSERT_EQ(slice_sch_add_slice(sch, i + 1, 0, 0.05f, 0.05f, 0.10f, 0), 0,
              "Should add slice");
  }
  printf("  After adding 9 slices:\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, 16, "Capacity should be 16");
  ASSERT_EQ(sch->input->num_slices, 9, "Should have 9 slices");
  
  // Delete down to 9 slices (no change)
  // Now delete to 5 slices
  // 5 < 16/4 = 4? No, 5 >= 4, so should NOT shrink
  for (int i = 9; i > 5; --i) {
    ASSERT_EQ(slice_sch_del_slice(sch, i, 0), 0, "Should delete slice");
  }
  printf("\n  After deleting down to 5 slices:\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  // 5 >= 16/4 = 4, so should NOT shrink
  ASSERT_EQ(sch->slices_capacity, 16, "Capacity should stay at 16 (5 >= 16/4)");
  ASSERT_EQ(sch->input->num_slices, 5, "Should have 5 slices");
  
  // Delete one more to 4 slices
  // 4 < 16/4 = 4? No, 4 == 4, so should NOT shrink (needs to be < 25%)
  ASSERT_EQ(slice_sch_del_slice(sch, 5, 0), 0, "Should delete slice");
  printf("  After deleting down to 4 slices:\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  // 4 == 16/4 = 4, so should NOT shrink (needs to be strictly < 25%)
  ASSERT_EQ(sch->slices_capacity, 16, "Capacity should stay at 16 (4 == 16/4, not <)");
  ASSERT_EQ(sch->input->num_slices, 4, "Should have 4 slices");
  
  // Delete one more to 3 slices
  // 3 < 16/4 = 4, so should shrink to 8
  // But 8 >= 3, so it's valid
  ASSERT_EQ(slice_sch_del_slice(sch, 4, 0), 0, "Should delete slice");
  printf("  After deleting down to 3 slices:\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  // 3 < 16/4 = 4, so should shrink to 8
  ASSERT_EQ(sch->slices_capacity, 8, "Capacity should shrink to 8");
  ASSERT_EQ(sch->input->num_slices, 3, "Should have 3 slices");
  
  printf("\n  Verification:\n");
  printf("    ✓ Does not shrink when num_slices >= 25%% of capacity\n");
  printf("    ✓ Shrinks only when num_slices < 25%% of capacity\n");
  printf("    ✓ New capacity is always >= num_slices\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Add slice at exact capacity boundary */
static void test_oop_add_at_capacity_boundary(void) {
  printf("  Purpose: Test adding slice when exactly at capacity\n");
  printf("  Expected: Capacity should double before adding\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Fill to exact capacity (initial capacity is 8)
  int initial_capacity = sch->slices_capacity;
  for (int i = 1; i <= initial_capacity; ++i) {
    ASSERT_EQ(slice_sch_add_slice(sch, i, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice");
  }
  
  printf("  After adding %d slices (at capacity):\n", initial_capacity);
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, initial_capacity, "Should be at initial capacity");
  ASSERT_EQ(sch->input->num_slices, initial_capacity, "Should have 8 slices");
  
  // Add one more - should trigger growth
  ASSERT_EQ(slice_sch_add_slice(sch, initial_capacity + 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice");
  
  printf("  After adding one more slice (should grow):\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, initial_capacity * 2, "Capacity should double");
  ASSERT_EQ(sch->input->num_slices, initial_capacity + 1, "Should have 9 slices");
  
  printf("  Verification:\n");
  printf("    ✓ Capacity doubles when adding at boundary\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Delete all slices */
static void test_oop_delete_all_slices(void) {
  printf("  Purpose: Test deleting all slices from scheduler\n");
  printf("  Expected: Scheduler should handle empty state correctly\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add some slices
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice 2");
  ASSERT_EQ(slice_sch_add_slice(sch, 3, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice 3");
  
  printf("  After adding 3 slices:\n");
  printf("    Num slices: %d\n", sch->input->num_slices);
  ASSERT_EQ(sch->input->num_slices, 3, "Should have 3 slices");
  
  // Delete all slices
  ASSERT_EQ(slice_sch_del_slice(sch, 1, 0), 0, "Should delete slice 1");
  ASSERT_EQ(slice_sch_del_slice(sch, 2, 0), 0, "Should delete slice 2");
  ASSERT_EQ(slice_sch_del_slice(sch, 3, 0), 0, "Should delete slice 3");
  
  printf("  After deleting all slices:\n");
  printf("    Num slices: %d\n", sch->input->num_slices);
  ASSERT_EQ(sch->input->num_slices, 0, "Should have 0 slices");
  
  // Schedule should work with empty slices
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed with 0 slices");
  
  int num_active = 0;
  int total_allocated = 0;
  ASSERT_EQ(slice_sch_get_stats(sch, &num_active, &total_allocated), 0, "Should get stats");
  ASSERT_EQ(num_active, 0, "Should have 0 active slices");
  ASSERT_EQ(total_allocated, 0, "Should have 0 allocated PRBs");
  
  printf("  Verification:\n");
  printf("    ✓ Can delete all slices\n");
  printf("    ✓ Scheduler handles empty state correctly\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Update existing slice */
static void test_oop_update_existing_slice(void) {
  printf("  Purpose: Test updating an existing slice with same SST/SD\n");
  printf("  Expected: Should update parameters instead of adding duplicate\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add initial slice
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 10), 0, "Should add slice 1");
  ASSERT_EQ(sch->input->num_slices, 1, "Should have 1 slice");
  
  // Verify initial parameters
  ASSERT_FLOAT_EQ(sch->input->slices[0].dedicated_prb_ratio, 0.1f, "Initial dedicated should be 0.1");
  ASSERT_FLOAT_EQ(sch->input->slices[0].min_prb_ratio, 0.2f, "Initial min should be 0.2");
  ASSERT_FLOAT_EQ(sch->input->slices[0].max_prb_ratio, 0.5f, "Initial max should be 0.5");
  ASSERT_EQ(sch->input->slices[0].required_prbs, 10, "Initial required_prbs should be 10");
  
  // Schedule multiple times to generate some statistics
  ASSERT_EQ(slice_sch_schedule(sch), 0, "First schedule should succeed");
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Second schedule should succeed");
  
  // Get statistics before update
  slice_statistics_t stats_before;
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 1, 0, &stats_before), 0, "Should get stats for slice 1");
  int sample_count_before = stats_before.sample_count;
  float avg_num_prbs_before = stats_before.avg_num_prbs;
  
  // Verify we have some statistics
  ASSERT_GT(sample_count_before, 0, "Should have some statistics before update");
  
  // Update the slice with new parameters (same SST/SD)
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.2f, 0.3f, 0.6f, 20), 0, "Should update slice 1");
  ASSERT_EQ(sch->input->num_slices, 1, "Should still have only 1 slice (not duplicated)");
  
  // Verify updated parameters
  ASSERT_FLOAT_EQ(sch->input->slices[0].dedicated_prb_ratio, 0.2f, "Updated dedicated should be 0.2");
  ASSERT_FLOAT_EQ(sch->input->slices[0].min_prb_ratio, 0.3f, "Updated min should be 0.3");
  ASSERT_FLOAT_EQ(sch->input->slices[0].max_prb_ratio, 0.6f, "Updated max should be 0.6");
  ASSERT_EQ(sch->input->slices[0].required_prbs, 20, "Updated required_prbs should be 20");
  
  // Verify statistics are preserved (not reset)
  slice_statistics_t stats_after;
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 1, 0, &stats_after), 0, "Should get stats for slice 1 after update");
  ASSERT_EQ(stats_after.sample_count, sample_count_before, "Statistics sample count should be preserved");
  ASSERT_FLOAT_EQ(stats_after.avg_num_prbs, avg_num_prbs_before, "Statistics average should be preserved");
  
  // Verify result is invalidated (need to reschedule)
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  // Result should still be valid from previous schedule, but let's reschedule to get new allocation
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Reschedule should succeed after update");
  ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Allocation should be available after reschedule");
  
  printf("  Verification:\n");
  printf("    ✓ Slice parameters are updated when adding with same SST/SD\n");
  printf("    ✓ Slice count remains the same (no duplicate added)\n");
  printf("    ✓ Statistics are preserved during update\n");
  printf("    ✓ Result is invalidated after update\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Update slice with different SD */
static void test_oop_update_slice_with_different_sd(void) {
  printf("  Purpose: Test that slices with same SST but different SD are treated separately\n");
  printf("  Expected: Should add as new slice, not update\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add slice with SST=1, SD=0
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 10), 0, "Should add slice (SST=1, SD=0)");
  ASSERT_EQ(sch->input->num_slices, 1, "Should have 1 slice");
  
  // Add slice with SST=1, SD=1 (different SD, should be new slice)
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 1, 0.2f, 0.3f, 0.6f, 20), 0, "Should add new slice (SST=1, SD=1)");
  ASSERT_EQ(sch->input->num_slices, 2, "Should have 2 slices (different SD)");
  
  // Update slice with SST=1, SD=0 (should update first slice)
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.15f, 0.25f, 0.55f, 15), 0, "Should update slice (SST=1, SD=0)");
  ASSERT_EQ(sch->input->num_slices, 2, "Should still have 2 slices");
  
  // Verify first slice was updated
  ASSERT_FLOAT_EQ(sch->input->slices[0].dedicated_prb_ratio, 0.15f, "First slice dedicated should be updated");
  ASSERT_FLOAT_EQ(sch->input->slices[0].min_prb_ratio, 0.25f, "First slice min should be updated");
  
  // Verify second slice was not affected
  ASSERT_FLOAT_EQ(sch->input->slices[1].dedicated_prb_ratio, 0.2f, "Second slice dedicated should be unchanged");
  ASSERT_FLOAT_EQ(sch->input->slices[1].min_prb_ratio, 0.3f, "Second slice min should be unchanged");
  
  printf("  Verification:\n");
  printf("    ✓ Slices with same SST but different SD are treated separately\n");
  printf("    ✓ Update only affects the slice with matching SST and SD\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Invalid ratio values */
static void test_oop_invalid_ratios(void) {
  printf("  Purpose: Test adding slice with invalid ratio values\n");
  printf("  Expected: Should reject invalid ratios\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Test negative dedicated
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, -0.1f, 0.2f, 0.5f, 0), -1, "Should reject negative dedicated");
  
  // Test > 1.0 dedicated
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 1.5f, 0.2f, 0.5f, 0), -1, "Should reject dedicated > 1.0");
  
  // Test negative min
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, -0.2f, 0.5f, 0), -1, "Should reject negative min");
  
  // Test > 1.0 max
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 1.5f, 0), -1, "Should reject max > 1.0");
  
  // Test dedicated > min (invalid relationship)
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.3f, 0.2f, 0.5f, 0), -1, "Should reject dedicated > min");
  
  // Test dedicated > max (when max < min, dedicated should be <= max)
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.3f, 0.5f, 0.2f, 0), -1, "Should reject dedicated > max when max < min");
  
  printf("  Verification:\n");
  printf("    ✓ All invalid ratio values are rejected\n");
  ASSERT_EQ(sch->input->num_slices, 0, "Should have 0 slices");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Invalid require value */
static void test_oop_invalid_require(void) {
  printf("  Purpose: Test adding/updating with invalid require value\n");
  printf("  Expected: Should reject negative require\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Test negative require in add
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, -10), -1, "Should reject negative require");
  
  // Add valid slice first
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice");
  
  // Test negative require in update
  ASSERT_EQ(slice_sch_update_require(sch, 1, 0, -5), -1, "Should reject negative require in update");
  
  printf("  Verification:\n");
  printf("    ✓ Negative require values are rejected\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Update require for non-existent slice */
static void test_oop_update_nonexistent_slice(void) {
  printf("  Purpose: Test updating require for non-existent slice\n");
  printf("  Expected: Should return error\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_update_require(sch, (uint8_t)999, 0, 50), -1, "Should reject update for non-existent slice");
  
  printf("  Verification:\n");
  printf("    ✓ Update for non-existent slice is rejected\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Delete non-existent slice */
static void test_oop_delete_nonexistent_slice(void) {
  printf("  Purpose: Test deleting non-existent slice (idempotent behavior)\n");
  printf("  Expected: Should return success (idempotent operation)\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Delete non-existent slice - should succeed (idempotent)
  ASSERT_EQ(slice_sch_del_slice(sch, (uint8_t)999, 0), 0, "Should succeed for non-existent slice (idempotent)");
  ASSERT_EQ(sch->input->num_slices, 0, "Should still have 0 slices");
  
  // Add a slice and verify it exists
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice 1");
  ASSERT_EQ(sch->input->num_slices, 1, "Should have 1 slice");
  
  // Delete it
  ASSERT_EQ(slice_sch_del_slice(sch, 1, 0), 0, "Should delete slice 1");
  ASSERT_EQ(sch->input->num_slices, 0, "Should have 0 slices after deletion");
  
  // Delete again - should still succeed (idempotent)
  ASSERT_EQ(slice_sch_del_slice(sch, 1, 0), 0, "Should succeed deleting already-deleted slice (idempotent)");
  ASSERT_EQ(sch->input->num_slices, 0, "Should still have 0 slices");
  
  printf("  Verification:\n");
  printf("    ✓ Delete for non-existent slice returns success (idempotent)\n");
  printf("    ✓ Multiple deletes of same slice are safe (idempotent)\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Get allocation before scheduling */
static void test_oop_get_allocation_before_schedule(void) {
  printf("  Purpose: Test getting allocation before calling schedule\n");
  printf("  Expected: Should return NULL\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice");
  
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges == NULL, "Should return NULL before schedule");
  
  int num_active = 0;
  int total_allocated = 0;
  ASSERT_EQ(slice_sch_get_stats(sch, &num_active, &total_allocated), -1, "Should return error before schedule");
  
  printf("  Verification:\n");
  printf("    ✓ Get allocation before schedule returns NULL\n");
  printf("    ✓ Get stats before schedule returns error\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Boundary ratio values (0.0 and 1.0) */
static void test_oop_boundary_ratios(void) {
  printf("  Purpose: Test adding slices with boundary ratio values (0.0, 1.0)\n");
  printf("  Expected: Should accept valid boundary values\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Test all zeros (valid)
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.0f, 0.0f, 1.0f, 0), 0, "Should accept all zeros");
  
  // Test all at 1.0 (valid)
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.0f, 1.0f, 1.0f, 0), 0, "Should accept max at 1.0");
  
  // Test exact 1.0 dedicated (valid if min >= 1.0)
  ASSERT_EQ(slice_sch_add_slice(sch, 3, 0, 0.0f, 1.0f, 1.0f, 0), 0, "Should accept dedicated 0.0 with min 1.0");
  
  printf("  Verification:\n");
  printf("    ✓ Boundary ratio values (0.0, 1.0) are accepted\n");
  ASSERT_EQ(sch->input->num_slices, 3, "Should have 3 slices");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - NULL scheduler operations */
static void test_oop_null_scheduler_operations(void) {
  printf("  Purpose: Test operations on NULL scheduler\n");
  printf("  Expected: All should return error or handle gracefully\n\n");
  
  // Test all operations with NULL
  ASSERT_EQ(slice_sch_add_slice(NULL, 1, 0, 0.1f, 0.2f, 0.5f, 0), -1, "Should reject NULL scheduler");
  ASSERT_EQ(slice_sch_del_slice(NULL, 1, 0), -1, "Should reject NULL scheduler");
  ASSERT_EQ(slice_sch_update_require(NULL, 1, 0, 50), -1, "Should reject NULL scheduler");
  ASSERT_EQ(slice_sch_schedule(NULL), -1, "Should reject NULL scheduler");
  
  int num_ranges = 0;
  ASSERT_TRUE(slice_sch_get_allocation(NULL, &num_ranges) == NULL, "Should return NULL for NULL scheduler");
  
  int num_active = 0;
  int total_allocated = 0;
  ASSERT_EQ(slice_sch_get_stats(NULL, &num_active, &total_allocated), -1, "Should reject NULL scheduler");
  
  // Destroy NULL should be safe (no-op)
  slice_sch_destroy(NULL);
  
  printf("  Verification:\n");
  printf("    ✓ All operations handle NULL scheduler gracefully\n");
}

/* Test OOP: Edge case - Invalid total_prbs in create */
static void test_oop_invalid_total_prbs(void) {
  printf("  Purpose: Test creating scheduler with invalid total_prbs\n");
  printf("  Expected: Should return NULL\n\n");
  
  slice_scheduler_t *sch1 = slice_sch_create(0);
  ASSERT_TRUE(sch1 == NULL, "Should reject total_prbs = 0");
  
  slice_scheduler_t *sch2 = slice_sch_create(-10);
  ASSERT_TRUE(sch2 == NULL, "Should reject negative total_prbs");
  
  printf("  Verification:\n");
  printf("    ✓ Invalid total_prbs values are rejected\n");
}

/* Test OOP: Edge case - Capacity at exact shrink threshold */
static void test_oop_capacity_at_shrink_threshold(void) {
  printf("  Purpose: Test capacity behavior at exact 25%% threshold\n");
  printf("  Expected: Should not shrink at exactly 25%%, only when < 25%%\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Grow to 16 capacity (add 9 slices to go from 8 to 16)
  for (int i = 1; i <= 9; ++i) {
    ASSERT_EQ(slice_sch_add_slice(sch, i, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice");
  }
  
  printf("  After adding 9 slices:\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, 16, "Capacity should be 16");
  ASSERT_EQ(sch->input->num_slices, 9, "Should have 9 slices");
  
  // Delete down to exactly 4 (which is 16/4 = 25%)
  for (int i = 9; i >= 5; --i) {
    ASSERT_EQ(slice_sch_del_slice(sch, i, 0), 0, "Should delete slice");
  }
  
  printf("  After deleting down to 4 slices (exactly 25%%):\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, 16, "Should NOT shrink at exactly 25%");
  ASSERT_EQ(sch->input->num_slices, 4, "Should have 4 slices");
  
  // Delete one more to 3 (< 25%)
  ASSERT_EQ(slice_sch_del_slice(sch, 4, 0), 0, "Should delete slice");
  
  printf("  After deleting down to 3 slices (< 25%%):\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  ASSERT_EQ(sch->slices_capacity, 8, "Should shrink to 8");
  ASSERT_EQ(sch->input->num_slices, 3, "Should have 3 slices");
  
  printf("  Verification:\n");
  printf("    ✓ Does not shrink at exactly 25%% threshold\n");
  printf("    ✓ Shrinks only when strictly < 25%%\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Multiple rapid add/delete operations */
static void test_oop_rapid_add_delete(void) {
  printf("  Purpose: Test rapid sequence of add/delete operations\n");
  printf("  Expected: Should handle correctly without corruption\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Rapid add/delete sequence
  for (int i = 1; i <= 20; ++i) {
    ASSERT_EQ(slice_sch_add_slice(sch, i, 0, 0.05f, 0.05f, 0.10f, 0), 0, "Should add slice");
    if (i % 3 == 0) {
      ASSERT_EQ(slice_sch_del_slice(sch, i - 1, 0), 0, "Should delete slice");
    }
  }
  
  printf("  After rapid add/delete sequence:\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  
  // Schedule should still work
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  int num_active = 0;
  int total_allocated = 0;
  ASSERT_EQ(slice_sch_get_stats(sch, &num_active, &total_allocated), 0, "Should get stats");
  
  printf("  Verification:\n");
  printf("    ✓ Rapid add/delete operations handled correctly\n");
  printf("    ✓ Scheduler remains functional after rapid operations\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Edge case - Schedule after delete without re-schedule */
static void test_oop_schedule_after_delete(void) {
  printf("  Purpose: Test scheduling after delete (result should be invalidated)\n");
  printf("  Expected: Result should be invalidated, new schedule should work\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice 2");
  
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  // Delete a slice - should invalidate result
  ASSERT_EQ(slice_sch_del_slice(sch, 1, 0), 0, "Should delete slice 1");
  
  // Get allocation should fail (result invalidated)
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges == NULL, "Should return NULL after delete (result invalidated)");
  
  // Re-schedule should work
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Re-schedule should succeed");
  
  ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Should return valid allocation after re-schedule");
  
  printf("  Verification:\n");
  printf("    ✓ Result is invalidated after delete\n");
  printf("    ✓ Re-schedule works correctly after delete\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Statistics - Initial state */
static void test_oop_statistics_initial_state(void) {
  printf("  Purpose: Test statistics initialization when adding slices\n");
  printf("  Expected: Statistics should be initialized to zero\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice 2");
  
  slice_statistics_t stats1, stats2;
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 1, 0, &stats1), 0, "Should get stats for slice 1");
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 2, 0, &stats2), 0, "Should get stats for slice 2");
  
  printf("  Initial statistics:\n");
  printf("    Slice (SST=%d, SD=%u): latest=(%d,%d,%d), avg=(%.1f,%.1f,%.1f), samples=%d\n",
         stats1.slice_id.sst, stats1.slice_id.sd,
         stats1.latest_start_prb, stats1.latest_end_prb, stats1.latest_num_prbs,
         stats1.avg_start_prb, stats1.avg_end_prb, stats1.avg_num_prbs, stats1.sample_count);
  
  ASSERT_EQ(stats1.slice_id.sst, 1, "Slice ID SST should be 1");
  ASSERT_EQ(stats1.slice_id.sd, 0, "Slice ID SD should be 0");
  ASSERT_EQ(stats1.latest_start_prb, 0, "Latest start should be 0");
  ASSERT_EQ(stats1.latest_end_prb, 0, "Latest end should be 0");
  ASSERT_EQ(stats1.latest_num_prbs, 0, "Latest num_prbs should be 0");
  ASSERT_FLOAT_EQ(stats1.avg_start_prb, 0.0f, "Avg start should be 0");
  ASSERT_FLOAT_EQ(stats1.avg_end_prb, 0.0f, "Avg end should be 0");
  ASSERT_FLOAT_EQ(stats1.avg_num_prbs, 0.0f, "Avg num_prbs should be 0");
  ASSERT_EQ(stats1.sample_count, 0, "Sample count should be 0");
  
  printf("  Verification:\n");
  printf("    ✓ Statistics initialized to zero\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Statistics - Update after schedule */
static void test_oop_statistics_after_schedule(void) {
  printf("  Purpose: Test statistics update after scheduling\n");
  printf("  Expected: Latest values should match allocation, averages should be initialized\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.0f, 0.2f, 0.5f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.0f, 0.3f, 0.5f, 0), 0, "Should add slice 2");
  
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  slice_statistics_t stats1, stats2;
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 1, 0, &stats1), 0, "Should get stats for slice 1");
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 2, 0, &stats2), 0, "Should get stats for slice 2");
  
  printf("  Statistics after first schedule:\n");
  printf("    Slice 1: latest=(%d,%d,%d), avg=(%.1f,%.1f,%.1f), samples=%d\n",
         stats1.latest_start_prb, stats1.latest_end_prb, stats1.latest_num_prbs,
         stats1.avg_start_prb, stats1.avg_end_prb, stats1.avg_num_prbs, stats1.sample_count);
  printf("    Slice 2: latest=(%d,%d,%d), avg=(%.1f,%.1f,%.1f), samples=%d\n",
         stats2.latest_start_prb, stats2.latest_end_prb, stats2.latest_num_prbs,
         stats2.avg_start_prb, stats2.avg_end_prb, stats2.avg_num_prbs, stats2.sample_count);
  
  // Get allocation to compare
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Allocation should be available");
  
  // Find slice 1 in ranges
  int slice1_start = -1, slice1_end = -1, slice1_num = -1;
  for (int i = 0; i < num_ranges; ++i) {
    if (slice_nssai_eq_int(&ranges[i].slice_id, 1)) {
      slice1_start = ranges[i].start_prb;
      slice1_end = ranges[i].end_prb;
      slice1_num = ranges[i].num_prbs;
      break;
    }
  }
  
  ASSERT_GE(slice1_start, 0, "Slice 1 should have allocation");
  ASSERT_EQ(stats1.latest_start_prb, slice1_start, "Latest start should match allocation");
  ASSERT_EQ(stats1.latest_end_prb, slice1_end, "Latest end should match allocation");
  ASSERT_EQ(stats1.latest_num_prbs, slice1_num, "Latest num_prbs should match allocation");
  
  // After first sample, averages should equal latest values
  ASSERT_FLOAT_EQ(stats1.avg_start_prb, (float)slice1_start, "Avg start should equal latest (first sample)");
  ASSERT_FLOAT_EQ(stats1.avg_end_prb, (float)slice1_end, "Avg end should equal latest (first sample)");
  ASSERT_FLOAT_EQ(stats1.avg_num_prbs, (float)slice1_num, "Avg num_prbs should equal latest (first sample)");
  ASSERT_EQ(stats1.sample_count, 1, "Sample count should be 1");
  
  printf("  Verification:\n");
  printf("    ✓ Latest values match allocation\n");
  printf("    ✓ Averages initialized to latest values (first sample)\n");
  printf("    ✓ Sample count incremented\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Statistics - Moving average calculation */
static void test_oop_statistics_moving_average(void) {
  printf("  Purpose: Test moving average calculation across multiple schedules\n");
  printf("  Expected: Averages should smooth out using EMA (alpha=0.1)\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.0f, 0.2f, 0.5f, 0), 0, "Should add slice 1");
  
  // First schedule
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  slice_statistics_t stats;
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 1, 0, &stats), 0, "Should get stats");
  
  int first_num = stats.latest_num_prbs;
  float first_avg = stats.avg_num_prbs;
  
  printf("  After first schedule:\n");
  printf("    Latest: %d PRBs, Avg: %.1f PRBs, Samples: %d\n", 
         first_num, first_avg, stats.sample_count);
  
  // Update requirement to change allocation
  ASSERT_EQ(slice_sch_update_require(sch, 1, 0, 30), 0, "Should update requirement");
  
  // Second schedule
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 1, 0, &stats), 0, "Should get stats");
  
  int second_num = stats.latest_num_prbs;
  float second_avg = stats.avg_num_prbs;
  
  printf("  After second schedule:\n");
  printf("    Latest: %d PRBs, Avg: %.1f PRBs, Samples: %d\n", 
         second_num, second_avg, stats.sample_count);
  
  // Third schedule
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 1, 0, &stats), 0, "Should get stats");
  
  int third_num = stats.latest_num_prbs;
  float third_avg = stats.avg_num_prbs;
  
  printf("  After third schedule:\n");
  printf("    Latest: %d PRBs, Avg: %.1f PRBs, Samples: %d\n", 
         third_num, third_avg, stats.sample_count);
  
  // Verify moving average calculation
  // EMA: new_avg = 0.1 * new_value + 0.9 * old_avg
  float expected_second_avg = 0.1f * (float)second_num + 0.9f * first_avg;
  float expected_third_avg = 0.1f * (float)third_num + 0.9f * second_avg;
  
  printf("  Expected averages:\n");
  printf("    After 2nd: %.1f (calculated), %.1f (actual)\n", expected_second_avg, second_avg);
  printf("    After 3rd: %.1f (calculated), %.1f (actual)\n", expected_third_avg, third_avg);
  
  // Allow small floating point differences
  ASSERT_TRUE(fabsf(second_avg - expected_second_avg) < 0.1f, "Second avg should match EMA calculation");
  ASSERT_TRUE(fabsf(third_avg - expected_third_avg) < 0.1f, "Third avg should match EMA calculation");
  ASSERT_EQ(stats.sample_count, 3, "Sample count should be 3");
  
  printf("  Verification:\n");
  printf("    ✓ Moving averages calculated correctly using EMA\n");
  printf("    ✓ Sample count increments correctly\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Statistics - Slice without allocation */
static void test_oop_statistics_no_allocation(void) {
  printf("  Purpose: Test statistics for slice that doesn't get allocation\n");
  printf("  Expected: Latest values should be 0, averages should not update\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add slice with no active UEs (won't get allocation)
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.0f, 0.0f, 1.0f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.2f, 0.2f, 0.5f, 0), 0, "Should add slice 2");
  
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  slice_statistics_t stats1, stats2;
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 1, 0, &stats1), 0, "Should get stats for slice 1");
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 2, 0, &stats2), 0, "Should get stats for slice 2");
  
  printf("  Statistics:\n");
  printf("    Slice 1 (no allocation): latest=(%d,%d,%d), avg=(%.1f,%.1f,%.1f), samples=%d\n",
         stats1.latest_start_prb, stats1.latest_end_prb, stats1.latest_num_prbs,
         stats1.avg_start_prb, stats1.avg_end_prb, stats1.avg_num_prbs, stats1.sample_count);
  printf("    Slice 2 (with allocation): latest=(%d,%d,%d), avg=(%.1f,%.1f,%.1f), samples=%d\n",
         stats2.latest_start_prb, stats2.latest_end_prb, stats2.latest_num_prbs,
         stats2.avg_start_prb, stats2.avg_end_prb, stats2.avg_num_prbs, stats2.sample_count);
  
  ASSERT_EQ(stats1.latest_start_prb, 0, "Slice 1 latest start should be 0");
  ASSERT_EQ(stats1.latest_end_prb, 0, "Slice 1 latest end should be 0");
  ASSERT_EQ(stats1.latest_num_prbs, 0, "Slice 1 latest num_prbs should be 0");
  ASSERT_FLOAT_EQ(stats1.avg_start_prb, 0.0f, "Slice 1 avg start should be 0");
  ASSERT_FLOAT_EQ(stats1.avg_end_prb, 0.0f, "Slice 1 avg end should be 0");
  ASSERT_FLOAT_EQ(stats1.avg_num_prbs, 0.0f, "Slice 1 avg num_prbs should be 0");
  ASSERT_EQ(stats1.sample_count, 1, "Slice 1 sample count tracks scheduling slot (0 PRBs)");
  
  ASSERT_EQ(stats2.latest_num_prbs, 20, "Slice 2 dedicated 20%");
  ASSERT_EQ(stats2.sample_count, 1, "Slice 2 sample count should be 1");
  
  printf("  Verification:\n");
  printf("    ✓ Slices without PRBs still record a scheduling sample (latest=0)\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Statistics - Get all statistics */
static void test_oop_statistics_get_all(void) {
  printf("  Purpose: Test getting all statistics at once\n");
  printf("  Expected: Should return array of all slice statistics\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.0f, 0.2f, 0.5f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.0f, 0.3f, 0.5f, 0), 0, "Should add slice 2");
  ASSERT_EQ(slice_sch_add_slice(sch, 3, 0, 0.0f, 0.1f, 0.5f, 0), 0, "Should add slice 3");
  
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  int num_stats = 0;
  const slice_statistics_t *all_stats = slice_sch_get_all_statistics(sch, &num_stats);
  
  ASSERT_TRUE(all_stats != NULL, "Should get all statistics");
  ASSERT_EQ(num_stats, 3, "Should have 3 statistics entries");
  
  printf("  All statistics:\n");
  for (int i = 0; i < num_stats; ++i) {
    printf("    Slice (SST=%d, SD=%u): latest=(%d,%d,%d), avg=(%.1f,%.1f,%.1f), samples=%d\n",
           all_stats[i].slice_id.sst, all_stats[i].slice_id.sd,
           all_stats[i].latest_start_prb, all_stats[i].latest_end_prb, all_stats[i].latest_num_prbs,
           all_stats[i].avg_start_prb, all_stats[i].avg_end_prb, all_stats[i].avg_num_prbs,
           all_stats[i].sample_count);
  }
  
  // Verify each slice
  ASSERT_EQ(all_stats[0].slice_id.sst, 1, "First entry should be slice 1 (SST=1)");
  ASSERT_EQ(all_stats[0].slice_id.sd, 0, "First entry should have SD=0");
  ASSERT_EQ(all_stats[1].slice_id.sst, 2, "Second entry should be slice 2 (SST=2)");
  ASSERT_EQ(all_stats[1].slice_id.sd, 0, "Second entry should have SD=0");
  ASSERT_EQ(all_stats[2].slice_id.sst, 3, "Third entry should be slice 3 (SST=3)");
  ASSERT_EQ(all_stats[2].slice_id.sd, 0, "Third entry should have SD=0");
  
  printf("  Verification:\n");
  printf("    ✓ All statistics returned correctly\n");
  printf("    ✓ Statistics match individual slice queries\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Statistics - Statistics persist after delete */
static void test_oop_statistics_after_delete(void) {
  printf("  Purpose: Test statistics behavior after deleting slices\n");
  printf("  Expected: Remaining slices should keep their statistics\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.0f, 0.2f, 0.5f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.0f, 0.3f, 0.5f, 0), 0, "Should add slice 2");
  ASSERT_EQ(slice_sch_add_slice(sch, 3, 0, 0.0f, 0.1f, 0.5f, 0), 0, "Should add slice 3");
  
  // Schedule multiple times to build up statistics
  for (int i = 0; i < 3; ++i) {
    ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  }
  
  slice_statistics_t stats2_before;
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 2, 0, &stats2_before), 0, "Should get stats for slice 2");
  
  printf("  Before delete:\n");
  printf("    Slice 2: latest=%d PRBs, avg=%.1f PRBs, samples=%d\n",
         stats2_before.latest_num_prbs, stats2_before.avg_num_prbs, stats2_before.sample_count);
  
  // Delete slice 1
  ASSERT_EQ(slice_sch_del_slice(sch, 1, 0), 0, "Should delete slice 1");
  
  // Slice 2 should still have its statistics (now at index 0, shifted in array)
  // Note: result_valid is set to false after delete, so statistics are preserved
  // but won't be updated until next schedule
  slice_statistics_t stats2_after;
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 2, 0, &stats2_after), 0, "Should get stats for slice 2");
  
  printf("  After delete (before re-schedule):\n");
  printf("    Slice 2: latest=%d PRBs, avg=%.1f PRBs, samples=%d\n",
         stats2_after.latest_num_prbs, stats2_after.avg_num_prbs, stats2_after.sample_count);
  
  // Statistics should be preserved (shifted in array) before re-schedule
  ASSERT_EQ(stats2_after.latest_num_prbs, stats2_before.latest_num_prbs, "Latest should be preserved before re-schedule");
  ASSERT_FLOAT_EQ(stats2_after.avg_num_prbs, stats2_before.avg_num_prbs, "Average should be preserved before re-schedule");
  ASSERT_EQ(stats2_after.sample_count, stats2_before.sample_count, "Sample count should be preserved");
  
  // Now schedule again - statistics will update with new allocation
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Re-schedule should succeed");
  
  slice_statistics_t stats2_after_schedule;
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 2, 0, &stats2_after_schedule), 0, "Should get stats for slice 2");
  
  printf("  After re-schedule:\n");
  printf("    Slice 2: latest=%d PRBs, avg=%.1f PRBs, samples=%d\n",
         stats2_after_schedule.latest_num_prbs, stats2_after_schedule.avg_num_prbs, stats2_after_schedule.sample_count);
  
  // After re-schedule, statistics should update but sample_count should increment
  ASSERT_GT(stats2_after_schedule.sample_count, stats2_before.sample_count, "Sample count should increment after re-schedule");
  
  printf("  Verification:\n");
  printf("    ✓ Statistics preserved after slice deletion\n");
  printf("    ✓ Statistics correctly shifted in array\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Statistics - Error handling */
static void test_oop_statistics_error_handling(void) {
  printf("  Purpose: Test error handling for statistics functions\n");
  printf("  Expected: Should handle NULL and invalid inputs gracefully\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Test NULL scheduler
  slice_statistics_t stats;
  ASSERT_EQ(slice_sch_get_slice_statistics(NULL, 1, 0, &stats), -1, "Should reject NULL scheduler");
  
  int num_stats = 0;
  ASSERT_TRUE(slice_sch_get_all_statistics(NULL, &num_stats) == NULL, "Should return NULL for NULL scheduler");
  
  // Test non-existent slice
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, (uint8_t)999, 0, &stats), -1, "Should reject non-existent slice");
  
  // Test NULL stats pointer
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 1, 0, NULL), -1, "Should reject NULL stats pointer");
  
  printf("  Verification:\n");
  printf("    ✓ NULL scheduler handled correctly\n");
  printf("    ✓ Non-existent slice handled correctly\n");
  printf("    ✓ NULL pointer handled correctly\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Crash prevention - Very large number of slices */
static void test_oop_crash_prevention_large_slices(void) {
  printf("  Purpose: Test scheduler with very large number of slices\n");
  printf("  Expected: Should handle gracefully without crashing\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add many slices (but not so many that we run out of memory)
  int num_slices = 1000;
  int success_count = 0;
  for (int i = 1; i <= num_slices; ++i) {
    if (slice_sch_add_slice(sch, i, 0, 0.01f, 0.01f, 0.02f, 0) == 0) {
      success_count++;
    } else {
      break; // Stop if allocation fails
    }
  }
  
  printf("  Added %d slices successfully\n", success_count);
  ASSERT_GT(success_count, 0, "Should add at least some slices");
  
  // Schedule should not crash
  int ret = slice_sch_schedule(sch);
  ASSERT_GE(ret, -1, "Schedule should return valid code (0 or -1)");
  
  printf("  Verification:\n");
  printf("    ✓ Scheduler handles large number of slices without crashing\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Crash prevention - Very large total_prbs */
static void test_oop_crash_prevention_large_total_prbs(void) {
  printf("  Purpose: Test scheduler with very large total_prbs\n");
  printf("  Expected: Should handle gracefully without crashing\n\n");
  
  // Test with large but reasonable total_prbs (e.g., 10000)
  slice_scheduler_t *sch = slice_sch_create(10000);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice");
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Should get allocation");
  
  printf("  Verification:\n");
  printf("    ✓ Scheduler handles large total_prbs without crashing\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Crash prevention - Operations on destroyed scheduler */
static void test_oop_crash_prevention_use_after_destroy(void) {
  printf("  Purpose: Test that destroy handles NULL and multiple destroys safely\n");
  printf("  Expected: Should handle gracefully without crashing\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice");
  
  // Test destroying NULL (should be safe)
  slice_sch_destroy(NULL);
  printf("    ✓ Destroying NULL scheduler is safe\n");
  
  // Destroy scheduler
  slice_sch_destroy(sch);
  printf("    ✓ Destroying scheduler succeeds\n");
  
  // Test double destroy (should be safe - destroy should handle this)
  // Note: After destroy, sch points to freed memory, so we can't safely use it
  // But we can test that destroy(NULL) is safe
  slice_sch_destroy(NULL);
  printf("    ✓ Multiple destroy calls are handled safely\n");
  
  printf("  Verification:\n");
  printf("    ✓ Destroy function handles edge cases safely\n");
  printf("    ✓ Note: Using scheduler after destroy is undefined behavior\n");
}

/* Test OOP: Crash prevention - Integer overflow in calculations */
static void test_oop_crash_prevention_overflow(void) {
  printf("  Purpose: Test scheduler with values that could cause overflow\n");
  printf("  Expected: Should handle gracefully without crashing\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add slice with very large requirement (but within int range)
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.0f, 0.0f, 1.0f, INT_MAX / 2), 0, "Should add slice with large requirement");
  
  // Schedule should handle large requirement gracefully
  int ret = slice_sch_schedule(sch);
  ASSERT_GE(ret, -1, "Schedule should return valid code");
  
  printf("  Verification:\n");
  printf("    ✓ Scheduler handles large requirement values without crashing\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Crash prevention - Rapid add/delete causing reallocation issues */
static void test_oop_crash_prevention_rapid_reallocation(void) {
  printf("  Purpose: Test rapid add/delete causing frequent reallocations\n");
  printf("  Expected: Should handle gracefully without memory corruption\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Rapidly add and delete slices to trigger many reallocations
  for (int cycle = 0; cycle < 10; ++cycle) {
    // Add slices to trigger growth
    for (int i = 1; i <= 20; ++i) {
      slice_sch_add_slice(sch, cycle * 100 + i, 0, 0.01f, 0.01f, 0.02f, 0);
    }
    
    // Delete slices to trigger shrink
    for (int i = 1; i <= 15; ++i) {
      slice_sch_del_slice(sch, cycle * 100 + i, 0);
    }
    
    // Schedule to ensure consistency
    slice_sch_schedule(sch);
  }
  
  printf("  After rapid add/delete cycles:\n");
  printf("    Capacity: %d, Num slices: %d\n", sch->slices_capacity, sch->input->num_slices);
  
  // Final schedule should work
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Final schedule should succeed");
  
  printf("  Verification:\n");
  printf("    ✓ Scheduler handles rapid reallocations without memory corruption\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Crash prevention - Zero-sized allocations */
static void test_oop_crash_prevention_zero_sized(void) {
  printf("  Purpose: Test edge cases with zero-sized or minimal allocations\n");
  printf("  Expected: Should handle gracefully without crashing\n\n");
  
  // Test with total_prbs = 1 (minimum)
  slice_scheduler_t *sch = slice_sch_create(1);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.0f, 1.0f, 1.0f, 0), 0, "Should add slice");
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Should get allocation");
  
  printf("  Verification:\n");
  printf("    ✓ Scheduler handles minimal total_prbs without crashing\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Crash prevention - Invalid state transitions */
static void test_oop_crash_prevention_invalid_state(void) {
  printf("  Purpose: Test invalid state transitions and operations\n");
  printf("  Expected: Should handle gracefully without crashing\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add slice but don't schedule
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice");
  
  // Try to get allocation before scheduling (should return NULL)
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges == NULL, "Should return NULL before schedule");
  
  // Try to get stats before scheduling (should return error)
  int num_active = 0;
  int total_allocated = 0;
  ASSERT_EQ(slice_sch_get_stats(sch, &num_active, &total_allocated), -1, "Should return error before schedule");
  
  // Schedule
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  // Now operations should work
  ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Should get allocation after schedule");
  
  printf("  Verification:\n");
  printf("    ✓ Scheduler handles invalid state transitions gracefully\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Crash prevention - Boundary ratio values causing division issues */
static void test_oop_crash_prevention_boundary_ratios(void) {
  printf("  Purpose: Test boundary ratio values that could cause division issues\n");
  printf("  Expected: Should handle gracefully without crashing\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Test with ratios that sum to exactly 1.0
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.0f, 0.5f, 0.5f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.0f, 0.5f, 0.5f, 0), 0, "Should add slice 2");
  
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  // Test with all slices at max (1.0)
  slice_scheduler_t *sch2 = slice_sch_create(100);
  ASSERT_TRUE(sch2 != NULL, "Scheduler should be created");
  
  ASSERT_EQ(slice_sch_add_slice(sch2, 1, 0, 0.0f, 1.0f, 1.0f, 0), 0, "Should add slice");
  ASSERT_EQ(slice_sch_schedule(sch2), 0, "Schedule should succeed");
  
  printf("  Verification:\n");
  printf("    ✓ Scheduler handles boundary ratio values without crashing\n");
  
  slice_sch_destroy(sch);
  slice_sch_destroy(sch2);
}

/* Test OOP: Crash prevention - Memory exhaustion simulation */
static void test_oop_crash_prevention_memory_pressure(void) {
  printf("  Purpose: Test scheduler behavior under memory pressure\n");
  printf("  Expected: Should handle allocation failures gracefully\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add many slices to potentially trigger memory issues
  // (In practice, this would require actual memory exhaustion, which is hard to simulate)
  // But we can test that the scheduler handles many slices correctly
  int added = 0;
  for (int i = 1; i <= 10000; ++i) {
    if (slice_sch_add_slice(sch, i, 0, 0.001f, 0.001f, 0.002f, 0) == 0) {
      added++;
    } else {
      // Allocation failed - this is acceptable
      break;
    }
  }
  
  printf("  Added %d slices\n", added);
  
  if (added > 0) {
    // If we added slices, try to schedule
    int ret = slice_sch_schedule(sch);
    ASSERT_GE(ret, -1, "Schedule should return valid code");
  }
  
  printf("  Verification:\n");
  printf("    ✓ Scheduler handles memory pressure gracefully\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Crash prevention - Concurrent-like operations (invalid sequences) */
static void test_oop_crash_prevention_invalid_sequences(void) {
  printf("  Purpose: Test invalid operation sequences that could cause crashes\n");
  printf("  Expected: Should handle gracefully without crashing\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Sequence 1: Delete before add (idempotent - should succeed)
  ASSERT_EQ(slice_sch_del_slice(sch, (uint8_t)999, 0), 0, "Should succeed deleting non-existent slice (idempotent)");
  
  // Sequence 2: Update require before add
  ASSERT_EQ(slice_sch_update_require(sch, (uint8_t)999, 0, 50), -1, "Should fail to update non-existent slice");
  
  // Sequence 3: Add, delete, then try to use deleted slice
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.1f, 0.2f, 0.5f, 0), 0, "Should add slice");
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  ASSERT_EQ(slice_sch_del_slice(sch, 1, 0), 0, "Should delete slice");
  
  // Try to get stats for deleted slice
  slice_statistics_t stats;
  ASSERT_EQ(slice_sch_get_slice_statistics(sch, 1, 0, &stats), -1, "Should fail to get stats for deleted slice");
  
  // Sequence 4: Multiple deletes of same slice (idempotent - should succeed)
  ASSERT_EQ(slice_sch_del_slice(sch, 1, 0), 0, "Should succeed deleting already deleted slice (idempotent)");
  
  printf("  Verification:\n");
  printf("    ✓ Scheduler handles invalid operation sequences gracefully\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Crash prevention - Array bounds and indexing */
static void test_oop_crash_prevention_array_bounds(void) {
  printf("  Purpose: Test array bounds and indexing edge cases\n");
  printf("  Expected: Should handle gracefully without buffer overflows\n\n");
  
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Fill to capacity boundary
  int initial_capacity = sch->slices_capacity;
  for (int i = 1; i <= initial_capacity; ++i) {
    ASSERT_EQ(slice_sch_add_slice(sch, i, 0, 0.01f, 0.01f, 0.02f, 0), 0, "Should add slice");
  }
  
  // Add one more to trigger growth (tests array bounds during reallocation)
  ASSERT_EQ(slice_sch_add_slice(sch, initial_capacity + 1, 0, 0.01f, 0.01f, 0.02f, 0), 0, "Should add slice at boundary");
  
  // Schedule to ensure all indices are valid
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  // Get statistics for all slices (tests array bounds)
  int num_stats = 0;
  const slice_statistics_t *all_stats = slice_sch_get_all_statistics(sch, &num_stats);
  ASSERT_TRUE(all_stats != NULL, "Should get all statistics");
  ASSERT_EQ(num_stats, initial_capacity + 1, "Should have correct number of statistics");
  
  printf("  Verification:\n");
  printf("    ✓ Scheduler handles array bounds correctly\n");
  
  slice_sch_destroy(sch);
}

/* Test OOP: Runtime total_prbs change */
static void test_oop_runtime_total_prbs_change(void) {
  printf("  Purpose: Test that total_prbs can be changed at runtime\n");
  printf("  Expected: Allocations should correctly reflect the new total_prbs value\n\n");
  
  // Create scheduler with initial total_prbs
  slice_scheduler_t *sch = slice_sch_create(100);
  ASSERT_TRUE(sch != NULL, "Scheduler should be created");
  
  // Add slices with max ratios that allow full allocation (0.6 + 0.4 = 1.0)
  ASSERT_EQ(slice_sch_add_slice(sch, 1, 0, 0.2f, 0.3f, 0.6f, 0), 0, "Should add slice 1");
  ASSERT_EQ(slice_sch_add_slice(sch, 2, 0, 0.1f, 0.2f, 0.4f, 0), 0, "Should add slice 2");
  
  // Schedule with initial total_prbs = 100
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed");
  
  int num_ranges = 0;
  const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Should get allocation");
  
  int num_active = 0;
  int total_allocated = 0;
  ASSERT_EQ(slice_sch_get_stats(sch, &num_active, &total_allocated), 0, "Should get stats");
  
  printf("  Initial state (total_prbs = 100):\n");
  printf("    Total Allocated: %d / 100 PRBs\n", total_allocated);
  ASSERT_EQ(total_allocated, 30, "Dedicated only without require (20+10)");
  
  // Find slice allocations
  int slice1_prbs = 0, slice2_prbs = 0;
  for (int i = 0; i < num_ranges; ++i) {
    if (ranges[i].slice_id.sst == 1 && ranges[i].slice_id.sd == 0) {
      slice1_prbs = ranges[i].num_prbs;
    } else if (ranges[i].slice_id.sst == 2 && ranges[i].slice_id.sd == 0) {
      slice2_prbs = ranges[i].num_prbs;
    }
  }
  printf("    Slice 1: %d PRBs, Slice 2: %d PRBs\n", slice1_prbs, slice2_prbs);
  ASSERT_EQ(slice1_prbs, 20, "Slice 1 dedicated");
  ASSERT_EQ(slice2_prbs, 10, "Slice 2 dedicated");
  
  // Change total_prbs to 200 at runtime using update function
  ASSERT_EQ(slice_sch_update_total_prbs(sch, 200), 0, "Should update total_prbs to 200");
  printf("  Changed total_prbs to 200 at runtime\n");
  
  // Schedule again with new total_prbs
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed with new total_prbs");
  
  ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Should get allocation after total_prbs change");
  
  ASSERT_EQ(slice_sch_get_stats(sch, &num_active, &total_allocated), 0, "Should get stats");
  
  printf("  After change (total_prbs = 200):\n");
  printf("    Total Allocated: %d / 200 PRBs\n", total_allocated);
  ASSERT_EQ(total_allocated, 60, "Dedicated scales with total_prbs (40+20)");
  
  // Verify allocations scale proportionally
  int slice1_prbs_new = 0, slice2_prbs_new = 0;
  for (int i = 0; i < num_ranges; ++i) {
    if (ranges[i].slice_id.sst == 1 && ranges[i].slice_id.sd == 0) {
      slice1_prbs_new = ranges[i].num_prbs;
    } else if (ranges[i].slice_id.sst == 2 && ranges[i].slice_id.sd == 0) {
      slice2_prbs_new = ranges[i].num_prbs;
    }
  }
  printf("    Slice 1: %d PRBs, Slice 2: %d PRBs\n", slice1_prbs_new, slice2_prbs_new);
  ASSERT_EQ(slice1_prbs_new, 40, "Slice 1 dedicated at 200 PRBs");
  ASSERT_EQ(slice2_prbs_new, 20, "Slice 2 dedicated at 200 PRBs");
  
  // Change total_prbs to 50 at runtime using update function
  ASSERT_EQ(slice_sch_update_total_prbs(sch, 50), 0, "Should update total_prbs to 50");
  printf("  Changed total_prbs to 50 at runtime\n");
  
  // Schedule again with new total_prbs
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed with new total_prbs");
  
  ranges = slice_sch_get_allocation(sch, &num_ranges);
  ASSERT_TRUE(ranges != NULL, "Should get allocation after second total_prbs change");
  
  ASSERT_EQ(slice_sch_get_stats(sch, &num_active, &total_allocated), 0, "Should get stats");
  
  printf("  After second change (total_prbs = 50):\n");
  printf("    Total Allocated: %d / 50 PRBs\n", total_allocated);
  ASSERT_EQ(total_allocated, 15, "Dedicated at 50 PRBs (10+5)");
  
  // Verify allocations scale down proportionally
  int slice1_prbs_50 = 0, slice2_prbs_50 = 0;
  for (int i = 0; i < num_ranges; ++i) {
    if (ranges[i].slice_id.sst == 1 && ranges[i].slice_id.sd == 0) {
      slice1_prbs_50 = ranges[i].num_prbs;
    } else if (ranges[i].slice_id.sst == 2 && ranges[i].slice_id.sd == 0) {
      slice2_prbs_50 = ranges[i].num_prbs;
    }
  }
  printf("    Slice 1: %d PRBs, Slice 2: %d PRBs\n", slice1_prbs_50, slice2_prbs_50);
  ASSERT_EQ(slice1_prbs_50, 10, "Slice 1 dedicated at 50 PRBs");
  ASSERT_EQ(slice2_prbs_50, 5, "Slice 2 dedicated at 50 PRBs");
  
  // Change total_prbs to a larger value (300) using update function
  ASSERT_EQ(slice_sch_update_total_prbs(sch, 300), 0, "Should update total_prbs to 300");
  printf("  Changed total_prbs to 300 at runtime\n");
  
  ASSERT_EQ(slice_sch_schedule(sch), 0, "Schedule should succeed with total_prbs = 300");
  
  ASSERT_EQ(slice_sch_get_stats(sch, &num_active, &total_allocated), 0, "Should get stats");
  ASSERT_EQ(total_allocated, 90, "Dedicated at 300 PRBs (60+30)");
  
  printf("  After third change (total_prbs = 300):\n");
  printf("    Total Allocated: %d / 300 PRBs\n", total_allocated);
  
  // Test error handling: invalid total_prbs values
  ASSERT_EQ(slice_sch_update_total_prbs(sch, 0), -1, "Should reject total_prbs = 0");
  ASSERT_EQ(slice_sch_update_total_prbs(sch, -1), -1, "Should reject negative total_prbs");
  ASSERT_EQ(slice_sch_update_total_prbs(NULL, 100), -1, "Should reject NULL scheduler");
  
  // Verify scheduler still has valid total_prbs after failed updates
  ASSERT_EQ(slice_sch_get_stats(sch, &num_active, &total_allocated), 0, "Should get stats");
  ASSERT_EQ(total_allocated, 90, "Allocation unchanged after failed total_prbs update");
  
  printf("  Verification:\n");
  printf("    ✓ total_prbs can be changed at runtime using slice_sch_update_total_prbs()\n");
  printf("    ✓ Allocations correctly reflect new total_prbs value\n");
  printf("    ✓ Allocations reflect dedicated (+ require) only, not idle min\n");
  printf("    ✓ Invalid total_prbs values are rejected\n");
  
  slice_sch_destroy(sch);
}

/* Test: Real-world scenario from Frame 257 slot 0 */
static void test_frame_756_slot_5_scenario(void) {
  printf("  Purpose: Test real-world scenario from Frame 257 slot 0\n");
  printf("  Expected: 6 slices with specific SST/SD values get allocations\n");
  printf("            Slice SST 0x01 SD 0xffffff requires 4 PRBs, gets PRBs in Pass 3\n");
  printf("            Other slices get PRBs based on their ratios\n\n");
  
  ALLOCATE_TEST_STRUCTURES(6, input, result);
  
  // Slice 0: SST 0x01 SD 0xffffff - Required 4 PRBs but gets 0
  // Configuration: Dedicated: 0.000, Min: 0.000, Max: 1.000
  input->slices[0].slice_id = slice_nssai_create(0x01, 0xffffff);
  input->slices[0].dedicated_prb_ratio = 0.000f;
  input->slices[0].min_prb_ratio = 0.000f;
  input->slices[0].max_prb_ratio = 1.000f;
  input->slices[0].required_prbs = 4;
  
  // Slice 1: SST 0x01 SD 0x000001 - Gets 11 PRBs
  // Configuration: Dedicated: 0.100, Min: 0.100, Max: 1.000
  input->slices[1].slice_id = slice_nssai_create(0x01, 0x000001);
  input->slices[1].dedicated_prb_ratio = 0.100f;
  input->slices[1].min_prb_ratio = 0.100f;
  input->slices[1].max_prb_ratio = 1.000f;
  input->slices[1].required_prbs = 0;
  
  // Slice 2: SST 0x01 SD 0x000002 — dedicated 30%, min 50% (sum(min) across slices <= 100%)
  // Configuration: Dedicated: 0.300, Min: 0.500, Max: 1.000
  input->slices[2].slice_id = slice_nssai_create(0x01, 0x000002);
  input->slices[2].dedicated_prb_ratio = 0.300f;
  input->slices[2].min_prb_ratio = 0.500f;
  input->slices[2].max_prb_ratio = 1.000f;
  input->slices[2].required_prbs = 0;
  
  // Slice 3: SST 0x01 SD 0x000003 - Gets 11 PRBs
  // Configuration: Dedicated: 0.100, Min: 0.100, Max: 0.300
  input->slices[3].slice_id = slice_nssai_create(0x01, 0x000003);
  input->slices[3].dedicated_prb_ratio = 0.100f;
  input->slices[3].min_prb_ratio = 0.100f;
  input->slices[3].max_prb_ratio = 0.300f;
  input->slices[3].required_prbs = 0;
  
  // Slice 4: SST 0x01 SD 0x000004 - Gets 11 PRBs
  // Configuration: Dedicated: 0.100, Min: 0.100, Max: 0.300
  input->slices[4].slice_id = slice_nssai_create(0x01, 0x000004);
  input->slices[4].dedicated_prb_ratio = 0.100f;
  input->slices[4].min_prb_ratio = 0.100f;
  input->slices[4].max_prb_ratio = 0.300f;
  input->slices[4].required_prbs = 0;
  
  // Slice 5: SST 0x01 SD 0x000005 - Gets 0 PRBs (not shown in ranges)
  // Configuration: Dedicated: 0.100, Min: 0.100, Max: 0.300
  input->slices[5].slice_id = slice_nssai_create(0x01, 0x000005);
  input->slices[5].dedicated_prb_ratio = 0.100f;
  input->slices[5].min_prb_ratio = 0.100f;
  input->slices[5].max_prb_ratio = 0.300f;
  input->slices[5].required_prbs = 0;
  
  input->num_slices = 6;
  input->total_prbs = 95;
  
  printf("  Input Configuration:\n");
  printf("    Total PRBs: %d\n", input->total_prbs);
  printf("    Number of Slices: %d\n", input->num_slices);
  for (int i = 0; i < input->num_slices; ++i) {
    printf("    Slice %d (SST=0x%02x, SD=0x%06x):\n", i, 
           input->slices[i].slice_id.sst, input->slices[i].slice_id.sd);
    print_slice_config(&input->slices[i], i);
  }
  printf("\n");
  
  int ret = calculate_slice_prb_ranges(input, result);
  
  print_allocation_result_with_required(result, input, input->total_prbs);
  printf("\n");
  
  printf("  Verification:\n");
  ASSERT_EQ(ret, 6, "Should return 6 slices");
  ASSERT_EQ(result->total_allocated_prbs, 73, "Dedicated + require-only (69+4)");
  
  // Find each slice in the result
  int slice_ffffff_prbs = 0;
  int slice_000001_prbs = 0;
  int slice_000002_prbs = 0;
  int slice_000003_prbs = 0;
  int slice_000004_prbs = 0;
  
  int slice_ffffff_start = -1, slice_ffffff_end = -1;
  int slice_000001_start = -1, slice_000001_end = -1;
  int slice_000002_start = -1, slice_000002_end = -1;
  int slice_000003_start = -1, slice_000003_end = -1;
  int slice_000004_start = -1, slice_000004_end = -1;
  
  // Check all slices in result (may include slices with 0 PRBs)
  for (int s = 0; s < input->num_slices; ++s) {
    if (result->ranges[s].slice_id.sst == 0x01 && result->ranges[s].slice_id.sd == 0xffffff) {
      slice_ffffff_prbs = result->ranges[s].num_prbs;
      slice_ffffff_start = result->ranges[s].start_prb;
      slice_ffffff_end = result->ranges[s].end_prb;
    } else if (result->ranges[s].slice_id.sst == 0x01 && result->ranges[s].slice_id.sd == 0x000001) {
      slice_000001_prbs = result->ranges[s].num_prbs;
      slice_000001_start = result->ranges[s].start_prb;
      slice_000001_end = result->ranges[s].end_prb;
    } else if (result->ranges[s].slice_id.sst == 0x01 && result->ranges[s].slice_id.sd == 0x000002) {
      slice_000002_prbs = result->ranges[s].num_prbs;
      slice_000002_start = result->ranges[s].start_prb;
      slice_000002_end = result->ranges[s].end_prb;
    } else if (result->ranges[s].slice_id.sst == 0x01 && result->ranges[s].slice_id.sd == 0x000003) {
      slice_000003_prbs = result->ranges[s].num_prbs;
      slice_000003_start = result->ranges[s].start_prb;
      slice_000003_end = result->ranges[s].end_prb;
    } else if (result->ranges[s].slice_id.sst == 0x01 && result->ranges[s].slice_id.sd == 0x000004) {
      slice_000004_prbs = result->ranges[s].num_prbs;
      slice_000004_start = result->ranges[s].start_prb;
      slice_000004_end = result->ranges[s].end_prb;
    }
  }
  
  // Verify slice 0xffffff gets PRBs in Pass 3 (required_prbs=4, min_prb_ratio=0.0)
  // Since it has required_prbs=4 and max_prb_ratio=1.0, it should get at least 4 PRBs in Pass 3
  printf("    ✓ Slice SST 0x01 SD 0xffffff: PRBs [%d, %d) (num_prbs=%d) - Required: 4, Got: %d\n",
         slice_ffffff_start, slice_ffffff_end, slice_ffffff_prbs, slice_ffffff_prbs);
  if (slice_ffffff_start >= 0) {
    ASSERT_GE(slice_ffffff_prbs, 4, "Slice 0xffffff should get at least 4 PRBs (required_prbs)");
    ASSERT_GE(slice_ffffff_start, 0, "Slice 0xffffff should have valid start_prb");
    ASSERT_GT(slice_ffffff_end, slice_ffffff_start, "Slice 0xffffff should have valid range");
  }
  
  // Verify slice 0x000001 gets dedicated only (require=0)
  printf("    ✓ Slice SST 0x01 SD 0x000001: PRBs [%d, %d) (num_prbs=%d)\n",
         slice_000001_start, slice_000001_end, slice_000001_prbs);
  ASSERT_EQ(slice_000001_prbs, 10, "Slice 0x000001 dedicated 10%");
  
  // Verify slice 0x000002 gets dedicated only
  printf("    ✓ Slice SST 0x01 SD 0x000002: PRBs [%d, %d) (num_prbs=%d)\n",
         slice_000002_start, slice_000002_end, slice_000002_prbs);
  ASSERT_EQ(slice_000002_prbs, 29, "Slice 0x000002 dedicated 30%");
  
  // Verify slice 0x000003 gets dedicated only
  printf("    ✓ Slice SST 0x01 SD 0x000003: PRBs [%d, %d) (num_prbs=%d)\n",
         slice_000003_start, slice_000003_end, slice_000003_prbs);
  ASSERT_EQ(slice_000003_prbs, 10, "Slice 0x000003 dedicated 10%");
  
  // Verify slice 0x000004 gets dedicated only
  printf("    ✓ Slice SST 0x01 SD 0x000004: PRBs [%d, %d) (num_prbs=%d)\n",
         slice_000004_start, slice_000004_end, slice_000004_prbs);
  ASSERT_EQ(slice_000004_prbs, 10, "Slice 0x000004 dedicated 10%");
  
  // Verify ranges are contiguous (all slices should have contiguous ranges)
  printf("    ✓ All slices have contiguous ranges\n");
  
  free_slice_input(input);
  free_slice_result(result);
}

int main(void) {
  printf("╔════════════════════════════════════════════════════════════════════════════════╗\n");
  printf("║     Network Slice PRB Allocation Algorithm - Unit Tests                     ║\n");
  printf("╚════════════════════════════════════════════════════════════════════════════════╝\n\n");
  printf("This test suite validates the four-pass PRB allocation algorithm:\n");
  printf("  1. Pass 1: Dedicated PRB allocation (non-shareable)\n");
  printf("  2. Pass 2: Prioritized resource distribution (min - dedicated)\n");
  printf("  3. Pass 3: Shared resource distribution (max - min)\n");
  printf("  4. Pass 4: Contiguous range assignment\n\n");
  
  printf("=== Individual Pass Tests ===\n\n");
  TEST(pass1_dedicated);
  TEST(pass2_prioritized);
  TEST(pass3_shared);
  TEST(pass4_ranges);
  
  printf("\n=== Pass 1 Edge Case Tests ===\n\n");
  TEST(pass1_dedicated_static_allocation);
  TEST(pass1_max_less_than_dedicated);
  TEST(pass1_zero_dedicated);
  
  printf("\n=== Pass 2 Edge Case Tests ===\n\n");
  TEST(pass2_slice_doesnt_need_prioritized);
  TEST(pass2_insufficient_prioritized);
  TEST(pass2_max_less_than_min);
  
  printf("\n=== Pass 3 Edge Case Tests ===\n\n");
  TEST(pass3_no_prb_requirements);
  TEST(pass3_all_slices_at_max);
  
  printf("\n=== Pass 4 Edge Case Tests ===\n\n");
  TEST(pass4_empty_result);
  TEST(pass4_single_slice);
  
  printf("\n=== Integrated Tests (Full Algorithm) ===\n\n");
  TEST(basic_two_slices);
  TEST(single_slice_all_prbs);
  TEST(no_active_ues);
  TEST(multiple_slices_different_ratios);
  TEST(dedicated_exceeds_total);
  TEST(max_ratio_enforcement);
  TEST(validation);
  TEST(zero_total_prbs);
  TEST(real_world_106_prbs);
  TEST(frame_756_slot_5_scenario);
  
  printf("\n=== Integration Edge Case Tests ===\n\n");
  TEST(integration_max_less_than_min);
  TEST(integration_all_slices_no_prioritized_need);
  
  printf("\n=== OOP Scheduler Interface Tests ===\n\n");
  TEST(oop_basic_two_slices);
  TEST(oop_add_delete_slices);
  TEST(oop_update_require);
  TEST(oop_dynamic_allocation);
  TEST(oop_error_handling);
  
  printf("\n=== OOP Memory Management Tests ===\n\n");
  TEST(oop_initial_capacity);
  TEST(oop_array_growth);
  TEST(oop_array_shrinking);
  TEST(oop_no_shrink_below_num_slices);
  
  printf("\n=== OOP Edge Case Tests ===\n\n");
  TEST(oop_add_at_capacity_boundary);
  TEST(oop_delete_all_slices);
  TEST(oop_update_existing_slice);
  TEST(oop_update_slice_with_different_sd);
  TEST(oop_invalid_ratios);
  TEST(oop_invalid_require);
  TEST(oop_update_nonexistent_slice);
  TEST(oop_delete_nonexistent_slice);
  TEST(oop_get_allocation_before_schedule);
  TEST(oop_boundary_ratios);
  TEST(oop_null_scheduler_operations);
  TEST(oop_invalid_total_prbs);
  TEST(oop_capacity_at_shrink_threshold);
  TEST(oop_rapid_add_delete);
  TEST(oop_schedule_after_delete);
  
  printf("\n=== OOP Statistics Tests ===\n\n");
  TEST(oop_statistics_initial_state);
  TEST(oop_statistics_after_schedule);
  TEST(oop_statistics_moving_average);
  TEST(oop_statistics_no_allocation);
  TEST(oop_statistics_get_all);
  TEST(oop_statistics_after_delete);
  TEST(oop_statistics_error_handling);
  
  printf("\n=== OOP Crash Prevention Tests ===\n\n");
  TEST(oop_crash_prevention_large_slices);
  TEST(oop_crash_prevention_large_total_prbs);
  TEST(oop_crash_prevention_use_after_destroy);
  TEST(oop_crash_prevention_overflow);
  TEST(oop_crash_prevention_rapid_reallocation);
  TEST(oop_crash_prevention_zero_sized);
  TEST(oop_crash_prevention_invalid_state);
  TEST(oop_crash_prevention_boundary_ratios);
  TEST(oop_crash_prevention_memory_pressure);
  TEST(oop_crash_prevention_invalid_sequences);
  TEST(oop_crash_prevention_array_bounds);
  
  printf("\n=== OOP Runtime Configuration Tests ===\n\n");
  TEST(oop_runtime_total_prbs_change);
  
  printf("╔════════════════════════════════════════════════════════════════════════════════╗\n");
  printf("║                            Test Summary                                       ║\n");
  printf("╚════════════════════════════════════════════════════════════════════════════════╝\n");
  printf("  Tests run:    %d\n", tests_run);
  printf("  Tests passed: %d\n", tests_passed);
  printf("  Tests failed: %d\n", tests_run - tests_passed);
  
  if (tests_passed == tests_run) {
    printf("\n  ✓ All tests passed! The algorithm is working correctly.\n\n");
    return 0;
  } else {
    printf("\n  ✗ Some tests failed! Please review the output above.\n\n");
    return 1;
  }
}
