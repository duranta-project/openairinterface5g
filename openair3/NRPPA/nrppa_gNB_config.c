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
  paramdef_t trp_info_params_desc[] = TRP_INFO_PARAMS_DESC;
  int num_params = sizeofArray(trp_info_params_desc);
  GET_PARAMS_LIST(TRP_INFO_ParamList, TRP_INFO_Params, TRP_INFO_PARAMS_DESC, CONFIG_STRING_TRP_INFO_CONFIG, NULL);

  if (TRP_INFO_ParamList.numelt > 0) {
    paramdef_t *params = TRP_INFO_ParamList.paramarray[0];

    uint8_t num_trp = *gpd(params, num_params, CONFIG_STRING_TRP_INFO_NUM_TRPS)->uptr;
    if (num_trp == 0) {
      return NULL;
    } else if (num_trp > MAX_NUM_TRPs) {
      AssertFatal(false, "Exceeded MAX number of TRPs\n");
    }

    uint8_t num_ids = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_IDS_LIST)->numelt;
    uint8_t num_x = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_X_AXIS_LIST)->numelt;
    uint8_t num_y = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_Y_AXIS_LIST)->numelt;
    uint8_t num_z = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_Z_AXIS_LIST)->numelt;
    uint8_t num_unit = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_UNIT_LIST)->numelt;

    if (num_trp != num_ids || num_trp != num_x || num_trp != num_y || num_trp != num_z || num_trp != num_unit) {
      AssertFatal(false, "Mismatch in the number of TRPs with the number of ids, x-axis, y-axis, z-axis, units\n");
    }

    trp_info_config = calloc_or_fail(1, sizeof(trp_info_config_t));
    trp_info_config->num_trp = num_trp;
    for (int l = 0; l < num_trp; l++) {
      trp_info_config->trps[l].id = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_IDS_LIST)->uptr[l];
      trp_info_config->trps[l].x_axis = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_X_AXIS_LIST)->uptr[l];
      trp_info_config->trps[l].y_axis = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_Y_AXIS_LIST)->uptr[l];
      trp_info_config->trps[l].z_axis = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_Z_AXIS_LIST)->uptr[l];
      trp_info_config->trps[l].unit = gpd(params, num_params, CONFIG_STRING_TRP_INFO_TRP_UNIT_LIST)->uptr[l];
      }
  } else {
    LOG_I(NR_PHY, "No " CONFIG_STRING_TRP_INFO_CONFIG " configuration found..!!\n");
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
    LOG_I(NR_PHY, "X-axis of TRP ID %d \t\t%d\n", trp_info_config->trps[i].id, trp_info_config->trps[i].x_axis);
    LOG_I(NR_PHY, "Y-axis of TRP ID %d \t\t%d\n", trp_info_config->trps[i].id, trp_info_config->trps[i].y_axis);
    LOG_I(NR_PHY, "Z-axis of TRP ID %d \t\t%d\n", trp_info_config->trps[i].id, trp_info_config->trps[i].z_axis);
  }
  LOG_I(NR_PHY, "-----------------------------------------\n");
}
