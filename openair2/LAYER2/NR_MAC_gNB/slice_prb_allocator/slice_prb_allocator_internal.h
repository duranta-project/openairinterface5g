/*!
 * \file slice_prb_allocator_internal.h
 * \brief Internal Network Slice PRB Range Allocation Algorithm (Functional Implementation)
 * 
 * This header contains the internal functional implementation of the PRB allocation algorithm.
 * It is intended for internal use only and should not be included by normal OAI users.
 * 
 * For public API, use slice_prb_allocator.h instead.
 */

#ifndef SLICE_PRB_ALLOCATOR_INTERNAL_H
#define SLICE_PRB_ALLOCATOR_INTERNAL_H

#include "slice_prb_allocator.h"  // For slice_prb_range_t
#include <stdint.h>
#include <stdbool.h>

/*! \brief Network slice configuration */
typedef struct {
  slice_nssai_t slice_id;       /*!< Slice identifier with SST and SD */
  float dedicated_prb_ratio; /*!< Dedicated PRB ratio (0.0-1.0), non-shareable */
  float min_prb_ratio;       /*!< Minimum PRB ratio (0.0-1.0), guaranteed */
  float max_prb_ratio;       /*!< Maximum PRB ratio (0.0-1.0), hard limit */
  int required_prbs;         /*!< Current PRB requirement for this slice (0 = not used, all symbols allocated) */
} slice_config_t;

/* Note: slice_prb_range_t is defined in slice_prb_allocator.h (public header) */

/*! \brief Input parameters for PRB allocation */
struct slice_alloc_input {
  int num_slices;                          /*!< Number of configured slices */
  int total_prbs;                          /*!< Total number of PRBs available */
  slice_config_t slices[];                 /*!< Flexible array member: slice configurations */
};

/*! \brief Output result of PRB allocation */
struct slice_alloc_result {
  int total_allocated_prbs;                /*!< Total PRBs allocated (should equal total_prbs) */
  slice_prb_range_t ranges[];              /*!< Flexible array member: PRB ranges for each slice */
};

/* Type aliases (matching forward declarations in public header) */
typedef struct slice_alloc_input slice_alloc_input_t;
typedef struct slice_alloc_result slice_alloc_result_t;

/*! \brief Allocate slice_alloc_input_t with flexible array member
 *  \param num_slices Number of slices
 *  \return Allocated structure or NULL on error
 *  \note Caller must free with free_slice_input()
 */
slice_alloc_input_t* allocate_slice_input(int num_slices);

/*! \brief Allocate slice_alloc_result_t with flexible array member
 *  \param num_slices Number of slices (for array size)
 *  \return Allocated structure or NULL on error
 *  \note Caller must free with free_slice_result()
 */
slice_alloc_result_t* allocate_slice_result(int num_slices);

/*! \brief Free slice_alloc_input_t
 *  \param input Structure to free (can be NULL)
 */
void free_slice_input(slice_alloc_input_t *input);

/*! \brief Free slice_alloc_result_t
 *  \param result Structure to free (can be NULL)
 */
void free_slice_result(slice_alloc_result_t *result);

/*! \brief Calculate PRB ranges for each slice
 *  \param input Input parameters (slice configs, total PRBs, PRB requirements)
 *  \param result Output result (PRB ranges for each slice)
 *  \return Number of slices with allocated PRBs, or -1 on error
 * 
 *  Algorithm:
 *  1. Allocate dedicated PRBs to all slices
 *  2. If dedicated allocations exceed total, scale them down proportionally
 *  3. Distribute remaining PRBs to meet minimum guarantees
 *  4. Distribute any remaining PRBs considering PRB requirements (slices with higher
 *     PRB needs get priority, up to their maximum limits)
 *  5. Assign contiguous PRB ranges
 * 
 *  Note: If required_prbs is 0 for a slice, requirement-based allocation is not used
 *        for that slice and algorithm falls back to proportional distribution.
 *  Note: Input and result structures must be allocated with allocate_slice_input()
 *        and allocate_slice_result() respectively.
 */
int calculate_slice_prb_ranges(const slice_alloc_input_t *input, slice_alloc_result_t *result);

/*! \brief Validate slice configuration
 *  \param input Input parameters to validate
 *  \return true if valid, false otherwise
 */
bool validate_slice_config(const slice_alloc_input_t *input);

/*! \brief Print allocation result (for debugging)
 *  \param result Allocation result to print
 *  \param num_slices Number of slices in the input (to know array size)
 */
void print_slice_allocation(const slice_alloc_result_t *result, int num_slices);

/*! \brief Pass 1: Allocate dedicated PRBs (non-shareable)
 *  \param input Input parameters
 *  \param result Result structure (will be updated with dedicated allocations)
 *  \param allocated_prbs Output: Total PRBs allocated after this pass
 *  \return 0 on success, -1 on error
 */
int pass1_allocate_dedicated(const slice_alloc_input_t *input, slice_alloc_result_t *result,
                              int *allocated_prbs);

/*! \brief Pass 2: Allocate prioritized resources (min - dedicated) based on required_prbs
 *  \param input Input parameters
 *  \param result Result structure (will be updated with prioritized allocations)
 *  \param allocated_prbs Input/Output: Current allocated PRBs, updated after this pass
 *  \param remaining_prbs Input/Output: Remaining PRBs, updated after this pass
 *  \return 0 on success, -1 on error
 */
int pass2_allocate_prioritized(const slice_alloc_input_t *input, slice_alloc_result_t *result,
                                int *allocated_prbs, int *remaining_prbs);

/*! \brief Pass 3: Allocate shared resources recursively by max_prb_ratio weights
 *
 *  Repeatedly splits the remaining pool across active slices proportional to each
 *  slice's max PRB cap (max_s / sum(max_s)). A slice that hits require or max
 *  leaves the active set; unused share from that round is returned to the pool.
 *
 *  \param input Input parameters
 *  \param result Result structure (will be updated with shared allocations)
 *  \param allocated_prbs Input/Output: Current allocated PRBs, updated after this pass
 *  \param remaining_prbs Input/Output: Remaining PRBs, updated after this pass
 *  \return 0 on success, -1 on error
 */
int pass3_allocate_shared(const slice_alloc_input_t *input, slice_alloc_result_t *result,
                          int *allocated_prbs, int *remaining_prbs);

/*! \brief Pass 4: Assign contiguous PRB ranges
 *  \param input Input parameters
 *  \param result Result structure (will be updated with start_prb and end_prb)
 *  \return 0 on success, -1 on error
 */
int pass4_assign_ranges(const slice_alloc_input_t *input, slice_alloc_result_t *result);

#endif /* SLICE_PRB_ALLOCATOR_INTERNAL_H */
