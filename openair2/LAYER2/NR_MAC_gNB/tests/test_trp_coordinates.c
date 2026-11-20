/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include "common/ran_context.h"
#include "LAYER2/NR_MAC_gNB/mac_rrc_dl_handler.h"
#include "LAYER2/NR_MAC_gNB/nr_mac_gNB.h"

static int failures;
static int responses;
static bool request_coordinates;

static void record_failure(const f1ap_trp_information_failure_t *failure)
{
  DevAssert(failure->transaction_id == 77 && failure->cause == F1AP_CAUSE_RADIO_NETWORK);
  failures++;
}

static void record_response(const f1ap_trp_information_resp_t *response)
{
  DevAssert(response->transaction_id == 77 && response->trp_information_list.trp_information_item_length == 1);
  const f1ap_trp_information_t *trp = &response->trp_information_list.trp_information_item[0];
  DevAssert(trp->trp_id == 7 && trp->trp_information_type_response_list.trp_information_type_response_item_length == 1);
  const f1ap_trp_information_type_response_item_t *item =
      &trp->trp_information_type_response_list.trp_information_type_response_item[0];
  if (request_coordinates) {
    DevAssert(item->present == F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_GEOGRAPHICALCOORDINATES);
    const f1ap_relative_cartesian_location_t *position = &item->choice.geographical_coordinates.trp_position_definition_type
                                                            .choice.referenced.reference_point_type.choice
                                                            .trp_position_relative_cartesian;
    DevAssert(position->xvalue == 0 && position->yvalue == 0 && position->zvalue == 0 && position->xyz_unit == 0);
  } else {
    DevAssert(item->present == F1AP_TRP_INFORMATION_TYPE_RESPONSE_ITEM_PR_PCI_NR && item->choice.pci_nr == 42);
  }
  responses++;
}

void test_trp_coordinates(int scenario)
{
  request_coordinates = scenario != 2;
  const bool has_coordinates = scenario == 1;
  trp_info_config_t config = {.num_trp = 1, .has_coordinates = has_coordinates, .trps = {{.id = 7}}};
  gNB_MAC_INST *mac = calloc_or_fail(1, sizeof(*mac));
  mac->trp_info_config = &config;
  mac->mac_rrc.trp_information_failure = record_failure;
  mac->mac_rrc.trp_information_response = record_response;
  f1ap_setup_req_t setup = {.num_cells_available = 1};
  setup.cell = calloc_or_fail(1, sizeof(*setup.cell));
  setup.cell[0].info.nr_pci = 42;
  mac->f1_config.setup_req = &setup;
  gNB_MAC_INST *instances[] = {mac};
  gNB_MAC_INST **saved = RC.nrmac;
  RC.nrmac = instances;
  f1ap_trp_information_type_item_pr type = request_coordinates ? F1AP_TRP_INFORMATION_TYPE_ITEM_GEO_COORDINATES
                                                            : F1AP_TRP_INFORMATION_TYPE_ITEM_NR_PCI;
  f1ap_trp_information_req_t request = {
      .transaction_id = 77,
      .trp_information_type_list = {.trp_information_type_list_length = 1, .trp_information_type_item = &type}};
  failures = responses = 0;
  trp_information_request(&request);
  DevAssert(failures == (scenario == 0) && responses == (scenario != 0));
  RC.nrmac = saved;
  free(setup.cell);
  free(mac);
}
