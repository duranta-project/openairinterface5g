/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdbool.h>
#include <stdint.h>
#include "rrc_cell_management.h"
#include "common/utils/assertions.h" // for DevAssert
#include "common/utils/collection/tree.h" // for RB_FOREACH
#include "common/utils/alg/find.h" // for find_if
#include "common/utils/ds/seq_arr.h" // for seq_arr_t, seq_arr_free
#include "common/utils/LOG/log.h" // for LOG_I, LOG_W, LOG_E
#include "openair2/F1AP/f1ap_ids.h" // for cu_exists_f1_ue_data, cu_get_f1_ue_data
#include "asn_application.h" // for ASN_STRUCT_FREE
#include "NR_MIB.h" // for asn_DEF_NR_MIB
#include "NR_SIB1.h" // for asn_DEF_NR_SIB1
#include "NR_MeasurementTimingConfiguration.h" // for asn_DEF_NR_MeasurementTimingConfiguration

/** @brief Comparison function for DU tree
 * @param[in] a Pointer to first DU container
 * @param[in] b Pointer to second DU container
 * @return Comparison result */
int du_compare(const nr_rrc_du_container_t *a, const nr_rrc_du_container_t *b)
{
  if (a->assoc_id > b->assoc_id)
    return 1;
  if (a->assoc_id == b->assoc_id)
    return 0;
  return -1; /* a->assoc_id < b->assoc_id */
}

// Generate RB tree functions for DU tree
RB_GENERATE(rrc_du_tree, nr_rrc_du_container_t, entries, du_compare);

/** @brief Find DU by SCTP association ID (uses DU RB-tree lookup)
 * @param[in] rrc Pointer to RRC instance
 * @param[in] assoc_id SCTP association ID
 * @return Pointer to the DU container, or NULL if not found
 * @note O(log N_DU) complexity using DU RB-tree lookup */
nr_rrc_du_container_t *get_du_by_assoc_id(gNB_RRC_INST *rrc, const sctp_assoc_t assoc_id)
{
  DevAssert(rrc != NULL);
  nr_rrc_du_container_t e = {.assoc_id = assoc_id};
  return RB_FIND(rrc_du_tree, &rrc->dus, &e);
}

/** @brief Add DU to RRC DU tree and increment rrc->num_dus on success
 * @param[in] rrc Pointer to RRC instance
 * @param[in] du Pointer to DU container to add
 * @return NULL if inserted successfully; pointer to existing DU (collision) if duplicate assoc_id */
nr_rrc_du_container_t *rrc_add_du(gNB_RRC_INST *rrc, nr_rrc_du_container_t *du)
{
  DevAssert(rrc != NULL);
  DevAssert(du != NULL);
  nr_rrc_du_container_t *collision = RB_INSERT(rrc_du_tree, &rrc->dus, du);
  if (collision == NULL)
    rrc->num_dus++;
  return collision;
}

/** @brief Free a DU container and its allocated fields (gNB_DU_name); free the DU itself
 * @param[in] du Pointer to DU container to free
 * @note Does NOT remove the DU from the tree or free du->cells; caller must do RB_REMOVE and seq_arr_free first */
void rrc_free_du_container(nr_rrc_du_container_t *du)
{
  DevAssert(du != NULL);
  free(du->gNB_DU_name);
  free(du);
}

/** @brief Remove DU from RRC DU tree, decrement rrc->num_dus, and free the DU and its resources
 * @param[in] rrc Pointer to RRC instance
 * @param[in] du Pointer to DU container to remove (caller must have removed and freed all cells first) */
void rrc_rm_du(gNB_RRC_INST *rrc, nr_rrc_du_container_t *du)
{
  DevAssert(rrc != NULL);
  DevAssert(du != NULL);
  nr_rrc_du_container_t *removed = RB_REMOVE(rrc_du_tree, &rrc->dus, du);
  AssertFatal(removed == du, "Failed to remove DU (assoc_id %d) from tree\n", du->assoc_id);
  rrc->num_dus--;
  seq_arr_free(&du->cells, NULL);
  rrc_free_du_container(du);
}

