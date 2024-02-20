#!/bin/bash

ip link set enp129s0f0v0 up
ifconfig enp129s0f0v0 10.10.1.10/24 up
ip link set enp129s0f0v1 up
ifconfig enp129s0f0v0 10.10.1.11/24 up
ip link set enp129s0f0v2 up
ifconfig enp129s0f0v0 10.10.1.12/24 up
ip link set enp129s0f0v3 up
ifconfig enp129s0f0v0 10.10.1.13/24 up