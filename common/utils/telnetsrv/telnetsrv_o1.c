/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include <sys/types.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define TELNETSERVERCODE
#include "telnetsrv.h"

#include "openair2/RRC/NR/nr_rrc_defs.h"
#include "openair2/LAYER2/NR_MAC_gNB/nr_mac_gNB.h"
#include "openair2/LAYER2/NR_MAC_gNB/nr_radio_config.h"
#include "openair2/LAYER2/NR_MAC_gNB/mac_proto.h"
#include "openair2/LAYER2/nr_rlc/nr_rlc_oai_api.c"
#include "common/utils/nr/nr_common.h"
#include "common/utils/ocp_itti/intertask_interface.h"

#define ERROR_MSG_RET(mSG, aRGS...) do { prnt("FAILURE: " mSG, ##aRGS); return 1; } while (0)

#define ISINITBWP "bwp3gpp:isInitialBwp"
//#define CYCLPREF  "bwp3gpp:cyclicPrefix"
#define NUMRBS    "bwp3gpp:numberOfRBs"
#define STARTRB   "bwp3gpp:startRB"
#define BWPSCS    "bwp3gpp:subCarrierSpacing"

#define SSBFREQ "nrcelldu3gpp:ssbFrequency"
#define ARFCNDL "nrcelldu3gpp:arfcnDL"
#define BWDL    "nrcelldu3gpp:bSChannelBwDL"
#define ARFCNUL "nrcelldu3gpp:arfcnUL"
#define BWUL    "nrcelldu3gpp:bSChannelBwUL"
#define PCI     "nrcelldu3gpp:nRPCI"
#define TAC     "nrcelldu3gpp:nRTAC"
#define MCC     "nrcelldu3gpp:mcc"
#define MNC     "nrcelldu3gpp:mnc"
#define SD      "nrcelldu3gpp:sd"
#define SST     "nrcelldu3gpp:sst"

typedef struct b {
  long int dl;
  long int ul;
} b_t;

typedef struct ue_stat {
  rnti_t rnti;
  b_t thr;
} ue_stat_t;

#define PRINTLIST_i(len, fmt, ...) \
  { \
    for (int i = 0; i < len; ++i) { \
      if (i != 0) prnt(", "); \
      prnt(fmt, __VA_ARGS__); \
    } \
  } \

