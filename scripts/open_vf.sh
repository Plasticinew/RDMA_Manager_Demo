#!/bin/bash

echo 4 > /sys/class/net/ens1f0/device/sriov_numvfs

./set_mac.sh 4 ens1f0

ip link set ens1f0v0 up
ifconfig ens1f0v0 10.10.1.2/24 up
ip link set ens1f0v1 up
ifconfig ens1f0v1 10.10.1.3/24 up
ip link set ens1f0v2 up
ifconfig ens1f0v2 10.10.1.4/24 up
ip link set ens1f0v3 up
ifconfig ens1f0v3 10.10.1.5/24 up