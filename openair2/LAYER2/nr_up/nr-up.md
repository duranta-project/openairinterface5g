<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# NR user plane (`nr_up`)

Monolithic gNB (`nr-softmodem`) downlink DRB path: PDCP precheck and transfer through
nr-up to RLC (async worker). Split CU/DU is out of scope here.

## Downlink data path

```mermaid
sequenceDiagram
  participant ul
  participant pdcp
  participant nrup
  participant rlc

  ul->>pdcp: nr_pdcp_data_req_drb()
  pdcp->>nrup: nr_up_dl_congestion_precheck()
  nrup->>nrup: nr_up_mono_dl_congestion_precheck()
  alt DROP
    Note over pdcp: no process_sdu, no SN
  else ALLOW
    pdcp->>pdcp: process_sdu()
    pdcp->>nrup: nr_up_dl_transfer()
    nrup->>nrup: nr_up_mono_deliver_drb()
    nrup->>nrup: nr_up_enqueue_rlc_data_req() (worker queue only)
    nrup->>nrup: nr_up_drb_budget_consume()
    Note over nrup: RLC worker thread (async)
    nrup->>rlc: nr_rlc_data_req()
    rlc-->>nrup: tx_space
    nrup->>nrup: nr_up_drb_budget_sync()
  end
```
