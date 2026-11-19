/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "log.h"

bool log_error;
bool log_info;
bool log_debug;

void init_log(bool error, bool info, bool debug)
{
  log_error = error;
  log_info = info;
  log_debug = debug;
}
