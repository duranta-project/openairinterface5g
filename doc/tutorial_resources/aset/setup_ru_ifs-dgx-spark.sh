#!/bin/bash
# DGX Spark / ConnectX-7: prepare five fronthaul VFs on one physical port.
#
# The Spark has one dual-port ConnectX-7. Each physical port is exposed through
# two PCIe roots, so four netdevs represent two interfaces:
#   port 0: aerial00 (0000:01:00.0) and aerial02 (0002:01:00.0)
#   port 1: aerial01 (0000:01:00.1) and aerial03 (0002:01:00.1)
# Create the VFs on only one PCIe function. The default is aerial00. Because
# port 1 occupies function .1, its five VFs are 0000:01:00.2 through .6:
#   VF0/1 = OAI DU U/C (VST), VF2/3 = O-RU U/C (VST), VF4 = Aerial DU (trunk).

set -euo pipefail

U_VLAN="${U_VLAN:-3}"
C_VLAN="${C_VLAN:-4}"
MTU="${MTU:-9600}"
PCI_PF="${PCI_PF:-0000:01:00.0}"
OAI_DU_MAC="${OAI_DU_MAC:-02:00:00:00:00:0a}"
AERIAL_DU_MAC="${AERIAL_DU_MAC:-02:00:00:00:00:1a}"
RU_MAC="${RU_MAC:-02:00:00:00:00:7b}"

if [ "$EUID" -ne 0 ]; then
  echo "ERROR: run this script with sudo." >&2
  exit 1
fi

PF_SYSFS="/sys/bus/pci/devices/${PCI_PF}"
if [ ! -d "$PF_SYSFS" ]; then
  echo "ERROR: PCI PF ${PCI_PF} does not exist." >&2
  exit 1
fi

IF_PF=$(basename "$(find "$PF_SYSFS/net" -mindepth 1 -maxdepth 1 -print -quit 2>/dev/null || true)")
if [ -z "$IF_PF" ]; then
  echo "ERROR: ${PCI_PF} has no network interface." >&2
  exit 1
fi

TOTAL_VFS=$(cat "$PF_SYSFS/sriov_totalvfs")
if [ "$TOTAL_VFS" -lt 5 ]; then
  echo "ERROR: ${PCI_PF} supports only ${TOTAL_VFS} VFs; five are required." >&2
  exit 1
fi

for mac in OAI_DU_MAC AERIAL_DU_MAC RU_MAC; do
  value=$(printf '%s' "${!mac}" | tr 'A-F' 'a-f')
  if ! printf '%s\n' "$value" | grep -Eq '^([0-9a-f]{2}:){5}[0-9a-f]{2}$'; then
    echo "ERROR: ${mac}=${value} is not a valid MAC address." >&2
    exit 1
  fi
  printf -v "$mac" '%s' "$value"
done

if [ "$OAI_DU_MAC" = "$AERIAL_DU_MAC" ] || [ "$OAI_DU_MAC" = "$RU_MAC" ] ||
   [ "$AERIAL_DU_MAC" = "$RU_MAC" ]; then
  echo "ERROR: OAI_DU_MAC, AERIAL_DU_MAC, and RU_MAC must be distinct." >&2
  exit 1
fi

list_vfs() {
  local vf
  for vf in "$PF_SYSFS"/virtfn*; do
    [ -e "$vf" ] || continue
    basename "$(readlink -f "$vf")"
  done | sort -V
}

PORT_NAME=$(cat "/sys/class/net/${IF_PF}/phys_port_name")
SWITCH_ID=$(cat "/sys/class/net/${IF_PF}/phys_switch_id")
echo "=== DGX Spark fronthaul: ${IF_PF} / ${PCI_PF}, physical port ${PORT_NAME} ==="
for candidate in /sys/class/net/*; do
  name=${candidate##*/}
  [ "$name" = "$IF_PF" ] && continue
  [ "$(cat "$candidate/phys_switch_id" 2>/dev/null || true)" = "$SWITCH_ID" ] || continue
  [ "$(cat "$candidate/phys_port_name" 2>/dev/null || true)" = "$PORT_NAME" ] || continue
  echo "Same physical port is also visible as ${name}; leave that function unused."
done
ip link show dev "$IF_PF"
ethtool -G "$IF_PF" rx 8160 tx 8160 2>/dev/null || true
ip link set "$IF_PF" up

# Recreate the VFs so their indices and BDF ordering are deterministic.
echo 0 > "$PF_SYSFS/sriov_numvfs"
if command -v devlink >/dev/null; then
  devlink dev eswitch set "pci/${PCI_PF}" mode legacy
fi
echo 5 > "$PF_SYSFS/sriov_numvfs"
modprobe mlx5_core
modprobe mlx5_ib 2>/dev/null || true

ip link set "$IF_PF" vf 0 mac "$OAI_DU_MAC" vlan "$U_VLAN" spoofchk off mtu "$MTU"
ip link set "$IF_PF" vf 1 mac "$OAI_DU_MAC" vlan "$C_VLAN" spoofchk off mtu "$MTU"
ip link set "$IF_PF" vf 2 mac "$RU_MAC" vlan "$U_VLAN" spoofchk off mtu "$MTU"
ip link set "$IF_PF" vf 3 mac "$RU_MAC" vlan "$C_VLAN" spoofchk off mtu "$MTU"
ip link set "$IF_PF" vf 4 mac "$AERIAL_DU_MAC" spoofchk off trust on mtu "$MTU"

mapfile -t PF_VFS < <(list_vfs)
if [ "${#PF_VFS[@]}" -ne 5 ]; then
  echo "ERROR: expected five VFs on ${PCI_PF}; found ${#PF_VFS[@]}." >&2
  exit 1
fi

for bdf in "${PF_VFS[@]}"; do
  if [ ! -e "/sys/bus/pci/devices/${bdf}/driver" ]; then
    echo "$bdf" > /sys/bus/pci/drivers_probe
  fi
  driver=$(basename "$(readlink -f "/sys/bus/pci/devices/${bdf}/driver")")
  if [ "$driver" != "mlx5_core" ]; then
    echo "ERROR: ${bdf} uses ${driver}, expected mlx5_core for the mlx5 DPDK PMD." >&2
    exit 1
  fi
done

echo "=== Topology ==="
echo "OAI DU : U=${PF_VFS[0]} C=${PF_VFS[1]} mac=${OAI_DU_MAC} VLAN ${U_VLAN}/${C_VLAN}"
echo "O-RU   : U=${PF_VFS[2]} C=${PF_VFS[3]} mac=${RU_MAC} VLAN ${U_VLAN}/${C_VLAN}"
echo "Aerial : DU=${PF_VFS[4]} mac=${AERIAL_DU_MAC} trunk (software VLAN ${U_VLAN})"
echo
echo "Expected by the checked-in DGX Spark configs:"
echo "  OAI DU ${PF_VFS[0]} ${PF_VFS[1]}; O-RU ${PF_VFS[2]} ${PF_VFS[3]}; Aerial ${PF_VFS[4]}"
if [ "${PF_VFS[*]}" != "0000:01:00.2 0000:01:00.3 0000:01:00.4 0000:01:00.5 0000:01:00.6" ]; then
  echo "WARNING: VF BDFs differ from the checked-in configs; update their dpdk_devices/nic values."
fi
