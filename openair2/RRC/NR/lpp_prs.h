/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef LPP_PRS_H_
#define LPP_PRS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

struct f1ap_prs_configuration_s;
struct f1ap_ng_ran_cgi_s;
bool lpp_prs_from_f1(const struct f1ap_prs_configuration_s *prs, const struct f1ap_ng_ran_cgi_s *cgi,
                     uint16_t pci, uint8_t **segment, size_t *segment_len);

struct NR_BCCH_DL_SCH_Message;

struct NR_BCCH_DL_SCH_Message *create_rrc_possib_from_segment(const uint8_t *segment, size_t length);

#endif /* LPP_PRS_H_ */