static int get_stats(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  if (buf)
    ERROR_MSG_RET("no parameter allowed\n");

  MessageDef *msg_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_GET_O1_STATS);
  MessageDef *resp_p;

  if (!itti_send_and_receive_msg_to_task(TASK_MAC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
    ERROR_MSG_RET("Timeout waiting for MAC response\n");
  }

  Mac_get_o1_stats *current = &resp_p->ittiMsg.mac_get_o1_stats;

  static uint64_t last_dl_used = 0;
  static uint64_t last_dl_total = 0;
  int diff_used = current->dl_used_prb_aggregate - last_dl_used;
  int diff_total = current->dl_total_prb_aggregate - last_dl_total;
  int load = diff_total > 0 ? 100 * diff_used / diff_total : 0;
  last_dl_used = current->dl_used_prb_aggregate;
  last_dl_total = current->dl_total_prb_aggregate;

  static struct timespec tp_last = {0};
  size_t diff_msec = (current->tp_now.tv_sec - tp_last.tv_sec) * 1000 +
                     (current->tp_now.tv_nsec - tp_last.tv_nsec) / 1000000;
  tp_last = current->tp_now;
  if (diff_msec == 0) diff_msec = 1; /* Avoid division by zero */

  static long last_total_dl[MAX_MOBILES_PER_GNB] = {0};
  static long last_total_ul[MAX_MOBILES_PER_GNB] = {0};

  ue_stat_t ue_stat[MAX_MOBILES_PER_GNB] = {0};
  int num_ues = current->num_ues;

  for (int i = 0; i < num_ues; i++) {
    const rnti_t rnti = current->ue_rlc_stats[i].rnti;

    if (last_total_dl[i] > current->ue_rlc_stats[i].txpdu_bytes)
      last_total_dl[i] = current->ue_rlc_stats[i].txpdu_bytes;
    if (last_total_ul[i] > current->ue_rlc_stats[i].rxpdu_bytes)
      last_total_ul[i] = current->ue_rlc_stats[i].rxpdu_bytes;

    ue_stat[i].rnti = rnti;
    ue_stat[i].thr.dl = (current->ue_rlc_stats[i].txpdu_bytes - last_total_dl[i]) * 8 / diff_msec;
    ue_stat[i].thr.ul = (current->ue_rlc_stats[i].rxpdu_bytes - last_total_ul[i]) * 8 / diff_msec;

    last_total_dl[i] = current->ue_rlc_stats[i].txpdu_bytes;
    last_total_ul[i] = current->ue_rlc_stats[i].rxpdu_bytes;
  }

  prnt("{\n");
    prnt("  \"o1-config\": {\n");

    prnt("    \"BWP\": {\n");
    prnt("      \"dl\": [{\n");
    prnt("        \"" ISINITBWP "\": true,\n");
    prnt("        \"" NUMRBS "\": %ld,\n", current->dl_numrbs);
    prnt("        \"" STARTRB "\": %ld,\n", current->dl_startrb);
    prnt("        \"" BWPSCS "\": %ld\n", current->dl_bwpscs);
    prnt("      }],\n");
    prnt("      \"ul\": [{\n");
    prnt("        \"" ISINITBWP "\": true,\n");
    prnt("        \"" NUMRBS "\": %ld,\n", current->ul_numrbs);
    prnt("        \"" STARTRB "\": %ld,\n", current->ul_startrb);
    prnt("        \"" BWPSCS "\": %ld\n", current->ul_bwpscs);
    prnt("      }]\n");
    prnt("    },\n");

    prnt("    \"NRCELLDU\": {\n");
    prnt("      \"" SSBFREQ "\": %ld,\n", current->ssbFrequency);
    prnt("      \"" ARFCNDL "\": %ld,\n", current->arfcnDL);
    prnt("      \"" BWDL "\": %ld,\n", current->bw_mhz);
    prnt("      \"" ARFCNUL "\": %ld,\n", current->arfcnUL);
    prnt("      \"" BWUL "\": %ld,\n", current->bw_mhz);
    prnt("      \"" PCI "\": %ld,\n", current->pci);
    prnt("      \"" TAC "\": %ld,\n", current->tac);
    prnt("      \"" MCC "\": \"%03d\",\n", current->mcc);
    prnt("      \"" MNC "\": \"%0*d\",\n", current->mnc_digit_length, current->mnc);
    prnt("      \"" SD  "\": %d,\n", current->sd);
    prnt("      \"" SST "\": %d\n", current->sst);
    prnt("    },\n");
    prnt("    \"device\": {\n");
    prnt("      \"gnbId\": %d,\n", current->gNB_DU_id);
    prnt("      \"gnbName\": \"%s\",\n", current->gNB_DU_name);
    prnt("      \"vendor\": \"OpenAirInterface\"\n");
    prnt("    }\n");
    prnt("  },\n");

    prnt("  \"O1-Operational\": {\n");
    prnt("    \"frame-type\": \"%s\",\n", current->frame_type == TDD ? "tdd" : "fdd");
    prnt("    \"band-number\": %ld,\n", current->band);
    prnt("    \"num-ues\": %d,\n", num_ues);
    prnt("    \"ues\": ["); PRINTLIST_i(num_ues, "%d", ue_stat[i].rnti); prnt("],\n");
    prnt("    \"load\": %d,\n", load);
    prnt("    \"ues-thp\": [");
      PRINTLIST_i(num_ues, "\n      {\"rnti\": %d, \"dl\": %ld, \"ul\": %ld}", ue_stat[i].rnti, ue_stat[i].thr.dl, ue_stat[i].thr.ul);
    prnt("\n    ]\n");
    prnt("  }\n");
  prnt("}\n");
  prnt("OK\n");

  itti_free(ITTI_MSG_ORIGIN_ID(resp_p), resp_p);
  return 0;
}

static int read_long(const char *buf, const char *end, const char *id, long *val)
{
  const char *curr = buf;
  while (isspace(*curr) && curr < end) // skip leading spaces
    curr++;
  int len = strlen(id);
  if (curr + len >= end)
    return -1;
  if (strncmp(curr, id, len) != 0) // check buf has id
    return -1;
  curr += len;
  while (isspace(*curr) && curr < end) // skip middle spaces
    curr++;
  if (curr >= end)
    return -1;
  int nread = sscanf(curr, "%ld", val);
  if (nread != 1)
    return -1;
  while (isdigit(*curr) && curr < end) // skip all digits read above
    curr++;
  if (curr > end)
    return -1;
  return curr - buf;
}

