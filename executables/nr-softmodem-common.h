/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NR_SOFTMODEM_COMMON_H
#define NR_SOFTMODEM_COMMON_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <execinfo.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/sched.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/sysinfo.h>
#include "assertions.h"

/* help strings definition for command line options, used in CMDLINE_XXX_DESC macros and printed when -h option is used */
#define CONFIG_HLP_DLMCS_PHYTEST  "Set the downlink MCS for PHYTEST mode\n"
#define CONFIG_HLP_DLMCST_PHYTEST "Set the downlink MCS table index for PHYTEST mode\n"
#define CONFIG_HLP_DLNL_PHYTEST   "Set the downlink nrOfLayers for PHYTEST mode\n"
#define CONFIG_HLP_DLPMI_PHYTEST  "Set the downlink Precoding Matrix Index for PHYTEST mode\n"
#define CONFIG_HLP_ULNL_PHYTEST   "Set the uplink nrOfLayers for PHYTEST mode\n"
#define CONFIG_HLP_DLBW_PHYTEST   "Set the number of PRBs used for DLSCH in PHYTEST mode\n"
#define CONFIG_HLP_ULBW_PHYTEST   "Set the number of PRBs used for ULSCH in PHYTEST mode\n"
#define CONFIG_HLP_UECAP_FILE     "path for UE Capabilities file (empty UE capabilities sent if not provided)\n"
#define CONFIG_HLP_USRP_ARGS      "set the arguments to identify USRP (same syntax as in UHD)\n"
#define CONFIG_HLP_TX_SUBDEV      "set the arguments to select tx_subdev (same syntax as in UHD)\n"
#define CONFIG_HLP_RX_SUBDEV      "set the arguments to select rx_subdev (same syntax as in UHD)\n"
#define CONFIG_HLP_VCD            "Enable VCD (generated file will is named openair_dump_eNB.vcd, read it with target/RT/USER/eNB.gtkw\n"
#define CONFIG_HLP_RE_CFG_FILE    "filename for reconfig.raw in phy-test mode\n"
#define CONFIG_HLP_RB_CFG_FILE    "filename for rbconfig.raw in phy-test mode\n"
#define CONFIG_HLP_UERXG         "set UE RX gain\n"
#define CONFIG_HLP_UETXG         "set UE TX gain\n"
#define CONFIG_HLP_UENANTR       "set UE number of rx antennas\n"
#define CONFIG_HLP_UENANTT       "set UE number of tx antennas\n"
#define CONFIG_HLP_UESCAN "set UE to scan all possible GSCN in current bandwidth\n"
#define CONFIG_HLP_UEFO          "set UE to enable estimation and compensation of frequency offset\n"
#define CONFIG_HLP_PRB_SA         "Set the number of PRBs for SA\n"
#define CONFIG_HLP_SSC            "Set the start subcarrier \n"
#define CONFIG_HLP_DISABLETIMECORR "disable UE timing correction\n"
#define CONFIG_HLP_ULMCS_PHYTEST  "Set the uplink MCS for PHYTEST mode\n"
#define CONFIG_HLP_ULMCST_PHYTEST "Set the uplink MCS table index for PHYTEST mode\n"
#define CONFIG_HLP_DLBW_PHYTEST  "Set the number of PRBs used for DLSCH in PHYTEST mode\n"
#define CONFIG_HLP_ULBW_PHYTEST  "Set the number of PRBs used for ULSCH in PHYTEST mode\n"
#define CONFIG_HLP_DLBM_PHYTEST  "Bitmap for DLSCH slots in period (slot 0 starts at LSB)\n"
#define CONFIG_HLP_ULBM_PHYTEST  "Bitmap for ULSCH slots in period (slot 0 starts at LSB)\n"
#define CONFIG_HLP_DLMOD_PHYTEST "For-DLSCH, if val is n, every nth slot in slots per frame will be scheduled. \n"
#define CONFIG_HLP_ULMOD_PHYTEST "For ULSCH, if val is n, every nth slot in slots per frame will be scheduled. \n"
void wait_gNBs(void);
#endif
