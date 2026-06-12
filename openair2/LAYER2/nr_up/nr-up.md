<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# NR user plane (`nr_up`)

[[_TOC_]]

`nr_up` sits between PDCP and the lower user-plane path on gNB and UE. It
decides whether a DRB SDU may proceed (flow control), then delivers
the sealed PDCP PDU either to local RLC (mono / UE) or over F1-U GTP (CU).
PDCP talks only to this API. Shared iface slots apply to both gNB and UE
(`transfer_user_data`, `data_delivery_status`, `dds_user_data`).

On the DU, the same manager holds DDS report state. `nr_up_du` owns F1-U DL
USER DATA RX, RLC-to-CU TX, and DDDS send toward the CU (38.425 PDU type 1).

## Why we need this layer

Without an early admission check, a DL SDU can be ciphered, given a PDCP SN,
and pushed toward RLC (or F1-U) only to be thrown away when a lower buffer is
full. That wastes crypto and queue work, and it opens a PDCP SN gap at the
receiver.

**TS 38.323** (PDCP) makes that ordering explicit.

Clause 5.2.1 (Transmit operation):

> For a PDCP SDU received from upper layers, the transmitting PDCP entity shall:
>
> - associate the COUNT value [...]
> - perform integrity protection, and ciphering [...]
> - set the PDCP SN of the PDCP Data PDU [...]
> - increment TX_NEXT by one
> - submit the resulting PDCP Data PDU to lower layer [...]

Clause 5.3 (SDU discard):

> If the corresponding PDCP Data PDU has already been submitted to lower layers,
> the discard is indicated to lower layers.
>
> NOTE 2: Discarding a PDCP SDU already associated with a PDCP SN causes a SN
> gap in the transmitted PDCP Data PDUs, which increases PDCP reordering delay
> in the receiving PDCP entity. It is up to UE implementation how to minimize
> SN gap after SDU discard.

So flow control belongs before `process_sdu` / SN advance: if the
SDU will not be sent, it must never consume an SN.

**TS 38.322** (RLC) clause 5.4 (SDU discard) ties discard at RLC to a PDCP
indication:

> When indicated from upper layer (i.e. PDCP) to discard a particular RLC SDU,
> the transmitting side of an AM RLC entity or the transmitting UM RLC entity
> shall discard the indicated RLC SDU, if neither the RLC SDU nor a segment
> thereof has been submitted to the lower layers. The transmitting side of an
> AM RLC entity shall not introduce an RLC SN gap when discarding an RLC SDU.

**TS 38.425** (NR user plane) clause 5.1 places flow control at the PDCP host:

> The NR user plane protocol layer is using services of the transport network
> layer in order to allow flow control of user data packets transferred from
> the node hosting PDCP to the corresponding node.

On F1, that is Desired Buffer Size / DDDS on the midhaul. The stop/send
decision lives at the node that hosts PDCP, before SN assignment.

That is why both layouts share `nr_up`:

- Monolithic gNB: PDCP and RLC are in one process, flow control via cached
  RLC TX budget in `nr_up`.
- F1 / CU: the same precheck and budget cache, the F1 backend refreshes the
  cache from DDDS and asks for status with Report Polling.
- F1 / DU: `nr_up_du` answers Report Polling and pushes DDDS (Desired Buffer
  Size hit/left 0, or DBS changed after `NR_UP_DDS_TIMER_MS`). GTP only
  carries the PDUs.

## Roles and init

PDCP calls nr-up only. The backend is chosen at `nr_up_init_*`:

| Role | PDCP deliver | nr-up init | Backend |
|------|--------------|------------|---------|
| gNB mono / UE | `nr_up_deliver_pdu_drb` | `nr_up_init_direct` | PDCP to RLC via `transfer_user_data` = `nr_up_direct_user_data`; RLC to PDCP via `nr_up_direct_dds_user_data` |
| gNB F1 / CU | `nr_up_deliver_pdu_drb` | `nr_up_init_cu` | `transfer_user_data` = `nr_up_f1u_dl_user_data`; DDDS RX via `nr_up_f1u_ddds_indication` |
| gNB F1 / DU | (no PDCP) | `nr_up_init_du` | `dds_user_data` = `nr_up_f1u_ddds_user_data`; `data_delivery_status` = `nr_up_f1u_ddds` |

Per-UE budget entries (`nr_up_ue_state_t` with `drb[]`):

- Insert: `nr_up_add_ue` from PDCP add (mono/UE), CU-UP GTP create, and DU
  F1 GTP create.
- Release: `nr_up_release_ue` / `nr_up_release_drb` from PDCP teardown and
  MAC F1-U tunnel delete.
