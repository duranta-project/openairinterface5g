/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_pdcp_asn1_utils.h"
#include "common/utils/LOG/log.h"
#include "nr_pdcp_entity.h"
#include "nr_pdcp_configuration.h"

#define ENCODE_DECODE(a, b)                          \
int decode_##a(int v)                                \
{                                                    \
  static const int tab[] = { VALUES_NR_PDCP_##b };   \
  AssertFatal(v >= 0 && v < SIZEOF_NR_PDCP_##b,      \
              "bad encoded value " #a " %d\n", v);   \
  return tab[v];                                     \
}                                                    \
                                                     \
int encode_##a(int v)                                \
{                                                    \
  static const int tab[] = { VALUES_NR_PDCP_##b };   \
  for (int ret = 0; ret < SIZEOF_NR_PDCP_##b; ret++) \
    if (tab[ret] == v)                               \
      return ret;                                    \
  AssertFatal(0, "bad " #a " value %d\n", v);        \
}

ENCODE_DECODE(t_reordering, T_REORDERING)
ENCODE_DECODE(sn_size_ul, SN_SIZE)
ENCODE_DECODE(sn_size_dl, SN_SIZE)
ENCODE_DECODE(discard_timer, DISCARD_TIMER)
ENCODE_DECODE(discard_timer_sl, DISCARD_TIMER_SL)
