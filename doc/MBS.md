<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# Multicast Broadcast Services (MBS) in 5G

## Overview

The gNB sends one PTM transmission on a G-RNTI with DCI format 4_1 or 4_2 in a common search space,
and every UE of the group receives it.
With multicast HARQ feedback enabled, every UE acknowledges each PTM transport block on its own PUCCH (HARQ ACK/NACK).
On NACK, only that UE gets a PTP retransmission on its C-RNTI.
To achieve this the whole user plane exists:

- **RAN**: SDAP -> PDCP (MRB) -> RLC (PTM entity) -> MAC (PTM)
- **UE**: MAC -> RLC -> PDCP -> SDAP

The deployment below uses the multicast test adapter. Core-driven broadcast
setup and its shared N3 user plane are also implemented; see the
[broadcast tutorial](MBS_Broadcast_Tutorial.md). Broadcast has no membership or
HARQ feedback and currently uses DCI 4_1.

Remaining multicast radio features:

- 3GPP also allows PTM retransmissions (TS 38.300 §16.10.5.4 / TS 38.213 §18).
- NACK-Only HARQ-ACK and a CFR smaller than the BWP (TS 38.213 §18)

## Example Deployment

Every UE runs in its own network namespace, so each has its own `oaitun_ue1`.
The MBS traffic is the DL traffic of the first UE.
Before you start run to get info

```bash
./tools/scripts/mbs.sh help
```

```bash
# Terminal 1
./tools/scripts/mbs.sh cn
./tools/scripts/mbs.sh nr # or ./tools/scripts/mbs.sh nr noharq

# Terminal 2
./tools/scripts/mbs.sh ue 1

# Terminal 3
./tools/scripts/mbs.sh ll 1

# Terminal 4
./tools/scripts/mbs.sh ue 2 -44

# Terminal 5
./tools/scripts/mbs.sh ll 2

# Terminal 6
./tools/scripts/mbs.sh dl 1
```

## DCI format 4_2

The multicast adapter supports DCI formats 4_1 and 4_2. Set `dci_format: 42` in
`MBS_TEST` in `ci-scripts/conf_files/mbs-gnb.yaml`, or add
`--MBS_TEST.[0].dci_format 42` to the gNB command. The default is `41`. The UE
learns the selected format and `pdsch-ConfigMulticast` through its
RRCReconfiguration; it needs no command-line change. The broadcast tutorial
continues to use its existing DCI 4_1 path.

The implemented 4_2 profile follows TS 38.212 §7.3.1.5.3:

- The CFR is the full initial DL BWP. Type-1 resource allocation addresses its
  PRBs directly, without the CORESET-0 granularity of format 4_1.
- One layer and one codeword, DMRS type 1 with a single front-loaded symbol,
  port 1000, no VRB interleaving, and the 64-QAM MCS table.
- The multicast TDA list copies the cell's common TDA list. Its length and the
  length of the existing PUCCH `dl-DataToUL-ACK` list determine the TDA and K1
  field widths, including zero-bit fields for single-entry lists.
- The dynamic HARQ-ACK codebook supplies the two-bit counter DAI. Feedback can
  be disabled or fixed to ACK/NACK; NACKs use the existing PTP retransmission
  path. Dedicated multicast PUCCH configurations are not generated.
- `sizeDCI-4-2` is signalled as the exact sum of the supported fields. The
  implementation retains the stack's 64-bit DCI payload limit. Unsupported
  PDSCH options and mismatched configured sizes are rejected.

Resource allocation type 0/dynamic switching, two codewords, other DMRS/port
profiles, TCI indication, dynamic PRB bundling, rate-matching/ZP-CSI-RS
indicators, priority indication, and DCI-controlled feedback enabling are not
implemented. Groups sharing the cell's MBS search space must use the same
format, and all members of a group must use the same payload size.

The DCI tests exercise the production gNB packer and UE extractor against the
field order of TS 38.212 §7.3.1.5.3, with different CFR, TDA-list, and K1-list
sizes. Run them with `ctest -R 'DciFormat4[12]' --output-on-failure` in a build
configured with `-DENABLE_TESTS=ON`. Live checks use the deployment above with
`dci_format` set to `42`, with and without HARQ feedback. With the broadcast
tutorial's Docker containers and the multicast adapter enabled, run
`python3 ci-scripts/tests/mbs-multicast.py --dci-format 42` for the first batch;
`--payload-bytes 1000` also exercises larger transport blocks.
The check requires a fresh group with no prior PTM traffic.