- Remap: `nr_up_update_ue_id` on F1 reestablishment (rekeys the same object).

## Transfer user data (shared precheck)

For each DRB SDU from PDCP, nr-up runs admission before assigning a PDCP SN.

1. `nr_pdcp_data_req_drb`
2. `nr_up_transfer_user_data_precheck(ue_id, drb_id, pdu_max_size)` -> ALLOW or DROP
3. On ALLOW: `process_sdu`, then `nr_up_deliver_pdu_drb` -> `nr_up_transfer_user_data`
   (`nr_up_report_polling` sets the flag there)
4. Backend: mono/UE `nr_up_direct_user_data`, F1/CU `nr_up_f1u_dl_user_data`

Budget cache (`nr_up_dbs_t` per DRB):

- `nr_up_manager_get_dbs` returns a copy under the manager lock.
- Unsynced after setup (`last_dbs_sync == NR_UP_DBS_NEVER_SYNCED`): ALLOW.
- ALLOW when last reported DBS is non-zero, `pdu_max_size` fits in
  `available_dbs`, and remaining budget is still above the soft-drop floor
  (about 20% of last DBS; `NR_UP_SOFT_DROP_THRESHOLD` = 80%).
- `nr_up_report_polling` runs in `nr_up_transfer_user_data` (stale / unsynced DBS and
  `NR_UP_REPORT_POLLING_TIMER_MS` = 20 ms). DROP sends poll-only
  `nr_up_transfer_user_data` (empty PDU) from precheck.
- After enqueue or GTP send of a real PDU: `nr_up_consume_dbs`.
- Sync from local RLC (mono) or DDDS RX (F1): `nr_up_sync_dbs`.

Report Polling follows TS 38.425 clause 5.5.3.3. Report Delivered stays off
(needs feedback on RLC AM in-sequence delivery).

## Data Delivery Status (toward the PDCP host)

**DDS** is the shared stack name (gNB and UE): report state `nr_up_dds_t`,
orchestrator `nr_up_data_delivery_status`, iface `data_delivery_status` /
`dds_user_data`. **DDDS** is the F1-U wire PDU (TS 38.425 DL DATA DELIVERY
STATUS); F1 helpers keep `ddds` in the name (`nr_up_f1u_ddds`,
`nr_up_f1u_ddds_indication`).

The node that hosts RLC reports how much data toward the air it can still take.
That report is Desired Buffer Size (DBS). On F1 it is DDDS over GTP-U. On mono
it is a local `nr_up_sync_dbs` into the same budget cache PDCP already uses.

`nr_up_data_delivery_status` is the single place that decides whether to report.
It follows TS 38.425 clause 5.4.2.1: Report Polling, DBS hit or leave 0, or DBS
changed after the DDS timer. A report updates `nr_up_dds_t` and then calls
`iface->data_delivery_status`. On F1, the same call can also carry the G-PDU
toward the CU (`len > 0`) with or without a DDDS report.

Flow in three steps (same for mono and F1):

1. Trigger: after RLC enqueue, or RLC SDU via `nr_up_dds_user_data`.
2. DDS: `nr_up_data_delivery_status` (get / precheck / set); then
   `iface->data_delivery_status` if `report_dds` or F1 `len > 0`.
3. User SDU toward the PDCP host when the trigger was `nr_up_dds_user_data`:
   mono/UE uses `nr_pdcp_data_ind` after the hub; F1 carries the G-PDU on the
   same GTP send as DDDS (`len > 0`), then CU `nr_up_f1u_ul_user_data_recv`.

## Transfer User Data and Data Delivery Status procedures (mono and F1)

