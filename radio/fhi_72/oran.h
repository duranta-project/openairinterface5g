/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _ORAN_H_
#define _ORAN_H_

#include "common_lib.h"

/* Purpose-built contract for the O-RAN 7.2 fronthaul, deliberately NOT openair0_device_t:
 * split 7.2 never does IQ-sample read/write, beamforming, or third-party RRU control, so none of
 * openair0_device_t's fields apply. This is the whole surface nr-ru.c needs.
 *
 * The FH thread itself (split7_thread(), in oran_isolate.c) lives inside the shared lib - it owns
 * xran startup and fh_south_in there directly, so neither needs a slot here. Only fh_south_out
 * still crosses into nr-ru.c's ru_tx_func(), which stays shared across every southbound split. */
typedef struct fhi72_transport_s {
  void *priv; // opaque per-instance state, owned by the fhi_72 shared lib
  int (*stop)(struct fhi72_transport_s *t);
  void (*end)(struct fhi72_transport_s *t);
  int (*get_stats)(struct fhi72_transport_s *t);
  // No RU antenna counts either: oran_fh_if4p5_south_out() derives them per xran port itself.
  void (*fh_south_out)(int32_t **txdataF_BF, uint16_t **beam_id, int frame, int slot);
} fhi72_transport_t;

/* Symbol exported (visibility default) by the O-RAN 7.2 FH shared library (radio/fhi_72), looked
 * up dynamically via load_transport_shlib() from executables/nr-ru.c and never called directly -
 * so linking nr-softmodem does NOT require xran/DPDK unless split 7.2 is actually configured.
 * Takes RU_t directly (rather than handing back a start function) because it spawns and owns the
 * FH thread itself - see split7_thread() in oran_isolate.c. */
typedef int (*fhi72_init_t)(openair0_config_t *cfg, RU_t *ru, fhi72_transport_t *transport);

#endif /* _ORAN_H_ */
