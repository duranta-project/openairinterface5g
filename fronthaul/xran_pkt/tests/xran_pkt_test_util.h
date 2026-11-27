/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

// Shared by the xran_pkt unit tests

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define FILL 0xA5 // written to outputs and buffers before a call, to see what the call writes

#define UNSET_LEN ((size_t)0xDEAD) // written to an encoder's len_out before a call

static inline bool is_fill(const void *p, size_t n)
{
  const uint8_t *b = p;
  for (size_t i = 0; i < n; i++)
    if (b[i] != FILL)
      return false;
  return true;
}

// Sets the outputs of an encoder before a call
static inline void reset_encode_out(uint8_t *buf, size_t n, size_t *len_out)
{
  memset(buf, FILL, n);
  *len_out = UNSET_LEN;
}

// True if an encoder did not write its outputs, which reset_encode_out() set
static inline bool encode_out_untouched(const uint8_t *buf, size_t n, size_t len_out)
{
  return is_fill(buf, n) && len_out == UNSET_LEN;
}
