#!/bin/bash



PCIID="0000:07:00.0 0000:07:00.1"

MOD=vfio
DRV=vfio-pci
NUM_VFS=4

modprobe $MOD
modprobe $DRV


for i in $PCIID
do

echo ${NUM_VFS} > /sys/bus/pci/devices/${i}/sriov_numvfs 
/home/alexz/dpdk/tools/dpdk_nic_bind.py -b ${DRV} $i 

done 
