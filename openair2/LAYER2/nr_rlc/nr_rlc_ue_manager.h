/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NR_RLC_UE_MANAGER_H_
#define _NR_RLC_UE_MANAGER_H_
#include "common/platform_types.h"
#include "nr_rlc_entity.h"
#include "common/platform_constants.h"
#include "common/ngran_types.h"

typedef void nr_rlc_ue_manager_t;

typedef enum {
  NR_RLC_OP_MODE_SPLIT_GNB,
  NR_RLC_OP_MODE_MONO_GNB,
  NR_RLC_OP_MODE_UE,
} nr_rlc_op_mode_t;

typedef void (*rlf_handler_t)(int rnti);

typedef struct nr_rlc_ue_t {
  int ue_id;
  void (*send_initial_srb0)(int ue_id, const uint8_t *sdu, int sdu_len);
  nr_rlc_entity_t *srb0;
  nr_rlc_entity_t *srb[3];
  nr_rlc_entity_t *drb[MAX_DRBS_PER_UE];
  nr_lcid_rb_t lcid2rb[32];
  rlf_handler_t rlf_handler;
} nr_rlc_ue_t;

/***********************************************************************/
/* manager functions                                                   */
/***********************************************************************/

nr_rlc_ue_manager_t *new_nr_rlc_ue_manager(nr_rlc_op_mode_t mode);

bool nr_rlc_manager_rlc_is_split(nr_rlc_ue_manager_t *_m);
int nr_rlc_manager_get_gnb_flag(nr_rlc_ue_manager_t *m);

void nr_rlc_manager_lock(nr_rlc_ue_manager_t *m);
void nr_rlc_manager_unlock(nr_rlc_ue_manager_t *m);

nr_rlc_ue_t *nr_rlc_manager_get_ue(nr_rlc_ue_manager_t *m, int ue_id);
void nr_rlc_manager_remove_ue(nr_rlc_ue_manager_t *m, int ue_id);

/***********************************************************************/
/* ue functions                                                        */
/***********************************************************************/

void nr_rlc_ue_add_srb_rlc_entity(nr_rlc_ue_t *ue, int srb_id, nr_rlc_entity_t *entity);
void nr_rlc_ue_add_drb_rlc_entity(nr_rlc_ue_t *ue, int drb_id, nr_rlc_entity_t *entity);

#endif /* _NR_RLC_UE_MANAGER_H_ */
