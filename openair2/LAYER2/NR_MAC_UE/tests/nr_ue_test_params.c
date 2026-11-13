/* SPDX-License-Identifier: LicenseRef-CSSL-1.0 */

#include "executables/nr-uesoftmodem.h"

nrUE_params_t *get_nrUE_params(void)
{
  static nrUE_params_t params;
  return &params;
}
