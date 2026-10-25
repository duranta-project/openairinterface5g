/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef CREATE_MPLANE_YANG_CONFIG_H
#define CREATE_MPLANE_YANG_CONFIG_H

#include "../ru-mplane-api.h"
#include "radio/COMMON/common_lib.h"

#include <libyang/libyang.h>

bool configure_ru_from_yang(const ru_session_t *ru_session, const split7_config_t *s7cfg, const size_t num_rus, char **result);

char *get_pm_content(const ru_session_t *ru_session, const char *active);

#endif /* CREATE_MPLANE_YANG_CONFIG_H */
