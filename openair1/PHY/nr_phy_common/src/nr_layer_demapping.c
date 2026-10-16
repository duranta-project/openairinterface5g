/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#include "nr_layer_demapping.h"
#include <string.h>
#include "common/utils/assertions.h"

/* Constant MO lets the compiler inline memcpy as fixed-size vector stores */
static inline void layer_demapping(uint8_t Nl, int nb_re, int16_t **llr_layers, int16_t *llr_out, int MO)
{
  int k = 0;
  for (int j = 0; j < nb_re; j++) {
    for (int l = 0; l < Nl; l++) {
      memcpy(llr_out + k, llr_layers[l] + j * MO, MO * sizeof(int16_t));
      k += MO;
    }
  }
}

void nr_layer_demapping(uint8_t Nl, uint8_t mod_order, int nb_re, int16_t **llr_layers, int16_t *llr_out)
{
  AssertFatal(Nl >= 1 && Nl <= 4, "Unsupported number of layers %d\n", Nl);

  if (Nl == 1) {
    /* Single layer: one contiguous memcpy, no interleaving needed. */
    memcpy(llr_out, llr_layers[0], nb_re * mod_order * sizeof(int16_t));
    return;
  }

  /* Switch on mod_order so MO is a compile-time literal, enabling the compiler
   * to inline memcpy as fixed-size vector stores. */
  switch (mod_order) {
    case 2:
      layer_demapping(Nl, nb_re, llr_layers, llr_out, 2);
      break;
    case 4:
      layer_demapping(Nl, nb_re, llr_layers, llr_out, 4);
      break;
    case 6:
      layer_demapping(Nl, nb_re, llr_layers, llr_out, 6);
      break;
    case 8:
      layer_demapping(Nl, nb_re, llr_layers, llr_out, 8);
      break;
    case 10:
      layer_demapping(Nl, nb_re, llr_layers, llr_out, 10);
      break;
    default:
      AssertFatal(0, "Unknown mod_order %d\n", mod_order);
  }
}
