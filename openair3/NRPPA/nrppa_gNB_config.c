/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nrppa_gNB_config.h"
#include "assertions.h"
#include "common/ran_context.h"
#include "trp_info_nr_paramdef.h"
#include "common/config/config_userapi.h"
#include "common/utils/LOG/log.h"

trp_info_config_t *RCconfig_nr_trp_info(void)
{
  trp_info_config_t *trp_info_config = NULL;
  GET_PARAMS_LIST(TRP_INFO_ParamList, TRP_INFO_Params, TRP_INFO_PARAMS_DESC, CONFIG_STRING_TRP_INFO_CONFIG, NULL);
  if (TRP_INFO_ParamList.numelt == 0) {
    LOG_I(NR_PHY, "No " CONFIG_STRING_TRP_INFO_CONFIG " configuration found\n");
    return NULL;
  }

  paramdef_t *params = TRP_INFO_ParamList.paramarray[0];
  const int num_params = sizeofArray(TRP_INFO_Params);
  const unsigned num_trp = *gpd(params, num_params, CONFIG_STRING_TRP_INFO_NUM_TRPS)->uptr;
  if (num_trp == 0)
    return NULL;
  if (num_trp > MAX_NUM_TRPs) {
    LOG_E(NR_PHY, "Number of TRPs %u exceeds maximum %d\n", num_trp, MAX_NUM_TRPs);
    return NULL;
  }

  const paramdef_t *ids = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_IDS_LIST);
  const paramdef_t *x = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_X_AXIS_LIST);
  const paramdef_t *y = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_Y_AXIS_LIST);
  const paramdef_t *z = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_Z_AXIS_LIST);
  const paramdef_t *units = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_UNIT_LIST);
  if (ids->numelt != num_trp) {
    LOG_E(NR_PHY, "TRPIDs must contain NumTRPs (%u) entries\n", num_trp);
    return NULL;
  }

  const bool has_coordinates = x->numelt || y->numelt || z->numelt || units->numelt;
  if (has_coordinates &&
      (x->numelt != num_trp || y->numelt != num_trp || z->numelt != num_trp || units->numelt != num_trp)) {
    LOG_E(NR_PHY, "TRPxAxis, TRPyAxis, TRPzAxis and Units must all contain NumTRPs (%u) entries or all be omitted\n", num_trp);
    return NULL;
  }

  trp_info_config = calloc_or_fail(1, sizeof(*trp_info_config));
  trp_info_config->num_trp = num_trp;
  trp_info_config->has_coordinates = has_coordinates;
  for (unsigned i = 0; i < num_trp; i++) {
    trp_info_config->trps[i].id = ids->uptr[i];
    if (has_coordinates) {
      trp_info_config->trps[i].x_axis = x->uptr[i];
      trp_info_config->trps[i].y_axis = y->uptr[i];
      trp_info_config->trps[i].z_axis = z->uptr[i];
      trp_info_config->trps[i].unit = units->uptr[i];
    }
  }
  return trp_info_config;
}

void dump_trp_info_config(const trp_info_config_t *trp_info_config)
{
  uint8_t num_trp = trp_info_config->num_trp;
  LOG_I(NR_PHY, "-----------------------------------------\n");
  LOG_I(NR_PHY, "Positioning Config\n");
  LOG_I(NR_PHY, "-----------------------------------------\n");
  LOG_I(NR_PHY, "Number of TRPs \t\t%d\n", num_trp);
  for (int i = 0; i < num_trp; i++) {
    if (!trp_info_config->has_coordinates) {
      LOG_I(NR_PHY, "TRP ID %u: coordinates not configured\n", trp_info_config->trps[i].id);
      continue;
    }
    LOG_I(NR_PHY, "X-axis of TRP ID %d \t\t%d\n", trp_info_config->trps[i].id, trp_info_config->trps[i].x_axis);
    LOG_I(NR_PHY, "Y-axis of TRP ID %d \t\t%d\n", trp_info_config->trps[i].id, trp_info_config->trps[i].y_axis);
    LOG_I(NR_PHY, "Z-axis of TRP ID %d \t\t%d\n", trp_info_config->trps[i].id, trp_info_config->trps[i].z_axis);
  }
  LOG_I(NR_PHY, "-----------------------------------------\n");
}
