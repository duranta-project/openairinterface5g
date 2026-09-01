/*!
 * \file slice_prb_allocator.h
 * \brief Network Slice PRB Range Allocation Algorithm - Public OOP Interface
 * 
 * This is the public API for the Network Slice PRB Range Allocation Algorithm.
 * Normal OAI users should use the OOP-like scheduler interface provided here.
 * 
 * For internal functional implementation, see slice_prb_allocator_internal.h
 *
 * MAC integration: gNB YAML Slices block → set_slice_config() creates
 * slice_scheduler_dl / slice_scheduler_ul on the MAC instance. Each slot,
 * nr_dl_schedule_ns() / nr_ul_schedule_ns() call slice_sch_update_require(),
 * slice_sch_schedule(), then pass slice_prb_range_t to nr_*_schedule() for
 * intra-slice UE scheduling.
 */

#ifndef SLICE_PRB_ALLOCATOR_H
#define SLICE_PRB_ALLOCATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  /* For NULL */

/*! \brief Slice identifier with SST and SD bit fields (S-NSSAI)
 *  \note This is named slice_nssai_t to avoid conflict with OAI's slice_id_t (uint8_t) in platform_types.h
 */
typedef struct {
  uint32_t sst : 8;   /*!< Slice/Service Type (8 bits, 0-255) */
  uint32_t sd : 24;   /*!< Slice Differentiator (24 bits, 0-0xffffff) */
} slice_nssai_t;

/*! \brief Helper: Create slice_nssai_t from SST and SD values
 *  \param sst Slice/Service Type (0-255)
 *  \param sd Slice Differentiator (0-0xffffff)
 *  \return slice_nssai_t structure
 */
static inline slice_nssai_t slice_nssai_create(uint8_t sst, uint32_t sd) {
  slice_nssai_t id = {.sst = sst, .sd = sd & 0xffffff};
  return id;
}

/*! \brief Helper: Create slice_nssai_t from integer (treats int as simple ID, sets sst=int, sd=0)
 *  \param id Integer slice identifier
 *  \return slice_nssai_t structure
 */
static inline slice_nssai_t slice_nssai_from_int(int id) {
  slice_nssai_t sid = {.sst = (uint8_t)(id & 0xff), .sd = 0};
  return sid;
}

/*! \brief Helper: Compare slice_nssai_t with integer slice_id
 *  \param sid slice_nssai_t structure
 *  \param id Integer slice identifier
 *  \return true if they match (based on sst field), false otherwise
 */
static inline bool slice_nssai_eq_int(const slice_nssai_t *sid, int id) {
  return (sid != NULL && sid->sst == (id & 0xff) && sid->sd == 0);
}

/*! \brief Helper: Compare two slice_nssai_t structures
 *  \param sid1 First slice_nssai_t structure
 *  \param sid2 Second slice_nssai_t structure
 *  \return true if they match, false otherwise
 */
static inline bool slice_nssai_eq(const slice_nssai_t *sid1, const slice_nssai_t *sid2) {
  return (sid1 != NULL && sid2 != NULL && 
          sid1->sst == sid2->sst && sid1->sd == sid2->sd);
}

/*! \brief PRB range allocation result for a slice (used in OOP interface) */
typedef struct {
  slice_nssai_t slice_id;  /*!< Slice ID with SST and SD */
  int start_prb;  /*!< Inclusive start PRB index */
  int end_prb;    /*!< Exclusive end PRB index (end_prb - start_prb = num_prbs) */
  int num_prbs;   /*!< Number of PRBs allocated */
} slice_prb_range_t;

/* ============================================================================
 * OOP-like Scheduler Interface
 * ============================================================================ */

/*! \brief Statistics for a single slice */
typedef struct {
  slice_nssai_t slice_id;                    /*!< Slice ID with SST and SD */
  int latest_start_prb;                    /*!< Latest start PRB index */
  int latest_end_prb;                      /*!< Latest end PRB index */
  int latest_num_prbs;                     /*!< Latest number of PRBs */
  float avg_start_prb;                     /*!< Moving average of start PRB */
  float avg_end_prb;                       /*!< Moving average of end PRB */
  float avg_num_prbs;                      /*!< Moving average of number of PRBs */
  int sample_count;                        /*!< Number of samples for moving average */
} slice_statistics_t;

/* Forward declarations for internal structures (opaque to users) */
typedef struct slice_alloc_input slice_alloc_input_t;
typedef struct slice_alloc_result slice_alloc_result_t;

/*! \brief Slice scheduler object that manages slices and allocations */
typedef struct {
  slice_alloc_input_t *input;              /*!< Internal: Input structure with slice configurations */
  slice_alloc_result_t *result;            /*!< Internal: Result structure with PRB allocations */
  slice_statistics_t *statistics;          /*!< Statistics for each slice */
  int slices_capacity;                     /*!< Current capacity for reallocation */
  bool result_valid;                       /*!< Whether the result is up-to-date */
} slice_scheduler_t;

/*! \brief Create and initialize a new slice scheduler
 *  \param total_prbs Total number of PRBs available
 *  \return Pointer to initialized scheduler, or NULL on error
 */
slice_scheduler_t* slice_sch_create(int total_prbs);

/*! \brief Destroy a slice scheduler and free resources
 *  \param obj Pointer to scheduler object (can be NULL)
 */
void slice_sch_destroy(slice_scheduler_t *obj);

