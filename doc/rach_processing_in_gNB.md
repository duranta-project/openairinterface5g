<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# The gNB RACH Processing

## 1. Collect I/Q Samples

The local RU main loop calls `process_nr_prach_ru()` for scheduled PRACH requests.
It copies the required I/Q samples and submits preprocessing to the existing RU
thread pool. The result stays in submission order in `prach_l1rx_queue`; L1 can
consume it after the worker marks it complete.

## 2. Process the Samples

After a slot notification, `L1_rx_thread()` calls `rx_func()`, which drains ready
PRACH results even in downlink slots. `L1_nr_prach_procedures()` detects one time
occasion with `rx_nr_prach()` and updates the noise estimate in occasion order.
`rx_func()` batches detections with the same reception frame/slot, delivers a
batch before exceeding FAPI capacity, and frees each request after all its
occasions. The RACH-only `NR_rach_indication()` callback invokes the existing
`handle_nr_rach() => nr_initiate_ra_proc()` path (or forwards the batch in PNF mode).

`nr_initiate_ra_proc()` creates a UE context if it needs, then sets `UE.ra.ra_state` to what it should do.

## 3. Send the RACH DL

`tx_func()` that is launched by `L1_tx_thread()` as it gets `L1_tx_out` msg (from main loop in [1.](#1-collect-iq-samples))
`tx_func()=>run_scheduler_monolithic()=>gNB_dlsch_ulsch_scheduler()=>nr_scheduler_RA()`
`nr_scheduler_RA()` process all existing UE contexts, to see it it has to schedule a RACH DL.

## 4. Global Synchronisation

`resp_L1` msg for ([2.](#2-process-the-samples)) is created in `tx_func()` that is launched by `L1_tx_thread()` (see [3.](#3-send-the-rach-dl)) when it receives slot indication as it gets `L1_tx_out` msg
this msg is sent by the main loop (see [1.](#1-collect-iq-samples))
`tx_func()=>run_scheduler_monolithic()=>nr_schedule_response ()=>nr_schedule_ul_tti_req ()=> nr_schedule_rx_prach()`
writes in the triggering structure to ([1.](#1-collect-iq-samples)) that it will have to process PRACH in a given future slot.