```mermaid
sequenceDiagram
  autonumber
  participant Upper as Upper layer
  participant PDCP as PDCP
  participant CUNR as NR-UP<br/>nr_up_transfer_user_data.c
  participant CUGTP as CU GTP F1-U
  participant DUGTP as DU GTP F1-U
  participant BACKEND as NR-UP
  participant RLC as RLC

  Note over CUNR,BACKEND: Init binds the backend<br/>mono/UE: nr_up_init_direct()<br/>F1 CU: nr_up_init_cu()
  alt DU
    Note over CUNR,BACKEND: f1_drb_gtpu_create:<br/>nr_up_init_du + nr_up_add_ue<br/>(data_delivery_status = nr_up_f1u_ddds<br/>dds_user_data = nr_up_f1u_ddds_user_data)
  else Mono OR UE
    Note over CUNR,BACKEND: nr_up_init_direct:<br/>manager_init + data_delivery_status = nr_up_direct_dds<br/>dds_user_data = nr_up_direct_dds_user_data<br/>transfer_user_data = nr_up_direct_user_data
  end

  Upper->>PDCP: nr_pdcp_data_req_drb()
  PDCP->>CUNR: nr_up_transfer_user_data_precheck()
  Note over CUNR: same precheck Mono/F1/UE (budget only)<br/>ALLOW: unsynced OR (non-zero DBS AND pdu fits available_dbs<br/>AND budget above soft-drop threshold)
  alt ALLOW
    CUNR->>PDCP: ALLOW
    PDCP->>PDCP: process_sdu() (assign PDCP SN)
    PDCP->>CUNR: nr_up_deliver_pdu_drb(req)
    CUNR->>CUNR: nr_up_transfer_user_data(req)
    CUNR->>CUNR: report_polling = nr_up_report_polling()
    CUNR->>CUNR: iface->transfer_user_data(req)
    Note over CUNR: send PDU + RP / PDU only
    alt mono/UE
      CUNR->>BACKEND: nr_up_direct_user_data(req)
      BACKEND->>BACKEND: nr_rlc_enqueue_data_req(req)
    else F1
      CUNR->>CUNR: nr_up_f1u_dl_user_data()
      opt report_polling
        Note over CUNR: Report Polling=1 in DL User Data
      end
      CUNR->>CUGTP: gtpv1uSendDirectWithNRUSeqNum()<br/>G-PDU + NR-U DL USER DATA
      CUGTP->>DUGTP: F1-U G-PDU
      DUGTP->>DUGTP: Gtpv1uHandleGpdu
      Note over DUGTP: callBack = nr_up_f1u_dl_user_data_recv
      DUGTP->>BACKEND: TEID callBack (is_poll)
    end
    alt PDU
      Note over CUNR: consume DBS after enqueue/GTP send
      CUNR->>CUNR: nr_up_consume_dbs()
    end
  else DROP
    Note over CUNR: no process_sdu, no SN, no PDU deliver
    Note over CUNR: poll-only empty req inside precheck
    CUNR->>CUNR: nr_up_transfer_user_data(empty)
    CUNR->>CUNR: report_polling = nr_up_report_polling()
    alt report_polling
      CUNR->>CUNR: iface->transfer_user_data(empty)
      Note over CUNR: same backends as ALLOW, empty PDU, no consume
    end
    CUNR->>PDCP: DROP
  end

  Note over BACKEND,RLC: 1) Trigger
  alt after RLC enqueue
    Note over BACKEND: nr_up_f1u_dl_user_data_recv() / RLC queue worker
    BACKEND->>RLC: nr_rlc_data_req()
    alt PDU != NULL
      RLC->>RLC: recv_sdu
    end
    RLC-->>BACKEND: remaining TX space
  else RLC SDU
    RLC->>BACKEND: nr_up_dds_user_data()
    Note over BACKEND: mono/UE: nr_up_direct_dds_user_data()<br/>F1: nr_up_f1u_ddds_user_data()
  end

  Note over BACKEND: 2) nr_up_data_delivery_status()<br/>get / precheck / set (TS 38.425 5.4.2.1)<br/>precheck: is_poll OR zero-DBS OR DDS timer
  BACKEND->>BACKEND: nr_up_data_delivery_status()
  opt report_dds OR F1 len>0
    BACKEND->>BACKEND: iface->data_delivery_status(req)
    alt mono/UE
      Note over BACKEND: nr_up_direct_dds()
      BACKEND->>BACKEND: nr_up_sync_dbs()
    else F1
      Note over BACKEND: nr_up_f1u_ddds()
      BACKEND->>DUGTP: gtpv1uSendDirect(pdu, report_dds)
      Note over BACKEND: G-PDU+DDDS / DDDS-alone / G-PDU-only
      opt report_dds
        DUGTP->>CUGTP: DL DATA DELIVERY STATUS
        CUGTP->>CUNR: nr_up_f1u_ddds_indication()
        CUNR->>CUNR: nr_up_sync_dbs()
      end
    end
  end

  Note over BACKEND,PDCP: 3) User SDU toward PDCP host (only if trigger was RLC SDU)
  opt RLC SDU
    alt mono/UE
      BACKEND->>PDCP: nr_pdcp_data_ind()
    else F1
      Note over DUGTP,CUGTP: G-PDU already on gtpv1uSendDirect when len>0
      CUGTP->>CUNR: nr_up_f1u_ul_user_data_recv()
      CUNR->>PDCP: do_pdcp_data_ind()
    end
  end
```
