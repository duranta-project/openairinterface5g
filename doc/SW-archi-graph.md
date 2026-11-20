<!-- SPDX-License-Identifier: CC-BY-4.0 -->

This document is a high-level overview over the L1 threading mechanism.

The slot pace is given by a fronthaul thread, which depends on the split:

- split 8 / IF5 (local RF, rfsimulator, eCPRI IF5): `ru_thread()` in
  `executables/nr-ru.c`, driven by time-domain RX samples;
- split 7.2 (`REMOTE_IF4p5`, O-RAN fronthaul through xran): `split7_thread()`
  in the `fhi_72` library (`radio/fhi_72/oran_isolate.c`), driven by xran slot
  callbacks.

Both feed the same L1 TX/RX pipeline (`L1_tx_thread()`, `L1_rx_thread()`) and
use the same queues (`L1_tx_out`, `resp_L1`, `L1_rx_out`, `prach_ru_queue`,
`prach_l1rx_queue`).

## Fronthaul setup

```mermaid
flowchart TB
    init_NR_RU --> split{"ru->if_south"}
    split -- "LOCAL_RF / REMOTE_IF5" --> start_split8
    split -- REMOTE_IF4p5 --> start_split72

    subgraph split8 ["split 8 (nr-ru.c)"]
        start_split8 --> load8["load device, nr_start_if()"] --> spawn8[["threadCreate(ru_thread)"]]
    end

    subgraph split72 ["split 7.2 (nr-ru.c -> fhi_72 library)"]
        start_split72 --> alloc["calloc gNB->fhi72_transport"] --> fill_split7_2_config
        fill_split7_2_config --> load72["load_transport_shlib(oran_fhi72_init)"] --> oran_fhi72_init
        oran_fhi72_init --> oran_fhi72_configure["oran_fhi72_configure(): xran init/config"]
        oran_fhi72_configure --> cbs["fhi72_transport_t callbacks:
          prec_bf = oran_prec_bf
          cp = oran_cp
          dl_up = oran_dl_up"]
        cbs --> spawn72[["threadCreate(split7_thread)"]]
    end
```

The split 7.2 interface is defined in `radio/COMMON/gnb_fhi72_if.h`:

- `split7_config_t` holds the cell/RU parameters needed by xran (bandwidth,
  frequencies, numerology, slot directions, TDD pattern, PRACH, `sl_ahead`),
  pointers to the RU frequency-domain buffers (`rxdataF`, `txdataF_BF`) and the
  RU thread pool used for per-antenna U-plane (de)compression. It is filled by
  `fill_split7_2_config()`.
- `fhi72_transport_t` holds `split7_config_t`, the handle of `split7_thread()`
  and the TX callbacks `prec_bf()`, `cp()` and `dl_up()` called from
  `tx_func()`. `PHY_VARS_gNB` only holds a pointer to it
  (`gNB->fhi72_transport`, `NULL` for split 8).
- `oran_fhi72_init()` is the library entry point (`fhi72_init_t`), loaded with
  `load_transport_shlib()`.

## Fronthaul thread, split 8: `ru_thread()`

```mermaid
flowchart TB
    ru_thread --> RFin["block fh_south_in (rx_rf / fh_if5_south_in)"] --> UL{is UL slot?}
    UL -- yes --> wait_free_rx_tti --> ru_rx_slot
    UL -- no  --> ru_push_tx_job
    subgraph ru_rx_slot
        feprx["feprx: fep_rx (DFT)"] --> rx_nr_prach_ru["rx_nr_prach_ru (per prach_ru_queue item)"] --> msg:prach_l1rx_queue
    end
    ru_rx_slot --> ru_push_tx_job["ru_push_tx_job: msg:L1_tx_out"]
    ru_push_tx_job -- async launch --> L1_tx_thread
    ru_push_tx_job --> RFin
```

The main thread is `ru_thread()`. It blocks on reception of radio samples
(time domain). In the case of an UL slot, it waits that no more than N UL
jobs are scheduled (via `wait_free_rx_tti()`, which waits on queue
`L1_rx_out`, cf. the RX L1 processing further below). Then, in
`ru_rx_slot()`, it performs RX front-end processing (RX FEP -> `fep_rx()`,
i.e. DFT) to reach a frequency domain representation of the RX signal, and
extracts PRACH (`rx_nr_prach_ru()`) for each PRACH occasion of this slot taken
from `prach_ru_queue`, handing it to L1 through `prach_l1rx_queue`.

