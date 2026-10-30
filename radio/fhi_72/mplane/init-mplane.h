/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef OAI_MPLANE_H
#define OAI_MPLANE_H

#include "ru-mplane-api.h"
#include "radio/fhi_72/oran.h"

bool init_mplane(ru_session_list_t *ru_session_list);

bool manage_ru(ru_session_t *ru_session, const split7_config_t *s7cfg, const size_t num_rus);

bool pm_conf(ru_session_t *ru_session, const char *active);

#endif /* OAI_MPLANE_H */
