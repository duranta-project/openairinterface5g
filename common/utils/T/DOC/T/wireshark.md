<!-- SPDX-License-Identifier: CC-BY-4.0 -->

# MAC PDUs and wireshark

First, read the [basic usage](./basic.md) to compile things.

It is possible to use `wireshark` to analyse MAC PDUs for UEs,
MIBs, SIBs, and random accesses seen by the eNodeB. It also
works for the gNB, as well as for the nrUE.

## Live usage

Launch wireshark and listen on the local interface (lo).
Set the filter to `udp.port==9999` and read below
for configuration.

Run the softmodem with the correct command line arguments
(see [basic usage](./basic.md)).

Then run:

```shell
./macpdu2wireshark -d ../T_messages.txt -live
```

Note: in some cases a MAC PDU may be bigger than the size of an UDP payload
(slightly less than 64 KiB). This is particularily the case for large
bandwidths in NR and MIMO. In this case macpdu2wireshark, sending UDP packets
in its default mode of operations, will fail. By using the option `-to-file`
a pcap file may be directly generated. You need to configure wireshark to
process the generated files (run `macpdu2wireshark -h` to see how to
configure wireshark for this case).

For other options of `macpdu2wireshark`, run:

```shell
./macpdu2wireshark -h
```

You can for example disable MIB and SIB logging, or log only
a given number of them.

## Record and replay

You need to record some specific traces. As of 2018-10-30
you have to use the group:
* `WIRESHARK`

That is, run:

```shell
./record -d ../T_messages.txt -o /tmp/record.raw -on WIRESHARK
```

With a recorded trace you then use `macpdu2wireshark`.

## Example

Download [example.raw](example.raw). This example contains
MIBs, SIBs and a connection made by an UE followed  by its
disconnection.

First step is to extract `T_messages.txt` from this trace.

```shell
./extract_config -i example.raw > extracted_T_messages.txt
```

Then you run `wireshark`, capture on `lo` and set the
filter to `udp.dstport==9999`.

While `wireshark` is running, you do:

```shell
./macpdu2wireshark -d extracted_T_messages.txt -i example.raw -no-mib -no-sib
```

In `wireshark` you should see something similar to the following:

![wireshark window with dissected MAC PDUs](https://raw.githubusercontent.com/duranta-project/assets/92564035bd1a5764d6ecb566ec673200ceefef8e/openairinterface5g/images/wireshark.png)

## Configure wireshark for LTE

To dissect the packets, you need to configure `wireshark`.

In the menu, choose `Edit->Preferences`.

In the preference window, unroll `Protocols`.

Go to `UDP` and activate `Try heuristic sub-dissectors first`.

Depending on your version of `wireshark`, you may need to go to `MAC-LTE`
and activate `Try Heuristic LTE-MAC over UDP framing` if this option
is present.

For newer versions (after 2.0 as says wireshark's documentation),
heuristic activation is moved to the `Enabled Protocols` window
(in the menu `Analyze`). In the `enabled protocols` window, search
for `lte` and select `mac_lte_udp` to have `MAC-LTE over UDP`.

You may also want to configure other parameters in `MAC-LTE`,
`RLC-LTE`, `PDCP-LTE`, and maybe others. Check the documentation
of `wireshark`.

It is advised to use a recent version of wireshark (say more than 2.4).

## Configure wireshark for NR

Use a recent version of wireshark. The steps below were done using
version 3.3.2. Maybe some options are different for your version
of wireshark. Adapt as necessary.

1) First, do the previous steps for LTE.
2) Then, in the menu, choose `Edit->Preferences`.
3) In the preference window, unroll `Protocols` and configure the following:

**MAC-NR**

Go to `MAC-NR`. Select:

* `Attempt to decode BCCH, PCCH and CCCH data using NR RRC dissector`
* `Attempt to dissect LCID 1-3 as srb1-3`

For `Source of LCID -> drb channel settings`:
* choose option `From static table`.
* click the `Edit...` button of `LCID -> DRB Mappings Table`.
* In the new window, click on `+`. Choose LCID `4`, DRBID `1`, UL RLC Bearer
Type `AM, SN Len=18`, same thing for DL RLC Bearer Type.
* Click OK.

**RLC-NR**

Now, go to `RLC-NR`. Select:

* `Call PDCP dissector for SRB PDUs`.
* `Call PDCP dissector for UL DRB PDUs` choose `18-bit SN`. Same for DL.
* `Call RRC dissector for CCCH PDUs`.
* `Try to reassemble AM frames`.

You don't need to select `May see RLC headers only` and
`Try to reassemble UM frames`.

**PDCP-NR**

Now, go to `PDCP-NR`. Select what you need in there.

It's good to select:
* `Show uncompressed User-Plane data as IP`
* `Show unciphered Signalling-Plane data as RRC`.
* `Only-RLC-frames` in `Do sequence number analysis`, but anything will do.
* `Traffic Info` as layer info to show, depending on what you want to analyse.

We don't use ROHC so you don't need to select `Attempt to decode ROHC data`.

You are done with the preferences. You can click OK.

4) Then, in the menu `Analyze`, choose `Enabled Protocols...`.
5) In the new window search for `nr` and select `mac_nr_udp` to have `MAC-NR over UDP`.

**NAS-5GS**

Under `NAS-5GS`, select:

* `Try to detect and decode 5G-EA0 ciphered messages`

And that's it. Maybe other settings can be changed, but those steps should be
enough for a start.

Note: not all versions of Wireshark correctly dissect encapsulated packets. For instance, we observed that Wireshark v3.4.8 did not call the PDCP dissector for a PDCP PDU split in two RLC SDUs. On the other hand, Wireshark v3.7.0 showed it just fine.

## Alternative to wireshark

While using `macpdu2wireshark` with the `-live` option, it is possible to use `tcpdump` instead of `wireshak` to record MAC PDUs and also other types of traffic (sctp, gtp) in the same output pcap file.

A "catch all" solution is to run:

```
sudo tcpdump -n -iany "udp port 9999 or udp port 2152 or sctp or port 38462 or port 38472 or port 38412 or port 2153"  -w /tmp/rec.pcap
```

(This is an example, other solutions are also possible.)
