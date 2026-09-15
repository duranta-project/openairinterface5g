#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-CSSL-1.0

import argparse
import mmap
import os
import struct

MAGIC = 0x4F41494E544E434C
VERSION = 1
RECORD = struct.Struct("=QIIQ")


def read_epoch(profile_path):
    with open(profile_path, encoding="ascii") as profile:
        for line in profile:
            if line.startswith("# start_unix_s="):
                return float(line.split("=", 1)[1])
    raise ValueError(f"{profile_path} has no # start_unix_s metadata")


def main():
    parser = argparse.ArgumentParser(description="Initialize the shared NTN sample clock before starting OCUDU")
    parser.add_argument("profile", help="NTN channel profile CSV")
    parser.add_argument("--name", default="/oai_ntn_sim_clock", help="POSIX shared-memory name")
    args = parser.parse_args()

    epoch_ns = round(read_epoch(args.profile) * 1e9)
    path = "/dev/shm/" + args.name.lstrip("/")
    try:
        fd = os.open(path, os.O_RDWR)
    except FileNotFoundError:
        try:
            fd = os.open(path, os.O_CREAT | os.O_EXCL | os.O_RDWR, 0o666)
        except FileExistsError:
            fd = os.open(path, os.O_RDWR)
    try:
        os.fchmod(fd, 0o666)
        os.ftruncate(fd, RECORD.size)
        with mmap.mmap(fd, RECORD.size) as shared:
            shared[:] = RECORD.pack(MAGIC, VERSION, 0, epoch_ns)
            shared.flush()
    finally:
        os.close(fd)
    print(f"Initialized {args.name} at Unix {epoch_ns / 1e9:.6f} s")


if __name__ == "__main__":
    main()