/** @brief Remove all cells from the DU (and from global cell tree), remove the DU from the tree, and free the DU
 * @param[in] rrc Pointer to RRC instance
 * @param[in] du Pointer to DU container to cleanup (removed from tree and freed) */
void rrc_cleanup_du(gNB_RRC_INST *rrc, nr_rrc_du_container_t *du)
{
  DevAssert(rrc != NULL);
  DevAssert(du != NULL);
  for (size_t i = seq_arr_size(&du->cells); i > 0; i--) {
    nr_rrc_cell_container_t **cell_ptr = seq_arr_at(&du->cells, i - 1);
    nr_rrc_cell_container_t *cell = *cell_ptr;
    LOG_I(NR_RRC, "Removing cell %lu from DU cells array\n", cell->info.cell_id);
    seq_arr_erase(&du->cells, cell_ptr);
    rrc_rm_cell(rrc, cell);
  }
  rrc_rm_du(rrc, du);
}

/** @brief Comparison function for cell tree
 * @param[in] a Pointer to first cell container
 * @param[in] b Pointer to second cell container
 * @return Comparison result */
int rrc_cell_cmp(const struct nr_rrc_cell_container_t *a, const struct nr_rrc_cell_container_t *b)
{
  if (a->info.cell_id < b->info.cell_id)
    return -1;
  if (a->info.cell_id > b->info.cell_id)
    return 1;
  return 0;
}

/** @brief Generate RB tree functions for cell tree */
RB_GENERATE(rrc_cell_tree, nr_rrc_cell_container_t, entries, rrc_cell_cmp);

/** @brief Find cell container by cell ID (lookup in global cell tree)
 * @param[in] cells Pointer to global cell tree
 * @param[in] cell_id Cell ID
 * @return Pointer to the cell container, or NULL if not found
 * @note O(log N_CELL) using RB-tree lookup */
nr_rrc_cell_container_t *get_cell_by_cell_id(struct rrc_cell_tree *cells, const uint64_t cell_id)
{
  DevAssert(cells != NULL);
  nr_rrc_cell_container_t search_key = {.info = {.cell_id = cell_id}};
  return RB_FIND(rrc_cell_tree, cells, &search_key);
}

/** @brief Add cell to global cell tree and increment rrc->num_cells on success
 * @param[in] rrc Pointer to RRC instance
 * @param[in] cell Pointer to cell container to add
 * @return NULL if inserted successfully; pointer to existing cell (collision) if duplicate cell_id */
nr_rrc_cell_container_t *rrc_add_cell(gNB_RRC_INST *rrc, nr_rrc_cell_container_t *cell)
{
  DevAssert(rrc != NULL);
  DevAssert(cell != NULL);
  nr_rrc_cell_container_t *collision = RB_INSERT(rrc_cell_tree, &rrc->cells, cell);
  if (collision == NULL)
    rrc->num_cells++;
  return collision;
}

/** @brief Remove cell from global cell tree, decrement rrc->num_cells, and free the cell
 * @param[in] rrc Pointer to RRC instance
 * @param[in] cell Pointer to cell container to remove */
void rrc_rm_cell(gNB_RRC_INST *rrc, nr_rrc_cell_container_t *cell)
{
  DevAssert(rrc != NULL);
  DevAssert(cell != NULL);
  nr_rrc_cell_container_t *removed = RB_REMOVE(rrc_cell_tree, &rrc->cells, cell);
  AssertFatal(removed == cell, "Failed to remove cell %ld from tree\n", cell->info.cell_id);
  rrc->num_cells--;
  rrc_free_cell_container(cell);
}

static bool eq_cell_id(const void *vval, const void *vit)
{
  const uint64_t *id = (const uint64_t *)vval;
  const nr_rrc_cell_container_t **elem_ptr = (const nr_rrc_cell_container_t **)vit;
  return (*elem_ptr) != NULL && (*elem_ptr)->info.cell_id == *id;
}

/** @brief Get cell by cell_id from DU's cells array
 * @param[in] cells Pointer to DU's cells sequence array
 * @param[in] cell_id Cell ID to search for
 * @return Pointer to the cell container, or NULL if not found */
