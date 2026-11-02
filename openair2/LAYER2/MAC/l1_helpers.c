/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/*!
 * \brief phy helper function
 */

#include "mac.h"
#include "mac_extern.h"
#include "common/utils/LOG/log.h"
#include "mac_proto.h"

int8_t get_Po_NOMINAL_PUSCH(module_id_t module_idP, uint8_t CC_id)
{
    LTE_RACH_ConfigCommon_t *rach_ConfigCommon = NULL;

    AssertFatal(CC_id == 0,
		"Transmission on secondary CCs is not supported yet\n");
    AssertFatal(UE_mac_inst[module_idP].radioResourceConfigCommon != NULL,
		"[UE %d] CCid %d FATAL radioResourceConfigCommon is NULL !!!\n",
		module_idP, CC_id);

    rach_ConfigCommon =
	UE_mac_inst[module_idP].radioResourceConfigCommon->
	rach_ConfigCommon;

    return (-120 +
	    (rach_ConfigCommon->
	     powerRampingParameters->preambleInitialReceivedTargetPower <<
	     1) + get_DELTA_PREAMBLE(module_idP, CC_id));
}

int8_t get_DELTA_PREAMBLE(module_id_t module_idP, int CC_id)
{

    AssertFatal(CC_id == 0,
		"Transmission on secondary CCs is not supported yet\n");
    uint8_t prachConfigIndex =
	UE_mac_inst[module_idP].radioResourceConfigCommon->
	prach_Config->prach_ConfigInfo->prach_ConfigIndex;
    uint8_t preambleformat;

    if (UE_mac_inst[module_idP].tdd_Config) {	// TDD
	if (prachConfigIndex < 20) {
	    preambleformat = 0;
	} else if (prachConfigIndex < 30) {
	    preambleformat = 1;
	} else if (prachConfigIndex < 40) {
	    preambleformat = 2;
	} else if (prachConfigIndex < 48) {
	    preambleformat = 3;
	} else {
	    preambleformat = 4;
	}
    } else {			// FDD
	preambleformat = prachConfigIndex >> 2;
    }

    switch (preambleformat) {
    case 0:
    case 1:
	return (0);

    case 2:
    case 3:
	return (-3);

    case 4:
	return (8);

    default:
	AssertFatal(1 == 0,
		    "[UE %d] ue_procedures.c: FATAL, Illegal preambleformat %d, prachConfigIndex %d\n",
		    module_idP, preambleformat, prachConfigIndex);
    }

}

int8_t get_deltaP_rampup(module_id_t module_idP, uint8_t CC_id)
{

    AssertFatal(CC_id == 0,
		"Transmission on secondary CCs is not supported yet\n");

    LOG_D(MAC, "[PUSCH]%d dB\n",
	  UE_mac_inst[module_idP].RA_PREAMBLE_TRANSMISSION_COUNTER << 1);
    return ((int8_t)
	    (UE_mac_inst[module_idP].
	     RA_PREAMBLE_TRANSMISSION_COUNTER << 1));

}
