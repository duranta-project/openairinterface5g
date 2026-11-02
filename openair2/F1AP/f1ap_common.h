/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef F1AP_COMMON_H_
#define F1AP_COMMON_H_

#include "common/openairinterface5g_limits.h"
#include "oai_asn1.h"
#include <openair2/RRC/NR/MESSAGES/asn1_msg.h>

#define F1AP_TRANSACTION_IDENTIFIER_NUMBER 3

#include "f1ap_default_values.h"

#include "conversions.h"
#include "common/platform_types.h"
#include "common/utils/LOG/log.h"
#include "intertask_interface.h"
#include "sctp_messages_types.h"
#include "f1ap_messages_types.h"
#include <arpa/inet.h>
#include "T.h"
#include "common/ran_context.h"

/* Checking version of ASN1C compiler */
#if (ASN1C_ENVIRONMENT_VERSION < ASN1C_MINIMUM_VERSION)
  # error "You are compiling f1ap with the wrong version of ASN1C"
#endif


#include "assertions.h"

#include "common/utils/LOG/log.h"
#include "f1ap_default_values.h"
#include "F1AP_ProtocolIE-Container.h"
#define F1AP_ERROR(x, args...) LOG_E(F1AP, x, ##args)
#define F1AP_WARN(x, args...)  LOG_W(F1AP, x, ##args)
#define F1AP_TRAF(x, args...)  LOG_I(F1AP, x, ##args)
#define F1AP_INFO(x, args...) LOG_I(F1AP, x, ##args)
#define F1AP_DEBUG(x, args...) LOG_I(F1AP, x, ##args)

//Forward declaration
#define F1AP_FIND_PROTOCOLIE_BY_ID(IE_TYPE, ie, container, IE_ID, mandatory) \
  do {\
    IE_TYPE **ptr; \
    ie = NULL; \
    for (ptr = container->protocolIEs->list.array; \
         ptr < &container->protocolIEs->list.array[container->protocolIEs->list.count]; \
         ptr++) { \
      if((*ptr)->id == IE_ID) { \
        ie = *ptr; \
        break; \
      } \
    } \
    if (mandatory) DevAssert(ie != NULL); \
  } while(0)

/** \brief Function array prototype.
 **/
struct F1AP_F1AP_PDU;
typedef struct F1AP_F1AP_PDU F1AP_F1AP_PDU_t;
typedef int (*f1ap_message_processing_t)(instance_t instance, sctp_assoc_t assoc_id, uint32_t stream, F1AP_F1AP_PDU_t *message_p);
int f1ap_handle_message(instance_t instance,
                        sctp_assoc_t assoc_id,
                        int32_t stream,
                        const uint8_t *const data,
                        const uint32_t data_length);

typedef struct f1ap_cudu_inst_s {
  f1ap_setup_req_t setupReq;

  /* The eNB IP address to bind */
  f1ap_net_config_t net_config;

  /* SCTP information */
  struct {
    sctp_assoc_t assoc_id;
  } du;
  uint16_t sctp_in_streams;
  uint16_t sctp_out_streams;

  /* GTP instance used by F1 */
  instance_t gtpInst;
} f1ap_cudu_inst_t;

uint8_t F1AP_get_next_transaction_identifier(instance_t mod_idP, instance_t cu_mod_idP);

f1ap_cudu_inst_t *getCxt(instance_t instanceP);

void createF1inst(instance_t instanceP, f1ap_setup_req_t *req, f1ap_net_config_t *nc);
void destroyF1inst(instance_t instance);

//lts: C struct type is not homogeneous, so we need macros instead of functions
#define addnRCGI(nRCGi, servedCelL) \
  MCC_MNC_TO_PLMNID((servedCelL)->plmn.mcc,(servedCelL)->plmn.mnc,(servedCelL)->plmn.mnc_digit_length, \
                    &((nRCGi).pLMN_Identity));        \
  NR_CELL_ID_TO_BIT_STRING((servedCelL)->nr_cellid, &((nRCGi).nRCellIdentity));
extern RAN_CONTEXT_t RC;

#endif /* F1AP_COMMON_H_ */
