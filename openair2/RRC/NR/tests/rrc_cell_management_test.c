/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "common/utils/utils.h" // for calloc_or_fail
#include "common/utils/assertions.h" // for AssertFatal, DevAssert
#include "common/utils/collection/tree.h" // for RB tree macros
#include "openair2/RRC/NR/rrc_cell_management.h" // for cell management functions
#include "openair2/RRC/NR/rrc_gNB_du.h" // for get_du_by_assoc_id
#include "common/utils/LOG/log.h" // for LOG_I, LOG_A, etc.
#include "common/utils/ds/seq_arr.h" // for seq_arr_free
#include "openair2/F1AP/f1ap_ids.h" // for cu_init_f1_ue_data, cu_add_f1_ue_data

// Test constants
static const sctp_assoc_t ASSOC_ID_DU1 = 8;
static const sctp_assoc_t ASSOC_ID_DU2 = 9;
static const sctp_assoc_t ASSOC_ID_DU3 = 10;
static const uint64_t DU_ID_1 = 1001;
static const uint64_t DU_ID_2 = 1002;
static const uint64_t DU_ID_3 = 1003;
static const uint64_t CELL_ID_1 = 12345678;
static const uint64_t CELL_ID_2 = 87654321;
static const uint64_t CELL_ID_3 = 11111111;
static const uint64_t CELL_ID_4 = 22222222;
static const uint64_t CELL_ID_5 = 33333333;
static const uint64_t CELL_ID_6 = 44444444;
static const uint64_t CELL_ID_7 = 55555555;
static const uint64_t CELL_ID_NONEXISTENT = 99999999;
static const uint16_t PCI_1 = 1;
static const uint16_t PCI_2 = 4;
static const uint16_t PCI_3 = 5;
static const uint16_t PCI_10 = 10;
static const uint16_t PCI_20 = 20;
static const uint16_t PCI_30 = 30;
static const uint16_t PCI_40 = 40;
static const uint16_t PCI_NONEXISTENT = 99;
static const uint8_t SERVING_CELL_ID_SCELL1 = 1;
static const uint8_t SERVING_CELL_ID_SCELL2 = 2;
static const uint32_t UE_ID = 1;

// Mock functions and variables
configmodule_interface_t *uniqCfg = NULL;
softmodem_params_t *get_softmodem_params(void)
{
  return NULL;
}

// Helper function to create a test cell container
static nr_rrc_cell_container_t *create_test_cell(uint64_t cell_id, uint16_t pci, sctp_assoc_t assoc_id)
{
  nr_rrc_cell_container_t *cell = calloc_or_fail(1, sizeof(*cell));
  cell->info.cell_id = cell_id;
  cell->info.pci = pci;
  cell->assoc_id = assoc_id;
  cell->info.mode = NR_MODE_TDD;
  cell->info.plmn.mcc = 222;
  cell->info.plmn.mnc = 1;
  cell->info.tac = 1;
  return cell;
}

// Helper function to create a test DU container
static nr_rrc_du_container_t *create_test_du(sctp_assoc_t assoc_id, uint64_t du_id, const char *name)
{
  nr_rrc_du_container_t *du = calloc_or_fail(1, sizeof(*du));
  du->assoc_id = assoc_id;
  du->gNB_DU_id = du_id;
  du->gNB_DU_name = strdup(name);
  AssertFatal(du->gNB_DU_name != NULL, "strdup failed for DU name");
  seq_arr_init(&du->cells, sizeof(nr_rrc_cell_container_t *));
  return du;
}

// Helper function to add DU to RRC instance
static void add_du_to_rrc(gNB_RRC_INST *rrc, nr_rrc_du_container_t *du)
{
  nr_rrc_du_container_t *collision = rrc_add_du(rrc, du);
  AssertFatal(collision == NULL, "rrc_add_du should succeed for new DU (assoc_id %d)", du->assoc_id);
}

