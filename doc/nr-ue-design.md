<!-- SPDX-License-Identifier: CC-BY-4.0 -->

## PHY and MAC Interface
The PHY sends scheduling requests and data indication to MAC via `nr_ue_dl_indication` for DL path and `nr_ue_ul_indication()` for UL path and the MAC sends scheduling configuration to PHY via `nr_ue_scheduled_response()`. The following diagram shows the interaction for PDCCH and PDSCH reception.
```mermaid
sequenceDiagram
    PHY->>+MAC: Requests for PDCCH config (via nr_ue_dl_indication)
    MAC->>+PHY: Schedules PDCCH reception (via nr_ue_scheduled_response)
    PHY->>+MAC: Indicates decoded DCI(s) (via nr_ue_dl_indication)
    MAC->>+PHY: Schedules PDSCH reception (via nr_ue_scheduled_response)
```

## Multi-threading Design
The `UE_thread` function in `nr-ue.c` is the main top level thread that interacts with the radio unit. Once the thread spawns, it starts the 'Initial Synchronization'. Once its complete, the regular processing of slots commences.

The UE exits when at any point in operation it gets out of synchronization. When the command line option `--non-stop` is used, the UE goes to 'Initial Synchronization' mode when it loses synchronization with gNB. However, this feature is not fully implemented and it is a work in progress at the time of writing this documentation. This will be the default behavior (not a command line option) when the feature is fully implemented.

UE uses actors which are threads dedicated to particular activity. Sync Actor handles initial sync. DL Actors handle DLSCH PHY procedures. UL procedures are are run on the UL Actor

![design](https://raw.githubusercontent.com/duranta-project/assets/92564035bd1a5764d6ecb566ec673200ceefef8e/openairinterface5g/images/nr-ue-threads.svg)

### Initial Synchronization Block
```mermaid
graph TD
    start(Start) -->|UE_thread| rxRu["RU read<br/>(Reads two frames)"]
    rxRu --> |Sync Actor| sync["SSB detection<br/>(PSS & SSS detection<br/>PBCH decoding<br/>SIB decoding)"]
    rxRu --> |UE_thread| rxRuDummy["RU read<br/>(Dummy read till sync detection to avoid buffer overflow at radio)"]
    sync --> |UE_thread| frameSync["Frame synchronization<br/>(Shift received samples to align with frame)"]
    rxRuDummy --> |UE_thread| frameSync
```
#### Regular Slot Processing
```mermaid
graph TD
    sync["Frame synchronization<br/>(Shift received samples to align with frame)"] -->|UE_thread| hw_read["RU read (slot n)"]
    hw_read --> |UE_thread| rxPreProc["PBCH & PDCCH decoding (slot n)"]
    hw_read --> |UL Actor| txProc["Tx processing (slot n+3)<br/>PUSCH encode<br/>PUCCH encode (wait for DLSCH in slot n+3-k1 to finish)<br/>RU write"]
    rxPreProc --> |DL Actor| rxProc["PDSCH decoding (slot n)"]
    rxPreProc --> |UE_thread| join(Merge)
    join --> |Go to next slot<br/>UE_thread| hw_read
```
