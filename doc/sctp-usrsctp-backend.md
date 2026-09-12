# Userspace SCTP backend (usrsctp) for F1-C and N2

## What this is

OAI's SCTP task can now run on either of two backends:

| backend   | transport                                                    |
|-----------|--------------------------------------------------------------|
| `kernel`  | libsctp and kernel sockets. The default, unchanged behaviour. |
| `usrsctp` | the FreeBSD SCTP stack in userspace, over a raw `IPPROTO_SCTP` socket. |

The userspace backend exists for platforms whose kernel is built without
`CONFIG_IP_SCTP` and cannot practically be rebuilt with it. It needs
`CAP_NET_RAW`, but **not** kernel SCTP: a raw socket only has to carry the
packets, not implement the protocol.

Traffic is ordinary SCTP over IP — **not** UDP-encapsulated — so the CU or AMF
sees a normal association and needs no configuration change.

### Why it was needed

On the Qualcomm SA8775P the vendor kernel ships with `CONFIG_IP_SCTP` unset, and
a rebuild that enables it breaks the compute-DSP glink endpoints. Kernel SCTP and
the DSP offloads were therefore mutually exclusive, and F1-C could not be brought
up at all while the offloads were in use. That is a concrete case, but the
constraint is general: any target with a fixed kernel lacking SCTP.

## Building

```
cmake -DENABLE_USRSCTP=ON ...
```

Default is `OFF`, so existing builds are unaffected. When enabled, usrsctp is
fetched with CPM (`sctplab/usrsctp`, tag `0.9.5.0`) like OAI's other third-party
dependencies.

### Licensing

usrsctp is **BSD-3-Clause** — permissive, and compatible with OAI's own licence.
Audited at tag `0.9.5.0`:

| | |
|---|---|
| Top-level `LICENSE.md` | BSD-3-Clause (Randall Stewart, Michael Tuexen) |
| SPDX tags in the tree | 42 x `BSD-3-Clause`, 1 x `BSD-2-Clause-FreeBSD` |
| `usrsctplib` sources | 61 of 61 carry a BSD notice; none missing one |
| GPL / LGPL | none anywhere in the tree |
| External link dependencies | none on Linux (`ws2_32`/`iphlpapi` are Windows-only) |

The single BSD-2-Clause-FreeBSD file is `usrsctplib/netinet/sctp_ss_functions.c`;
still permissive, just the two-clause variant, which is ordinary for
FreeBSD-derived code.

Two consequences worth being explicit about:

