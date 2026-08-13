/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include "nr_sched_registries.h"

#define DEFINE_NR_SCHED_POLICY_REGISTRY(registry, fn_type) SCHED_REGISTRY_DEFINE(registry, fn_type);
NR_SCHED_POLICY_REGISTRIES(DEFINE_NR_SCHED_POLICY_REGISTRY)
#undef DEFINE_NR_SCHED_POLICY_REGISTRY
