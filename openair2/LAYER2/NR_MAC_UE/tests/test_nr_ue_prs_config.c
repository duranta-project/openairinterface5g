/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include "NR_MAC_UE/mac_proto.h"

static NR_UE_MAC_INST_t mac;

NR_UE_MAC_INST_t *__wrap_get_mac_inst(module_id_t module_id)
{
  DevAssert(module_id == 0);
  return &mac;
}

int main(void)
{
  logInit();
  pthread_mutex_init(&mac.if_mutex, NULL);
  nr_ue_prs_configuration_t *configuration = calloc_or_fail(1, sizeof(*configuration));
  configuration->source = NR_PRS_SOURCE_POS_SIB;
  configuration->num_targets = 2;
  for (int g = 0; g < 2; g++) {
    nr_ue_prs_target_config_t *target = &configuration->targets[g];
    target->num_resources = 2;
    for (int r = 0; r < 2; r++) {
      target->resources[r] = (nr_ue_prs_resource_config_t){.resource_set_period = 40960,
                                                        .resource_set_offset = 3,
                                                        .resource_offset = r + 1,
                                                        .resource_repetition = 2,
                                                        .resource_time_gap = 4,
                                                        .num_rbs = 24,
                                                        .num_symbols = 6,
                                                        .symbol_start = 2,
                                                        .rb_offset = 8,
                                                        .comb_size = 4,
                                                        .re_offset = r,
                                                        .nprs_id = 100 + g * 2 + r};
    }
  }
  nr_rrc_mac_config_prs(0, configuration);
  DevAssert(mac.prs_num_gnbs == 2);
  for (int g = 0; g < 2; g++) {
    const nr_ue_prs_config_t *c = mac.prs_config[g];
    DevAssert(c->num_resources == 2 && c->period == 40960 && c->offset == 3);
    DevAssert(c->repetition == 2 && c->time_gap == 4);
    for (int r = 0; r < 2; r++) {
      const fapi_nr_dl_config_prs_pdu_rel16_t *pdu = &c->resource[r].pdu;
      DevAssert(c->resource[r].slot_offset == r + 1);
      DevAssert(pdu->nid_prs == 100 + g * 2 + r && pdu->comb_offset == r);
      DevAssert(pdu->comb_size == 4 && pdu->num_symbols == 6 && pdu->first_symbol == 2);
      DevAssert(pdu->num_rbs == 24 && pdu->start_rb == 8);
    }
  }
  configuration->num_targets = 1;
  configuration->targets[0].num_resources = 1;
  configuration->targets[0].resources[0].nprs_id = 4095;
  nr_rrc_mac_config_prs(0, configuration);
  DevAssert(mac.prs_num_gnbs == 1 && mac.prs_config[0]->num_resources == 1);
  DevAssert(mac.prs_config[0]->resource[0].pdu.nid_prs == 4095);
  configuration->num_targets = 0;
  nr_rrc_mac_config_prs(0, configuration);
  DevAssert(mac.prs_num_gnbs == 0);
  nr_ue_free_prs_config(&mac);
  free(configuration);
  pthread_mutex_destroy(&mac.if_mutex);
  logTerm();
  return 0;
}
