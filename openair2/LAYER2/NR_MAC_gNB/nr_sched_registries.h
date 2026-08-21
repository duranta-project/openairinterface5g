/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#ifndef NR_SCHED_REGISTRIES_H
#define NR_SCHED_REGISTRIES_H

#include "common/utils/sched_registry.h"
#include "nr_mac_gNB.h"

#define NR_SCHED_POLICY_REGISTRIES(M)                  \
  M(dl_preprocessor_policy, nr_pp_impl_dl)             \
  M(dl_ri_pmi_select_policy, nr_dl_ri_pmi_select_fn)   \
  M(dl_tda_select_policy, nr_dl_tda_select_fn)         \
  M(dl_beam_select_policy, nr_dl_beam_select_fn)       \
  M(dl_mcs_select_policy, nr_dl_mcs_select_fn)         \
  M(dl_rb_alloc_policy, nr_dl_rb_alloc_fn)             \
  M(dl_lcid_alloc_policy, nr_dl_lcid_alloc_fn)         \
  M(ul_preprocessor_policy, nr_pp_impl_ul)             \
  M(ul_ri_tpmi_select_policy, nr_ul_ri_tpmi_select_fn) \
  M(ul_tda_select_policy, nr_ul_tda_select_fn)         \
  M(ul_beam_select_policy, nr_ul_beam_select_fn)       \
  M(ul_mcs_select_policy, nr_ul_mcs_select_fn)         \
  M(ul_rb_alloc_policy, nr_ul_rb_alloc_fn)

#define DECLARE_NR_SCHED_POLICY_REGISTRY(registry, fn_type) SCHED_REGISTRY_DECLARE(registry, fn_type);
NR_SCHED_POLICY_REGISTRIES(DECLARE_NR_SCHED_POLICY_REGISTRY)
#undef DECLARE_NR_SCHED_POLICY_REGISTRY

SCHED_REGISTRY_DECLARE(dl_harq_result_observer, nr_dl_harq_result_observer_fn);
SCHED_REGISTRY_DECLARE(ul_harq_result_observer, nr_ul_harq_result_observer_fn);

void nr_notify_dl_harq_result(gNB_MAC_INST *mac, const nr_harq_result_t *result);
void nr_notify_ul_harq_result(gNB_MAC_INST *mac, const nr_harq_result_t *result);

#endif /* NR_SCHED_REGISTRIES_H */
