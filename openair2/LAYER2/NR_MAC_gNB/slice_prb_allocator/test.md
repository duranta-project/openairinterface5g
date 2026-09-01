# Test Suite Documentation

This document describes all test cases for the Network Slice PRB Allocation Algorithm.

## Test Organization

The test suite is organized into several categories:
1. **Individual Pass Tests** - Test each pass function in isolation
2. **Pass Edge Case Tests** - Test edge cases for each pass
3. **Integrated Tests** - Test the full algorithm end-to-end
4. **Integration Edge Case Tests** - Test edge cases spanning multiple passes

## Test Categories

### Individual Pass Tests

These tests verify each pass function works correctly in isolation.

#### Test 1: `test_pass1_dedicated`
- **Purpose**: Test Pass 1 (dedicated allocation) in isolation
- **Input**: 2 slices, dedicated=30% and 20%, total=100 PRBs
- **Expected**: 
  - Slice 1 gets 30 PRBs, Slice 2 gets 20 PRBs
  - Total allocated: 50 PRBs
- **Edge Case**: Also tests scaling when dedicated exceeds total (60% + 60% = 120%)
  - Expected: Proportional scaling to 50/50 split

#### Test 2: `test_pass2_prioritized`
- **Purpose**: Test Pass 2 (prioritized allocation) in isolation
- **Input**: After Pass 1, slices need prioritized resources based on `required_prbs`
- **Expected**:
  - Slice 1: 20 (dedicated) + 10 (prioritized) = 30 PRBs
  - Slice 2: 10 (dedicated) + 15 (prioritized) = 25 PRBs
  - Total allocated: 55 PRBs, Remaining: 45 PRBs

#### Test 3: `test_pass3_shared`
- **Purpose**: Test Pass 3 (shared allocation) in isolation
- **Input**: After Pass 1+2, 45 PRBs remaining (shared resources)
- **Expected**: Shared resources distributed based on PRB deficits or capacity
- **Verification**: All PRBs allocated, slices grow up to max limits

#### Test 4: `test_pass4_ranges`
- **Purpose**: Test Pass 4 (range assignment) in isolation
- **Input**: PRBs already allocated: Slice 1=30, Slice 2=20, Slice 3=50
- **Expected**: Contiguous ranges [0,30), [30,50), [50,100)
- **Verification**: Ranges are contiguous and non-overlapping

### Pass 1 Edge Case Tests

#### Test 5: `test_pass1_dedicated_static_allocation`
- **Purpose**: Test Pass 1 with dedicated = min = max (static allocation)
- **Input**: 2 slices, all ratios = 50%
- **Expected**: 
  - Both slices get 50 PRBs
  - All 100 PRBs allocated in Pass 1
  - No scaling needed

#### Test 6: `test_pass1_max_less_than_dedicated`
- **Purpose**: Test Pass 1 when max < dedicated (invalid config, but should handle gracefully)
- **Input**: Slice with dedicated=50%, max=30%
- **Expected**: Dedicated is capped at max (30 PRBs)
- **Verification**: Algorithm handles invalid config gracefully

#### Test 7: `test_pass1_zero_dedicated`
- **Purpose**: Test Pass 1 with zero dedicated (slice still active)
- **Input**: Slice with dedicated=0%, min=30%, max=50%
- **Expected**: 
  - Slice gets 0 PRBs in Pass 1
  - Slice is still counted as active
  - PRBs allocated in later passes

### Pass 2 Edge Case Tests

