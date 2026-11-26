#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-CSSL-1.0
"""Check static and realtime SIB9 reception with RF-simulator UEs."""
import argparse
import ipaddress
import json
import os
from pathlib import Path
import re
import resource
import signal
import socket
import subprocess
import time
import uuid
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
STATIC_UTC = 390898880000


def decoded(text):
    text = re.sub(r"\x1b\[[0-9;]*m", "", text)
    result = []
    for match in re.finditer(r"<BCCH-DL-SCH-Message>.*?</BCCH-DL-SCH-Message>", text, re.S):
        try:
            result.append(ET.fromstring(match[0]))
        except ET.ParseError:
            continue
    return result


def observe(path, nci, mode):
    text = path.read_text(errors="replace") if path.exists() else ""
    messages = decoded(text)
    sib1 = [x.find(".//systemInformationBlockType1") for x in messages
            if x.find(".//systemInformationBlockType1") is not None]
    sib9 = [x.find(".//sib9") for x in messages if x.find(".//sib9") is not None]
    identity = any(int(re.sub(r"\s", "", x.findtext(".//cellIdentity", "0")), 2) == nci for x in sib1)
    enabled = mode != "disabled"
    checks = {"cell_identity": identity, "sib2_received": "Found SIB2" in text,
              "sib9_mapping": any(x.find(".//sibType9") is not None for x in sib1) == enabled,
              "sib9_received": bool(sib9) == enabled}
    values = [int(x.findtext(".//timeInfoUTC")) for x in sib9]
    if enabled:
        checks["basic_only"] = bool(sib9) and all(x.find(".//referenceTimeInfo-r16") is None for x in sib9)
        if mode == "static":
            checks["static_value"] = bool(values) and all(v == STATIC_UTC for v in values)
        else:
            # A broad software observation envelope; not an RF timing-accuracy test.
            checks["system_epoch"] = bool(values) and all(abs(v / 100 - 2208988800 - time.time()) < 30 for v in values)
    return {"checks": checks, "time_info_utc": values, "sib9_xer": [ET.tostring(x, encoding="unicode") for x in sib9]}


def child_limits():
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    resource.setrlimit(resource.RLIMIT_MEMLOCK, (0, 0))


