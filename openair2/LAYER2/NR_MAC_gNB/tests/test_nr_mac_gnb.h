/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

/* gNB MAC tests are written in C (the MAC headers are not C++ compatible) and called from
 * GoogleTest in test_nr_mac_gnb.cpp. */

#ifndef TEST_NR_MAC_GNB_H
#define TEST_NR_MAC_GNB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void nr_mac_gnb_test_init(void);

/* returns the number of other SI DCIs checked, or -1 on a wrong CORESET0 */
int test_other_sib_ssb_index(uint64_t ssb_bitmap);

#ifdef __cplusplus
}
#endif

#endif
