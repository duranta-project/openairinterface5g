/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#ifndef NFAPI_PNF_PACING_H
#define NFAPI_PNF_PACING_H

#include <stdbool.h>
#include <time.h>

typedef struct {
  bool initialized;
  struct timespec next_slot;
} nfapi_pnf_pacer_t;

long nfapi_pnf_slot_duration_ns(unsigned int mu);

bool nfapi_pnf_pacer_next(nfapi_pnf_pacer_t *pacer,
                          unsigned int mu,
                          struct timespec now,
                          struct timespec *deadline);

#endif /* NFAPI_PNF_PACING_H */