nr_rrc_cell_container_t *rrc_get_cell_for_du(seq_arr_t *cells, uint64_t cell_id)
{
  DevAssert(cells);
  elm_arr_t elm = find_if(cells, &cell_id, eq_cell_id);
  if (elm.found) {
    // elm.it points to an element in seq_arr, which stores nr_rrc_cell_container_t*
    nr_rrc_cell_container_t **cell_ptr = (nr_rrc_cell_container_t **)elm.it;
    return *cell_ptr;
  }
  return NULL;
}

static bool eq_pci_in_du(const void *vval, const void *vit)
{
  const uint16_t *pci = (const uint16_t *)vval;
  const nr_rrc_cell_container_t **elem_ptr = (const nr_rrc_cell_container_t **)vit;
  return (*elem_ptr) != NULL && (*elem_ptr)->info.pci == *pci;
}

/** @brief Get cell by PCI from DU's cells array
 * @param[in] cells Pointer to DU's cells sequence array
 * @param[in] pci PCI to search for
 * @return Pointer to the cell container, or NULL if not found
 * @note PCI must be unique within a DU; returns the first match */
nr_rrc_cell_container_t *rrc_get_cell_by_pci_for_du(const seq_arr_t *cells, uint16_t pci)
{
  DevAssert(cells);
  elm_arr_t elm = find_if((seq_arr_t *)cells, &pci, eq_pci_in_du);
  if (elm.found) {
    // elm.it points to an element in seq_arr, which stores nr_rrc_cell_container_t*
    nr_rrc_cell_container_t **cell_ptr = (nr_rrc_cell_container_t **)elm.it;
    return *cell_ptr;
  }
  return NULL;
}

/** @brief Add cell to DU's cells array
 * @param[in] cells Pointer to DU's cells sequence array
 * @param[in] cell Pointer to cell container to add
 * @return Pointer to the added cell, or NULL on duplicate cell_id/PCI or error
 * @note Rejects duplicate cell_id or PCI within the DU */
nr_rrc_cell_container_t *rrc_add_cell_to_du(seq_arr_t *cells, nr_rrc_cell_container_t *cell)
{
  DevAssert(cells);
  DevAssert(cell);

  // Check for duplicate cell_id within this DU
  nr_rrc_cell_container_t *exists = rrc_get_cell_for_du(cells, cell->info.cell_id);
  if (exists) {
    LOG_E(NR_RRC, "Trying to add an already existing cell with ID=%lu\n", cell->info.cell_id);
    return NULL;
  }

  // Check for duplicate PCI within this DU (PCI must be unique within a DU)
  nr_rrc_cell_container_t *existing_pci = rrc_get_cell_by_pci_for_du(cells, cell->info.pci);
  if (existing_pci != NULL) {
    LOG_E(NR_RRC,
          "Trying to add cell with ID=%lu, but PCI %d already exists in this DU (cell ID %lu)\n",
          cell->info.cell_id,
          cell->info.pci,
          existing_pci->info.cell_id);
    return NULL;
  }

  seq_arr_push_back(cells, &cell, sizeof(nr_rrc_cell_container_t *));
  LOG_I(NR_RRC, "Added cell %lu to DU cells array (total cells = %ld)\n", cell->info.cell_id, seq_arr_size(cells));

  return cell;
}

/** @brief Free a cell container and all its associated resources
 * @param[in] cell Pointer to the cell container to free
 * @note Frees ASN.1 structures (mib, sib1, mtc) and the cell container itself
 * @note Does NOT remove the cell from the tree - caller must do RB_REMOVE first */
void rrc_free_cell_container(nr_rrc_cell_container_t *cell)
{
  DevAssert(cell);
  ASN_STRUCT_FREE(asn_DEF_NR_MIB, cell->mib);
  ASN_STRUCT_FREE(asn_DEF_NR_SIB1, cell->sib1);
  ASN_STRUCT_FREE(asn_DEF_NR_MeasurementTimingConfiguration, cell->mtc);
  free(cell);
}

