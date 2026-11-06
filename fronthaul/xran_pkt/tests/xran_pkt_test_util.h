/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Shared by the xran_pkt unit tests. Include it in one file of each test executable: it defines exit_function().

#pragma once

#include <stdio.h>
#include <stdlib.h>

// The AssertError() handler for tests that link the codecs without the OAI utilities library
void exit_function(const char *file, const char *function, const int line, const char *s, const int assertflag)
{
  fprintf(stderr, "Error at %s:%s:%d - %s\n", file, function, line, s ? s : "None");
  exit(1);
}
