/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NR_RLC_UE_MANAGER_H_
#define _NR_RLC_UE_MANAGER_H_
#include "common/platform_types.h"
#include "nr_rlc_entity.h"
#include "common/platform_constants.h"
#include "common/ngran_types.h"
#include "common/openairinterface5g_limits.h"
#include <pthread.h>

typedef void nr_rlc_ue_manager_t;

typedef enum {
  NR_RLC_OP_MODE_SPLIT_GNB,
  NR_RLC_OP_MODE_MONO_GNB,
  NR_RLC_OP_MODE_UE,
} nr_rlc_op_mode_t;

typedef void (*rlf_handler_t)(int rnti);

/* connected UEs plus UEs in random access */
#define NR_RLC_MAX_UES (MAX_MOBILES_PER_GNB + 32)

/* One lock per UE: the same entity is written by the DL SDU thread, the
 * scheduler and L1_rx.  The contexts are never freed, only reused, so a lock
 * stays valid for a thread that looked the UE up just before its removal. */
typedef struct nr_rlc_ue_t {
  pthread_mutex_t lock;
  bool in_use;
  int ue_id;
  nr_rlc_entity_t *srb0;
  nr_rlc_entity_t *srb[3];
  nr_rlc_entity_t *drb[MAX_DRBS_PER_UE];
  nr_lcid_rb_t lcid2rb[32];
  rlf_handler_t rlf_handler;
} __attribute__((aligned(64))) nr_rlc_ue_t;

/***********************************************************************/
/* manager functions                                                   */
/***********************************************************************/

nr_rlc_ue_manager_t *new_nr_rlc_ue_manager(nr_rlc_op_mode_t mode);

bool nr_rlc_manager_rlc_is_split(nr_rlc_ue_manager_t *_m);
int nr_rlc_manager_get_gnb_flag(nr_rlc_ue_manager_t *m);

/* both return the UE with its lock held, to release with nr_rlc_manager_unlock_ue() */
nr_rlc_ue_t *nr_rlc_manager_find_ue(nr_rlc_ue_manager_t *m, int ue_id); // NULL if the UE does not exist
nr_rlc_ue_t *nr_rlc_manager_get_ue(nr_rlc_ue_manager_t *m, int ue_id); // creates the UE if needed
void nr_rlc_manager_unlock_ue(nr_rlc_ue_t *ue);

void nr_rlc_manager_remove_ue(nr_rlc_ue_manager_t *m, int ue_id);
/* returns the UE with its lock held, NULL if from_id does not exist or to_id already does */
nr_rlc_ue_t *nr_rlc_manager_update_ue_id(nr_rlc_ue_manager_t *m, int from_id, int to_id);

/***********************************************************************/
/* ue functions                                                        */
/***********************************************************************/

void nr_rlc_ue_add_srb_rlc_entity(nr_rlc_ue_t *ue, int srb_id, nr_rlc_entity_t *entity);
void nr_rlc_ue_add_drb_rlc_entity(nr_rlc_ue_t *ue, int drb_id, nr_rlc_entity_t *entity);

#endif /* _NR_RLC_UE_MANAGER_H_ */