// Helper function to find cell by PCI in global tree (for testing PCI reuse across network)
// Returns first match; PCI is not globally unique
static nr_rrc_cell_container_t *get_cell_by_pci(struct rrc_cell_tree *cells, const uint16_t pci)
{
  nr_rrc_cell_container_t *cell = NULL;
  RB_FOREACH (cell, rrc_cell_tree, cells) {
    if (cell->info.pci == pci)
      return cell;
  }
  return NULL;
}

/** @brief Test DU lookup by assoc_id
 * @note Tests the functions:
 * - get_du_by_assoc_id() */
static void test_du_lookup(void)
{
  LOG_I(NR_RRC, "Testing DU lookup by assoc_id\n");

  gNB_RRC_INST rrc = {0};
  RB_INIT(&rrc.dus);
  RB_INIT(&rrc.cells);

  /* Sub-test 1: Create multiple DUs and insert into RRC DU tree */
  struct {
    nr_rrc_du_container_t *du;
    sctp_assoc_t assoc_id;
    uint64_t du_id;
  } dus[] = {
      {create_test_du(ASSOC_ID_DU1, DU_ID_1, "DU-1"), ASSOC_ID_DU1, DU_ID_1},
      {create_test_du(ASSOC_ID_DU2, DU_ID_2, "DU-2"), ASSOC_ID_DU2, DU_ID_2},
      {create_test_du(ASSOC_ID_DU3, DU_ID_3, "DU-3"), ASSOC_ID_DU3, DU_ID_3},
  };

  for (size_t i = 0; i < sizeof(dus) / sizeof(dus[0]); i++) {
    add_du_to_rrc(&rrc, dus[i].du);
  }

  /* Sub-test 2: Test lookup existing DUs */
  for (size_t i = 0; i < sizeof(dus) / sizeof(dus[0]); i++) {
    nr_rrc_du_container_t *found = get_du_by_assoc_id(&rrc, dus[i].assoc_id);
    AssertFatal(found == dus[i].du, "DU with assoc_id %d should be found", dus[i].assoc_id);
    AssertFatal(found->gNB_DU_id == dus[i].du_id, "DU ID should match (expected %lu, got %lu)", dus[i].du_id, found->gNB_DU_id);
  }

  /* Sub-test 3: Test lookup non-existing assoc_id */
  static const sctp_assoc_t ASSOC_ID_NONEXISTENT = 99;
  nr_rrc_du_container_t *found = get_du_by_assoc_id(&rrc, ASSOC_ID_NONEXISTENT);
  AssertFatal(found == NULL, "Non-existing assoc_id %d should return NULL", ASSOC_ID_NONEXISTENT);

  /* Sub-test 4: Test invalid assoc_id (0 is considered invalid) */
  static const sctp_assoc_t ASSOC_ID_INVALID = 0;
  found = get_du_by_assoc_id(&rrc, ASSOC_ID_INVALID);
  AssertFatal(found == NULL, "Invalid assoc_id %d should return NULL", ASSOC_ID_INVALID);

  LOG_A(NR_RRC, "DU lookup test passed\n");

  /* Sub-test 5: Cleanup with free of DUs and their cell containers */
  for (size_t i = 0; i < sizeof(dus) / sizeof(dus[0]); i++) {
    rrc_cleanup_du(&rrc, dus[i].du);
  }
}

/** @brief Test cell lookup, cell-DU association, and UE cell association
 * @note Tests the functions:
 * - get_cell_by_cell_id()
 * - rrc_get_cell_for_du()
 * - rrc_get_cell_by_pci_for_du()
 * - rrc_add_cell_to_du()
 * - rrc_add_ue_serving_cell()
 * - rrc_get_ue_serving_cell_by_id()
 * - rrc_get_pcell_for_ue()
 * - rrc_remove_ue_scells_from_du()
 * - rrc_free_cell_container() */
