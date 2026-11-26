/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_ue_csi.h"
#include "common/utils/LOG/log.h"

bool configure_csi_resource_mapping(fapi_nr_dl_config_csirs_pdu_rel15_t *out,
                                    const NR_CSI_RS_ResourceMapping_t *resourceMapping,
                                    uint32_t bwp_size,
                                    uint32_t bwp_start)
{
  const long configured_start = resourceMapping->freqBand.startingRB;
  const long configured_rbs = resourceMapping->freqBand.nrofRBs;
  // TS 38.331 section 6.3.2, CSI-FrequencyOccupation: the 24-RB minimum applies to the signalled width, before BWP clipping.
  if (bwp_size == 0 || bwp_size > MAX_BWP_SIZE || bwp_start > MAX_BWP_SIZE - bwp_size || configured_start < 0
      || configured_start >= MAX_BWP_SIZE || configured_start % 4 != 0 || configured_rbs < 24 || configured_rbs > MAX_BWP_SIZE + 1
      || configured_rbs % 4 != 0) {
    LOG_W(NR_MAC, "Invalid CSI-RS range: BWP %u+%u, CSI-RS %ld+%ld\n", bwp_start, bwp_size, configured_start, configured_rbs);
    return false;
  }

  // TS 38.214 section 5.2.2.3.1: adjust the start before limiting the bandwidth.
  const uint32_t start_rb = configured_start < bwp_start ? bwp_start : configured_start;
  const uint32_t bwp_end = bwp_start + bwp_size;
  if (start_rb >= bwp_end) {
    LOG_W(NR_MAC, "CSI-RS starts at RB %u outside BWP %u+%u\n", start_rb, bwp_start, bwp_size);
    return false;
  }
  const uint32_t remaining_rbs = bwp_end - start_rb;
  const uint32_t nr_of_rbs = configured_rbs < remaining_rbs ? configured_rbs : remaining_rbs;
  const uint32_t minimum_rbs = bwp_size < 24 ? bwp_size : 24;
  if (nr_of_rbs < minimum_rbs) {
    LOG_W(NR_MAC, "CSI-RS has %u RBs in BWP %u+%u, but requires at least %u\n", nr_of_rbs, bwp_start, bwp_size, minimum_rbs);
    return false;
  }

  const BIT_STRING_t *allocation = NULL;
  size_t allocation_size = 1;
  switch (resourceMapping->frequencyDomainAllocation.present) {
    case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_row1:
      allocation = &resourceMapping->frequencyDomainAllocation.choice.row1;
      break;
    case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_row2:
      allocation = &resourceMapping->frequencyDomainAllocation.choice.row2;
      allocation_size = 2;
      break;
    case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_row4:
      allocation = &resourceMapping->frequencyDomainAllocation.choice.row4;
      break;
    case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_other:
      allocation = &resourceMapping->frequencyDomainAllocation.choice.other;
      break;
    default:
      return false;
  }
  if (!allocation->buf || allocation->size != allocation_size || resourceMapping->firstOFDMSymbolInTimeDomain < 0
      || resourceMapping->firstOFDMSymbolInTimeDomain > 13
      || (resourceMapping->firstOFDMSymbolInTimeDomain2
          && (*resourceMapping->firstOFDMSymbolInTimeDomain2 < 2 || *resourceMapping->firstOFDMSymbolInTimeDomain2 > 12))
      || resourceMapping->density.present < NR_CSI_RS_ResourceMapping__density_PR_dot5
      || resourceMapping->density.present > NR_CSI_RS_ResourceMapping__density_PR_three) {
    LOG_W(NR_MAC, "Invalid CSI-RS resource mapping\n");
    return false;
  }

  fapi_nr_dl_config_csirs_pdu_rel15_t pdu = *out;
  fapi_nr_dl_config_csirs_pdu_rel15_t *csirs_config_pdu = &pdu;
  csirs_config_pdu->start_rb = start_rb;
  csirs_config_pdu->nr_of_rbs = nr_of_rbs;

  csirs_config_pdu->symb_l0 = resourceMapping->firstOFDMSymbolInTimeDomain;
  csirs_config_pdu->symb_l1 = 0;
  if (resourceMapping->firstOFDMSymbolInTimeDomain2)
    csirs_config_pdu->symb_l1 = *resourceMapping->firstOFDMSymbolInTimeDomain2;
  csirs_config_pdu->cdm_type = resourceMapping->cdm_Type;

  csirs_config_pdu->freq_density = resourceMapping->density.present;
  if ((resourceMapping->density.present == NR_CSI_RS_ResourceMapping__density_PR_dot5)
      && (resourceMapping->density.choice.dot5 == NR_CSI_RS_ResourceMapping__density__dot5_evenPRBs))
    csirs_config_pdu->freq_density--;

  switch (resourceMapping->frequencyDomainAllocation.present) {
    case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_row1:
      csirs_config_pdu->row = 1;
      csirs_config_pdu->freq_domain = ((resourceMapping->frequencyDomainAllocation.choice.row1.buf[0]) >> 4) & 0x0f;
      break;
    case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_row2:
      csirs_config_pdu->row = 2;
      csirs_config_pdu->freq_domain = (((resourceMapping->frequencyDomainAllocation.choice.row2.buf[1] >> 4) & 0x0f)
                                       | ((resourceMapping->frequencyDomainAllocation.choice.row2.buf[0] << 4) & 0xff0));
      break;
    case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_row4:
      csirs_config_pdu->row = 4;
      csirs_config_pdu->freq_domain = ((resourceMapping->frequencyDomainAllocation.choice.row4.buf[0]) >> 5) & 0x07;
      break;
    case NR_CSI_RS_ResourceMapping__frequencyDomainAllocation_PR_other:
      csirs_config_pdu->freq_domain = ((resourceMapping->frequencyDomainAllocation.choice.other.buf[0]) >> 2) & 0x3f;
      // determining the row of table 7.4.1.5.3-1 in 38.211
      switch (resourceMapping->nrofPorts) {
        case NR_CSI_RS_ResourceMapping__nrofPorts_p1:
          LOG_W(NR_MAC, "Resource with 1 CSI port cannot use other CSI-RS rows\n");
          return false;
        case NR_CSI_RS_ResourceMapping__nrofPorts_p2:
          csirs_config_pdu->row = 3;
          break;
        case NR_CSI_RS_ResourceMapping__nrofPorts_p4:
          csirs_config_pdu->row = 5;
          break;
        case NR_CSI_RS_ResourceMapping__nrofPorts_p8:
          if (resourceMapping->cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm4_FD2_TD2)
            csirs_config_pdu->row = 8;
          else {
            int num_k = 0;
            for (int k = 0; k < 6; k++)
              num_k += (((csirs_config_pdu->freq_domain) >> k) & 0x01);
            if (num_k == 4)
              csirs_config_pdu->row = 6;
            else
              csirs_config_pdu->row = 7;
          }
          break;
        case NR_CSI_RS_ResourceMapping__nrofPorts_p12:
          if (resourceMapping->cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm4_FD2_TD2)
            csirs_config_pdu->row = 10;
          else
            csirs_config_pdu->row = 9;
          break;
        case NR_CSI_RS_ResourceMapping__nrofPorts_p16:
          if (resourceMapping->cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm4_FD2_TD2)
            csirs_config_pdu->row = 12;
          else
            csirs_config_pdu->row = 11;
          break;
        case NR_CSI_RS_ResourceMapping__nrofPorts_p24:
          if (resourceMapping->cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm4_FD2_TD2)
            csirs_config_pdu->row = 14;
          else {
            if (resourceMapping->cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm8_FD2_TD4)
              csirs_config_pdu->row = 15;
            else
              csirs_config_pdu->row = 13;
          }
          break;
        case NR_CSI_RS_ResourceMapping__nrofPorts_p32:
          if (resourceMapping->cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm4_FD2_TD2)
            csirs_config_pdu->row = 17;
          else {
            if (resourceMapping->cdm_Type == NR_CSI_RS_ResourceMapping__cdm_Type_cdm8_FD2_TD4)
              csirs_config_pdu->row = 18;
            else
              csirs_config_pdu->row = 16;
          }
          break;
        default:
          LOG_W(NR_MAC, "Invalid number of ports in CSI-RS resource\n");
          return false;
      }
      break;
    default:
      return false;
  }
  if (csirs_config_pdu->freq_domain == 0) {
    LOG_W(NR_MAC, "Empty CSI-RS frequency domain allocation\n");
    return false;
  }
  *out = pdu;
  return true;
}

bool nr_ue_csi_ratematching_is_supported(const fapi_nr_dl_config_dlsch_pdu_rel15_t *dlsch_pdu)
{
  for (int i = 0; i < dlsch_pdu->numCsiRsForRateMatching; i++) {
    const fapi_nr_dl_config_csirs_pdu_rel15_t *csi_pdu = &dlsch_pdu->csiRsForRateMatching[i];
    // The PHY rate-matching bitmap currently assumes CSI-RS spans the whole DLSCH BWP.
    if (csi_pdu->start_rb > dlsch_pdu->BWPStart
        || csi_pdu->start_rb + csi_pdu->nr_of_rbs < dlsch_pdu->BWPStart + dlsch_pdu->BWPSize) {
      LOG_W(NR_MAC, "CSI-RS rate matching over only part of the DLSCH BWP is not supported\n");
      return false;
    }
  }
  return true;
}
