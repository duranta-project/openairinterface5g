/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef __CODING_UNITARY_DEFS__h__
#define __CODING_UNITARY_DEFS__h__
int oai_exit=0;
const int NB_UE_INST = 1;
#include "openair1/PHY/defs_UE.h"
PHY_VARS_UE ***PHY_vars_UE_g;
#include "common/ran_context.h"
RAN_CONTEXT_t RC;

int8_t quantize(double D, double x, uint8_t B) {
  double qxd;
  int16_t maxlev;
  qxd = floor(x / D);
  maxlev = 1 << (B - 1); //(char)(pow(2,B-1));

  if (qxd <= -maxlev)
    qxd = -maxlev;
  else if (qxd >= maxlev)
    qxd = maxlev - 1;

  return ((int8_t) qxd);
}


#endif

