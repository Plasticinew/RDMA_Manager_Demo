
qemu-img create -f qcow2 disk.qcow2 16G

sudo apt install libvirt-clients qemu qemu-kvm libvirt-daemon-system bridge-utils

sudo systemctl enable libvirtd
sudo systemctl start libvirtd

sudo virt-install --virt-type kvm --name vm --ram 4096 --vcpus 2 \
    --location ubuntu-22.04.4-live-server-amd64.iso,initrd=casper/initrd,kernel=casper/vmlinuz \
    --disk disk.qcow2,format=qcow2 --network network=default --hostdev=0000:41:00.2 \
    --graphics=none --console=pty,target_type=serial --extra-args="console=tty0 console=ttyS0" \
    --os-type=linux --os-variant=ubuntu22.04