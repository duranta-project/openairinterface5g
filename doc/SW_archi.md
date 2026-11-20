<!-- SPDX-License-Identifier: CC-BY-4.0 -->


## The main thread is in ru_thread()
The infinite loop:
### rx_rf()
  Collect radio signal samples from RF board  
    all SDR processing is triggered by I/Q sample reception and it's date (timestamp)  
    TX I/Q samples will have a date in the future, compared to RX timestamp  
    called for each 5G NR slot  
    it blocks until data is available  
    the internal time comes from the RF board sampling numbers  
    (each sample has a incremental number representing a very accurate timing)  
raw incoming data is in buffer called "rxdata"  
    We derivate frame number, slot number, ... from the RX timestamp

#### NR_UL_indication()
This block processes data already decoded and stored in structures behind UL_INFO


* handle_nr_rach()  
process data from RACH primary detection  
if the input is a UE RACH detection

* handle_nr_uci()  
handles uplink control information, i.e., for the moment HARQ feedback.

* handle_nr_ulsch()  
handles ulsch data prepared by nr_fill_indication()


#### phy_procedures_gNB_uespec_RX()
* nr_decode_pucch0()  
actual CCH channel decoding form rxdataF (rx data in frequency domain)  
populates UL_INFO.uci_ind, actual uci data is in gNB->pucch  

* nr_ulsch_procedures()

    * actual ULsch decoding

   * nr_ulsch_decoding()

   * nr_fill_indication()   


#### phy_procedures_gNB_TX()
* nr_common_signal_procedures()  
generate common signals

* nr_generate_dci()
generate DCI: the scheduling informtion for each UE in both DL and UL

* nr_generate_pdsch()  
generate DL shared channel (user data)


#### nr_feptx0
do the inverse DFT

#### tx_rf()
send radio signal samples to the RF board  
the samples numbers are the future time for these samples emission on-air


## Scheduler

The main scheduler function `gNB_dlsch_ulsch_scheduler()` is called from
`NR_slot_indication()` (see above), once per slot. It holds the MAC scheduler
lock for the whole slot. The scheduler uses internal maps of used RBs,
`vrb_map` and `vrb_map_UL` (per beam), so each specific channel scheduler can
see the already allocated RBs in each slot (the vrb_map of the slot is cleared
when it starts).