Afterwards, it triggers TX processing by pushing a message into the FIFO queue
`L1_tx_out` (`ru_push_tx_job()`), which asynchronously starts a TX job in
`L1_tx_thread()` (see below). After that, it blocks again on reception on the
radio.

## Fronthaul thread, split 7.2: `split7_thread()`

```mermaid
flowchart TB
    subgraph xran ["xran (timing/worker lcores)"]
        rx_cb["oai_xran_fh_rx_callback (per port, full slot)"] --> slot_ready{"slot complete on all xran ports?"}
        slot_ready -- yes --> msg:slot_fifo
        slot_ready -- no --> rx_cb
    end

    split7_thread --> trx_oran_start["trx_oran_start: xran_start()"] --> wait_sync --> wrap["drop slots until SFN wrap (tti 0)"]
    wrap --> S_in["block oai_xran_wait_slot (slot_fifo)
      skip to newest slot if backlog >= MAX_SLOT_BACKLOG"]
    msg:slot_fifo -.-> S_in
    S_in --> UL{is UL/mixed slot?}
    UL -- yes --> wait_free_rx_tti --> process_ul_data
    UL -- no --> ru_push_tx_job

    subgraph process_ul_data
        prach_q["get_next_nr_prach (prach_ru_queue)"] --> ul_jobs[["ul_up_task per (xran port, antenna) on RU thread pool:
          xran_fh_rx_read_slot_ant -> rxdataF
          xran_fh_rx_prach_read_slot_ant -> PRACH buffer"]]
        ul_jobs --> join_task_ans --> msg:prach_l1rx_queue
    end

    process_ul_data --> ru_push_tx_job["ru_push_tx_job: msg:L1_tx_out (slot + sl_ahead)"]
    ru_push_tx_job -- async launch --> L1_tx_thread
    ru_push_tx_job --> S_in
    S_in -- oai_exit / L1_rx_out aborted --> stop72["trx_oran_stop, trx_oran_end"]
```

In split 7.2, there is no `ru_thread()`. xran receives the U-plane in its own
lcores and calls `oai_xran_fh_rx_callback()` per xran port once the UL
deadline of a slot passed. `slot_ready()` pushes the slot into `slot_fifo` once
all xran ports completed it (ports may have different UL deadlines).

`split7_thread()` starts xran, waits for the SFN wrap (xran accepts the
U-plane and sends the C-plane only from frame 0, slot 0 on), then loops:

- block on `oai_xran_wait_slot()`; if it falls behind by
  `MAX_SLOT_BACKLOG` slots or more, it skips to the newest slot;
- in an UL (or mixed) slot, wait for a free `rxdataF` slot with
  `wait_free_rx_tti()` (same as `ru_thread()`), then `process_ul_data()`
  reads PUSCH and PRACH from the xran buffers with one task per (xran port,
  antenna) on the RU thread pool, decompressing the IQ into the
  `RU_RX_SLOT_DEPTH` slot position of `rxdataF` used by L1, and hands PRACH to
  L1 through `prach_l1rx_queue`. No DFT is needed, as 7.2 is a
  frequency-domain interface;
- push the TX job for slot + `sl_ahead` into `L1_tx_out` (`ru_push_tx_job()`,
  without timestamp).

On exit, it stops xran. It is joined in `term_gNB_Tpool()` after `L1_rx_out`
is aborted, as it may block in `wait_free_rx_tti()`.

## L1 TX: `L1_tx_thread()`

```mermaid
flowchart TB
    L1_tx_thread --> TX_in[block L1_tx_out] --> NR_slot_indication

    subgraph tx_func

        NR_slot_indication --> msg:resp_L1 --> DL{is DL/mixed slot?}
        DL -- yes --> phy_procedures_gNB_TX --> split{"gNB->fhi72_transport?"}
        DL -- no --> split
        NR_slot_indication["run the scheduler:
          - monolithic: run_scheduler_monolithic()
          - nFAPI: send indication via pnf_send_slot_ind()"]
        msg:resp_L1 -- async launch --> L1_RX_thread

        split -- "no: split 8" --> tx_split8
        split -- "yes: split 7.2" --> tx_split72

        subgraph tx_split8
            feptx_ofdm["DL: ru->feptx_ofdm (precoding + IDFT)"] --> fh_south_ctrl["analog BF: ru->fh_south_ctrl"] --> fh_south_out["DL, continuous TX or rfsim: ru->fh_south_out"]
        end

        subgraph tx_split72
            prec_bf["DL: t->prec_bf (oran_prec_bf: precoding/BF to txdataF_BF)"] --> cp["analog BF: t->cp (oran_cp: UL/DL C-plane per xran port)"] --> dl_up[["DL: t->dl_up (oran_dl_up: compress DL U-plane per (xran port, antenna) on RU thread pool)"]]
        end
    end
    tx_split8 --> TX_in
    tx_split72 --> TX_in
```