/*! \brief Add or update a slice in the scheduler
 *  \param obj Scheduler object
 *  \param sst Slice/Service Type (0-255)
 *  \param sd Slice Differentiator (0-0xffffff)
 *  \param dedicated Dedicated PRB ratio (0.0-1.0)
 *  \param min Minimum PRB ratio (0.0-1.0)
 *  \param max Maximum PRB ratio (0.0-1.0)
 *  \param require Current PRB requirement (0 = not used)
 *  \return 0 on success (added or updated), -1 on error
 *  \note If a slice with the same SST/SD already exists, its parameters will be updated.
 *        Statistics for existing slices are preserved.
 */
int slice_sch_add_slice(slice_scheduler_t *obj, uint8_t sst, uint32_t sd, float dedicated,
                        float min, float max, int require);

/*! \brief Delete a slice from the scheduler
 *  \param obj Scheduler object
 *  \param sst Slice/Service Type (0-255)
 *  \param sd Slice Differentiator (0-0xffffff)
 *  \return 0 on success (deleted or already doesn't exist), -1 on error
 *  \note This operation is idempotent: deleting a non-existing slice returns success
 *        since the desired state (slice not in scheduler) is already achieved.
 */
int slice_sch_del_slice(slice_scheduler_t *obj, uint8_t sst, uint32_t sd);

/*! \brief Update the PRB requirement for a slice
 *  \param obj Scheduler object
 *  \param sst Slice/Service Type (0-255)
 *  \param sd Slice Differentiator (0-0xffffff)
 *  \param require New PRB requirement (0 = not used)
 *  \return 0 on success, -1 on error (slice not found)
 */
int slice_sch_update_require(slice_scheduler_t *obj, uint8_t sst, uint32_t sd, int require);

/*! \brief Get the PRB requirement for a slice
 *  \param obj Scheduler object
 *  \param sst Slice/Service Type (0-255)
 *  \param sd Slice Differentiator (0-0xffffff)
 *  \param require Output: PRB requirement (0 = not used)
 *  \return 0 on success, -1 on error (slice not found)
 */
int slice_sch_get_require(const slice_scheduler_t *obj, uint8_t sst, uint32_t sd, int *require);

/*! \brief Update the total PRBs available for allocation
 *  \param obj Scheduler object
 *  \param total_prbs New total number of PRBs (must be > 0)
 *  \return 0 on success, -1 on error
 */
int slice_sch_update_total_prbs(slice_scheduler_t *obj, int total_prbs);

/*! \brief Perform scheduling/allocation of PRBs to slices
 *  \param obj Scheduler object
 *  \return 0 on success, -1 on error
 */
int slice_sch_schedule(slice_scheduler_t *obj);

/*! \brief Get the current allocation result (const)
 *  \param obj Scheduler object
 *  \param num_ranges Output: Number of ranges in the result
 *  \return Pointer to const array of PRB ranges, or NULL on error
 */
const slice_prb_range_t* slice_sch_get_allocation(const slice_scheduler_t *obj, int *num_ranges);

/*! \brief Get allocation statistics
 *  \param obj Scheduler object
 *  \param num_active_slices Output: Number of slices (all slices are considered active)
 *  \param total_allocated_prbs Output: Total allocated PRBs
 *  \return 0 on success, -1 on error
 */
int slice_sch_get_stats(const slice_scheduler_t *obj, int *num_active_slices, int *total_allocated_prbs);

/*! \brief Get statistics for a specific slice
 *  \param obj Scheduler object
 *  \param sst Slice/Service Type (0-255)
 *  \param sd Slice Differentiator (0-0xffffff)
 *  \param stats Output: Statistics structure (can be NULL to just check existence)
 *  \return 0 on success, -1 if slice not found
 */
int slice_sch_get_slice_statistics(const slice_scheduler_t *obj, uint8_t sst, uint32_t sd, slice_statistics_t *stats);

/*! \brief Get all slice statistics
 *  \param obj Scheduler object
 *  \param num_stats Output: Number of statistics entries
 *  \return Pointer to statistics array, or NULL on error
 */
const slice_statistics_t* slice_sch_get_all_statistics(const slice_scheduler_t *obj, int *num_stats);

/*! \brief Get number of slices in the scheduler
 *  \param obj Scheduler object
 *  \return Number of slices, or -1 on error
 */
int slice_sch_get_num_slices(const slice_scheduler_t *obj);

/*! \brief Get total PRBs available for allocation
 *  \param obj Scheduler object
 *  \return Total number of PRBs, or -1 on error
 */
int slice_sch_get_total_prbs(const slice_scheduler_t *obj);

/*! \brief Get slice configuration by SST and SD
 *  \param obj Scheduler object
 *  \param sst Slice/Service Type (0-255)
 *  \param sd Slice Differentiator (0-0xffffff)
 *  \param sst_out Output: SST (can be NULL)
 *  \param sd_out Output: SD (can be NULL)
 *  \param dedicated_out Output: Dedicated PRB ratio (can be NULL)
 *  \param min_out Output: Minimum PRB ratio (can be NULL)
 *  \param max_out Output: Maximum PRB ratio (can be NULL)
 *  \return 0 on success, -1 if slice not found
 */
int slice_sch_get_slice_config(const slice_scheduler_t *obj, uint8_t sst, uint32_t sd,
                               uint8_t *sst_out, uint32_t *sd_out,
                               float *dedicated_out, float *min_out, float *max_out);

/*! \brief Get slice NSSAI by index
 *  \param obj Scheduler object
 *  \param slice_index Slice index (0-based)
 *  \return Pointer to slice_nssai_t, or NULL on error
 */
const slice_nssai_t* slice_sch_get_slice_nssai(const slice_scheduler_t *obj, int slice_index);

#endif /* SLICE_PRB_ALLOCATOR_H */
