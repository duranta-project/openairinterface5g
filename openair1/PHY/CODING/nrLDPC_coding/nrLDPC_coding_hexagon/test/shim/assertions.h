/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */
/* Stand-in for common/utils/assertions.h, whose include chain (pthread, malloc.h)
 * does not exist in the standalone Hexagon runtime. */
#ifndef LDPC_SIMTEST_ASSERTIONS_H
#define LDPC_SIMTEST_ASSERTIONS_H
#include <stdio.h>
#include <stdlib.h>
#define AssertFatal(cond, ...)       \
  do {                               \
    if (!(cond)) {                   \
      printf("FATAL: " __VA_ARGS__); \
      abort();                       \
    }                                \
  } while (0)
#endif