## Development

### Implemented broadcast integration

The broadcast path already provides the following integration. It does not
use `MBS_TEST` or redirect a UE's unicast traffic.

| Implemented behavior | Code path |
|---|---|
| Receive the core-created session and its tunnel information | `ngap_gNB_handle_broadcast_session_setup()` in [ngap_gNB_handlers.c](../openair3/NGAP/ngap_gNB_handlers.c) |
| Pass broadcast bearer setup to the CU-UP, directly or over E1AP | `bc_bearer_context_setup` interface and `e1_bc_bearer_context_setup()` in [cucp_cuup_handler.c](../openair2/LAYER2/nr_pdcp/cucp_cuup_handler.c) |
| Pass broadcast context setup to the DU, directly or over F1AP, and create the group | `DU_handle_BROADCAST_CONTEXT_SETUP_REQUEST()` in [f1ap_mbs.c](../openair2/F1AP/f1ap_mbs.c), then `broadcast_context_setup()` in [mac_rrc_dl_handler.c](../openair2/LAYER2/NR_MAC_gNB/mac_rrc_dl_handler.c) and `nr_mac_add_mbs_group()` |
| Receive payload on the core's multicast group and C-TEID and submit it to the MRB | `gtpv1InitMulticast()` / `gtpv1uRegisterIncomingTeid()` in [gtp_itf.cpp](../openair3/ocp-gtpu/gtp_itf.cpp), then `nr_pdcp_mbs_broadcast_deliver()` / `nr_pdcp_mrb_data_req()` in [nr_pdcp_oai_api.c](../openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.c) |
| Configure the UE's broadcast RLC and PDCP from the announced bearer | `nr_rrc_ue_process_sib20()` / `nr_rrc_ue_add_mbs_broadcast_mrb()` in [rrc_UE.c](../openair2/RRC/NR_UE/rrc_UE.c) |

