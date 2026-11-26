/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef LPP_PRS_PERIOD_H
#define LPP_PRS_PERIOD_H

#include <stdint.h>

/* NR-DL-PRS-Periodicity-and-ResourceSetSlotOffset-r16: CHOICE alternatives in ASN.1 declaration order, indexed by SCS. */
static const uint32_t lpp_prs_periods[4][17] = {
    {4, 5, 8, 10, 16, 20, 32, 40, 64, 80, 160, 320, 640, 1280, 2560, 5120, 10240},
    {8, 10, 16, 20, 32, 40, 64, 80, 128, 160, 320, 640, 1280, 2560, 5120, 10240, 20480},
    {16, 20, 32, 40, 64, 80, 128, 160, 256, 320, 640, 1280, 2560, 5120, 10240, 20480, 40960},
    {32, 40, 64, 80, 128, 160, 256, 320, 512, 640, 1280, 2560, 5120, 10240, 20480, 40960, 81920},
};

#endif