class Run:
    def __init__(self, args):
        self.args = args
        self.out = args.output.resolve()
        self.out.mkdir(parents=True, exist_ok=False)
        self.children = {}
        self.history = []
        self.observations = []
        self.network = None
        self.prefix = "sib9-" + uuid.uuid4().hex[:8]
        self.addresses = ["127.0.0.2", "127.0.0.3", "127.0.0.4"]
        self.image = None

    def setup(self):
        if self.args.container:
            self.image = json.loads(subprocess.check_output(["docker", "image", "inspect", self.args.image]))[0]["Id"]
            subnet = ipaddress.ip_network("172.30.92.0/24")
            ids = subprocess.check_output(["docker", "network", "ls", "-q"], text=True).split()
            nets = json.loads(subprocess.check_output(["docker", "network", "inspect", *ids])) if ids else []
            routes = json.loads(subprocess.check_output(["ip", "-j", "-4", "route"]))
            occupied = [x["Subnet"] for n in nets for x in n.get("IPAM", {}).get("Config", []) or [] if x.get("Subnet")]
            occupied += [r["dst"] for r in routes if r.get("dst") not in (None, "default")]
            if any(subnet.overlaps(ipaddress.ip_network(x)) for x in occupied if ":" not in x):
                raise RuntimeError("test subnet already occupied")
            self.network = self.prefix
            subprocess.run(["docker", "network", "create", "--internal", "--subnet", str(subnet), self.network], check=True)
            self.addresses = ["172.30.92." + str(i) for i in (2, 3, 4)]
        else:
            for address in self.addresses:
                with socket.socket(socket.AF_INET, socket.SOCK_STREAM, socket.IPPROTO_SCTP) as probe:
                    probe.bind((address, 38472))
            for port in (4043, 4044):
                with socket.socket() as probe:
                    probe.bind(("0.0.0.0", port))
        profiles = self.out / "profiles"
        profiles.mkdir()
        for name in ("cu.conf", "du-mcg.conf", "du-scg.conf"):
            text = (ROOT / "conf_files/sib9" / name).read_text()
            for old, new in zip(("127.0.0.2", "127.0.0.3", "127.0.0.4"), self.addresses):
                text = text.replace(old, new)
            if self.args.mode == "disabled" and name == "cu.conf":
                text = text.replace("cu_sibs = (2, 9);", "cu_sibs = (2);")
                text = re.sub(r"\s*sib9_config = \{[^}]*\};", "", text)
            if self.args.mode == "realtime":
                text = text.replace('timeSource="static"; timeInfoUTC=390898880000L;', 'timeSource="realtime";')
                text = text.replace('mode="static";', 'mode="realtime";')
            (profiles / name).write_text(text)
        if self.args.topology == "monolithic":
            text = (profiles / "du-mcg.conf").read_text().replace('tr_n_preference  = "f1";', 'tr_n_preference  = "local_RRC";')
            text = re.sub(r"\s*gNB_DU_ID\s*=\s*[^;]+;", "", text)
            cu = (profiles / "cu.conf").read_text()
            profile = re.search(r"cu_sibs = .*?;", cu)[0]
            if self.args.mode != "disabled":
                profile += "\n" + re.search(r"sib9_config = \{[^}]*\};", cu)[0]
            profile += '\n amf_ip_address = ({ipv4="127.0.0.5";});\n NETWORK_INTERFACES = {'
            profile += 'GNB_IPV4_ADDRESS_FOR_NG_AMF="127.0.0.6"; GNB_IPV4_ADDRESS_FOR_NGU="127.0.0.6"; GNB_PORT_FOR_S1U=2152;};'
            text = text.replace("gNB_ID = 0xe00;", "gNB_ID = 0xe00;\n" + profile)
            (profiles / "gnb.conf").write_text(text)

    def start(self, role, args, address=None):
        stream = (self.out / (role + ".log")).open("w")
        name = None
        if self.args.container:
            name = self.prefix + "-" + role
            cmd = ["docker", "run", "--name", name, "--network", self.network, "--ulimit", "memlock=0:0",
                   "--ulimit", "core=0:0", "-v", str(self.out / "profiles") + ":/profiles:ro"]
            if address:
                cmd += ["--ip", address]
            cmd += [self.image, "/oai/" + args[0], *args[1:]]
            proc = subprocess.Popen(cmd, stdout=stream, stderr=subprocess.STDOUT, start_new_session=True)
        else:
            cmd = [str(self.args.build.resolve() / args[0]), *args[1:]]
            cmd = [x.replace("/profiles/", str(self.out / "profiles") + "/") for x in cmd]
            proc = subprocess.Popen(cmd, cwd=self.args.build, env=dict(os.environ, LD_LIBRARY_PATH=str(self.args.build.resolve())),
                                    stdout=stream, stderr=subprocess.STDOUT, start_new_session=True, preexec_fn=child_limits)
        self.children[role] = (proc, stream, name)
        self.history.append({"role": role, "argv": cmd, "pid": proc.pid, "image": self.image})

    def stop(self, role):
        proc, stream, name = self.children[role]
        errors = []
        removed = name is None
        if name:
            try:
                subprocess.run(["docker", "stop", "--signal", "SIGINT", "--timeout", "8", name], check=True,
                               stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, timeout=20)
            except (OSError, subprocess.SubprocessError) as exc:
                errors.append(str(exc))
            try:
                subprocess.run(["docker", "rm", "-f", name], check=True,
                               stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, timeout=20)
                removed = True
            except (OSError, subprocess.SubprocessError) as exc:
                errors.append(str(exc))
        elif proc.poll() is None:
            os.killpg(proc.pid, signal.SIGINT)
        try:
            proc.wait(timeout=12)
        except subprocess.TimeoutExpired:
            os.killpg(proc.pid, signal.SIGKILL)
            proc.wait(timeout=12)
        finally:
            stream.close()
            row = next(x for x in reversed(self.history) if x["role"] == role)
            row["exit_code"] = proc.returncode
        if removed and proc.poll() is not None:
            del self.children[role]
        if errors:
            raise RuntimeError("; ".join(errors))

    def wait(self, condition, timeout=150):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if any(p.poll() is not None for p, _, _ in self.children.values()):
                raise RuntimeError("a child exited before reception completed")
            if condition():
                return
            time.sleep(1)
        raise RuntimeError("reception timed out")

    def acquire(self, phase):
        cells = [("mcg", 3948960000, 77, 4043, self.addresses[1], 917505)]
        if self.args.topology == "split":
            cells += [("scg", 3604800000, 78, 4044, self.addresses[2], 917506)]
        for name, freq, band, port, address, nci in cells:
            self.start(phase + "-" + name, ["nr-uesoftmodem", "--rfsim", "-r", "106", "--numerology", "1", "-C", str(freq),
                        "--band", str(band), "--rfsimulator.[0].serveraddr", address, "--rfsimulator.[0].serverport", str(port),
                        "--log_config.ASN1_debug", "1", "--thread-pool", "-1", "-E", "--uicc0.imsi", "001010000000001"])
            time.sleep(2)
        def received():
            return all(all(observe(self.out / (phase + "-" + name + ".log"), nci, self.args.mode)["checks"].values())
                       for name, _, _, _, _, nci in cells)
        self.wait(received)
        def ready():
            return all("Entering ITTI signals handler" in
                       (self.out / (phase + "-" + c[0] + ".log")).read_text(errors="replace") for c in cells)
        self.wait(ready)
        if self.args.mode == "disabled":
            time.sleep(3)
        observations = {name: observe(self.out / (phase + "-" + name + ".log"), nci, self.args.mode)
                        for name, _, _, _, _, nci in cells}
        self.observations.append(observations)
        for name, *_ in reversed(cells):
            self.stop(phase + "-" + name)
        if not all(all(x["checks"].values()) for x in observations.values()):
            raise RuntimeError("receiver checks failed")

    def execute(self):
        error = None
        try:
            self.setup()
            if self.args.configs_only:
                return
            if self.args.topology == "split":
                roles = [("cu", "cu.conf", self.addresses[0]), ("mcg", "du-mcg.conf", self.addresses[1]),
                         ("scg", "du-scg.conf", self.addresses[2])]
            else:
                roles = [("gnb", "gnb.conf", self.addresses[1])]
            for role, file, address in roles:
                self.start(role, ["nr-softmodem", "-O", "/profiles/" + file, "--rfsim", "--thread-pool", "-1", "-E"], address)
                time.sleep(2)
            self.acquire("first")
            time.sleep(2)
            self.acquire("second")
            if self.args.mode == "realtime":
                if not all(max(self.observations[1][cell]["time_info_utc"]) > max(self.observations[0][cell]["time_info_utc"])
                           for cell in self.observations[0]):
                    raise RuntimeError("UTC did not advance between fresh acquisitions")
        except BaseException as exc:
            error = type(exc).__name__ + ": " + str(exc)
        finally:
            for role in reversed(list(self.children)):
                try:
                    self.stop(role)
                except Exception as exc:
                    error = error or "cleanup failed: " + str(exc)
            if self.network:
                try:
                    subprocess.run(["docker", "network", "rm", self.network], check=True,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT, timeout=20)
                except Exception as exc:
                    error = error or "network cleanup failed: " + str(exc)
            if not self.args.configs_only:
                if any(x.get("exit_code") != 0 for x in self.history):
                    error = error or "a child did not exit cleanly"
                (self.out / "result.json").write_text(json.dumps({"mode": self.args.mode, "topology": self.args.topology,
                    "container": self.args.container, "error": error, "processes": self.history, "observations": self.observations,
                    "limits": ["independent SA receivers, not NR-DC activation",
                               "host estimate, not RF accuracy", "no AMF or data service"]}, indent=2) + "\n")
        if error:
            raise RuntimeError(error)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--build", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--mode", choices=("static", "realtime", "disabled"), default="static")
    p.add_argument("--topology", choices=("split", "monolithic"), default="split")
    p.add_argument("--container", action="store_true")
    p.add_argument("--image", default="sib9-basic:local")
    p.add_argument("--configs-only", action="store_true")
    args = p.parse_args()
    if args.container and args.configs_only:
        p.error("configuration generation does not require containers")
    def terminate(signum, frame):
        raise KeyboardInterrupt("termination requested")
    signal.signal(signal.SIGTERM, terminate)
    Run(args).execute()


if __name__ == "__main__":
    main()
