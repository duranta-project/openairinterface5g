/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <stdio.h>
#include <stdbool.h>

#include "openair2/LAYER2/NR_MAC_gNB/mac_config.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"

#define TELNETSERVERCODE
#include "telnetsrv.h"

#define ERROR_MSG_RET(mSG, aRGS...) \
  do {                              \
    prnt(mSG, ##aRGS);              \
    return 1;                       \
  } while (0)

/**
 * Module brief:
 * This module updates the NTN assistance information broadcast in SIB19 while the gNB runs, for
 * setups in which the satellite channel is emulated outside of the gNB (SDR boards and an external
 * channel emulator). With RFsimulator, the channel model updates SIB19 itself.
 * See common/utils/telnetsrv/DOC/telnetntn.md.
 *
 * Loading the module:
 * sudo ./nr-softmodem -O <NTN gNB conf file> --telnetsrv --telnetsrv.shrmod ntn
 */

#define UPDATE_SIB19_ARGS "<ta_common> <drift> <drift_variant> <x> <y> <z> <vx> <vy> <vz> <epoch_lead_ms>"

/**
 * @brief Update the SIB19 of every NTN cell
 * @param buf: the SIB19 values in the units of their fields (TS 38.331), and the lead in ms of the
 *             instant they describe over the current time of the gNB, which becomes epochTime-r17
 * @param debug: Debug flag
 * @param prnt: Print function
 * @return 0 on success, 1 on failure
 */
static int ntn_update_sib19(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  gnb_sat_position_update_t update = {0};
  if (!buf
      || sscanf(buf,
                "%u %d %u %d %d %d %d %d %d %d",
                &update.delay,
                &update.drift,
                &update.accel,
                &update.position.X,
                &update.position.Y,
                &update.position.Z,
                &update.velocity.X,
                &update.velocity.Y,
                &update.velocity.Z,
                &update.epoch_lead_ms)
             != 10)
    ERROR_MSG_RET("usage: ntn update_sib19 " UPDATE_SIB19_ARGS "\n");
  if (update.epoch_lead_ms < 1 || update.epoch_lead_ms > NR_SIB19_MAX_EPOCH_LEAD_MS)
    ERROR_MSG_RET("epoch_lead_ms must be 1 to %d\n", NR_SIB19_MAX_EPOCH_LEAD_MS);
  if (!nr_update_sib19(&update))
    ERROR_MSG_RET("no cell with an NTN configuration\n");

  prnt("SIB19 updated, epoch %d ms ahead\n", update.epoch_lead_ms);
  return 0;
}

static telnetshell_cmddef_t ntn_cmds[] = {
    {"update_sib19", UPDATE_SIB19_ARGS, ntn_update_sib19},
    {"", "", NULL},
};

static telnetshell_vardef_t ntn_vars[] = {{"", 0, 0, NULL}};

void add_ntn_cmds(void)
{
  add_telnetcmd("ntn", ntn_vars, ntn_cmds);
}