static void test_cell_lookup(void)
{
  LOG_I(NR_RRC, "Testing cell lookup functions\n");

  cu_init_f1_ue_data();

  gNB_RRC_INST rrc = {0};
  RB_INIT(&rrc.dus);
  RB_INIT(&rrc.cells);

  nr_rrc_du_container_t *du1 = create_test_du(ASSOC_ID_DU1, DU_ID_1, "DU-1");
  nr_rrc_du_container_t *du2 = create_test_du(ASSOC_ID_DU2, DU_ID_2, "DU-2");
  add_du_to_rrc(&rrc, du1);
  add_du_to_rrc(&rrc, du2);

  /* Sub-test 1: Create multiple cells per DU and insert into global cell tree and DU's seq_arr */
  struct {
    nr_rrc_cell_container_t *cell;
    uint64_t cell_id;
    uint16_t pci;
    nr_rrc_du_container_t *du;
  } cells[] = {
      {create_test_cell(CELL_ID_1, PCI_10, ASSOC_ID_DU1), CELL_ID_1, PCI_10, du1},
      {create_test_cell(CELL_ID_2, PCI_20, ASSOC_ID_DU1), CELL_ID_2, PCI_20, du1},
      {create_test_cell(CELL_ID_3, PCI_1, ASSOC_ID_DU1), CELL_ID_3, PCI_1, du1},
      {create_test_cell(CELL_ID_4, PCI_2, ASSOC_ID_DU2), CELL_ID_4, PCI_2, du2},
      {create_test_cell(CELL_ID_5, PCI_3, ASSOC_ID_DU2), CELL_ID_5, PCI_3, du2},
  };

  // Insert cells: check for duplicate cell_id first, then insert into global tree and DU's seq_arr
  for (size_t i = 0; i < sizeofArray(cells); i++) {
    nr_rrc_cell_container_t *existing = get_cell_by_cell_id(&rrc.cells, cells[i].cell_id);
    AssertFatal(existing == NULL, "Cell ID %lu should not exist before insertion", cells[i].cell_id);
    // Insert into global cell tree
    nr_rrc_cell_container_t *collision = rrc_add_cell(&rrc, cells[i].cell);
    AssertFatal(collision == NULL, "rrc_add_cell should succeed for new cell (ID %lu)", cells[i].cell_id);
    // Insert into DU's seq_arr
    nr_rrc_cell_container_t *added = rrc_add_cell_to_du(&cells[i].du->cells, cells[i].cell);
    AssertFatal(added != NULL, "rrc_add_cell_to_du should succeed for new cell (ID %lu)", cells[i].cell_id);
  }

  nr_rrc_cell_container_t *found;
  /* Sub-test 2: Test lookup by cell_id */
  found = get_cell_by_cell_id(&rrc.cells, CELL_ID_1);
  AssertFatal(found != NULL && found == cells[0].cell, "Cell ID %lu should be found", CELL_ID_1);
  found = get_cell_by_cell_id(&rrc.cells, CELL_ID_4);
  AssertFatal(found != NULL && found == cells[3].cell, "Cell ID %lu should be found", CELL_ID_4);

  /* Sub-test 3: Test lookup by PCI (returns first match) */
  found = get_cell_by_pci(&rrc.cells, PCI_10);
  AssertFatal(found != NULL && found->info.cell_id == CELL_ID_1,
              "Cell with PCI %d should be found (expected cell_id %lu)",
              PCI_10,
              CELL_ID_1);

  /* Sub-test 4: Test lookup non-existing cell */
  found = get_cell_by_cell_id(&rrc.cells, CELL_ID_NONEXISTENT);
  AssertFatal(found == NULL, "Non-existing cell_id should return NULL");
  found = get_cell_by_pci(&rrc.cells, PCI_NONEXISTENT);
  AssertFatal(found == NULL, "Non-existing PCI should return NULL");

  /* Sub-test 5: Test rrc_get_cell_for_du() - DU-specific cell lookup */
  found = rrc_get_cell_for_du(&du1->cells, CELL_ID_1);
  AssertFatal(found != NULL && found->info.cell_id == CELL_ID_1, "rrc_get_cell_for_du should find cell in DU1");
  found = rrc_get_cell_for_du(&du2->cells, CELL_ID_4);
  AssertFatal(found != NULL && found->info.cell_id == CELL_ID_4, "rrc_get_cell_for_du should find cell in DU2");
  // Test non-existing cell in DU
  nr_rrc_cell_container_t *not_found = rrc_get_cell_for_du(&du1->cells, CELL_ID_NONEXISTENT);
  AssertFatal(not_found == NULL, "rrc_get_cell_for_du should return NULL for non-existing cell");
  // Test cell from different DU
  not_found = rrc_get_cell_for_du(&du1->cells, CELL_ID_4);
  AssertFatal(not_found == NULL, "rrc_get_cell_for_du should return NULL for cell from different DU");

  /* Sub-test 6: Test rrc_get_cell_by_pci_for_du() - DU-specific PCI lookup */
  nr_rrc_cell_container_t *cell_in_du = rrc_get_cell_by_pci_for_du(&du1->cells, PCI_10);
  AssertFatal(cell_in_du != NULL, "rrc_get_cell_by_pci_for_du should find cell with PCI %d in DU1", PCI_10);
  AssertFatal(cell_in_du->info.cell_id == CELL_ID_1,
              "Found cell should have correct cell_id (expected %lu, got %lu)",
              CELL_ID_1,
              cell_in_du->info.cell_id);
  cell_in_du = rrc_get_cell_by_pci_for_du(&du2->cells, PCI_2);
  AssertFatal(cell_in_du != NULL, "rrc_get_cell_by_pci_for_du should find cell with PCI %d in DU2", PCI_2);
  AssertFatal(cell_in_du->info.cell_id == CELL_ID_4,
              "Found cell should have correct cell_id (expected %lu, got %lu)",
              CELL_ID_4,
              cell_in_du->info.cell_id);
  // Test non-existing PCI in DU
  cell_in_du = rrc_get_cell_by_pci_for_du(&du1->cells, PCI_NONEXISTENT);
  AssertFatal(cell_in_du == NULL, "rrc_get_cell_by_pci_for_du should return NULL for non-existing PCI");
  // Test PCI from different DU (should not find it in this DU)
  cell_in_du = rrc_get_cell_by_pci_for_du(&du1->cells, PCI_2);
  AssertFatal(cell_in_du == NULL, "rrc_get_cell_by_pci_for_du should return NULL for PCI from different DU");

  /* Sub-test 8: Test PCI reuse across different DUs (allowed globally) */
  // Add a cell to DU2 with the same PCI as a cell in DU1 (PCI reuse is allowed globally)
  nr_rrc_cell_container_t *cell_on_du2_same_pci = create_test_cell(CELL_ID_6, PCI_10, ASSOC_ID_DU2);
  nr_rrc_cell_container_t *already_in_tree = get_cell_by_cell_id(&rrc.cells, CELL_ID_6);
  AssertFatal(already_in_tree == NULL, "Cell ID %lu should not exist before insertion", CELL_ID_6);
  // Insert into global cell tree (PCI reuse is allowed)
  nr_rrc_cell_container_t *tree_collision = rrc_add_cell(&rrc, cell_on_du2_same_pci);
  AssertFatal(tree_collision == NULL, "rrc_add_cell should succeed for new cell (ID %lu)", CELL_ID_6);
  // Add to DU2 (should succeed - PCI reuse across DUs is allowed)
  nr_rrc_cell_container_t *added_to_du2 = rrc_add_cell_to_du(&du2->cells, cell_on_du2_same_pci);
  AssertFatal(added_to_du2 != NULL, "rrc_add_cell_to_du should succeed for cell with PCI reused from different DU");
  // Verify both cells with same PCI exist in global tree (fetch by cell_id)
  nr_rrc_cell_container_t *cell1 = get_cell_by_cell_id(&rrc.cells, CELL_ID_1);
  nr_rrc_cell_container_t *cell6 = get_cell_by_cell_id(&rrc.cells, CELL_ID_6);
  AssertFatal(cell1 != NULL && cell6 != NULL, "Both cells with same PCI should exist in global tree");
  AssertFatal(cell1->info.pci == PCI_10 && cell6->info.pci == PCI_10, "Both cells should have PCI %d", PCI_10);
  // get_cell_by_pci returns first match only; verify it returns one of them
  nr_rrc_cell_container_t *first_match_global = get_cell_by_pci(&rrc.cells, PCI_10);
  AssertFatal(first_match_global != NULL, "get_cell_by_pci should find a cell with PCI %d", PCI_10);
  AssertFatal(first_match_global == cell1 || first_match_global == cell6,
              "get_cell_by_pci should return one of the two cells with PCI %d",
              PCI_10);
  // Verify each DU can find its own cell by PCI
  nr_rrc_cell_container_t *cell_in_du1 = rrc_get_cell_by_pci_for_du(&du1->cells, PCI_10);
  AssertFatal(cell_in_du1 != NULL && cell_in_du1->info.cell_id == CELL_ID_1, "DU1 should find its own cell with PCI %d", PCI_10);
  nr_rrc_cell_container_t *cell_in_du2 = rrc_get_cell_by_pci_for_du(&du2->cells, PCI_10);
  AssertFatal(cell_in_du2 != NULL && cell_in_du2->info.cell_id == CELL_ID_6, "DU2 should find its own cell with PCI %d", PCI_10);

  /* Sub-test 9: Test PCI reuse within same DU (should be rejected) */
  nr_rrc_cell_container_t *cell_with_duplicate_pci = create_test_cell(CELL_ID_NONEXISTENT, PCI_10, ASSOC_ID_DU1);
  // Try to add to DU1 - should fail because PCI_10 already exists in DU1
  nr_rrc_cell_container_t *add_result = rrc_add_cell_to_du(&du1->cells, cell_with_duplicate_pci);
  AssertFatal(add_result == NULL, "rrc_add_cell_to_du should reject cell with duplicate PCI within same DU");
  rrc_free_cell_container(cell_with_duplicate_pci);

  /* Sub-test 10: Verify cell counts per DU */
  static const int expected_du1_cells = 3;
  static const int expected_du2_cells = 3; // includes cell_on_du2_same_pci
  size_t count_du1 = seq_arr_size(&du1->cells);
  size_t count_du2 = seq_arr_size(&du2->cells);
  AssertFatal(count_du1 == expected_du1_cells, "DU 1 should have %d cells (got %zu)", expected_du1_cells, count_du1);
  AssertFatal(count_du2 == expected_du2_cells, "DU 2 should have %d cells (got %zu)", expected_du2_cells, count_du2);

  /* Sub-test 11: Test UE cell association - create test UE context */
  gNB_RRC_UE_t ue = {0};
  ue.rrc_ue_id = UE_ID;
  seq_arr_init(&ue.serving_cells, sizeof(ue_serving_cell_t));

  /* Register F1 UE data for this UE at the CU */
  f1_ue_data_t f1_data = {
      .secondary_ue = 0,
      .e1_assoc_id = 0,
      .du_assoc_id = ASSOC_ID_DU1,
  };
  bool f1_added = cu_add_f1_ue_data(UE_ID, &f1_data);
  AssertFatal(f1_added, "cu_add_f1_ue_data should succeed for test UE");

  /* Sub-test 12: Test rrc_add_ue_serving_cell() - Add PCell, ue_get_pcell_entry(), rrc_get_pcell_for_ue() */
  ue_serving_cell_t *added_pcell = rrc_add_ue_serving_cell(&ue, cells[0].cell, RRC_PCELL_INDEX);
  AssertFatal(added_pcell != NULL && added_pcell->nci == CELL_ID_1 && added_pcell->serving_cell_id == RRC_PCELL_INDEX,
              "rrc_add_ue_serving_cell should succeed");
  const ue_serving_cell_t *pcell_entry = ue_get_pcell_entry(&ue);
  AssertFatal(pcell_entry != NULL && pcell_entry->nci == CELL_ID_1 && pcell_entry->serving_cell_id == RRC_PCELL_INDEX,
              "ue_get_pcell_entry should return correct PCell entry");
  nr_rrc_cell_container_t *pcell = rrc_get_pcell_for_ue(&rrc, &ue);
  AssertFatal(pcell == cells[0].cell, "rrc_get_pcell_for_ue should return same cell as PCell");

  /* Sub-test 13: Test adding SCells and rrc_get_ue_serving_cell_by_id() */
  ue_serving_cell_t *added_scell = rrc_add_ue_serving_cell(&ue, cells[1].cell, SERVING_CELL_ID_SCELL1);
  AssertFatal(added_scell != NULL && added_scell->serving_cell_id == SERVING_CELL_ID_SCELL1,
              "rrc_add_ue_serving_cell should succeed for SCell");
  AssertFatal(added_scell->assoc_id == ASSOC_ID_DU1,
              "SCell should have correct assoc_id (expected %d, got %d)",
              ASSOC_ID_DU1,
              added_scell->assoc_id);
  // Test rrc_get_ue_serving_cell_by_id() for PCell and SCell
  ue_serving_cell_t *found_serving_cell = rrc_get_ue_serving_cell_by_id(&ue, RRC_PCELL_INDEX);
  AssertFatal(found_serving_cell != NULL && found_serving_cell == added_pcell, "rrc_get_ue_serving_cell_by_id should find PCell");
  found_serving_cell = rrc_get_ue_serving_cell_by_id(&ue, SERVING_CELL_ID_SCELL1);
  AssertFatal(found_serving_cell != NULL && found_serving_cell == added_scell, "rrc_get_ue_serving_cell_by_id should find SCell");
  // Test non-existing serving cell ID
  ue_serving_cell_t *not_found_serving = rrc_get_ue_serving_cell_by_id(&ue, SERVING_CELL_ID_SCELL2);
  AssertFatal(not_found_serving == NULL, "rrc_get_ue_serving_cell_by_id should return NULL for non-existing serving cell ID");
  static const size_t expected_serving_cells_2 = 2;
  AssertFatal(seq_arr_size(&ue.serving_cells) == expected_serving_cells_2,
              "UE should have %zu serving cells (got %zu)",
              expected_serving_cells_2,
              seq_arr_size(&ue.serving_cells));

  // Add second SCell
  nr_rrc_cell_container_t *scell2 = create_test_cell(CELL_ID_7, PCI_30, ASSOC_ID_DU1);
  nr_rrc_cell_container_t *already_present = get_cell_by_cell_id(&rrc.cells, CELL_ID_7);
  AssertFatal(already_present == NULL, "Cell ID %lu should not exist before insertion", CELL_ID_7);
  // Insert into global cell tree
  nr_rrc_cell_container_t *collision = rrc_add_cell(&rrc, scell2);
  AssertFatal(collision == NULL, "rrc_add_cell should succeed for new cell (ID %lu)", CELL_ID_7);
  // Insert into DU's seq_arr
  nr_rrc_cell_container_t *added_scell2_du = rrc_add_cell_to_du(&du1->cells, scell2);
  AssertFatal(added_scell2_du != NULL, "rrc_add_cell_to_du should succeed for new cell (ID %lu)", CELL_ID_7);
  ue_serving_cell_t *added_scell2 = rrc_add_ue_serving_cell(&ue, scell2, SERVING_CELL_ID_SCELL2);
  AssertFatal(added_scell2 != NULL, "rrc_add_ue_serving_cell should add new SCell");
  AssertFatal(added_scell2->serving_cell_id == SERVING_CELL_ID_SCELL2, "New SCell should have correct serving_cell_id (expected %d, got %d)", SERVING_CELL_ID_SCELL2, added_scell2->serving_cell_id);
  AssertFatal(added_scell2->assoc_id == ASSOC_ID_DU1,
              "SCell should have correct assoc_id (expected %d, got %d)",
              ASSOC_ID_DU1,
              added_scell2->assoc_id);

  /* Sub-test 14: Test multiple PCell enforcement - adding another PCell should fail if one already exists */
  nr_rrc_cell_container_t *test_cell = create_test_cell(CELL_ID_NONEXISTENT, PCI_NONEXISTENT, ASSOC_ID_DU1);
  AssertFatal(rrc_add_ue_serving_cell(&ue, test_cell, RRC_PCELL_INDEX) == NULL,
              "Adding another PCell when one already exists should fail");
  AssertFatal(rrc_get_pcell_for_ue(&rrc, &ue) == cells[0].cell,
              "PCell should still be the original one when adding duplicate PCell fails");
  rrc_free_cell_container(test_cell);

  /* Sub-test 15: Test rrc_add_ue_serving_cell() with existing PCell (same nci, should return existing) */
  ue_serving_cell_t *existing_pcell = rrc_add_ue_serving_cell(&ue, cells[0].cell, RRC_PCELL_INDEX);
  AssertFatal(existing_pcell != NULL && existing_pcell->nci == CELL_ID_1,
              "rrc_add_ue_serving_cell should succeed for existing PCell with same nci");
  AssertFatal(existing_pcell == added_pcell, "Should return existing PCell entry, not create new one");

  /* Sub-test 16: Test maximum serving cells limit */
  static const uint64_t test_cell_id_base = 10000000;
  static const uint8_t starting_serving_cell_id = 3;
  nr_rrc_cell_container_t *test_cells[RRC_MAX_NUM_SERVING_CELLS] = {0};
  size_t test_cell_count = 0;
  for (uint8_t i = starting_serving_cell_id; i < RRC_MAX_NUM_SERVING_CELLS; i++) {
    uint64_t test_cell_id = test_cell_id_base + i;
    uint16_t test_pci = PCI_40 + i;
    test_cells[test_cell_count] = create_test_cell(test_cell_id, test_pci, ASSOC_ID_DU1);
    // Insert into global cell tree
    nr_rrc_cell_container_t *existing = rrc_add_cell(&rrc, test_cells[test_cell_count]);
    if (existing != NULL) {
      LOG_E(NR_RRC, "Cell %lu already exists in tree, skipping insertion\n", test_cell_id);
      rrc_free_cell_container(test_cells[test_cell_count]);
      test_cells[test_cell_count] = NULL;
      break;
    }
    // Insert into DU's seq_arr
    rrc_add_cell_to_du(&du1->cells, test_cells[test_cell_count]);
    if (rrc_add_ue_serving_cell(&ue, test_cells[test_cell_count], i) == NULL)
      break;
    test_cell_count++;
  }
  size_t final_count = seq_arr_size(&ue.serving_cells);
  AssertFatal(final_count <= RRC_MAX_NUM_SERVING_CELLS,
              "Should not exceed max serving cells (got %zu, max %d)",
              final_count,
              RRC_MAX_NUM_SERVING_CELLS);

  /* Sub-test 17: Test rrc_remove_ue_scells_from_du() */
  size_t before_remove = seq_arr_size(&ue.serving_cells);
  rrc_remove_ue_scells_from_du(&ue, ASSOC_ID_DU1);
  AssertFatal(seq_arr_size(&ue.serving_cells) < before_remove, "Should remove some serving cells");
  AssertFatal(ue_get_pcell_entry(&ue) == NULL, "PCell should be removed when it has matching assoc_id");

  /* Sub-test 18: Test rrc_get_pcell_for_ue() edge case (UE with no serving cells) */
  gNB_RRC_UE_t ue_empty = {0};
  seq_arr_init(&ue_empty.serving_cells, sizeof(ue_serving_cell_t));
  AssertFatal(rrc_get_pcell_for_ue(&rrc, &ue_empty) == NULL,
              "rrc_get_pcell_for_ue should return NULL for UE with no serving cells");
  seq_arr_free(&ue_empty.serving_cells, NULL);

  LOG_A(NR_RRC, "Cell lookup test passed\n");

  /* Sub-test 19: Cleanup */
  seq_arr_free(&ue.serving_cells, NULL);
  rrc_cleanup_du(&rrc, du1);
  rrc_cleanup_du(&rrc, du2);
}

int main()
{
  // Initialize logging system
  logInit();
  set_log(NR_RRC, OAILOG_INFO);

  LOG_I(NR_RRC, "Starting RRC Cell Management Tests\n");

  // DU lookup tests
  test_du_lookup();
  // Cell lookup, cell-DU association, and UE cell association tests
  test_cell_lookup();

  LOG_A(NR_RRC, "All RRC Cell Management tests passed successfully!\n");
  return 0;
}