static bool eq_serving_cell_id(const void *vval, const void *vit)
{
  const uint8_t *id = (const uint8_t *)vval;
  const ue_serving_cell_t *elem = (const ue_serving_cell_t *)vit;
  return elem->serving_cell_id == *id;
}

/** @brief Get serving cell from UE's serving_cells array by serving_cell_id
 * @param[in] ue UE context
 * @param[in] serving_cell_id ServCellIndex (0 = PCell, 1-31 = SCell)
 * @return Pointer to serving cell entry, or NULL if not found */
ue_serving_cell_t *rrc_get_ue_serving_cell_by_id(const gNB_RRC_UE_t *ue, uint8_t serving_cell_id)
{
  DevAssert(ue);
  elm_arr_t elm = find_if((seq_arr_t *)&ue->serving_cells, &serving_cell_id, eq_serving_cell_id);
  if (elm.found)
    return (ue_serving_cell_t *)elm.it;
  return NULL;
}

/** @brief Add a serving cell to UE's serving_cells array
 * @param[in] ue UE context
 * @param[in] cell Cell container (nci and assoc_id taken from here)
 * @param[in] serving_cell_id ServCellIndex (0 = PCell, 1-31 = SCell)
 * @return Pointer to the added or existing entry, or NULL on error
 * @note Uniqueness: (serving_cell_id, nci) must be unique. Same pair returns existing;
 *       same serving_cell_id, different nci returns NULL. */
ue_serving_cell_t *rrc_add_ue_serving_cell(gNB_RRC_UE_t *ue, const nr_rrc_cell_container_t *cell, uint8_t serving_cell_id)
{
  DevAssert(ue);
  DevAssert(serving_cell_id < RRC_MAX_NUM_SERVING_CELLS);

  // Check maximum number of items in list
  size_t current_count = seq_arr_size(&ue->serving_cells);
  if (current_count >= RRC_MAX_NUM_SERVING_CELLS) {
    LOG_E(NR_RRC, "Maximum number of serving cells (%d) reached\n", RRC_MAX_NUM_SERVING_CELLS);
    return NULL;
  }

  // Check if serving cell with this serving_cell_id already exists
  ue_serving_cell_t *existing = rrc_get_ue_serving_cell_by_id(ue, serving_cell_id);
  if (existing != NULL) {
    // If it's the same nci, return existing
    if (existing->nci == cell->info.cell_id) {
      LOG_D(NR_RRC, "Serving cell %d already has cell %ld\n", serving_cell_id, cell->info.cell_id);
      return existing;
    }
    // Reject if already exists with different nci
    LOG_E(NR_RRC, "Serving cell with serving_cell_id %d already exists with different nci\n", serving_cell_id);
    return NULL;
  }

  LOG_I(NR_RRC,
        "Adding new %s (cell %ld, assoc_id %d)\n",
        serving_cell_id == RRC_PCELL_INDEX ? "PCell" : "SCell",
        cell->info.cell_id,
        cell->assoc_id);
  ue_serving_cell_t new_cell = {.nci = cell->info.cell_id, .serving_cell_id = serving_cell_id, .assoc_id = cell->assoc_id, .pci = cell->info.pci};

  size_t elt_size = sizeof(ue_serving_cell_t);
  if (serving_cell_id == RRC_PCELL_INDEX) {
    /* PCell must match F1 UE data (serving_cells and F1 UE data stay in sync). */
    DevAssert(cu_exists_f1_ue_data(ue->rrc_ue_id));
    DevAssert(cu_get_f1_ue_data(ue->rrc_ue_id).du_assoc_id == cell->assoc_id);
    /* Ensure PCell is always at index 0 in serving_cells */
    size_t n = seq_arr_size(&ue->serving_cells);
    seq_arr_push_back(&ue->serving_cells, &new_cell, elt_size);
    if (n > 0) {
      /* If there are existing cells, shift them right to make room for the new PCell */
      void *first = seq_arr_front(&ue->serving_cells);
      memmove((uint8_t *)first + elt_size, first, n * elt_size);
      memcpy(first, &new_cell, elt_size);
    }
  } else {
    /* SCells are appended after PCell (if any) */
    seq_arr_push_back(&ue->serving_cells, &new_cell, elt_size);
  }
  return rrc_get_ue_serving_cell_by_id(ue, serving_cell_id);
}

