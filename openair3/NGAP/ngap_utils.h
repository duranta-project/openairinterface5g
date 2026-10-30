/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef NGAP_UTILS_H_
#define NGAP_UTILS_H_

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "common/utils/LOG/log.h"
#include "assertions.h"
#include "common/utils/eq_check.h"

#define NGAP_UE_ID_FMT "0x%06" PRIX32

#define NGAP_ERROR(x, args...) LOG_E(NGAP, x, ##args)
#define NGAP_WARN(x, args...) LOG_W(NGAP, x, ##args)
#define NGAP_TRAF(x, args...) LOG_I(NGAP, x, ##args)
#define NGAP_INFO(x, args...) LOG_I(NGAP, x, ##args)
#define NGAP_DEBUG(x, args...) LOG_D(NGAP, x, ##args)

#define NGAP_FIND_PROTOCOLIE_BY_ID(IE_TYPE, ie, container, IE_ID, mandatory)                                                   \
  do {                                                                                                                         \
    IE_TYPE **ptr;                                                                                                             \
    ie = NULL;                                                                                                                 \
    for (ptr = container->protocolIEs->list.array; ptr < &container->protocolIEs->list.array[container->protocolIEs->list.count]; \
         ptr++) {                                                                                                              \
      if ((*ptr)->id == IE_ID) {                                                                                               \
        ie = *ptr;                                                                                                             \
        break;                                                                                                                 \
      }                                                                                                                        \
    }                                                                                                                          \
    if (ie == NULL) {                                                                                                          \
      if (mandatory) {                                                                                                         \
        AssertFatal(NGAP, "NGAP_FIND_PROTOCOLIE_BY_ID ie is NULL (searching for ie: %ld)\n", IE_ID);                           \
      } else {                                                                                                                 \
        NGAP_DEBUG("NGAP_FIND_PROTOCOLIE_BY_ID ie is NULL (searching for ie: %ld)\n", IE_ID);                                  \
      }                                                                                                                        \
    }                                                                                                                          \
  } while (0);                                                                                                                 \
  if (mandatory && !ie)                                                                                                        \
  return -1

#endif /* NGAP_UTILS_H_ */
