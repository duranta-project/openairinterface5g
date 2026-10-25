<!-- SPDX-License-Identifier: CC-BY-4.0 -->

This document describes the driver for the xran driver, implementing O-RAN FHI
7.2 interface. This is for developers; for user documentation, consider the
[O-RAN 7.2 tutorial instead](../../doc/ORAN_FHI7.2_Tutorial.md). 

The main source files and their purpose are as follows:

- `oaioran.c`: definition of xran-specific callback functions.
- `oran-config.c`: reading of 7.2-specific configuration and preparing xran
  configration.
- `oran-init.c`: initialization of xran.
- `oran_isolate.c`: main entry point and definition of function pointers for OAI
  callbacks.

The entry point for the driver library is `oran_fhi72_init()` in `oran_isolate.c`.
This function fills the `fhi72_transport_t` callbacks, prepares xran
configuration through `get_xran_config()` and spawns the FH thread
`split7_thread()`. `get_xran_config()` reads 7.2 specific configuration and
deduces some of the parameters (e.g., frequency) from OAI-provided split 7.2
configuration in `split7_config_t`.

Actual xran initialization happens in `oai_oran_initialize()` in `oran-init.c`,
called from `split7_thread()`.
It calls various helper function in the same file to set up DPDK memory buffer
(pools). Notably, these callbacks are installed:

- `oai_xran_fh_rx_callback()` through `xran_5g_fronthault_config()`: callback
  for PUSCH data; at the full-slot UL deadline, clears the packet counters of
  the previous slot and signals `split7_thread()` once all xran ports
  completed the slot (PUSCH and PRACH data of that slot are then complete).
- `oai_xran_fh_rx_prach_callback()` through `xran_5g_prach_req()`: callback for
  PRACH data; clears the PRACH packet counters of the previous slot.
- `oai_physide_dl_tti_call_back()` through `xran_timingsource_reg_tticb()`:
  empty, but must be registered: xran only enables U-plane RX and C-plane from
  the first SFN wrap on if a TTI callback exists.

More detailed information about the xran callbacks can be taken from the xran
documentation.

During normal operation, `split7_thread()` reads PUSCH and PRACH through
`process_ul_data()`, and OAI calls into the driver through `fhi72_transport_t`:
`oran_prec_bf()` for precoding, `oran_dl_up()` for PDSCH
and `oran_cp()` for CP UL/DL.

For PDSCH, `oran_dl_up()` calls `xran_fh_tx_send_slot()` that
optionally compresses IQ data, then writes it into IQ buffers of xran.

For PUSCH/PRACH, `split7_thread()` waits in `oai_xran_wait_slot()` until
`oai_xran_fh_rx_callback()` signals a complete slot, then `process_ul_data()`
calls `xran_fh_rx_read_slot()`/`xran_fh_rx_prach_read_slot()` for that slot. If
processing falls more than a few slots behind, it skips to the newest slot.
