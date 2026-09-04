/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __NR_PHY_MEAS__H__
#define __NR_PHY_MEAS__H__

#include "time_meas.h"
#include "PHY/impl_defs_nr.h"

#define START_MEAS_FULL_SLOT(ts, current, type) if (current == type) { start_meas(ts); }
#define STOP_MEAS_FULL_SLOT(ts, current, type) if (current == type) { stop_meas(ts); }
#define MERGE_MEAS_FULL_SLOT(dst, src, current, type) if (current == type) { merge_meas(dst, src); }

#endif /* __NR_PHY_MEAS__H__ */
