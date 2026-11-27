/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __PHY_NR_REFSIG_NR_MOD_TABLE__H__
#define __PHY_NR_REFSIG_NR_MOD_TABLE__H__

#define NR_MOD_TABLE_SIZE_SHORT 686

extern c16_t nr_qpsk_mod_table[4];
extern simde__m128i nr_qpsk_byte_mod_table[256];

extern int32_t nr_16qam_mod_table[16];
extern int64_t nr_16qam_byte_mod_table[256];

extern int64_t nr_64qam_mod_table[4096];

extern int32_t nr_256qam_mod_table[256];

extern int32_t nr_1024qam_mod_table[1024];
#endif
