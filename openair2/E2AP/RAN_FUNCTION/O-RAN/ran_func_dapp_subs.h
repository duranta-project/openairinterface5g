/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef RAN_FUNC_SM_DAPP_SUBSCRIPTION_AGENT_H
#define RAN_FUNC_SM_DAPP_SUBSCRIPTION_AGENT_H

#include "openair2/E2AP/flexric/src/sm/dapp_sm/ie/dapp_data_ie.h"
#include "common/utils/ds/seq_arr.h"

/**
 * @brief Per-SM subscription bookkeeping for the DAPP service model.
 *
 * Tracks which RIC request IDs are registered for each indication format.
 * Used by the DAPP agent to remove subscriptions when the RIC deletes them.
 */
typedef struct {
  seq_arr_t frmt_1_subs; /**< RIC request IDs subscribed to Format 1 (E3 data reports) */
  seq_arr_t frmt_2_subs; /**< RIC request IDs subscribed to Format 2 (E3 subscription map) */
} dapp_subs_data_t;

/**
 * @brief Initialize both per-SM subscription lists.
 *
 * Must be called once before any insert/remove operations.
 *
 * @param dapp_subs_data  Pointer to the per-SM subscription state.
 */
void init_dapp_subs_data(dapp_subs_data_t *dapp_subs_data);

/**
 * @brief Remove a RIC request ID from all per-SM lists and global registries.
 *
 * Searches both frmt_1_subs and frmt_2_subs for @p ric_req_id.
 * For each list where it is found, erases it locally and calls the
 * corresponding global remove function. Logs a warning if the ID
 * is not found in either list.
 *
 * @param dapp_subs_data  Pointer to the per-SM subscription state.
 * @param ric_req_id      The RIC request ID to remove.
 */
void remove_dapp_subs_data(dapp_subs_data_t *dapp_subs_data, uint32_t ric_req_id);

/**
 * @brief Record a RIC request ID in the per-SM Format 1 list.
 *
 * @param d           Pointer to the per-SM subscription state.
 * @param ric_req_id  The RIC request ID to insert.
 */
void insert_frmt_1_ric_id(dapp_subs_data_t *d, uint32_t ric_req_id);

/**
 * @brief Add a RIC request ID to the global Format 1 registry.
 *
 * The global registry is used by generate_e2_indication_from_e3_dapp_report()
 * to find all subscribers that should receive E3 data report indications.
 * Duplicates are silently ignored.
 *
 * @param ric_req_id  The RIC request ID to register.
 */
void ric_subs_frmt1_add(uint32_t ric_req_id);

/**
 * @brief Remove a RIC request ID from the global Format 1 registry.
 *
 * @param ric_req_id  The RIC request ID to unregister.
 */
void ric_subs_frmt1_remove(uint32_t ric_req_id);

/**
 * @brief Snapshot all RIC request IDs in the global Format 1 registry.
 *
 * Allocates and returns a caller-owned array of RIC request IDs.
 * The caller must free() the returned array.
 *
 * @param[out] out  Set to the allocated array, or NULL if empty.
 * @return          Number of IDs in the snapshot (0 if none).
 */
size_t ric_subs_frmt1_snapshot(uint32_t **out);

/**
 * @brief Record a RIC request ID in the per-SM Format 2 list.
 *
 * @param d           Pointer to the per-SM subscription state.
 * @param ric_req_id  The RIC request ID to insert.
 */
void insert_frmt_2_ric_id(dapp_subs_data_t *d, uint32_t ric_req_id);

/**
 * @brief Add a RIC request ID to the global Format 2 registry.
 *
 * The global registry is used by generate_e2_indication_dapp_e3_subscriptions()
 * to find all subscribers that should receive subscription map indications.
 * Duplicates are silently ignored.
 *
 * @param ric_req_id  The RIC request ID to register.
 */
void ric_subs_frmt2_add(uint32_t ric_req_id);

/**
 * @brief Remove a RIC request ID from the global Format 2 registry.
 *
 * @param ric_req_id  The RIC request ID to unregister.
 */
void ric_subs_frmt2_remove(uint32_t ric_req_id);

/**
 * @brief Snapshot all RIC request IDs in the global Format 2 registry.
 *
 * Allocates and returns a caller-owned array of RIC request IDs.
 * The caller must free() the returned array.
 *
 * @param[out] out  Set to the allocated array, or NULL if empty.
 * @return          Number of IDs in the snapshot (0 if none).
 */
size_t ric_subs_frmt2_snapshot(uint32_t **out);

#endif