/** @brief Remove serving cells that belong to a specific DU
 * @param[in] ue UE context
 * @param[in] assoc_id Association ID of the DU
 * @note Removes all serving cells (including PCell) that belong to the specified DU.
 *       Uses stored assoc_id for O(k) comparison without cell lookup. */
void rrc_remove_ue_scells_from_du(gNB_RRC_UE_t *ue, sctp_assoc_t assoc_id)
{
  DevAssert(ue);
  LOG_I(NR_RRC, "Removing SCells from DU (assoc_id=%d) for UE %d\n", assoc_id, ue->rrc_ue_id);

  // Collect indices of SCells to delete (don't modify array while iterating)
  uint32_t indices_to_delete[RRC_MAX_NUM_SERVING_CELLS];
  size_t num_to_delete = 0;
  uint32_t index = 0;

  FOR_EACH_SEQ_ARR (ue_serving_cell_t *, scell, &ue->serving_cells) {
    // Use stored assoc_id for direct comparison
    if (scell->assoc_id == assoc_id) {
      indices_to_delete[num_to_delete++] = index;
    }
    index++;
  }

  // Now delete the collected serving cells from end to beginning for safety
  for (size_t i = num_to_delete; i > 0; i--) {
    uint32_t idx = indices_to_delete[i - 1];
    void *it = seq_arr_at(&ue->serving_cells, idx);
    seq_arr_erase(&ue->serving_cells, it);
  }

}

/** @brief Get PCell serving cell entry for a UE based on serving_cells
 * @param[in] ue UE context
 * @return Pointer to serving cell entry for PCell, or NULL if not found
 * @note PCell is always at index 0. */
const ue_serving_cell_t *ue_get_pcell_entry(const gNB_RRC_UE_t *ue)
{
  DevAssert(ue);
  size_t count = seq_arr_size(&ue->serving_cells);
  if (count == 0)
    return NULL;

  /* PCell is always the first serving cell */
  const ue_serving_cell_t *first = seq_arr_front(&ue->serving_cells);
  DevAssert(first != NULL);
  DevAssert(first->serving_cell_id == RRC_PCELL_INDEX);
  return first;
}

/** @brief Get UE's PCell container from the global cell tree
 * @param[in] rrc RRC instance (global cell tree)
 * @param[in] ue UE context
 * @return Pointer to PCell container, or NULL if UE has no PCell or cell not in tree */
nr_rrc_cell_container_t *rrc_get_pcell_for_ue(gNB_RRC_INST *rrc, const gNB_RRC_UE_t *ue)
{
  DevAssert(rrc != NULL);
  DevAssert(ue != NULL);
  const ue_serving_cell_t *pcell_entry = ue_get_pcell_entry(ue);
  if (pcell_entry == NULL)
    return NULL;
  return get_cell_by_cell_id(&rrc->cells, pcell_entry->nci);
}

/** @brief Update UE's PCell in serving_cells array
 * @param[in] UE UE context
 * @param[in] cell Cell container to set as PCell
 * @return Pointer to added serving cell entry, or NULL on failure
 * @note If UE already has a PCell, removes all serving cells from the old PCell's DU first */
ue_serving_cell_t *rrc_update_ue_pcell(gNB_RRC_UE_t *UE, const nr_rrc_cell_container_t *cell)
{
  DevAssert(UE != NULL);
  DevAssert(cell != NULL);

  // If UE already has a PCell, remove all serving cells from that DU first
  const ue_serving_cell_t *existing_pcell = ue_get_pcell_entry(UE);
  if (existing_pcell != NULL) {
    rrc_remove_ue_scells_from_du(UE, existing_pcell->assoc_id);
  }

  // Add the new PCell
  return rrc_add_ue_serving_cell(UE, cell, RRC_PCELL_INDEX);
}
