#!/bin/bash

wget https://github.com/redis/hiredis/archive/master.zip
unzip master.zip
cd hiredis-master
make; sudo make install
sudo mkdir /usr/lib/hiredis
sudo cp /usr/local/lib/libhiredis.so.1.3.0 /usr/lib/hiredis/
sudo mkdir /usr/include/hiredis
sudo cp /usr/local/include/hiredis/hiredis.h /usr/include/hiredis/
sudo ldconfig
cd ..; rm -rf hiredis-master; rm -rf master.zip

sudo apt install libvirt-clients qemu qemu-kvm libvirt-daemon-system bridge-utils cmake -y

wget https://content.mellanox.com/ofed/MLNX_OFED-5.4-1.0.3.0/MLNX_OFED_LINUX-5.4-1.0.3.0-ubuntu20.04-x86_64.tgz

tar -zxvf MLNX_OFED_LINUX-5.4-1.0.3.0-ubuntu20.04-x86_64.tgz

cd MLNX_OFED_LINUX-5.4-1.0.3.0-ubuntu20.04-x86_64

sudo ./mlnxofedinstall --force

cd ../; sudo rm -rf MLNX_OFED_LINUX-5.4-1.0.3.0-ubuntu20.04-x86_64*

sudo sed -i "s/GRUB_CMDLINE_LINUX_DEFAULT=\"/GRUB_CMDLINE_LINUX_DEFAULT=\"iommu=pt/" /etc/default/grub

sudo grub-mkconfig -o /boot/grub/grub.cfg

sudo reboot