`nr_mac_update_timers()` updates the per-UE timers. RLC and PDCP are not run by
the scheduler; RLC timers run in `nr_rlc_ms_tick()`, see [RLC](#rlc).

In order, it calls:

* `schedule_nr_mib()`, `schedule_nr_sib1()`, `schedule_nr_other_sib()`: MIB,
  SIB1 and other SIBs
* `schedule_nr_prach()`: schedules the (fixed) PRACH region in advance
* `schedule_nr_pcch()`: paging
* `nr_csirs_scheduling()`, `nr_schedule_periodic_srs()`,
  `nr_csi_meas_reporting()`, `nr_sr_reporting()`, `nr_measgap_scheduling()`:
  periodic CSI-RS, SRS, CSI reports in PUCCH, SR and measurement gaps
* `nr_schedule_RA()`: for each ongoing RA process, schedules Msg2 via
  `nr_generate_Msg2()` (and the Msg3 PUSCH), Msg4 and their retransmissions
* `nr_schedule_ulsch()`: UL scheduling, see below
* `nr_schedule_ue_spec()`: DL scheduling, see below
* `nr_schedule_pucch()`: fills the PUCCH FAPI PDUs for this slot

`nr_schedule_ulsch()` and `nr_schedule_ue_spec()` are divided into a
"preprocessor" and a "postprocessor": the first makes the scheduling
decisions, the second fills the FAPI structures (DCI, PDSCH/PUSCH PDUs,
TX_data) to indicate to the PHY what it is supposed to do. The preprocessor is
exchangeable (`pre_processor_dl`/`pre_processor_ul`): `nr_preprocessor_phytest()`
and `nr_ul_preprocessor_phytest()` schedule one user for phytest,
`nr_dlsch_preprocessor()` and `nr_ulsch_preprocessor()` multiple users through
a pipeline of exchangeable policies, see
[scheduler architecture](./MAC/scheduler-architecture.md):

1. collect candidate UEs (retransmissions and UEs with data or SR/BSR; for DL,
   buffer status via `nr_mac_rlc_status_ind()`)
2. RI/PMI (DL) or RI/TPMI (UL) selection
3. beam selection (multiple beams only)
4. time-domain allocation selection
5. MCS selection from the BLER state
6. RB allocation policy per beam (proportional fair by default), which
   validates CCE (`find_pdcch_candidate()`) and, for DL, PUCCH for HARQ
   feedback (`nr_acknack_scheduling()`) before committing an allocation in
   the vrb_map
7. TBS computation and postprocessing of the scheduled UEs; the DL data is
   pulled from RLC with `nr_mac_rlc_multi_data_req()`

## RRC
RRC is a regular ITTI task with a loop on queue `TASK_RRC_GNB`. It receives
NGAP, F1AP, E1AP and XnAP messages and timers; it is not triggered per slot.

The RRC communicates with the MAC scheduler through F1AP messages (F1 setup,
UE context setup/modification/release, DL/UL RRC message transfer), even in
monolithic mode, where they are direct function calls instead of SCTP.

## RLC
RLC code is new implementation, not using OAI mechanisms: it is implemented
directly on pthreads, ignoring OAI common functions. It is a library, called
from the MAC scheduler, PDCP and the F1 interface threads, protected by a
mutex.

RLC data is isolated and encapsulated.
It is stored under a global var: `nr_rlc_ue_manager`, created by
`nr_rlc_module_init()`. RLC entities are per UE and do not share data between
UEs.

When adding a UE, external code has to call `nr_rlc_add_srb()` and/or
`nr_rlc_add_drb()`, to remove it: `nr_rlc_remove_ue()`. Entities are
reconfigured with `nr_rlc_reconfigure_entity()`, re-established with
`nr_rlc_reestablish_entity()` and released with `nr_rlc_release_entity()`.

`nr_rlc_ms_tick()` must be called every ms to manage the internal timers; the
softmodem registers it in the time manager (`time_manager_start()`).

`successful_delivery()` and `max_retx_reached()` are entity callbacks; the
latter triggers radio link failure through the handler set by
`nr_rlc_set_rlf_handler()`.

### RLC data flow

#### TX Flow

Incoming data to be transmitted is forwarded to RLC by PDCP via `nr_rlc_data_req()`.

At the transport layer, in downlink (DL) at the gNB and uplink (UL) at UE, the scheduler relies on knowing the quantity of data awaiting transmission, therefore is using the following MAC/RLC interface functions:

* `nr_mac_rlc_multi_data_req()` (gNB DLSCH) or `nr_mac_rlc_data_req()` to retrieve data bytes to be transmitted by RLC and to fill the MAC SDU
* `nr_mac_rlc_status_ind()` to request and set the number of bytes scheduled for transmission by the RLC

Subsequently, the scheduler issues commands to lower layers.

#### RX Flow

In the RX chain, in downlink (DL) at the UE and uplink (UL) at gNB, the transport layer pushes data into RLC through `nr_mac_rlc_data_ind()`. Following this, RLC forwards the data to PDCP by invoking `pdcp_data_ind()` via a complex internal callback mechanism (`deliver_sdu()`).

## PDCP

The PDCP implementation is secured by a general mutex, akin to the design of the RLC layer. This setup ensures that PDCP data remains isolated and encapsulated.

The PDCP layer is initialized by `nr_pdcp_layer_init()`.

To manage UE connections, `nr_pdcp_add_srbs()` is employed for adding UE SRBs in PDCP, while `nr_pdcp_remove_UE()` is used for their removal. Similarly, `nr_pdcp_add_drb()` adds UE DRB in PDCP, with `nr_pdcp_remove_UE()` handling their removal.

### PDCP Tx flow

On the Tx side (downlink in gNB), the entry functions `nr_pdcp_data_req_drb()` and `nr_pdcp_data_req_srb()` are called by the upper layer. The upper layer could be SDAP (from GTP, or from the `gnb_tun_read_thread` thread, `sdap_tun_read_thread()`, which reads directly from a TUN interface if the 3GPP core is not used). The PDCP internals for `nr_pdcp_data_req_srb()` and `nr_pdcp_data_req_drb()` are thread-safe. Within these functions, the PDCP manager protects access to the SDU processing function of PDCP (`process_sdu()` callback, corresponding to `nr_pdcp_entity_process_sdu()`) using mutex. The resulting PDU is queued (`enqueue_rlc_data_req()`) and pushed to RLC by `rlc_data_req_thread()` calling `nr_rlc_data_req()`.

### PDCP Rx flow

At the Rx side, `pdcp_data_ind()` serves as the entry point for receiving data from RLC. Within `pdcp_data_ind()`, the PDCP manager mutex protects access to the PDU receiving function of PDCP (`recv_pdu()` callback corresponding to `nr_pdcp_entity_recv_pdu()` for DRBs). Following this, the `deliver_sdu_drb()` function dispatches the received data to the SDAP sublayer.

### PDCP security

nr_pdcp_config_set_security(): sets the keys for AS security of a UE

## AM DRB traffic flow in OAI

A sequence diagram of the traffic flow across PDCP and RLC layers. By default, data traffic is directed towards AM DRBs.

This is the flow for downlink, involving MAC and upper layers:

```mermaid
 sequenceDiagram
    title Downlink AM DRB traffic flow
    box Purple gNB
    participant GG as GTP / TUN
    participant SG as SDAP
    participant PG as PDCP
    participant RG as RLC
    participant MG as MAC
    end
    GG->>SG: sdap_data_req
    note over SG: nr_sdap_tx_entity
    SG->>PG: nr_pdcp_data_req_drb
    note over PG: deliver_pdu_drb_gnb
    note over PG: nr_pdcp_entity_process_sdu
    note over PG: enqueue_rlc_data_req
    PG->>RG: nr_rlc_data_req via rlc_data_req_thread
    note over RG: nr_rlc_entity_am_recv_sdu
    MG-->>RG: nr_mac_rlc_multi_data_req
    note over RG: nr_rlc_entity_am_generate_pdu
    note over RG: generate_tx_pdu
    note over RG: serialize_sdu
    RG-->>MG: PDU to MAC
    MG-->MUE: UL TX / RX procedures
    box Blue UE
    participant MUE as MAC
    participant RUE as RLC
    participant PUE as PDCP
    participant SUE as SDAP
    participant GUE as GTP / TUN
    end
    MUE-->>RUE: SDU to RLC
    RUE->>PUE: pdcp_data_ind
    note over PUE: nr_pdcp_entity_recv_pdu
    note over PUE: deliver_sdu_drb
    PUE->>SUE: sdap_data_ind
    note over SUE: nr_sdap_rx_entity
    SUE->>GUE: send to GTP-U
```
and for uplink:

```mermaid
 sequenceDiagram
    title Uplink AM DRB traffic flow
    box Blue UE
    participant GUE as GTP / TUN
    participant SUE as SDAP
    participant PUE as PDCP
    participant RUE as RLC
    participant MUE as MAC
    end
    GUE->>SUE: sdap_data_req
    note over SUE: nr_sdap_tx_entity
    SUE->>PUE: nr_pdcp_data_req_drb
    note over PUE: process_sdu
    note over PUE: deliver_pdu_drb_ue
    note over PUE: enqueue_rlc_data_req
    note over PUE: signal to rlc_data_req_thread
    PUE->>RUE: nr_rlc_data_req
    note over RUE: nr_rlc_entity_am_recv_sdu
    RUE-->>MUE: PDU to MAC
    note over MUE: nr_ue_get_sdu
    MUE-->MG: UL TX / RX procedures
    box Purple gNB
    participant MG as MAC
    participant RG as RLC
    participant PG as PDCP
    participant SG as SDAP
    participant GG as GTP / TUN
    end
    MG->>RG: nr_mac_rlc_data_ind
    note over RG: nr_rlc_entity_am_recv_pdu
    note over RG: reception_actions
    note over RG: reassemble_and_deliver
    note over RG: deliver_sdu
    RG->>PG: pdcp_data_ind
    note over PG: enqueue_pdcp_data_ind
    note over PG: do_pdcp_data_ind via pdcp_data_ind_thread
    note over PG: nr_pdcp_entity_recv_pdu
    note over PG: deliver_sdu_drb
    PG->>SG: sdap_data_ind
    note over SG: nr_sdap_rx_entity
    SG->>GG: send to GTP-U
```

## GTP
GTP-U is implemented in `openair3/ocp-gtpu`. It is mostly a library called
directly from other threads; its state (`globGtp`) is protected by a mutex.

### Initialization

`gtpv1Init()` creates a GTP-U instance: a UDP socket on a given local address,
and a receiver thread `gtpv1uReceiver()` for it. The gNB/CU-UP creates one
instance for N3 (NG-U, towards the core) and one for F1-U (between CU-UP and
DU).

The ITTI task `TASK_GTPV1_U` (`gtpv1uTask()`) only handles a few legacy
messages (e.g., `GTPV1U_REQ` to create an instance, end markers).

### Tunnels

`newGtpuCreateTunnel()` is the generic tunnel creation function; it is wrapped
by `gtpv1u_create_ngu_tunnel()` (NR), and `gtpv1u_create_s1u_tunnel()`/
`gtpv1u_create_x2u_tunnel()` (LTE, NSA). The parameters are:

1. outgoing TEID and remote address, associated with the outgoing pair
   (ue_id, bearer id)
2. incoming pair (ue_id, bearer id) and callbacks for incoming data: a
   generic one and, for N3, an SDAP one (with QFI)

It returns the incoming TEID. E.g., in the CU-UP (`cucp_cuup_handler.c`), the
N3 tunnel delivers DL data to `sdap_data_req()` (or `nr_pdcp_data_req_drb()`),
and the F1-U tunnel delivers UL data to `cu_f1u_data_req()`; in the DU, the
F1-U tunnel delivers DL data to RLC (`DURecvCb()`).

### Outgoing packets

`gtpv1uSendDirect()` and variants (`gtpv1uSendDirectWithQFI()` from SDAP on N3,
`gtpv1uSendDirectWithNRUSeqNum()` from PDCP on F1-U) look up the TEID of the
pair (ue_id, bearer id), encode the GTP-U header and send the packet from the
calling thread.

### Incoming packets

`gtpv1uReceiver()` reads the socket, looks up the incoming TEID and calls the
associated callback with its pair (ue_id, bearer id), in the receiver thread
context.

## NGAP
NGAP is an ITTI task (`TASK_NGAP`, `ngap_gNB_task()`), with SCTP handled by
the SCTP task. NGAP messages are mostly exchanged with the RRC task.
