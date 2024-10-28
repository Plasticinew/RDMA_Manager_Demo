#!/bin/bash

echo 4 > /sys/class/net/ens2f0/device/sriov_numvfs

./set_mac.sh 4 ens2f0

ip link set ens2f0v0 up
ifconfig ens2f0v0 10.10.1.14/24 up
ip link set ens2f0v1 up
ifconfig ens2f0v1 10.10.1.15/24 up
ip link set ens2f0v2 up
ifconfig ens2f0v2 10.10.1.16/24 up
ip link set ens2f0v3 up
ifconfig ens2f0v3 10.10.1.17/24 up