#### Test 8: `test_pass2_slice_doesnt_need_prioritized`
- **Purpose**: Test Pass 2 when slice doesn't need prioritized resources
- **Input**: 
  - Slice 1: required=15, current=20 (doesn't need prioritized)
  - Slice 2: required=30, current=10 (needs prioritized)
- **Expected**:
  - Slice 1: No change (20 PRBs)
  - Slice 2: Gets prioritized (25 PRBs)
  - Slice 1's prioritized resources remain available for Pass 3

#### Test 9: `test_pass2_insufficient_prioritized`
- **Purpose**: Test Pass 2 with insufficient prioritized resources
- **Input**: 
  - Slice 1 needs 30 prioritized, Slice 2 needs 30 prioritized
  - Only 40 PRBs available (need 60)
- **Expected**: Proportional scaling
  - Slice 1: 20 + 20 = 40 PRBs
  - Slice 2: 10 + 20 = 30 PRBs
  - Scale factor: 40/60 = 0.667

#### Test 10: `test_pass2_max_less_than_min`
- **Purpose**: Test Pass 2 when max < min (max takes precedence)
- **Input**: Slice with dedicated=20%, min=50%, max=30%
- **Expected**: 
  - Allocation capped at max (30 PRBs)
  - Min (50 PRBs) not fully met
  - Max limit takes precedence

### Pass 3 Edge Case Tests

#### Test 11: `test_pass3_no_prb_requirements`
- **Purpose**: Test Pass 3 without PRB requirements (capacity-based only)
- **Input**: 
  - Slice 1: 30 PRBs, max=50, can_add=20
  - Slice 2: 20 PRBs, max=50, can_add=30
  - 50 PRBs remaining, no `required_prbs` specified
- **Expected**: Proportional distribution based on capacity
  - Slice 1: 30 + 20 = 50 PRBs
  - Slice 2: 20 + 30 = 50 PRBs

#### Test 12: `test_pass3_all_slices_at_max`
- **Purpose**: Test Pass 3 when all slices are at max limit
- **Input**: 
  - Slice 1: 50 PRBs (at max=50)
  - Slice 2: 30 PRBs (at max=30)
  - 20 PRBs remaining
- **Expected**: 
  - No allocation possible
  - 20 PRBs remain unallocated
  - All slices stay at their max

### Pass 4 Edge Case Tests

#### Test 13: `test_pass4_empty_result`
- **Purpose**: Test Pass 4 with no PRBs allocated
- **Input**: All slices have num_prbs = 0
- **Expected**: 
  - No ranges assigned
  - No errors
  - All start_prb and end_prb = 0

#### Test 14: `test_pass4_single_slice`
- **Purpose**: Test Pass 4 with single slice
- **Input**: Single slice with 100 PRBs
- **Expected**: Range [0, 100)

### Integrated Tests (Full Algorithm)

#### Test 15: `test_basic_two_slices`
- **Purpose**: Basic two-slice allocation with different ratios
- **Input**: 2 slices, dedicated=30%/20%, min=30%/20%, max=50%/50%
- **Expected**: Both slices get 50 PRBs each (at max limits)

#### Test 16: `test_single_slice_all_prbs`
- **Purpose**: Single slice gets all PRBs
- **Input**: 1 slice, dedicated=100%, min=100%, max=100%
- **Expected**: Slice gets all 100 PRBs

#### Test 17: `test_no_active_ues`
- **Purpose**: Slices with zero ratios get no PRBs
- **Input**: 2 slices with zero ratios
- **Expected**: Both slices get 0 PRBs

#### Test 18: `test_multiple_slices_different_ratios`
- **Purpose**: Multiple slices with different ratios
- **Input**: 3 slices with varying ratios
- **Expected**: Proportional allocation respecting max limits

#### Test 19: `test_dedicated_exceeds_total`
- **Purpose**: Dedicated allocations exceed total PRBs
- **Input**: 2 slices, both dedicated=60%
- **Expected**: Proportional scaling to 50/50 split

#### Test 20: `test_max_ratio_enforcement`
- **Purpose**: Max ratio is enforced as hard limit
- **Input**: Slices with max limits
- **Expected**: No slice exceeds its max_prb_ratio

#### Test 21: `test_validation`
- **Purpose**: Input validation
- **Input**: Invalid configurations (ratios out of range, invalid relationships)
- **Expected**: Validation fails, function returns error

#### Test 22: `test_zero_total_prbs`
- **Purpose**: Zero total PRBs
- **Input**: total_prbs = 0
- **Expected**: Validation fails

#### Test 23: `test_real_world_106_prbs`
- **Purpose**: Real-world scenario with 106 PRBs (typical 5G NR)
- **Input**: 2 slices, total=106 PRBs
- **Expected**: Both slices get 53 PRBs each (50% each)

### Integration Edge Case Tests

#### Test 24: `test_integration_max_less_than_min`
- **Purpose**: Full algorithm with max < min (edge case)
- **Input**: Slice with dedicated=20%, min=50%, max=30%
- **Expected**: 
  - Max takes precedence
  - Slice gets 30 PRBs (capped at max)
  - Min (50 PRBs) not fully met

#### Test 25: `test_integration_all_slices_no_prioritized_need`
- **Purpose**: All slices don't need prioritized resources
- **Input**: 
  - Slice 1: required=15, current=20 (doesn't need)
  - Slice 2: required=5, current=10 (doesn't need)
- **Expected**: 
  - Pass 2: No prioritized allocation
  - Pass 3: All remaining PRBs distributed as shared resources
  - All 100 PRBs allocated

## Test Coverage Summary

### Pass 1 Coverage
- ✅ Normal dedicated allocation
- ✅ Proportional scaling when exceeds total
- ✅ Static allocation (dedicated = min = max)
- ✅ Max < dedicated (capping)
- ✅ Zero dedicated
- ✅ Single slice
- ✅ Multiple slices

### Pass 2 Coverage
- ✅ Normal prioritized allocation
- ✅ Slice doesn't need prioritized (shareable)
- ✅ Insufficient prioritized resources (proportional scaling)
- ✅ Max < min (max takes precedence)
- ✅ No required_prbs (fallback to traditional min)
- ✅ Multiple slices with different needs

### Pass 3 Coverage
- ✅ PRB requirement-based allocation
- ✅ Capacity-based allocation (no requirements)
- ✅ All slices at max (cannot allocate)
- ✅ Single slice with shared resources
- ✅ Multiple slices with varying capacities

### Pass 4 Coverage
- ✅ Normal range assignment
- ✅ Multiple slices
- ✅ Single slice
- ✅ Empty result (no PRBs)
- ✅ Contiguous and non-overlapping ranges

### Integration Coverage
- ✅ Full algorithm with all passes
- ✅ Edge cases spanning multiple passes
- ✅ Real-world scenarios
- ✅ Invalid configurations
- ✅ Boundary conditions

## Running Tests

```bash
cd slice_prb_allocator
make run
```

Or manually:
```bash
make
./test_slice_prb_allocator
```

## Test Output

Each test prints:
- Test purpose and expected behavior
- Input configuration
- Allocation result
- Verification steps
- Pass/fail status

## Test Statistics

- **Total Tests**: 25
- **Individual Pass Tests**: 4
- **Pass Edge Case Tests**: 9
- **Integrated Tests**: 9
- **Integration Edge Case Tests**: 2
- **Coverage**: All edge cases and normal scenarios

## Edge Cases Covered

1. ✅ Dedicated exceeds total (scaling)
2. ✅ Max < min (max takes precedence)
3. ✅ Max < dedicated (capping)
4. ✅ Min < dedicated (adjustment)
5. ✅ Zero dedicated
6. ✅ Static allocation (dedicated = min = max)
7. ✅ Slice doesn't need prioritized resources
8. ✅ Insufficient prioritized resources
9. ✅ All slices at max limit
10. ✅ No PRB requirements
11. ✅ Empty result
12. ✅ Single slice
13. ✅ No active UEs
14. ✅ Invalid configurations
15. ✅ Zero total PRBs
