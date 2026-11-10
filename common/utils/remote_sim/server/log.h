/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <stdbool.h>

extern bool log_error;
extern bool log_info;
extern bool log_debug;

#define error(...) do { if (log_error) printf(__VA_ARGS__); } while (0)
#define info(...) do { if (log_info) printf(__VA_ARGS__); } while (0)
#define debug(...) do { if (log_debug) printf(__VA_ARGS__); } while (0)

void init_log(bool error, bool info, bool debug);

#endif /* _LOG_H_ */