Broadcast MRB creation at the gNB still uses fixed PDCP parameters in
`broadcast_context_setup()` at the DU. E1AP/F1AP setup signalling exists, but
MRB data cannot cross a CU/DU split: `deliver_pdu_mrb_gnb()` still delivers
locally to RLC. The broadcast session-to-PDCP mapping is also limited to one
session. See the [broadcast tutorial's gaps](MBS_Broadcast_Tutorial.md#known-gaps)
for the MCCH, DCI and UE-interface limitations.

The call graphs below show implemented multicast functions. Their TODO lists
describe the remaining session-control and transport integration.

### Creating MBS groups

1. Picks the G-RNTI and stores the group config and cell.
2. Reserves PTM HARQ processes.
3. Creates the PTM RLC entity.
4. Builds the multicast CORESET and search space.
5. Registers the group for the scheduler to schedule it every slot.

```bash
executables/nr-softmodem.c                                      main()
└─ executables/nr-softmodem.c                                   create_gNB_tasks()
   └─ openair2/GNB_APP/gnb_config.c                             RCconfig_nr_macrlc()
      └─ openair2/LAYER2/NR_MAC_gNB/nr_mbs_test_adapter.c       nr_mbs_test_adapter_init()
         └─ openair2/LAYER2/NR_MAC_gNB/nr_mbs_test_adapter.c    nr_mbs_test_thread()
            └─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_mbs.c   nr_mac_add_mbs_group()
```

**[TODO]**

- `nr_mac_add_mbs_group()`:
  - Today: Creates the group from `nr_mbs_group_config_t` and returns its G-RNTI. The multicast test adapter supplies the `MBS_TEST` settings; the broadcast path calls the same function from `broadcast_context_setup()` with the core's session identity.
  - TODO: Add the F1AP **Multicast** Context Setup caller (TS 38.473 §8.14.6), using the session's configuration. The implemented Broadcast Context Setup does not handle this multicast procedure.

### Joining a UE to MBS

1. Adds the multicast configuration (G-RNTI, CFR, PTM RLC bearer) to each UE.
2. Sends it to the CU, which sends the RRCReconfiguration to the UE.
3. Reserves the PTM HARQ processes of the UE.
4. Once the group has a member, creates the MRB PDCP.
5. The UE creates its MRB PDCP and configures the G-RNTI and the search space.

```bash
openair2/LAYER2/NR_MAC_gNB/nr_mbs_test_adapter.c              nr_mbs_test_thread()
├─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_mbs.c             nr_mac_add_mbs_member()
│  ├─ openair2/LAYER2/NR_MAC_gNB/nr_radio_config.c            add_mbs_cellGroupConfig()
│  └─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_primitives.c   nr_mac_request_reconfiguration()
└─ openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.c                  nr_pdcp_add_mrb()
```

```bash
openair2/RRC/NR_UE/rrc_UE.c                   nr_rrc_ue_process_rrcReconfiguration()
├─ openair2/RRC/NR_UE/rrc_UE.c                nr_rrc_add_mbs_test_mrb()
└─ openair2/LAYER2/NR_MAC_UE/config_ue.c      nr_rrc_mac_config_req_cg()
   ├─ openair2/LAYER2/NR_MAC_UE/config_ue.c   configure_g_rnti()
   └─ openair2/LAYER2/NR_MAC_UE/config_ue.c   configure_cfr_multicast()
```

**[TODO]**

- `nr_mac_add_mbs_member()`
  - Today: Adds the multicast part to the CellGroupConfig of the UE and sends it to the CU and reserves the PTM HARQ processes. The test adapter (`join_ues()`) calls it every 100 ms for every connected UE.
  - TODO: Keep the function as is but call it when the DU receives a UE Context Setup/Modification that carries the multicast session of the UE (Multicast MBS Session List, TS 38.473 §9.3.1.272).

- `nr_mac_request_reconfiguration()`
  - Today: Sends the new CellGroupConfig to the CU in a UE Context Modification Required. The CU sends it to the UE in an RRCReconfiguration, but without any MRB-ToAddMod. If the CU refuses, `ue_context_modification_refuse()` (`mac_rrc_dl_handler.c`) removes the UE from the group again.
  - TODO: The CU RRC adds the MRB-ToAddMod of the session to that RRCReconfiguration. If the CU builds the multicast CellGroupConfig itself, remove the call of `add_mbs_cellGroupConfig()` in `nr_mac_add_mbs_member()`, which then only keeps the MAC state.

- `nr_pdcp_add_mrb()`
  - Today: Creates the gNB MRB PDCP entity, without ciphering or integrity (TS 38.323 §5.8, §5.9). The multicast adapter calls it on the first join; broadcast calls it from `broadcast_context_setup()`. Both use the fixed SN size and t-Reordering of `nr_pdcp_mbs_test.h`.
  - TODO: Create the gNB MRB from CU-UP bearer setup using the session's PDCP-Config, and carry the DU-allocated group context to the CU-UP. E1AP broadcast setup currently creates the tunnel, not the MRB PDCP. Remove the fixed test parameters once both services use signalled configuration.

- `nr_rrc_add_mbs_test_mrb()`
  - Today: When the UE receives the PTM RLC bearer, it creates its own MRB PDCP with the fixed values of `nr_pdcp_mbs_test.h`, attached to its first PDU session. This is needed because no MRB-ToAddMod is received.
  - TODO: For multicast, replace this helper with MRB PDCP creation from the MRB-ToAddMod of RadioBearerConfig (TS 38.331 §6.3.2). Broadcast already uses its announced MRB-InfoBroadcast through `nr_rrc_ue_add_mbs_broadcast_mrb()` and does not call this helper.

### Data transmission

1. The DL data enters the MRB PDCP and is queued in the PTM RLC entity.
2. Every DL slot, the group is scheduled before unicast with its configured DCI 4_1 or 4_2.
3. The UE decodes the configured MBS DCI and passes the data up to its MRB PDCP.

```bash
openair2/LAYER2/NR_MAC_gNB/nr_mbs_test_adapter.c   shared_delivery()
└─ openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.c       nr_pdcp_mrb_data_req()
   └─ openair2/LAYER2/nr_rlc/nr_rlc_oai_api.c      nr_rlc_mrb_recv_sdu()
```

```bash
openair2/LAYER2/NR_MAC_gNB/gNB_scheduler.c                gNB_dlsch_ulsch_scheduler()
└─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_mbs.c         nr_schedule_mbs()
   └─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_mbs.c      nr_schedule_mbs_group()
      ├─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_mbs.c   nr_mbs_acknack_scheduling()
      ├─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_mbs.c   nr_mbs_generate_mac_pdu()
      └─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_mbs.c   nr_mbs_fill_pdcch_pdsch()
```

```bash
openair2/NR_UE_PHY_INTERFACE/NR_IF_Module.c       nr_ue_dl_processing()
├─ openair2/LAYER2/NR_MAC_UE/nr_ue_procedures.c   nr_ue_process_dci_dl_mbs()
└─ openair2/LAYER2/NR_MAC_UE/nr_ue_procedures.c   nr_ue_process_mac_pdu()
   └─ openair2/LAYER2/nr_pdcp/nr_pdcp_oai_api.c   nr_pdcp_mrb_data_ind()
      └─ openair2/SDAP/nr_sdap/nr_sdap.c          sdap_mrb_data_ind()
```

**[TODO]**

- `nr_pdcp_mrb_data_req()`
  - Today: Applies PDCP and queues the result in the PTM RLC entity. Broadcast already supplies real shared-N3 payload through `nr_pdcp_mbs_broadcast_deliver()`. Multicast still supplies one PDU session's DL traffic through `shared_delivery()` and the redirect hook in `sdap_data_req()`.
  - TODO: Connect multicast session setup and its shared N3 tunnel to the MRB (TS 23.247 §7.2.1.4), replacing `shared_delivery()` and `nr_sdap_set_dl_redirect()`. The existing broadcast tunnel does not replace the multicast adapter.

- `nr_rlc_mrb_recv_sdu()`
  - Today: Puts a PDCP PDU in the PTM RLC entity of the G-RNTI. It works only when PDCP and RLC run in the same process. The `nr-cuup` stub aborts (`executables/nr-cuup.c`), and `deliver_pdu_mrb_gnb()` rejects a CU node before local delivery.
  - TODO: With a CU/DU split, F1-U must carry the MRB PDUs to the DU and call it there. Therefore, keep this function as is and consider how to call it when CU/DU is supported.

- `sdap_mrb_data_ind()`
  - Today: Multicast writes to the associated PDU session's tun; the test adapter associates the MRB with the first PDU session. `mbs.sh ll` gives the other UEs the IP of UE 1 because the adapter redirects unicast-addressed traffic. Broadcast carries the original multicast IP packets to the NAS-provided tun and needs no IP reassignment.
  - TODO: Replace the multicast adapter's unicast traffic and fixed PDU-session association with delivery for the actual multicast session, so `mbs.sh ll` no longer needs to reassign UE addresses. A dedicated UE interface for broadcast sessions is also still missing.

### ACK/NACK and Retransmissions

1. Each UE sends its ACK/NACK for the PTM transport block on its own PUCCH.
2. On ACK, the HARQ process is freed for that UE.
3. On NACK, the transport block is retransmitted with PTP.

```bash
openair2/LAYER2/NR_MAC_UE/nr_ue_procedures.c      get_downlink_ack()
└─ openair2/LAYER2/NR_MAC_UE/nr_ue_procedures.c   get_multicast_downlink_ack()
```

```bash
openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_uci.c      handle_dl_harq()
└─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_mbs.c   nr_mbs_handle_dl_harq()
```

```bash
openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_dlsch.c         nr_schedule_ue_spec()
└─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_dlsch.c      nr_dl_schedule()
   └─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_dlsch.c   get_retx_mcs()
```

### Leaving MBS

1. When a UE is released, it is removed from every group.
2. The PTM HARQ processes go back to unicast.

```bash
openair2/LAYER2/NR_MAC_gNB/mac_rrc_dl_handler.c            ue_context_release_command()
└─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_primitives.c   mac_remove_nr_ue()
   └─ openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_mbs.c       nr_mac_remove_mbs_member()
```

```bash
openair2/LAYER2/NR_MAC_gNB/gNB_scheduler_mbs.c   nr_mac_release_mbs_group()
```

**[TODO]**

- `nr_mac_remove_mbs_member()`
  - Today: Removes the UE from the group and gives its PTM HARQ processes back to unicast. It is called on UE release (`mac_remove_nr_ue()`), when the CU refuses the join, and inside the group-release helper.
  - TODO: Also call it when a UE Context Modification removes the multicast session of a UE that stays connected. The gNB must generate the RRCReconfiguration removing its G-RNTI and PTM RLC bearer; the UE already processes the corresponding release lists.

- `nr_mac_release_mbs_group()`
  - Today: Removes all members, deletes the PTM RLC entity and frees the group. Nothing calls it yet.
  - TODO: Call it when the DU receives the F1AP Multicast Context Release (TS 38.473 §8.14.7).
