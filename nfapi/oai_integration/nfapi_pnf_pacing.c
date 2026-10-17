/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include "nfapi_pnf_pacing.h"

#include <stddef.h>

long nfapi_pnf_slot_duration_ns(unsigned int mu)
{
  return mu <= 4 ? 1000000L >> mu : 0;
}

bool nfapi_pnf_pacer_next(nfapi_pnf_pacer_t *pacer,
                          unsigned int mu,
                          struct timespec now,
                          struct timespec *deadline)
{
  const long slot_duration_ns = nfapi_pnf_slot_duration_ns(mu);
  if (pacer == NULL || deadline == NULL || slot_duration_ns == 0)
    return false;

  if (!pacer->initialized) {
    pacer->next_slot = now;
    pacer->initialized = true;
    return false;
  }

  pacer->next_slot.tv_nsec += slot_duration_ns;
  if (pacer->next_slot.tv_nsec >= 1000000000L) {
    pacer->next_slot.tv_sec++;
    pacer->next_slot.tv_nsec -= 1000000000L;
  }

  if (now.tv_sec > pacer->next_slot.tv_sec
      || (now.tv_sec == pacer->next_slot.tv_sec && now.tv_nsec >= pacer->next_slot.tv_nsec))
    return false;

  *deadline = pacer->next_slot;
  return true;
}
