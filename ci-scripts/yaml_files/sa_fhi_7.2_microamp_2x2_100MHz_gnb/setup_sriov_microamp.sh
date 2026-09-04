#!/bin/sh
# SPDX-License-Identifier: MIT

set -eu

IF_NAME=ens2f1
# O-DU C/U-plane MAC address and VLAN, has to match the "du-mac" and
# "ecpri-vlan-tag" settings of the Microamp RU
C_U_PLANE_MAC_ADDR=50:7C:6F:31:00:61
C_U_PLANE_VLAN=600
# MTU has to match "mtu" in the fhi_72 section of the gNB configuration file
MTU=9216
NUM_VFs=1
# has to match "dpdk_devices" in the fhi_72 section of the gNB configuration file
C_U_PLANE_PCI=41:11.0
DPDK_DEVBIND_PREFIX=/usr/local/bin

ethtool -G $IF_NAME rx 8160
ethtool -G $IF_NAME tx 8160
sh -c "echo 0 > /sys/class/net/$IF_NAME/device/sriov_numvfs"
sh -c "echo $NUM_VFs > /sys/class/net/$IF_NAME/device/sriov_numvfs"
modprobe -r iavf
modprobe iavf
# one VF is used for both C- and U-planes
ip link set $IF_NAME vf 0 mac $C_U_PLANE_MAC_ADDR vlan $C_U_PLANE_VLAN spoofchk off mtu $MTU
sleep 1
${DPDK_DEVBIND_PREFIX}/dpdk-devbind.py --unbind $C_U_PLANE_PCI
modprobe vfio-pci
${DPDK_DEVBIND_PREFIX}/dpdk-devbind.py --bind vfio-pci $C_U_PLANE_PCI
echo "Successfully configured C-PLANE and U-PLANE:
  - C/U-PLANE MAC: $C_U_PLANE_MAC_ADDR, VLAN: $C_U_PLANE_VLAN, PCI: $C_U_PLANE_PCI"
exit 0
