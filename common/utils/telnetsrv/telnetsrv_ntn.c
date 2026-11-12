/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <inttypes.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

#include "openair2/LAYER2/NR_MAC_gNB/mac_config.h"
#include "openair2/LAYER2/NR_MAC_COMMON/nr_mac_common.h"
#include "executables/softmodem-common.h"

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

#define SIB19_VALUES_ARGS "<ta_common> <drift> <drift_variant> <x> <y> <z> <vx> <vy> <vz>"
#define UPDATE_SIB19_ARGS SIB19_VALUES_ARGS " <epoch_lead_ms>"
#define UPDATE_SIB19_UTC_ARGS SIB19_VALUES_ARGS " <epoch_utc_ms>"

/**
 * @brief Parse the nine SIB19 values that both commands start with
 * @param buf: the arguments of the command
 * @param update: [out] the SIB19 values
 * @return the arguments that follow the nine values, or NULL if the values are not all there
 */
static const char *parse_sib19_values(const char *buf, gnb_sat_position_update_t *update)
{
  int end = 0;
  if (!buf
      || sscanf(buf,
                "%u %d %u %d %d %d %d %d %d%n",
                &update->delay,
                &update->drift,
                &update->accel,
                &update->position.X,
                &update->position.Y,
                &update->position.Z,
                &update->velocity.X,
                &update->velocity.Y,
                &update->velocity.Z,
                &end)
             != 9)
    return NULL;
  return buf + end;
}

/**
 * @brief Apply an update whose epoch lead is set, and report it
 * @param update: the SIB19 values and the epoch lead
 * @param prnt: Print function
 * @return 0 on success, 1 on failure
 */
static int apply_sib19_update(gnb_sat_position_update_t *update, telnet_printfunc_t prnt)
{
  if (!nr_update_sib19(update))
    ERROR_MSG_RET("no cell with an NTN configuration\n");

  prnt("SIB19 updated, epoch %d ms ahead\n", update->epoch_lead_ms);
  return 0;
}

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
  const char *epoch = parse_sib19_values(buf, &update);
  if (!epoch || sscanf(epoch, "%d", &update.epoch_lead_ms) != 1)
    ERROR_MSG_RET("usage: ntn update_sib19 " UPDATE_SIB19_ARGS "\n");
  if (update.epoch_lead_ms < 1 || update.epoch_lead_ms > NR_SIB19_MAX_EPOCH_LEAD_MS)
    ERROR_MSG_RET("epoch_lead_ms must be 1 to %d\n", NR_SIB19_MAX_EPOCH_LEAD_MS);
  return apply_sib19_update(&update, prnt);
}

/**
 * @brief Update the SIB19 of every NTN cell, with the epoch given in UTC
 * @param buf: the SIB19 values in the units of their fields (TS 38.331), and the UTC time in ms since
 *             1970-01-01 of the instant they describe, which becomes epochTime-r17. The clock of the
 *             source has to agree with the clock of the gNB host.
 * @param debug: Debug flag
 * @param prnt: Print function
 * @return 0 on success, 1 on failure
 */
static int ntn_update_sib19_utc(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  gnb_sat_position_update_t update = {0};
  int64_t epoch_utc_ms;
  const char *epoch = parse_sib19_values(buf, &update);
  if (!epoch || sscanf(epoch, "%" SCNd64, &epoch_utc_ms) != 1 || epoch_utc_ms < 0)
    ERROR_MSG_RET("usage: ntn update_sib19_utc " UPDATE_SIB19_UTC_ARGS "\n");
  // the clock of the host places the epoch, and the frames of RFsimulator do not follow it
  if (IS_SOFTMODEM_RFSIM)
    ERROR_MSG_RET("update_sib19_utc needs a gNB that runs at wall-clock speed, which RFsimulator does not: use update_sib19\n");

  struct timespec now;
  clock_gettime(CLOCK_REALTIME, &now);
  const int64_t lead_ms = nr_sib19_lead_from_utc(epoch_utc_ms, &now);
  if (lead_ms < 1 || lead_ms > NR_SIB19_MAX_EPOCH_LEAD_MS)
    ERROR_MSG_RET("epoch_utc_ms must be 1 to %d ms ahead of the gNB clock, and is %" PRId64 " ms\n",
                  NR_SIB19_MAX_EPOCH_LEAD_MS,
                  lead_ms);
  update.epoch_lead_ms = lead_ms;
  return apply_sib19_update(&update, prnt);
}

static telnetshell_cmddef_t ntn_cmds[] = {
    {"update_sib19", UPDATE_SIB19_ARGS, ntn_update_sib19},
    {"update_sib19_utc", UPDATE_SIB19_UTC_ARGS, ntn_update_sib19_utc},
    {"", "", NULL},
};

static telnetshell_vardef_t ntn_vars[] = {{"", 0, 0, NULL}};

void add_ntn_cmds(void)
{
  add_telnetcmd("ntn", ntn_vars, ntn_cmds);
}
