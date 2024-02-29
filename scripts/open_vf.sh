#!/bin/bash

echo 4 > /sys/class/net/enp65s0f0np0/device/sriov_numvfs

./set_mac.sh 4 enp65s0f0np0

ip link set enp65s0f0v0 up
ifconfig enp65s0f0v0 10.10.1.10/24 up
ip link set enp65s0f0v1 up
ifconfig enp65s0f0v0 10.10.1.11/24 up
ip link set enp65s0f0v2 up
ifconfig enp65s0f0v0 10.10.1.12/24 up
ip link set enp65s0f0v3 up
ifconfig enp65s0f0v0 10.10.1.13/24 up