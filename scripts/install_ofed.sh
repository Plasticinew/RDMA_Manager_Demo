#!/bin/bash

sudo apt install libvirt-clients qemu qemu-kvm libvirt-daemon-system bridge-utils cmake -y

wget https://content.mellanox.com/ofed/MLNX_OFED-5.8-3.0.7.0/MLNX_OFED_LINUX-5.8-3.0.7.0-ubuntu22.04-x86_64.tgz

tar -zxvf MLNX_OFED_LINUX-5.8-3.0.7.0-ubuntu22.04-x86_64.tgz

cd MLNX_OFED_LINUX-5.8-3.0.7.0-ubuntu22.04-x86_64

sudo ./mlnxofedinstall --force

cd ../; sudo rm -rf MLNX_OFED_LINUX-5.8-3.0.7.0-ubuntu22.04-x86_64*

sudo sed -i "s/GRUB_CMDLINE_LINUX_DEFAULT=\"/GRUB_CMDLINE_LINUX_DEFAULT=\"iommu=pt/" /etc/default/grub

sudo grub-mkconfig -o /boot/grub/grub.cfg

sudo reboot
