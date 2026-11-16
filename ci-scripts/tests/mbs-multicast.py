# SPDX-License-Identifier: LicenseRef-CSSL-1.0

"""Check the first MBS multicast batch in the tutorial's three-UE Docker deployment.

Use the broadcast tutorial's containers with the MBS_TEST adapter enabled instead
of creating a core broadcast session. Wait for all three members to join and send
no warm-up traffic before running this script.
"""

import argparse
import json
import re
import subprocess
import time


def docker(*args):
    return subprocess.check_output(["docker", *args], text=True, stderr=subprocess.STDOUT)


def logs(container):
    started = docker("inspect", "--format", "{{.State.StartedAt}}", container).strip()
    return docker("logs", "--since", started, container)


def received():
    return [int(docker("exec", f"mbs-ue{n}", "cat", "/sys/class/net/oaitun_ue1/statistics/rx_packets")) for n in (1, 2, 3)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dci-format", choices=(41, 42), type=int, default=42)
    parser.add_argument("--payload-bytes", type=int, choices=range(0, 1401), default=0, metavar="0..1400")
    args = parser.parse_args()
    gnb_log = logs("mbs-gnb")
    if f"DCI 4_{args.dci_format % 10}" not in gnb_log:
        raise RuntimeError("The gNB has not configured the requested MBS DCI format")
    stats = re.findall(r"MBS test: .* members (\d+) SDUs injected \d+ PTM transmissions (\d+)", gnb_log)
    if not stats or stats[-1] != ("3", "0"):
        raise RuntimeError("A fresh multicast group with three members and zero PTM transmissions is required")
    rnti = re.findall(r"UE RNTI ([0-9a-f]+) CU-UE-ID 1 ", gnb_log)[-1]
    destination = None
    for n in (1, 2, 3):
        if re.search(r"RNTI 0x" + rnti, logs(f"mbs-ue{n}"), re.IGNORECASE):
            interface = json.loads(docker("exec", f"mbs-ue{n}", "ip", "-j", "-4", "addr", "show", "oaitun_ue1"))
            destination = interface[0]["addr_info"][0]["local"]
            break
    if destination is None:
        raise RuntimeError("Cannot identify the first CU UE's PDU session address")
    before = received()
    for packet in range(1, 21):
        command = ["exec", "mbs-cn", "sendip", "-p", "ipv4", "-is", "192.168.77.10", "-id", destination]
        if args.payload_bytes:
            command += ["-d", "m" * args.payload_bytes]
        docker(*command, destination)
        time.sleep(0.2)
        delta = [a - b for a, b in zip(received(), before)]
        print(f"Sent {packet:2}: UE counters {delta}", flush=True)
    time.sleep(2)
    delta = [a - b for a, b in zip(received(), before)]
    for n, count in enumerate(delta, 1):
        print(f"ue{n}: {count} of 20")
    return 0 if delta == [20, 20, 20] else 1


if __name__ == "__main__":
    raise SystemExit(main())
