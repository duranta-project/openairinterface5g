# SPDX-License-Identifier: LicenseRef-CSSL-1.0

"""Check the first broadcast batch after following doc/MBS_Broadcast_Tutorial.md.

Start a fresh deployment, create the broadcast session and multicast route, and
wait for all three UEs to announce the session before running this script. Do not
send warm-up traffic: the regression occurs when PTM HARQ processes are first reused.
"""

import re
import subprocess
import time


def docker(*args):
    return subprocess.check_output(["docker", *args], text=True, stderr=subprocess.STDOUT)


def received():
    return [
        int(docker("exec", f"mbs-ue{n}", "cat", "/sys/class/net/oaitun_ue1/statistics/rx_packets"))
        for n in (1, 2, 3)
    ]


def current_logs(container):
    started = docker("inspect", "--format", "{{.State.StartedAt}}", container).strip()
    return docker("logs", "--since", started, container)


def main():
    stats = re.findall(r"MBS broadcast session G-RNTI [0-9a-f]+: (\d+) PTM transmissions", current_logs("mbs-gnb"))
    if not stats or int(stats[-1]) != 0:
        raise RuntimeError("A fresh broadcast session with zero PTM transmissions is required")
    for n in (1, 2, 3):
        if "MBS: broadcast session with service ID" not in current_logs(f"mbs-ue{n}"):
            raise RuntimeError(f"UE {n} has not announced the broadcast session")

    before = received()
    for packet in range(1, 21):
        docker("exec", "mbs-cn", "sendip", "-p", "ipv4", "-is", "192.168.77.10", "-id", "239.0.0.25", "239.0.0.25")
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
