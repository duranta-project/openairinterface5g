/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _OPENAIR2_LAYER2_NR_PDCP_ASN1_UTILS_H_
#define _OPENAIR2_LAYER2_NR_PDCP_ASN1_UTILS_H_

int decode_t_reordering(int v);
int decode_sn_size_ul(int v);
int decode_sn_size_dl(int v);
int decode_discard_timer(int v);
int decode_discard_timer_sl(int v);

int encode_t_reordering(int v);
int encode_sn_size_ul(int v);
int encode_sn_size_dl(int v);
int encode_discard_timer(int v);


#endif /* _OPENAIR2_LAYER2_NR_PDCP_ASN1_UTILS_H_ */
