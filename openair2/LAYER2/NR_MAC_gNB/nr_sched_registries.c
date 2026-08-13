/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include "nr_sched_registries.h"

#define DEFINE_NR_SCHED_POLICY_REGISTRY(registry, fn_type) SCHED_REGISTRY_DEFINE(registry, fn_type);
NR_SCHED_POLICY_REGISTRIES(DEFINE_NR_SCHED_POLICY_REGISTRY)
#undef DEFINE_NR_SCHED_POLICY_REGISTRY

SCHED_REGISTRY_DEFINE(dl_harq_result_observer, nr_dl_harq_result_observer_fn);
SCHED_REGISTRY_DEFINE(ul_harq_result_observer, nr_ul_harq_result_observer_fn);

void nr_notify_dl_harq_result(gNB_MAC_INST *mac, const nr_harq_result_t *result)
{
  for (int i = 0; i < mac->num_dl_harq_result_observers; ++i)
    mac->dl_harq_result_observers[i](mac, result, mac->sched_stateful_data);
}

void nr_notify_ul_harq_result(gNB_MAC_INST *mac, const nr_harq_result_t *result)
{
  for (int i = 0; i < mac->num_ul_harq_result_observers; ++i)
    mac->ul_harq_result_observers[i](mac, result, mac->sched_stateful_data);
}
