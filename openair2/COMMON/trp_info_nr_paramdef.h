/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __TRP_INFO_NR_PARAMDEF__H__
#define __TRP_INFO_NR_PARAMDEF__H__

/* Positioning configuration section names */
#define CONFIG_STRING_TRP_INFO_CONFIG "positioning_config"

/* Global parameters */

/* TRP configuration parameters names */
#define CONFIG_STRING_TRP_INFO_NUM_TRPS "NumTRPs"
#define CONFIG_STRING_TRP_INFO_TRP_IDS_LIST "TRPIDs"
#define CONFIG_STRING_TRP_INFO_TRP_X_AXIS_LIST "TRPxAxis"
#define CONFIG_STRING_TRP_INFO_TRP_Y_AXIS_LIST "TRPyAxis"
#define CONFIG_STRING_TRP_INFO_TRP_Z_AXIS_LIST "TRPzAxis"
#define CONFIG_STRING_TRP_INFO_TRP_UNIT_LIST "Units"

/* Help string for Positioning parameters */
#define HELP_STRING_TRP_INFO_NUM_TRPS "Number of TRPs connected with gNB(max 4)\n"
#define HELP_STRING_TRP_INFO_TRP_IDS_LIST "User defined IDs for each TRP \n"
#define HELP_STRING_TRP_INFO_TRP_X_AXIS_LIST "x-axis value of each TRP \n"
#define HELP_STRING_TRP_INFO_TRP_Y_AXIS_LIST "y-axis value of each TRP \n"
#define HELP_STRING_TRP_INFO_TRP_Z_AXIS_LIST "z-axis value of each TRP \n"
#define HELP_STRING_TRP_INFO_TRP_UNIT_LIST "XYZ unit value of each TRP \n"

/*----------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/*                                            TRP configuration                parameters */
/*   optname                                         helpstr                  paramflags    XXXptr              defXXXval type
 * numelt  */
/*----------------------------------------------------------------------------------------------------------------------------------------------------------------*/
// clang-format off
#define TRP_INFO_PARAMS_DESC { \
  {CONFIG_STRING_TRP_INFO_NUM_TRPS,             HELP_STRING_TRP_INFO_NUM_TRPS,            0,  .uptr=NULL,         .defuintval=0,              TYPE_UINT,       0},  \
  {CONFIG_STRING_TRP_INFO_TRP_IDS_LIST,         HELP_STRING_TRP_INFO_TRP_IDS_LIST,        0,  .uptr=NULL,         .defintarrayval=0,          TYPE_UINTARRAY,  0},  \
  {CONFIG_STRING_TRP_INFO_TRP_X_AXIS_LIST,      HELP_STRING_TRP_INFO_TRP_X_AXIS_LIST,     0,  .uptr=NULL,         .defintarrayval=0,          TYPE_UINTARRAY,  0},  \
  {CONFIG_STRING_TRP_INFO_TRP_Y_AXIS_LIST,      HELP_STRING_TRP_INFO_TRP_Y_AXIS_LIST,     0,  .uptr=NULL,         .defintarrayval=0,          TYPE_UINTARRAY,  0},  \
  {CONFIG_STRING_TRP_INFO_TRP_Z_AXIS_LIST,      HELP_STRING_TRP_INFO_TRP_Z_AXIS_LIST,     0,  .uptr=NULL,         .defintarrayval=0,          TYPE_UINTARRAY,  0},  \
  {CONFIG_STRING_TRP_INFO_TRP_UNIT_LIST,        HELP_STRING_TRP_INFO_TRP_UNIT_LIST,       0,  .uptr=NULL,         .defintarrayval=0,          TYPE_UINTARRAY,  0},  \
}
// clang-format on

#endif