The `L1_tx_thread()` processes individual TX jobs sequentially, by waiting for
new messages on queue `L1_tx_out`, signalling individual TX jobs. For each
message, it calls `tx_func()` which does in order:

- run the scheduler through `NR_slot_indication`, which corresponds to a
  "Slot.indication" in FAPI parlance. This runs the scheduler, and schedules a
  given slot (either downlink, uplink, or both).
- trigger RX processing by pushing a message into the FIFO queue `resp_L1`,
  asynchronously starting an RX job in `L1_rx_thread()` (see below).
- process the current L1 TX job through `phy_procedures_gNB_TX()` in DL or
  mixed slots
- hand the slot to the fronthaul, chosen once by `gNB->fhi72_transport`:
  - split 8, `tx_split8()`: precoding and IDFT (`feptx_ofdm()`), beam control
    (`fh_south_ctrl()`) and write to the radio board (`fh_south_out()`), the
    latter also in UL slots with continuous TX or rfsimulator;
  - split 7.2, `tx_split72()`: precoding/beamforming into `txdataF_BF`
    (`prec_bf()`), C-plane for analog beamforming (`cp()`, both UL and DL
    sections as per the slot direction) and DL U-plane (`dl_up()`, only in
    DL/mixed slots). xran sends the packets on air time; there is no
    timestamp.

After these steps, `tx_func()` return to `L1_tx_thread()`, which will wait for
the next TX job.

## L1 RX: `L1_rx_thread()`

```mermaid
flowchart TB
    L1_rx_thread --> RX_in[block resp_L1] --> L1_nr_prach_procedures

    subgraph rx_func

        L1_nr_prach_procedures["L1_nr_prach_procedures (per prach_l1rx_queue item)"] --> phase_comp{apply phase comp.?}
        phase_comp -- yes --> apply_nr_rotation --> phy_procedures_gNB_uespec_RX
        phase_comp -- no --> phy_procedures_gNB_uespec_RX
        phy_procedures_gNB_uespec_RX --> NR_ul_indication --> msg:L1_rx_out
        NR_ul_indication["run the scheduler: NR_UL_indication()"]
        msg:L1_rx_out -- async signal free --> FH["ru_thread (split 8) / split7_thread (split 7.2)"]
    end
    msg:L1_rx_out --> RX_in
```

The `L1_rx_thread()` processes individual RX jobs sequentially. It waits for a
new RX job through the queue `resp_L1`, and then calls `rx_func()`, which does
in order:

- run PRACH processing via `L1_nr_prach_procedures()` for each PRACH occasion
  in `prach_l1rx_queue`, filled by `ru_rx_slot()` (split 8) or
  `process_ul_data()` (split 7.2)
- optionally apply rotation to the RX signal if phase compensation is to be
  applied
- run the current L1 RX job through (`phy_procedures_gNB_uespec_RX()`), which
  notably includes PUCCH, PUSCH, SRS processing
- call the scheduler through `NR_ul_indication()`, which corresponds to FAPI
  uplink messages (e.g., `RX_data.indication`, `CRC.indication`,
  `UCI.indication` etc.)
- signal completion via FIFO queue `L1_rx_out()`, which tells the fronthaul
  thread (`ru_thread()` or `split7_thread()`) that RX processing finished and
  the `rxdataF` slot can be overwritten.

The signalling of scheduler data is done through a variable `UL_INFO`, which is
filled by `L1_nr_prach_procedures()` (for PRACH) and `phy_procedures_gNB_uespec_RX()`
(for PUCCH, PUSCH, SRS).

After these steps, `rx_func()` returns to `L1_rx_thread()`, which will wait the
next RX job.

Note that while individual TX (RX) jobs are run sequentially through
`L1_tx_thread()` (`L1_rx_thread()`), both TX and RX processing run in
parallel.
