#!/bin/bash
# GH200 / BlueField-3: prepare fronthaul VFs on a SINGLE PF (first BF3 card).
#
# Internal DU<->RU forwarding needs both endpoints on the SAME PF's eSwitch
# (legacy eswitch has no internal path between PFs). The fronthaul VFs live on
# aerial00 / 0000:01:00.0; the NIC forwards VF<->PF and VF<->VF by dest MAC.
#
#   aerial00 / 0000:01:00.0 / mlx5_0 / /dev/mst/mt41692_pciconf0
#     VF0 = OAI DU U-plane   VF1 = OAI DU C-plane   (hardware port-VLANs, VST)
#     VF2 = RU U-plane       VF3 = RU C-plane        (hardware port-VLANs, VST)
#     VF4 = Aerial cuBB DU   (trunk, no VST; cuBB software-tags VLAN 3 itself)
#   UE behind O-RU via vrtsim
#
# Two DIFFERENT DUs use this fronthaul, one at a time; they terminate VLANs differently:
#   * OAI built-in-L1 DU: binds VF0/VF1, sends UNTAGGED, relies on VF hardware port-VLANs
#     (VST). xran sets up_vlan_tag/cp_vlan_tag = 0. du_addr = OAI_DU_MAC.
#   * NVIDIA Aerial cuBB DU: binds the trunk VF4 and VLAN-tags in software (cuphycontroller
#     vlan: 3), like the field cuphycontroller configs. cuBB's RX flow matches the FULL 802.1Q
#     tci (PCP included), and the RU VF's VST inserts the UL tag with PCP 0 by default — so
#     cuphycontroller must use pcp: 0 (tci 0x0003) to match, else UL leaks to the kernel netdev
#     (uplane_rx stays 0). VF4 is a trunk so cuBB's self-tag isn't doubled. du_addr = AERIAL_DU_MAC.
#     (cuBB can NOT use the PF here: legacy VEB does not loop VF->PF-MAC traffic internally, so
#      the O-RU's UL never reaches a PF-bound cuBB. A same-PF VF works via VF<->VF forwarding.)
#
# Each role uses a DISTINCT locally-administered MAC so the eSwitch can steer between them and
# the checked-in configs stay host-independent (no burned-in PF MAC).
#
# Mellanox VFs must use mlx5_core for DPDK (NOT vfio-pci). See doc/ORAN_FHI7.2_Tutorial.md.
#
# After running, confirm BDFs and update conf/*.gh200.conf dpdk_devices / *_addr if needed.

set -eu

U_VLAN=3
C_VLAN=4
MTU=9600
DRIVER=mlx5_core

# Single PF for all fronthaul VFs.
PCI_PF=0000:01:00.0
IF_PF=aerial00
MST_PF=/dev/mst/mt41692_pciconf0
# Locally administered / distinct unicast MACs for the three roles. All are made up
# (02: prefix => locally administered) so the checked-in configs are host-independent.
# Override via env and update the confs if you change them.
OAI_DU_MAC="${OAI_DU_MAC:-02:00:00:00:00:0a}"
AERIAL_DU_MAC="${AERIAL_DU_MAC:-02:00:00:00:00:1a}"
RU_MAC="${RU_MAC:-02:00:00:00:00:7b}"

DPDK_DEVBIND="${DPDK_DEVBIND:-/usr/local/bin/dpdk-devbind.py}"
if [ ! -x "$DPDK_DEVBIND" ]; then
  DPDK_DEVBIND=$(command -v dpdk-devbind.py || true)
fi

list_vfs() {
  local pf="$1"
  for v in "/sys/bus/pci/devices/${pf}"/virtfn*; do
    [ -e "$v" ] || continue
    basename "$(readlink -f "$v")"
  done | sort
}

bind_mlx5() {
  local bdf="$1"
  sudo "$DPDK_DEVBIND" --unbind "${bdf#0000:}" 2>/dev/null || \
    sudo "$DPDK_DEVBIND" --unbind "$bdf" 2>/dev/null || true
  sudo "$DPDK_DEVBIND" --bind "$DRIVER" "${bdf#0000:}" || \
    sudo "$DPDK_DEVBIND" --bind "$DRIVER" "$bdf"
}

OAI_DU_MAC=$(printf '%s' "$OAI_DU_MAC" | tr 'A-F' 'a-f')
AERIAL_DU_MAC=$(printf '%s' "$AERIAL_DU_MAC" | tr 'A-F' 'a-f')
RU_MAC=$(printf '%s' "$RU_MAC" | tr 'A-F' 'a-f')

if [ "$OAI_DU_MAC" = "$AERIAL_DU_MAC" ] || [ "$OAI_DU_MAC" = "$RU_MAC" ] || [ "$AERIAL_DU_MAC" = "$RU_MAC" ]; then
  echo "ERROR: OAI_DU_MAC, AERIAL_DU_MAC, and RU_MAC must be distinct." >&2
  echo "  OAI_DU_MAC=${OAI_DU_MAC} AERIAL_DU_MAC=${AERIAL_DU_MAC} RU_MAC=${RU_MAC}" >&2
  exit 1
fi