bool running = true; // in the beginning, the softmodem is started automatically
static int set_config(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  if (!buf)
    ERROR_MSG_RET("need param: o1 config param1 val1 [param2 val2 ...]\n");
  if (running)
    ERROR_MSG_RET("cannot set parameters while L1 is running\n");
  const char *end = buf + strlen(buf);

  /* we need to update the following fields to change frequency and/or
   * bandwidth:
   * --gNBs.[0].servingCellConfigCommon.[0].absoluteFrequencySSB 620736            -> SSBFREQ
   * --gNBs.[0].servingCellConfigCommon.[0].dl_absoluteFrequencyPointA 620020      -> ARFCNDL
   * --gNBs.[0].servingCellConfigCommon.[0].dl_carrierBandwidth 51                 -> BWDL
   * --gNBs.[0].servingCellConfigCommon.[0].initialDLBWPlocationAndBandwidth 13750 -> NUMRBS + STARTRB
   * --gNBs.[0].servingCellConfigCommon.[0].ul_carrierBandwidth 51                 -> BWUL?
   * --gNBs.[0].servingCellConfigCommon.[0].initialULBWPlocationAndBandwidth 13750 -> ?
   */

  int processed = 0;
  int pos = 0;

  long ssbfreq;
  processed = read_long(buf + pos, end, SSBFREQ, &ssbfreq);
  if (processed < 0)
    ERROR_MSG_RET("could not read " SSBFREQ " at index %d\n", pos + processed);
  pos += processed;
  prnt("setting " SSBFREQ ":   %ld [len %d]\n", ssbfreq, pos);

  long arfcn;
  processed = read_long(buf + pos, end, ARFCNDL, &arfcn);
  if (processed < 0)
    ERROR_MSG_RET("could not read " ARFCNDL " at index %d\n", pos + processed);
  pos += processed;
  prnt("setting " ARFCNDL ":        %ld [len %d]\n", arfcn, pos);

  long bwdl;
  processed = read_long(buf + pos, end, BWDL, &bwdl);
  if (processed < 0)
    ERROR_MSG_RET("could not read " BWDL " at index %d\n", pos + processed);
  pos += processed;
  prnt("setting " BWDL ":  %ld [len %d]\n", bwdl, pos);

  long numrbs;
  processed = read_long(buf + pos, end, NUMRBS, &numrbs);
  if (processed < 0)
    ERROR_MSG_RET("could not read " NUMRBS " at index %d\n", pos + processed);
  pos += processed;
  prnt("setting " NUMRBS ":         %ld [len %d]\n", numrbs, pos);

  long startrb;
  processed = read_long(buf + pos, end, STARTRB, &startrb);
  if (processed < 0)
    ERROR_MSG_RET("could not read " STARTRB " at index %d\n", pos + processed);
  pos += processed;
  prnt("setting " STARTRB ":             %ld [len %d]\n", startrb, pos);

  int locationAndBandwidth = PRBalloc_to_locationandbandwidth0(numrbs, startrb, MAX_BWP_SIZE);
  prnt("inferred locationAndBandwidth:       %d\n", locationAndBandwidth);
  prnt("OK\n");
  return 0;
}

static int set_bwconfig(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  if (running)
    ERROR_MSG_RET("cannot set parameters while L1 is running\n");
  if (!buf)
    ERROR_MSG_RET("need param: o1 bwconfig <BW>\n");

  char *end = NULL;
  if (NULL != (end = strchr(buf, '\n')))
    *end = 0;
  if (NULL != (end = strchr(buf, '\r')))
    *end = 0;

  int bw_value = atoi(buf);
  if (bw_value != 20 && bw_value != 40 && bw_value != 60 && bw_value != 100)
    ERROR_MSG_RET("unhandled option %s\n", buf);

  MessageDef *msg_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_SET_BWCONFIG);
  msg_p->ittiMsg.mac_set_bwconfig.bw_value = bw_value;
  MessageDef *resp_p;

  if (!itti_send_and_receive_msg_to_task(TASK_MAC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
    ERROR_MSG_RET("Timeout waiting for MAC response\n");
  }

  itti_free(ITTI_MSG_ORIGIN_ID(resp_p), resp_p);
  prnt("OK\n");
  return 0;
}

extern int stop_L1(module_id_t gnb_id);
static int stop_modem(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  UNUSED(buf);
  if (!running)
    ERROR_MSG_RET("cannot stop, nr-softmodem not running\n");

  MessageDef *msg_p = itti_alloc_new_message(TASK_MAC_GNB, 0, MAC_STOP_MODEM);
  MessageDef *resp_p;

  if (!itti_send_and_receive_msg_to_task(TASK_MAC_GNB, TASK_TELNET, msg_p, &resp_p, 1000)) {
    ERROR_MSG_RET("Timeout waiting for MAC response\n");
  }

  itti_free(ITTI_MSG_ORIGIN_ID(resp_p), resp_p);
  running = false;
  prnt("OK\n");
  return 0;
}

extern int start_L1L2(module_id_t gnb_id, nr_cell_sched_t *cell);
static int start_modem(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  UNUSED(buf);
  if (running)
    ERROR_MSG_RET("cannot start, nr-softmodem already running\n");
  start_L1L2(0, &RC.nrmac[0]->cells[0]);
  running = true;
  prnt("OK\n");
  return 0;
}

extern void du_clear_all_ue_states();
static int remove_mac_ues(char *buf, int debug, telnet_printfunc_t prnt)
{
  UNUSED(debug);
  UNUSED(buf);
  du_clear_all_ue_states();
  prnt("OK\n");
  return 0;
}

static telnetshell_cmddef_t o1cmds[] = {
  {"stats", "", get_stats},
  {"config", "[]", set_config},
  {"bwconfig", "", set_bwconfig},
  {"stop_modem", "", stop_modem},
  {"start_modem", "", start_modem},
  {"remove_mac_ues", "", remove_mac_ues},
  {"", "", NULL},
};

static telnetshell_vardef_t o1vars[] = {
  {"", 0, 0, NULL}
};

void add_o1_cmds(void) {
  add_telnetcmd("o1", o1vars, o1cmds);
}