- **Nothing is vendored.** CPM fetches the sources at configure time, so this
  repository redistributes no BSD-licensed code. The attribution obligations
  (retain the copyright notice; do not use the authors' names to endorse) attach
  to *binaries* built with `ENABLE_USRSCTP=ON`, and so to whoever ships them.
- **A default build has no new licence surface at all.** With the flag `OFF` the
  binary contains zero `usrsctp_*` symbols, which is checkable directly:

  ```
  nm nr-softmodem | grep -c ' usrsctp_'    # 0 with OFF, ~180 with ON
  ```

## Selecting the backend

The choice is made **at runtime**, on first use, by trying to open a kernel SCTP
socket and falling back if that fails:

```
[SCTP] kernel has no SCTP support (Protocol not supported), falling back to usrsctp
[SCTP] SCTP backend: usrsctp (userspace stack on a raw IPPROTO_SCTP socket)
```

`OAI_SCTP_BACKEND=kernel|usrsctp` forces either, which is how the userspace path
is exercised on a machine that has kernel SCTP.

`ENABLE_USRSCTP` decides only whether the backend is *compiled in*. It is
deliberately not a configure-time probe of the running kernel, because OAI is
routinely cross-compiled — a probe would test the build host rather than the
target — and because `<netinet/sctp.h>` being present in a sysroot says nothing
about `CONFIG_IP_SCTP` in the kernel that will run the binary. One image may also
be booted against kernels that differ in SCTP support.

## Design

`openair3/SCTP/oai_sctp.h` defines a backend-neutral API; all SCTP calls in
`sctp_common.c` and `sctp_eNB_task.c` go through it. The kernel arm is a straight
passthrough, so behaviour with `CONFIG_IP_SCTP` present is unchanged.

**Handles stay `int`.** usrsctp sockets are `struct socket *`, are not file
descriptors, and cannot be given to `epoll` — but the SCTP task is built around
`epoll` and dispatches on `events[i].data.fd`. Each usrsctp socket is therefore
paired with an `eventfd`, and it is the eventfd that is returned as the handle
and registered with ITTI. usrsctp's upcall drains the message into a per-handle
queue and posts one token; the task wakes exactly as before.

The eventfd is `EFD_SEMAPHORE` deliberately: one token per queued message keeps
the counter and the queue in step. A counter drained in a single read would lose
wakeups whenever two messages arrived before the task was scheduled.

## Maintenance note: translate constants, never forward them

The two stacks share names but not values. Six distinct classes were found during
bring-up, and **none produced a compile error** — each silently did the wrong
thing. Anything crossing the backend boundary is expressed in neutral types and
re-encoded on each side, in the translation unit where that stack's headers are
in scope.

| what | Linux | usrsctp |
|------|-------|---------|
| `MSG_NOTIFICATION` | 0x8000 | 0x2000 |
| `SCTP_INITMSG` | 2 | 3 |
| `SCTP_NODELAY` | 3 | 4 |
| `SCTP_STATUS` | 14 | 0x100 |
| `SCTP_EVENTS` | 11 | does not exist (uses per-type `SCTP_EVENT`) |
| `struct sctp_event_subscribe` | 14 fields | 11 fields |
| `SCTP_BINDX_ADD_ADDR` | 0x01 | 0x8001 |
| `SCTP_ASSOC_CHANGE` | 0x8001 | 0x0001 |
| `SCTP_REMOTE_ERROR` | 0x8004 | 0x0003 |
| `SCTP_PARTIAL_DELIVERY_EVENT` | 0x8006 | 0x0007 |
| `SCTP_COMM_UP` (`sac_state`) | 0 | 1 |

Two of these deserve emphasis:

* Notification types are **not** a constant offset. Linux numbers from
  `SCTP_SN_TYPE_BASE` (`1<<15`) but orders `SEND_FAILED` before `REMOTE_ERROR`,
  so a blanket `+0x8000` mis-maps `REMOTE_ERROR` and `PARTIAL_DELIVERY`. The
  symptom of getting this wrong is subtle: the association establishes,
  heartbeats flow normally, and F1 Setup is simply never sent.
* usrsctp passes the `bindx` flag **straight to `setsockopt()` as the option
  number**, so a wrong value does not merely differ — it names an option that
  does not exist.

An option with no translation is refused loudly rather than forwarded, because
forwarding is precisely what failed.

## Lifecycle difference from the kernel backend

A userspace stack dies with its process. With kernel SCTP the kernel owns the
socket and sends `SHUTDOWN`/`ABORT` when the process goes away, so the peer drops
the association at once. usrsctp runs inside the gNB, so exiting without closing
sends nothing and the peer is left heartbeating an association that will never
answer. On F1-C the CU then keeps the DU registered and refuses reconnects:

```
[NR_RRC] E gNB-DU ID: existing DU gNB-DU-OAI on assoc_id 891 already has ID 3584,
           rejecting requesting gNB-DU
```

Open associations are therefore closed from an `atexit()` handler, which covers
normal shutdown. **`SIGKILL` cannot be covered** — there is no kernel left to
clean up — and the peer falls back to its heartbeat timeout. If F1 Setup Failure
appears unexpectedly, check the CU log for that duplicate-ID line before
suspecting a configuration mismatch: it means a previous DU was hard-killed.

## Status

Verified against a live OAI CU on a kernel with `CONFIG_IP_SCTP` unset:

```
[MAC]     received F1 Setup Response from CU gNB-CU
[MAC]     received gNB-DU configuration update acknowledge
[NR_RRC]  Accepting DU 3584 (gNB-DU-OAI) ... cell PLMN 208.99 Cell ID 1 is in service
```

Consecutive DU restarts register, serve and tear down cleanly. N2 uses the same
task and backend but has not been exercised separately.