# Same MAC on U and C VFs of a role; hardware VLAN tags separate the planes.
OAI_DU_U_PLANE_MAC=$OAI_DU_MAC
OAI_DU_C_PLANE_MAC=$OAI_DU_MAC
RU_U_PLANE_MAC=$RU_MAC
RU_C_PLANE_MAC=$RU_MAC

echo "=== Single-PF fronthaul (all VFs on ${IF_PF} / ${PCI_PF}) ==="
echo "OAI DU mac=${OAI_DU_MAC}   Aerial DU mac=${AERIAL_DU_MAC}   RU mac=${RU_MAC}"
ip a show dev "$IF_PF"

sudo ethtool -G "$IF_PF" rx 8160 tx 8160 2>/dev/null || true
sudo ip link set "$IF_PF" up

# Recreate 5 VFs on the single PF (VF0/1 OAI DU, VF2/3 O-RU, VF4 Aerial cuBB DU).
echo 0 | sudo tee "/sys/bus/pci/devices/${PCI_PF}/sriov_numvfs" >/dev/null
echo 5 | sudo tee "/sys/bus/pci/devices/${PCI_PF}/sriov_numvfs" >/dev/null

sudo modprobe mlx5_core
sudo modprobe mlx5_ib 2>/dev/null || true

# VF0/1 = OAI DU U/C (VST), VF2/3 = RU U/C (VST): app sends untagged, NIC tags/strips.
# The RU VF VST inserts the UL tag with PCP 0 (default qos), which cuphycontroller matches
# via pcp: 0. VF4 = Aerial cuBB DU as a TRUNK (no VST): cuBB software-tags VLAN 3 itself, and
# trust on lets the trunk VF accept the O-RU's VLAN-tagged UL ingress.
sudo ip link set "$IF_PF" vf 0 mac "$OAI_DU_U_PLANE_MAC" vlan "$U_VLAN" spoofchk off mtu "$MTU"
sudo ip link set "$IF_PF" vf 1 mac "$OAI_DU_C_PLANE_MAC" vlan "$C_VLAN" spoofchk off mtu "$MTU"
sudo ip link set "$IF_PF" vf 2 mac "$RU_U_PLANE_MAC" vlan "$U_VLAN" spoofchk off mtu "$MTU"
sudo ip link set "$IF_PF" vf 3 mac "$RU_C_PLANE_MAC" vlan "$C_VLAN" spoofchk off mtu "$MTU"
sudo ip link set "$IF_PF" vf 4 mac "$AERIAL_DU_MAC" spoofchk off trust on mtu "$MTU"

sleep 1

mapfile -t PF_VFS < <(list_vfs "$PCI_PF")
SRIOV_COUNT="${#PF_VFS[@]}"
echo "expect 5 SR-IOV: ${SRIOV_COUNT}"

if [ "$SRIOV_COUNT" -ne 5 ]; then
  echo "ERROR: expected 5 VFs on ${PCI_PF}; got ${SRIOV_COUNT}" >&2
  echo "VFs: ${PF_VFS[*]:-none}" >&2
  exit 1
fi

VF_OAI_DU_U="${PF_VFS[0]}"
VF_OAI_DU_C="${PF_VFS[1]}"
VF_RU_U="${PF_VFS[2]}"
VF_RU_C="${PF_VFS[3]}"
VF_AERIAL_DU="${PF_VFS[4]}"

for bdf in "$VF_OAI_DU_U" "$VF_OAI_DU_C" "$VF_RU_U" "$VF_RU_C" "$VF_AERIAL_DU"; do
  bind_mlx5 "$bdf"
done

echo "=== Topology ==="
echo "All on ${IF_PF} (${PCI_PF} ${MST_PF}) — internal VF<->VF forwarding, no external switch"
echo "OAI DU  : U=${VF_OAI_DU_U} C=${VF_OAI_DU_C} mac=${OAI_DU_MAC} VLAN ${U_VLAN}/${C_VLAN} (HW-tagged, VST)"
echo "Aerial  : DU=${VF_AERIAL_DU} mac=${AERIAL_DU_MAC} trunk/no-VST (cuBB software-tags VLAN ${U_VLAN}, pcp 0)"
echo "O-RU    : U=${VF_RU_U} C=${VF_RU_C} mac=${RU_MAC} VLAN ${U_VLAN}/${C_VLAN} (HW-tagged, VST)"
echo "UE behind O-RU via vrtsim (shared /run/vrtsim)"
echo "Fronthaul: same-PF VFs; destination MAC selects OAI DU, Aerial DU, or O-RU endpoint."
echo ""
echo "The conf/*.gh200*.conf and conf/cuphycontroller_P5G_ASET.yaml addresses are already set to the"
echo "MACs/BDFs above (all locally-administered). Only edit them if you override the *_MAC vars."
echo ""
echo "=== DPDK device status (expect mlx5_core on VFs) ==="
sudo "$DPDK_DEVBIND" --status | grep -E "${VF_OAI_DU_U#0000:}|${VF_OAI_DU_C#0000:}|${VF_RU_U#0000:}|${VF_RU_C#0000:}|${VF_AERIAL_DU#0000:}" || true
