/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 *
 * OAI-local NS slice IE definitions for ran_func_slice.c.
 *
 * Keeps openairinterface5g buildable when the flexric submodule stays on
 * upstream ef6d722f2. Runtime wire format still requires libslice_sm.so built
 * from flexric branch nws-3gpp-impl (see nws/build_scripts/build_oai_flexric_quick.sh).
 *
 * Layout mirrors flexric nws-3gpp-impl src/sm/slice_sm/ie/slice_data_ie.h.
 */

#ifndef OAI_NS_SLICE_IE_H
#define OAI_NS_SLICE_IE_H

#include <stddef.h>
#include <stdint.h>

#include "openair2/E2AP/flexric/src/sm/slice_sm/ie/slice_data_ie.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NS_SLICE_DEFAULT_SD 0xffffffu

#ifndef SLICE_CTRL_SM_V0_NS_SET_POLICY
/* Upstream flexric ends the enum here; NS_SET_POLICY uses the same value. */
#define SLICE_CTRL_SM_V0_NS_SET_POLICY SLICE_CTRL_SM_V0_END
#endif

typedef enum {
  NS_SLICE_DIR_DL = 0,
  NS_SLICE_DIR_UL = 1,
} ns_slice_dir_e;

typedef struct {
  uint8_t sst;
  uint32_t sd;
  ns_slice_dir_e direction;
  float dedicated_pct;
  float min_pct;
  float max_pct;
} ns_slice_policy_entry_t;

typedef struct {
  uint32_t len_entries;
  ns_slice_policy_entry_t *entries;
} ns_slice_policy_list_t;

/* Extended layouts (must match custom FlexRIC slice_data_ie.h). */
typedef struct {
  slice_conf_t slice_conf;
  ue_slice_conf_t ue_slice_conf;
  ns_slice_policy_list_t ns_policy;
  int64_t tstamp;
} oai_slice_ind_msg_layout_t;

typedef struct {
  slice_conf_t add_mod_slice;
  del_slice_conf_t del_slice;
  ue_slice_conf_t ue_slice;
  ns_slice_policy_list_t ns_set_policy;
} oai_slice_ctrl_msg_u_layout_t;

static inline oai_slice_ind_msg_layout_t *oai_slice_ind_msg(slice_ind_msg_t *msg)
{
  return (oai_slice_ind_msg_layout_t *)msg;
}

static inline const oai_slice_ind_msg_layout_t *oai_slice_ind_msg_const(const slice_ind_msg_t *msg)
{
  return (const oai_slice_ind_msg_layout_t *)msg;
}

static inline ns_slice_policy_list_t *oai_slice_ctrl_ns_policy(slice_ctrl_msg_t *msg)
{
  return &((oai_slice_ctrl_msg_u_layout_t *)&msg->u)->ns_set_policy;
}

static inline const ns_slice_policy_list_t *oai_slice_ctrl_ns_policy_const(const slice_ctrl_msg_t *msg)
{
  return &((const oai_slice_ctrl_msg_u_layout_t *)&msg->u)->ns_set_policy;
}

#ifdef __cplusplus
}
#endif

#endif /* OAI_NS_SLICE_IE_H */
