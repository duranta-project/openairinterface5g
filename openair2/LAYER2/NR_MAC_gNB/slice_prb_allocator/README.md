# Network Slice PRB Range Allocation Algorithm

## Table of Contents
1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Algorithm Description](#algorithm-description)
4. [Detailed Algorithm Explanation](#detailed-algorithm-explanation)
5. [Usage Examples](#usage-examples)
6. [Test Coverage](#test-coverage)
7. [Integration](#integration)
8. [Performance](#performance)
9. [References](#references)

---

## Overview

This module implements a **four-pass algorithm** for allocating Physical Resource Blocks (PRBs) to network slices based on dedicated, minimum, and maximum PRB ratios, with optional PRB requirement-based prioritization. The algorithm is designed for frequency-domain network slicing in 5G NR systems.

### PRB Ratio Definitions

Before understanding the algorithm, it's crucial to understand the three types of PRB ratios:

#### 1. `dedicated_prb_ratio` (Dedicated PRB Ratio)
- **Definition**: Non-shareable, guaranteed PRB allocation that is always reserved for the slice
- **Range**: 0.0 to 1.0 (0% to 100%)
- **Behavior**: 
  - Allocated in **Pass 1** before any other allocation
  - **Always reserved** - cannot be shared with other slices, even if the slice doesn't need it
  - If total dedicated allocations exceed 100%, all are scaled down proportionally
- **Use Case**: Critical slices that need guaranteed isolation (e.g., URLLC for ultra-low latency)
- **Example**: `dedicated_prb_ratio = 0.20` means 20% of PRBs are always reserved for this slice

#### 2. `min_prb_ratio` (Minimum PRB Ratio)
- **Definition**: Minimum guaranteed PRB allocation that the slice should receive when it needs resources
- **Range**: 0.0 to 1.0 (0% to 100%), must be ≥ `dedicated_prb_ratio`
- **Behavior**:
  - The difference `(min_prb_ratio - dedicated_prb_ratio)` represents **prioritized but shareable** resources
  - **If slice needs it** (based on `required_prbs`): Slice gets priority to use these resources
  - **If slice doesn't need it**: Other slices can use these resources
  - Allocated in **Pass 2** based on actual need (`required_prbs`)
  - Must be ≥ `dedicated_prb_ratio` (dedicated is part of the minimum)
- **Use Case**: Slices that need guaranteed minimum bandwidth when they have traffic
- **Example**: 
  - `dedicated_prb_ratio = 0.20`, `min_prb_ratio = 0.30`
  - 20% is always reserved (dedicated)
  - 10% is prioritized - slice gets it if needed, otherwise shareable

#### 3. `max_prb_ratio` (Maximum PRB Ratio)
- **Definition**: Hard upper limit on PRB allocation that a slice can never exceed
- **Range**: 0.0 to 1.0 (0% to 100%), must be ≥ `min_prb_ratio`
- **Behavior**:
  - The difference `(max_prb_ratio - min_prb_ratio)` represents **fully shared resources**
  - These resources are **shared between all slices** and distributed proportionally
  - Enforced as a **hard limit** in all passes (Pass 1, 2, and 3)
  - Slice can grow up to this limit in Pass 3 if resources are available
  - Never exceeded, even if more PRBs are available
- **Use Case**: Allowing slices to share additional resources beyond their minimum guarantees
- **Example**: 
  - `min_prb_ratio = 0.30`, `max_prb_ratio = 0.50`
  - 30% is minimum (dedicated + prioritized)
  - 20% is shared - can be used by any slice up to their max limit

#### Relationship Between Ratios

The ratios must satisfy: **`dedicated_prb_ratio ≤ min_prb_ratio ≤ max_prb_ratio`**

**Three-Tier Resource Model**:
1. **Dedicated** (`dedicated_prb_ratio`): Always reserved, non-shareable (Pass 1)
2. **Prioritized** (`min_prb_ratio - dedicated_prb_ratio`): Prioritized but shareable - slice gets priority if needed based on `required_prbs` (Pass 2)
3. **Shared** (`max_prb_ratio - min_prb_ratio`): Fully shared - distributed proportionally among all slices (Pass 3)

**Example Configuration**:
```
Slice 1 (URLLC):
  dedicated_prb_ratio = 0.20  (20% always reserved, non-shareable)
  min_prb_ratio = 0.20        (20% total: 20% dedicated + 0% prioritized)
  max_prb_ratio = 0.40        (20% shared resources: 40% - 20%)

Slice 2 (eMBB):
  dedicated_prb_ratio = 0.10  (10% always reserved, non-shareable)
  min_prb_ratio = 0.30        (30% total: 10% dedicated + 20% prioritized)
  max_prb_ratio = 0.60        (30% shared resources: 60% - 30%)

Resource breakdown (for 100 PRBs):
- Slice 1: 20 PRBs dedicated, 0 PRBs prioritized, 20 PRBs shared (max 40)
- Slice 2: 10 PRBs dedicated, 20 PRBs prioritized, 30 PRBs shared (max 60)
- Total: 30 PRBs dedicated, 20 PRBs prioritized, 50 PRBs shared
```

### Slice Identifier Structure

Slice identifiers use the `slice_nssai_t` struct with SST (Slice/Service Type) and SD (Slice Differentiator) bit fields, following the 5G S-NSSAI (Single Network Slice Selection Assistance Information) format:

```c
typedef struct {
  uint32_t sst : 8;   // Slice/Service Type (0-255)
  uint32_t sd : 24;   // Slice Differentiator (0-0xffffff)
} slice_nssai_t;
```

**Note**: This is named `slice_nssai_t` to avoid conflict with OAI's existing `slice_id_t` (uint8_t) in `platform_types.h`.

**Helper Functions**:
- `slice_nssai_create(sst, sd)`: Create a `slice_nssai_t` from SST and SD values
- `slice_nssai_from_int(id)`: Convert an integer to `slice_nssai_t` (sets `sst=id`, `sd=0`)
- `slice_nssai_eq(sid1, sid2)`: Compare two `slice_nssai_t` structures
- `slice_nssai_eq_int(sid, id)`: Compare `slice_nssai_t` with an integer (compares SST only)

**Example**:
```c
// Create slice ID with SST=1, SD=0
slice_nssai_t slice1 = slice_nssai_create(1, 0);

// Create from integer (for backward compatibility)
slice_nssai_t slice2 = slice_nssai_from_int(2);  // SST=2, SD=0

// Compare slice IDs
if (slice_nssai_eq(&slice1, &slice2)) {
    // Slices match
}
```

### Key Features

- **Three-Tier Allocation**: Dedicated (non-shareable) → Prioritized (shareable with priority) → Shared (fully shareable)
- **Hard Limits**: `max_prb_ratio` always enforced; `min_prb_ratio` guaranteed when resources available
- **Contiguous Allocation**: PRBs allocated contiguously for frequency-domain slicing
- **Proportional Scaling**: Handles over-allocation with fair proportional distribution
- **PRB Requirements**: Optional `required_prbs` enables priority-based allocation
- **5G S-NSSAI Support**: Slice identifiers use SST and SD bit fields following 3GPP standards

---

## Quick Start

### Directory Structure

```
slice_prb_allocator/
├── slice_prb_allocator.h          # Header file with data structures and function declarations
├── slice_prb_allocator.c          # Implementation of the allocation algorithm
├── test_slice_prb_allocator.c     # Comprehensive unit tests
├── Makefile                       # Build configuration
├── README.md                      # This file (comprehensive documentation)
└── .gitignore                     # Git ignore file
```

### Compilation

**Using Makefile (recommended):**
```bash
cd slice_prb_allocator
make          # Build test executable
make run      # Build and run tests
make clean    # Clean build artifacts
```

**Manual compilation:**
```bash
cd slice_prb_allocator
gcc -Wall -Wextra -std=c11 -O2 -g test_slice_prb_allocator.c slice_prb_allocator.c -o test_slice_prb_allocator -lm
./test_slice_prb_allocator
```

### Basic Usage

```c
#include "slice_prb_allocator.h"

// Prepare input
slice_alloc_input_t input = {0};
// Slice 1: SST=1, SD=0 (eMBB slice)
input.slices[0].slice_id = slice_nssai_create(1, 0);
input.slices[0].dedicated_prb_ratio = 0.33f;
input.slices[0].min_prb_ratio = 0.33f;
input.slices[0].max_prb_ratio = 0.50f;
input.slices[0].required_prbs = 40;  // Optional: PRB requirement (0 = not used)

// Slice 2: SST=2, SD=0 (URLLC slice)
input.slices[1].slice_id = slice_nssai_create(2, 0);
input.slices[1].dedicated_prb_ratio = 0.20f;
input.slices[1].min_prb_ratio = 0.20f;
input.slices[1].max_prb_ratio = 0.50f;
input.slices[1].required_prbs = 30;  // Optional: PRB requirement

input.num_slices = 2;
input.total_prbs = 106;

// Allocate PRBs
slice_alloc_result_t result = {0};
int num_active = calculate_slice_prb_ranges(&input, &result);

// Use results
for (int s = 0; s < MAX_NUM_SLICES; ++s) {
    if (result.ranges[s].num_prbs > 0) {
        printf("Slice (SST=%d, SD=%u): PRBs [%d, %d) (%d PRBs)\n",
               result.ranges[s].slice_id.sst,
               result.ranges[s].slice_id.sd,
               result.ranges[s].start_prb,
               result.ranges[s].end_prb,
               result.ranges[s].num_prbs);
    }
}
```

**Note**: Slice identifiers use the `slice_nssai_t` struct with SST (Slice/Service Type, 8 bits, 0-255) and SD (Slice Differentiator, 24 bits, 0-0xffffff) bit fields, following the 5G S-NSSAI (Single Network Slice Selection Assistance Information) format.

---

## Algorithm Description

The algorithm implements a **three-tier resource allocation model** in **four passes**:

### Four-Pass Algorithm

### Pass 1: Dedicated PRB Allocation
- Allocates dedicated (non-shareable) PRBs to each slice with active UEs
- If total dedicated allocations exceed available PRBs, they are scaled down proportionally
- Ensures `dedicated_prb_ratio <= min_prb_ratio <= max_prb_ratio`

### Pass 2: Prioritized Resource Distribution
- Distributes prioritized resources (`min - dedicated`) to slices that need them
- Based on `required_prbs`: only allocates if `required_prbs > current_allocation`
- If slice doesn't need prioritized resources, they remain available for Pass 3
- Respects `max_prb_ratio` as a hard limit during allocation

### Pass 3: Shared Resource Distribution
- Distributes shared resources (`max - min`) among all slices
- Fully shareable - distributed proportionally based on:
  - PRB requirements (`required_prbs`) if specified, or
  - Remaining capacity (up to max limit) if no requirements
- Each slice can grow up to its `max_prb_ratio` limit

### Pass 4: Contiguous Range Assignment
- Assigns contiguous PRB ranges to each slice
- Ranges are non-overlapping and cover all allocated PRBs

### Algorithm Flow

```
┌─────────────────────────────────────────────────────────┐
│              Three-Tier Resource Allocation              │
│  Dedicated → Prioritized → Shared → Contiguous Ranges   │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
        ┌───────────────────────────────────┐
        │  Pass 1: Dedicated Resources      │
        │  - Allocate non-shareable PRBs      │
        │  - Always reserved (dedicated)     │
        │  - Scale down if exceeds total     │
        └───────────────┬───────────────────┘
                        │
                        ▼
        ┌───────────────────────────────────┐
        │  Pass 2: Prioritized Resources     │
        │  - Allocate if slice needs them    │
        │  - Based on required_prbs          │
        │  - Shareable if slice doesn't need│
        └───────────────┬───────────────────┘
                        │
                        ▼
        ┌───────────────────────────────────┐
        │  Pass 3: Shared Resources          │
        │  - Fully shared between slices     │
        │  - Distributed proportionally      │
        │  - Up to max_prb_ratio limit       │
        └───────────────┬───────────────────┘
                        │
                        ▼
        ┌───────────────────────────────────┐
        │  Pass 4: Contiguous Range Assign  │
        │  - Assign [start, end) ranges     │
        │  - Ensure no gaps or overlaps     │
        └───────────────────────────────────┘
```

---

## Detailed Algorithm Explanation

### Problem Statement

Given:
- **N** network slices, each with:
  - `dedicated_prb_ratio`: Non-shareable PRB allocation (0.0 - 1.0)
  - `min_prb_ratio`: Minimum guaranteed PRB allocation (0.0 - 1.0)
  - `max_prb_ratio`: Maximum allowed PRB allocation (0.0 - 1.0)
  - `required_prbs`: Optional current PRB requirement (0 = not used)
- **Total PRBs**: Total number of available PRBs (e.g., 106 for typical 5G NR)

Find:
- Contiguous PRB ranges `[start_prb, end_prb)` for each slice
- Allocation must satisfy:
  1. `dedicated_prb_ratio ≤ min_prb_ratio ≤ max_prb_ratio`
  2. Each slice gets at least `dedicated_prb_ratio`
  3. Each slice gets at least `min_prb_ratio` when resources are available
  4. No slice exceeds `max_prb_ratio` (hard limit)
  5. All PRBs are allocated (no gaps)
  6. Ranges are contiguous and non-overlapping

### Pass 1: Dedicated PRB Allocation

**Purpose**: Allocate non-shareable dedicated PRBs to each slice.

**Steps**:
1. For each slice:
   - Calculate `dedicated_prbs = total_prbs × dedicated_prb_ratio`
   - Calculate `min_prbs = total_prbs × min_prb_ratio`
   - Calculate `max_prbs = total_prbs × max_prb_ratio`
   - Ensure `min_prbs ≥ dedicated_prbs` (adjust if needed)
   - Ensure `dedicated_prbs ≤ max_prbs` (cap if needed)
   - Allocate `dedicated_prbs` to the slice
   - Add to `allocated_prbs` counter

2. **Over-allocation Check**:
   - If `allocated_prbs > total_prbs`:
     - Calculate scale factor: `scale = total_prbs / allocated_prbs`
     - Scale down each slice's allocation proportionally
     - Recalculate `allocated_prbs`

**Example**:
```
Total PRBs: 100
Slice 1: dedicated = 60% → 60 PRBs
Slice 2: dedicated = 60% → 60 PRBs
Total allocated: 120 PRBs > 100 PRBs

Scale factor: 100/120 = 0.833
Slice 1: 60 × 0.833 = 50 PRBs
Slice 2: 60 × 0.833 = 50 PRBs
Total: 100 PRBs ✓
```

**Why Scale Down?**
- **Fairness**: Each slice gets a proportional share of its dedicated request
- **No Starvation**: No slice gets zero PRBs if it requested dedicated resources
- **Consistency**: The relative ratios between slices are preserved

**Mathematical Formulation**:
```
If Σ(dedicated_i) > total_prbs:
  scale = total_prbs / Σ(dedicated_i)
  allocated_i = dedicated_i × scale
```

### Pass 2: Minimum Guarantee Distribution (Prioritized Resources)

**Purpose**: Allocate prioritized resources (`min_prb_ratio - dedicated_prb_ratio`) to slices that need them based on `required_prbs`.

**Important Semantics**: 
- **Dedicated** (`dedicated_prb_ratio`): Always reserved, non-shareable (allocated in Pass 1)
- **Prioritized** (`min_prb_ratio - dedicated_prb_ratio`): Shareable but prioritized
  - **Common Rule**: Pass 2 allocates up to `min(required_prbs, min_prb_ratio)` (within the prioritized portion)
  - If `required_prbs < min_prb_ratio`: Allocate up to `required_prbs` in Pass 2, remaining prioritized resources (`min_prb_ratio - required_prbs`) are available for Pass 3
  - If `required_prbs ≥ min_prb_ratio`: Allocate up to `min_prb_ratio` in Pass 2

**Example**: If a slice has `dedicated_prb_ratio = 0.20` and `min_prb_ratio = 0.30`:
- 20% is dedicated (always reserved, Pass 1)
- 10% is prioritized (Pass 2) - slice gets it if `required_prbs` indicates need, otherwise shareable

**Steps**:
1. Calculate `remaining_prbs = total_prbs - allocated_prbs`

2. **Calculate Prioritized Resource Needs** (common rule: allocate up to `min(required_prbs, min_prb_ratio)`):
   - For each active slice:
     - `min_prbs = total_prbs × min_prb_ratio` (total minimum target)
     - `dedicated_prbs = total_prbs × dedicated_prb_ratio` (already allocated in Pass 1)
     - `prioritized_prbs = min_prbs - dedicated_prbs` (prioritized but shareable portion)
     - **Common Rule**: `target_prbs = min(required_prbs, min_prbs)` (if `required_prbs = 0`, then `target_prbs = 0`)
     - **Check if slice needs prioritized resources**:
       - If `target_prbs > current_allocation` and `prioritized_prbs > 0`:
         - `prioritized_needed = min(prioritized_prbs, target_prbs - current_allocation)`
         - Add to `total_prioritized_needed`
       - If `target_prbs ≤ current_allocation` (slice doesn't need more):
         - Slice doesn't claim prioritized resources (they remain shareable for Pass 3)

3. **Allocate Prioritized Resources**:
   - If `total_prioritized_needed > 0`:
     - Calculate scale: `scale = min(1.0, remaining_prbs / total_prioritized_needed)`
     - For each slice that needs prioritized resources:
       - Calculate `additional = prioritized_needed × scale`
       - **Critical Check**: Ensure `current + additional ≤ max_prbs` (respect max limit)
       - **Critical Check**: Ensure `current + additional ≤ target_prbs` (where `target_prbs = min(required_prbs, min_prbs)`)
       - If adding `additional` would exceed limits, cap appropriately
       - Allocate `additional` PRBs (capped if needed)
       - Update `allocated_prbs` and `remaining_prbs`
   - **Note**: If a slice doesn't need prioritized resources (`target_prbs ≤ current_allocation`), those resources remain available for Pass 3
   - **Note**: If `required_prbs < min_prb_ratio`, the remaining prioritized resources (`min_prb_ratio - required_prbs`) are available for Pass 3

**Example 1: Common Rule - Allocate up to min(required_prbs, min_prb_ratio)**:
```
After Pass 1:
Slice 1: 20 PRBs (dedicated), min = 30 PRBs, max = 50 PRBs, required = 35 PRBs
Slice 2: 10 PRBs (dedicated), min = 25 PRBs, max = 50 PRBs, required = 30 PRBs
Remaining: 70 PRBs

Common Rule: target = min(required_prbs, min_prb_ratio)
Slice 1: target = min(35, 30) = 30 PRBs, current = 20, needs 10 PRBs
Slice 2: target = min(30, 25) = 25 PRBs, current = 10, needs 15 PRBs

Prioritized resources (min - dedicated):
Slice 1: 30 - 20 = 10 PRBs prioritized → needs 10 (within prioritized portion)
Slice 2: 25 - 10 = 15 PRBs prioritized → needs 15 (within prioritized portion)
Total prioritized needed: 25 PRBs

Available: 70 PRBs > 25 PRBs needed
Scale = 1.0

Slice 1: 20 + 10 = 30 PRBs (reaches target = 30, remaining 5 needed in Pass 3)
Slice 2: 10 + 15 = 25 PRBs (reaches target = 25, remaining 5 needed in Pass 3)
Remaining: 70 - 25 = 45 PRBs (available for Pass 3)
```

**Example 2: Common Rule - required_prbs < min_prb_ratio**:
```
After Pass 1:
Slice 1: 20 PRBs (dedicated), min = 30 PRBs, max = 50 PRBs, required = 15 PRBs
Slice 2: 10 PRBs (dedicated), min = 25 PRBs, max = 50 PRBs, required = 30 PRBs
Remaining: 70 PRBs

Common Rule: target = min(required_prbs, min_prb_ratio)
Slice 1: target = min(15, 30) = 15 PRBs, current = 20, doesn't need more (target ≤ current)
Slice 2: target = min(30, 25) = 25 PRBs, current = 10, needs 15 PRBs

Prioritized resources (min - dedicated):
Slice 1: 30 - 20 = 10 PRBs prioritized → not needed (target ≤ current), available for Pass 3
Slice 2: 25 - 10 = 15 PRBs prioritized → needs 15 (within prioritized portion)
Total prioritized needed: 15 PRBs (only slice 2 needs it)

Available: 70 PRBs > 15 PRBs needed
Scale = 1.0

Slice 1: 20 PRBs (no change, target = 15 already met)
Slice 2: 10 + 15 = 25 PRBs (reaches target = 25, remaining 5 needed in Pass 3)
Remaining: 70 - 15 = 55 PRBs (slice 1's prioritized 10 PRBs + 45 PRBs available for Pass 3)
```

**Example 3: Common Rule - required_prbs = 0 (no allocation in Pass 2)**:
```
After Pass 1:
Slice 0: 0 PRBs (dedicated), min = 0 PRBs, max = 95 PRBs, required = 4 PRBs
Slice 1: 10 PRBs (dedicated), min = 10 PRBs, max = 95 PRBs, required = 0 PRBs
Slice 2: 29 PRBs (dedicated), min = 67 PRBs, max = 95 PRBs, required = 0 PRBs
Remaining: 26 PRBs

Common Rule: target = min(required_prbs, min_prb_ratio)
Slice 0: target = min(4, 0) = 0 PRBs (no allocation in Pass 2, handled in Pass 3)
Slice 1: target = min(0, 10) = 0 PRBs (no allocation in Pass 2, prioritized resources available for Pass 3)
Slice 2: target = min(0, 67) = 0 PRBs (no allocation in Pass 2, prioritized resources available for Pass 3)

Total prioritized needed: 0 PRBs (all slices have required_prbs = 0 or min = 0)

Available for Pass 2: 26 PRBs, but nothing to allocate
Remaining after Pass 2: 26 PRBs

Pass 3: Slice 0 will get PRBs based on required_prbs = 4
```

**Example 4: Common Rule - required_prbs > min_prb_ratio**:
```
After Pass 1:
Slice 1: 20 PRBs (dedicated), min = 30 PRBs, max = 50 PRBs, required = 40 PRBs
Slice 2: 10 PRBs (dedicated), min = 25 PRBs, max = 50 PRBs, required = 35 PRBs
Remaining: 70 PRBs

Common Rule: target = min(required_prbs, min_prb_ratio)
Slice 1: target = min(40, 30) = 30 PRBs, current = 20, needs 10 PRBs
Slice 2: target = min(35, 25) = 25 PRBs, current = 10, needs 15 PRBs

Prioritized resources (min - dedicated):
Slice 1: 30 - 20 = 10 PRBs prioritized → needs 10 (within prioritized portion)
Slice 2: 25 - 10 = 15 PRBs prioritized → needs 15 (within prioritized portion)
Total prioritized needed: 25 PRBs

Available: 70 PRBs > 25 PRBs needed
Scale = 1.0

Slice 1: 20 + 10 = 30 PRBs (reaches target = 30, remaining 10 needed in Pass 3)
Slice 2: 10 + 15 = 25 PRBs (reaches target = 25, remaining 10 needed in Pass 3)
Remaining: 70 - 25 = 45 PRBs (available for Pass 3)
```

**Example 5: Max Limit Constraint**:
```
After Pass 1:
Slice 1: 20 PRBs (dedicated), min = 30 PRBs, max = 25 PRBs, required = 35 PRBs
Slice 2: 10 PRBs (dedicated), min = 25 PRBs, max = 50 PRBs, required = 30 PRBs
Remaining: 70 PRBs

Prioritized needs:
Slice 1: needs 15 PRBs (35 - 20), but max = 25, so can only add 5
Slice 2: needs 20 PRBs (30 - 10), prioritized = 15, so needs 15
Total prioritized needed: 20 PRBs (5 + 15)

Available: 70 PRBs > 20 PRBs needed
Scale = 1.0

Slice 1: 20 + 5 = 25 PRBs (capped at max, can't meet required)
Slice 2: 10 + 15 = 25 PRBs (gets prioritized resources)
Remaining: 70 - 20 = 50 PRBs
```

**Key Rules for Pass 2**:
- **Common Rule**: Allocate up to `min(required_prbs, min_prb_ratio)` (within the prioritized portion)
  - Calculate `target_prbs = min(required_prbs, min_prb_ratio)` (if `required_prbs = 0`, then `target_prbs = 0`)
  - Allocate up to `target_prbs` in Pass 2
  - If `required_prbs < min_prb_ratio`: Remaining prioritized resources (`min_prb_ratio - required_prbs`) are available for Pass 3
- Never exceed `max_prb_ratio` in any pass

**Key Points**:
- Prioritized resources are only allocated if `required_prbs > current_allocation`
- If slice doesn't need prioritized resources, they remain available for Pass 3 (shared)
- Proportional scaling ensures fairness when resources are constrained
- Max limit is always enforced (may prevent meeting prioritized need)
- Backward compatible: if `required_prbs == 0`, uses traditional min guarantee logic

### Pass 3: Shared Resource Distribution

**Purpose**: Distribute shared resources (`max_prb_ratio - min_prb_ratio`) among all slices proportionally.

**Important Semantics**:
- **Shared resources** (`max - min`) are fully shareable between all slices
- These resources are distributed proportionally based on:
  - PRB requirements (`required_prbs`) if specified, or
  - Remaining capacity (up to max limit) if no requirements
- Each slice can grow up to its `max_prb_ratio` limit
- Resources are shared fairly among all slices that haven't reached their max

**Steps**:
1. **Calculate Remaining Capacity** (shared resources available):
   - For each active slice:
     - `max_prbs = total_prbs × max_prb_ratio`
     - `can_add = max_prbs - current_allocation` (shared resources this slice can use)
     - If `can_add > 0`, add to `total_capacity`

2. **Calculate PRB Deficits** (if requirements specified):
   - For each active slice with `required_prbs > 0`:
     - `prb_deficit = required_prbs - current_allocation`
     - If `prb_deficit > 0`, add to `total_prb_deficit`

3. **Allocation Strategy**:
   - **If PRB requirements exist**: Allocate shared resources based on PRB deficits (slices with higher deficits get priority)
   - **Otherwise**: Allocate shared resources proportionally based on remaining capacity
   - In both cases, respect `max_prb_ratio` limits (shared resources are bounded by max)

**Example 1: Shared Resources with PRB Requirements**:
```
After Pass 2:
Slice 1: 30 PRBs (dedicated + prioritized), min = 30 PRBs, max = 50 PRBs, required = 45 PRBs
Slice 2: 25 PRBs (dedicated + prioritized), min = 25 PRBs, max = 50 PRBs, required = 35 PRBs
Remaining: 45 PRBs (shared resources)

Shared resources available:
Slice 1: can_add = 50 - 30 = 20 PRBs (shared)
Slice 2: can_add = 50 - 25 = 25 PRBs (shared)
Total shared capacity: 45 PRBs

PRB deficits:
Slice 1: 45 - 30 = 15 PRBs needed
Slice 2: 35 - 25 = 10 PRBs needed
Total deficit: 25 PRBs

Allocate shared resources based on deficits (proportional, capped at deficit):
Slice 1: (15/25) × 45 = 27 PRBs, but:
  - Capped at deficit: min(27, 15) = 15 PRBs
  - Capped at capacity: min(15, 20) = 15 PRBs
  → 30 + 15 = 45 PRBs (meets requirement)
Slice 2: (10/25) × 45 = 18 PRBs, but:
  - Capped at deficit: min(18, 10) = 10 PRBs
  - Capped at capacity: min(10, 25) = 10 PRBs
  → 25 + 10 = 35 PRBs (meets requirement)
Allocated: 15 + 10 = 25 PRBs
Remaining: 45 - 25 = 20 PRBs

Distribute remaining 20 PRBs proportionally (based on capacity):
Slice 1: can_add = 50 - 45 = 5 PRBs
Slice 2: can_add = 50 - 35 = 15 PRBs
Total capacity: 20 PRBs
Slice 1: (5/20) × 20 = 5 PRBs → 45 + 5 = 50 PRBs (at max)
Slice 2: (15/20) × 20 = 15 PRBs → 35 + 15 = 50 PRBs (at max)
```

**Example 2: Shared Resources without PRB Requirements**:
```
After Pass 2:
Slice 1: 30 PRBs, min = 30 PRBs, max = 50 PRBs, required = 0 (not specified)
Slice 2: 25 PRBs, min = 25 PRBs, max = 50 PRBs, required = 0 (not specified)
Remaining: 45 PRBs (shared resources)

Shared resources available:
Slice 1: can_add = 50 - 30 = 20 PRBs
Slice 2: can_add = 50 - 25 = 25 PRBs
Total shared capacity: 45 PRBs

Distribute shared resources proportionally:
Slice 1: (20/45) × 45 = 20 PRBs → 30 + 20 = 50 PRBs (at max)
Slice 2: (25/45) × 45 = 25 PRBs → 25 + 25 = 50 PRBs (at max)
```

**Key Points**:
- Shared resources are distributed proportionally based on remaining capacity or PRB deficits
- Slices with more remaining capacity (closer to max) get proportionally more
- Ensures efficient resource utilization and fairness

### Pass 4: Contiguous Range Assignment

**Purpose**: Assign contiguous PRB ranges `[start_prb, end_prb)` to each slice.

**Steps**:
1. Initialize `current_prb = 0`
2. For each slice (in order) with `num_prbs > 0`:
   - `start_prb = current_prb`
   - `end_prb = current_prb + num_prbs`
   - `current_prb = end_prb`

**Result**: Non-overlapping, contiguous ranges covering all PRBs.

**Example**:
```
After Pass 3:
Slice 1: 50 PRBs
Slice 2: 50 PRBs

Range assignment:
Slice 1: [0, 50)   → PRBs 0-49
Slice 2: [50, 100) → PRBs 50-99
Total: 100 PRBs ✓
```

---

## Usage Examples

### Example 0: OOP-like Scheduler Interface

The module provides an OOP-like interface for managing slices dynamically:

```c
#include "slice_prb_allocator.h"

// Create scheduler
slice_scheduler_t *sch = slice_sch_create(106);  // 106 total PRBs

// Add slices with SST and SD
// Slice 1: SST=1, SD=0 (eMBB)
slice_sch_add_slice(sch, 1, 0, 0.33f, 0.33f, 0.50f, 0);

// Slice 2: SST=2, SD=0 (URLLC)
slice_sch_add_slice(sch, 2, 0, 0.20f, 0.20f, 0.50f, 0);

// Schedule allocation
slice_sch_schedule(sch);

// Get allocation results
int num_ranges = 0;
const slice_prb_range_t *ranges = slice_sch_get_allocation(sch, &num_ranges);
for (int i = 0; i < num_ranges; ++i) {
    printf("Slice (SST=%d, SD=%u): PRBs [%d, %d) (%d PRBs)\n",
           ranges[i].slice_id.sst,
           ranges[i].slice_id.sd,
           ranges[i].start_prb,
           ranges[i].end_prb,
           ranges[i].num_prbs);
}

// Get statistics for a specific slice
slice_statistics_t stats;
if (slice_sch_get_slice_statistics(sch, 1, 0, &stats) == 0) {
    printf("Slice (SST=%d, SD=%u): avg PRBs=%.1f\n",
           stats.slice_id.sst, stats.slice_id.sd, stats.avg_num_prbs);
}

// Update PRB requirement for a slice
slice_sch_update_require(sch, 1, 0, 40);

// Delete a slice
slice_sch_del_slice(sch, 2, 0);

// Cleanup
slice_sch_destroy(sch);
```

### Example 1: Basic Two-Slice Allocation

**Input**:
- Total PRBs: 100
- Slice 1: dedicated=30%, min=30%, max=50%
- Slice 2: dedicated=20%, min=20%, max=50%

**Result**: Both slices get 50 PRBs each, respecting their maximum limits.

### Example 2: Dedicated Exceeds Total

**Input**:
- Total PRBs: 100
- Slice 1: dedicated=60%, min=60%, max=60%
- Slice 2: dedicated=60%, min=60%, max=60%

**Result**: Fair 50/50 split despite both requesting 60% (proportional scaling).

### Example 3: Real-World 106 PRB Scenario

**Input**:
- Total PRBs: 106 (typical 5G NR bandwidth)
- Slice 1 (eMBB): dedicated=33%, min=33%, max=50%
- Slice 2 (URLLC): dedicated=20%, min=20%, max=50%

**Result**: Both slices get 53 PRBs (50% each), maximizing resource utilization.

### Example 4: With PRB Requirements

**Input**:
- Total PRBs: 100
- Slice 1: dedicated=30%, min=30%, max=50%, required_prbs=45
- Slice 2: dedicated=20%, min=20%, max=50%, required_prbs=35

**Result**: 
- Pass 1: Slice 1 gets 30 PRBs, Slice 2 gets 20 PRBs
- Pass 2: Both meet minimums (Slice 1: 30→30, Slice 2: 20→20)
- Pass 3: Allocate based on PRB deficits (Slice 1 needs 15 more, Slice 2 needs 15 more)
  - Slice 1: 30 + 15 = 45 PRBs (meets requirement)
  - Slice 2: 20 + 15 = 35 PRBs (meets requirement)
  - Remaining: 20 PRBs distributed proportionally up to max

---

## Test Coverage

The unit tests cover:

1. **Basic allocation** - Two slices with different ratios
2. **Single slice** - All PRBs allocated to one slice
3. **No active UEs** - Slices without active UEs get no PRBs
4. **Multiple slices** - Three slices with different ratios
5. **Dedicated exceeds total** - Proportional scaling when dedicated > 100%
6. **Max ratio enforcement** - Hard limit on maximum PRBs
7. **Validation** - Input validation and error handling
8. **Edge cases** - Zero PRBs, invalid ratios, etc.
9. **Real-world scenario** - 106 PRBs with 2 slices (typical 5G NR)

Run tests with:
```bash
make run
```

---

## Edge Cases and Handling

1. **No Active UEs**: Slice is skipped, gets 0 PRBs
2. **Dedicated = Min = Max**: Static allocation, Pass 1 handles all
3. **Minimums Exceed Total**: Proportional scaling in Pass 2
4. **Maximum Limits Prevent Full Allocation**: Pass 3 respects max limits
5. **Single Slice**: Gets all PRBs up to max limit
6. **Dedicated > Total**: Proportional scaling in Pass 1
7. **Max < Min**: Max limit takes precedence
8. **Rounding Errors**: Handled with proper rounding (`.5` offset)

---

## Algorithm Constraints and Verification

### Constraint Validation

1. **Ratio Relationships**: `dedicated_prb_ratio ≤ min_prb_ratio ≤ max_prb_ratio`
   - Validated in `validate_slice_config()`
   - Ensures logical consistency of slice configuration

2. **Ratio Range**: All ratios must be in [0.0, 1.0]
   - Validated in `validate_slice_config()`
   - Prevents invalid percentage values

3. **Total Allocation**: Sum of allocated PRBs equals `total_prbs`
   - Verified in Pass 4: all PRBs are allocated contiguously
   - No gaps or unallocated PRBs

4. **Contiguous Ranges**: PRB ranges are contiguous and non-overlapping
   - Ensured in Pass 4: ranges are assigned sequentially
   - `end_prb` of slice N = `start_prb` of slice N+1

5. **Hard Limits**: `max_prb_ratio` is never exceeded
   - Enforced in Pass 1, 2, and 3
   - Checked before every allocation: `if (allocated + additional > max_prbs)`

### Algorithm Correctness Verification

#### Pass 1 Verification
- ✅ Allocates `dedicated_prb_ratio` to each active slice
- ✅ Handles over-allocation with proportional scaling
- ✅ Ensures `dedicated ≤ max` (capped if needed)
- ✅ Ensures `min ≥ dedicated` (adjusted if needed)

#### Pass 2 Verification
- ✅ Allocates prioritized resources based on `required_prbs` (only if slice needs them)
- ✅ Handles insufficient resources with proportional scaling
- ✅ Respects `max_prb_ratio` limit (may prevent meeting prioritized need if max < min)
- ✅ Updates state correctly (`allocated_prbs`, `remaining_prbs`)
- ✅ Prioritized resources not claimed remain available for Pass 3

#### Pass 3 Verification
- ✅ Considers PRB requirements when specified (`required_prbs > 0`)
- ✅ Falls back to capacity-based allocation when no requirements
- ✅ Respects `max_prb_ratio` limit in all cases
- ✅ Distributes all remaining PRBs when possible

#### Pass 4 Verification
- ✅ Assigns contiguous, non-overlapping ranges
- ✅ Covers all allocated PRBs (no gaps)
- ✅ Maintains correct `start_prb` and `end_prb` relationships

---

## Integration

This algorithm is extracted from the OAI gNB MAC scheduler (`gNB_scheduler_dlsch.c`). To integrate:

1. Include the header file in your scheduler:
   ```c
   #include "slice_prb_allocator.h"
   ```

2. Convert OAI slice structures to `slice_config_t`:
   ```c
   slice_config_t slice_config;
   // Create slice_nssai_t from SST and SD (or use slice_nssai_from_int() for backward compatibility)
   slice_config.slice_id = slice_nssai_create(oai_slice->sst, oai_slice->sd);
   // Or if you have an integer slice_id: slice_config.slice_id = slice_nssai_from_int(oai_slice->slice_id);
   slice_config.dedicated_prb_ratio = oai_slice->dedicated_prb_ratio;
   slice_config.min_prb_ratio = oai_slice->min_prb_ratio;
   slice_config.max_prb_ratio = oai_slice->max_prb_ratio;
   slice_config.required_prbs = calculate_current_prb_requirement(oai_slice);
   ```

3. Call `calculate_slice_prb_ranges()` with appropriate input

4. Use the resulting PRB ranges for frequency-domain scheduling

---

## Performance

### Complexity Analysis

**Time Complexity**:
- **Pass 1**: O(N) - Single loop through slices
- **Pass 2**: O(N) - Calculate needs, then allocate
- **Pass 3**: O(N) - Calculate capacity/deficits, then distribute
- **Pass 4**: O(N) - Assign ranges
- **Total**: **O(N)** where N is the number of slices

**Space Complexity**:
- **Input**: O(N) - Array of slice configurations
- **Output**: O(N) - Array of PRB ranges
- **Temporary variables**: O(1) - Counters and accumulators
- **Total**: **O(N)** for storing results

### Practical Performance

For typical use cases:
- **N = 2-6 slices** (common in 5G networks)
- **Total PRBs = 100-106** (typical bandwidths)
- **Execution time**: < 1 microsecond on modern CPUs
- **Memory**: < 1 KB for all data structures

---

## Algorithm Properties

### Correctness Guarantees

1. **Constraint Satisfaction**:
   - ✅ `dedicated_prb_ratio ≤ min_prb_ratio ≤ max_prb_ratio` (validated)
   - ✅ All allocations ≤ `max_prb_ratio` (hard limit enforced)
   - ✅ All allocations ≥ `dedicated_prb_ratio` (when resources available)

2. **Completeness**:
   - ✅ All PRBs are allocated (no gaps)
   - ✅ Ranges are contiguous and non-overlapping

3. **Fairness**:
   - ✅ Proportional scaling when resources are constrained
   - ✅ No slice gets zero if it has active UEs and requested resources
   - ✅ PRB requirements are considered when specified

### Invariants

Throughout the algorithm, the following invariants are maintained:

1. `Σ(allocated_i) ≤ total_prbs` (never over-allocate)
2. `allocated_i ≤ max_prbs_i` (respect maximum limits)
3. `allocated_i ≥ dedicated_prbs_i` (when possible)
4. `remaining_prbs = total_prbs - Σ(allocated_i)` (consistent accounting)

---

## References

- **3GPP TS 38.300**: NR and NG-RAN Overall Description
- **3GPP TS 38.321**: NR Medium Access Control (MAC) protocol specification
- **OAI RAN**: OpenAirInterface Radio Access Network

---

## Conclusion

The Network Slice PRB Allocation Algorithm provides a robust, fair, and efficient mechanism for distributing PRB resources among network slices. Its four-pass design ensures:

- **Predictability**: Dedicated and minimum guarantees
- **Flexibility**: Maximum limits allow dynamic sharing
- **Efficiency**: Proportional distribution maximizes utilization
- **Correctness**: Hard limits and constraints are always respected
- **Adaptability**: PRB requirements allow priority-based allocation

The algorithm is designed for real-time execution in 5G NR MAC schedulers, with O(N) complexity suitable for typical network configurations.
