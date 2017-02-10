#!/usr/bin/env python
# vi: sw=4 ts=4:

"""
    Mnemonic:   vfd_pre_start.py
    Abstract:   This script calls the 'dpdk_nic_bind' script to bind PF's and VF's to vfio-pci
    Date:       April 2016
    Author:     Dhanunjaya Naidu Ravada (dr3662@att.com)
    Mod:        2016 7 Apr - Created script
                2016 8 Apr - fix to index out of bound error
                2016 22 Apr - remove unloading ixgbevf driver
                2016 30 May - wait for vf's to create
                2017 10 Feb - update the code with parallel execution of VF's bind
"""

import subprocess
import json
import sys
import logging
from logging.handlers import RotatingFileHandler
import os
import time
import multiprocessing

VFD_CONFIG = '/etc/vfd/vfd.cfg'
SYS_DIR = "/sys/devices"
LOG_DIR = '/var/log/vfd'

# logging
log = logging.getLogger('vfd_pre_start')

def setup_logging(logfile):
    handler = RotatingFileHandler(os.path.join(LOG_DIR, logfile), maxBytes=200000, backupCount=20)
    log_formatter = logging.Formatter('%(asctime)s  %(process)s %(levelname)s %(name)s [-] %(message)s')
    handler.setFormatter(log_formatter)
    log.setLevel(logging.INFO)
    log.addHandler(handler)

def is_vfio_pci_loaded():
    try:
        subprocess.check_call('lsmod | grep vfio_pci >/dev/null', shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

# load vfio-pci module
def load_vfio_pci_driver():
    try:
        subprocess.check_call('modprobe vfio-pci', shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

# get pciids from vfd.cfg
def get_pciids():
    with open(VFD_CONFIG) as data_file:
        try:
            data = json.load(data_file)
        except ValueError:
            log.error("%s is not a valid json", VFD_CONFIG)
            sys.exit(1)
    return data['pciids']

# unbind pciid
def unbind_pfs(dev_id):
    unbind_cmd = "dpdk_nic_bind --force -u %s" % dev_id
    try:
        msg = subprocess.check_output(unbind_cmd, shell=True)
        log.info(unbind_cmd)
        if "Routing" in msg:
            log.error(msg)
            return False
        return True
    except subprocess.CalledProcessError:
        return False

def get_vfids(dev_id):
    cmd='find %s -name %s -type d | while read d; do echo "$d"; ls -l $d | grep virtfn| sed \'s!.*/!!\'; done' % (SYS_DIR, dev_id)
    vfids = subprocess.check_output(cmd, shell=True).split('\n')[1:]
    log.info("[%s]: %s", dev_id, vfids)
    return filter(None, vfids)

# bind pf's and vf's to vfio-pci
def bind_pf_vfs(dev_id):
    bind_cmd = "dpdk_nic_bind --force -b vfio-pci %s" % dev_id
    try:
        subprocess.check_call(bind_cmd, shell=True)
        log.info(bind_cmd)
        return True
    except subprocess.CalledProcessError:
        return False

# check whether vfio-pci driver is attached to pf or vf
def driver_attach(dev_id):
    index = 0
    cmd = 'lspci -k -s %s' % dev_id
    try:
        driver_name = subprocess.check_output(cmd, shell=True).splitlines()[2].split(':')[1].lstrip()
        if driver_name == 'vfio-pci':
            return [index, True]
        return [index, False]
    except IndexError:
        index = 1
        return [index, False]

# get the pci cards in the group which must be attached to the vfio-pci driver
def get_pciids_group(dev_id):
    group_pciids = []
    group_num = None
    cmd = "find /sys/kernel/iommu_groups -type l|grep %s | awk -F/ '{print $(NF-2)}'" % dev_id
    group_num = subprocess.check_output(cmd, shell=True)
    if group_num != None:
        cmd = "find /sys/kernel/iommu_groups -type l|grep groups.%s/" % int(group_num)
        list_pciids = subprocess.check_output(cmd, shell=True)
        for pciid in list_pciids.splitlines():
            group_pciids.append(pciid.split('/')[-1])
    return group_pciids

# get the vendor details
def check_vendor(pciids):
    not_ixgbe_vendor = []
    for pciid in pciids:
        cmd = "lspci -vm -s %s" % pciid
        try:
            vendor_name = subprocess.check_output(cmd, shell=True).splitlines()[2].split(':')[1].lstrip()
            if vendor_name == 'Intel Corporation':
                continue
            else:
                not_ixgbe_vendor.append(pciid)
        except IndexError:
            log.error("Not able to find valid vendor %s", pciid)
            sys.exit(1)
    return not_ixgbe_vendor

if __name__ == '__main__':
    setup_logging('vfd_upstart.log')

    pciids = []
    for value in get_pciids():
        if 'id' in value:
            pciids.append(value['id'])
        else:
            pciids.append(value)

    group_pciids = []
    for pciid in pciids:
        group_pciids = group_pciids + get_pciids_group(pciid)

    pciids = list(set(pciids) | set(group_pciids))
    log.info("pciids: %s", pciids)

#    for pciid in pciids:
#        vfs_count = len(get_vfids(pciid))
#        if vfs_count < 32:
#            log.error("It seems 32 VF's are not Created for this pf [%s] check 'dpdk_nic_bind --st'", pciid)
#            sys.exit(1)

    not_ixgbe_vendor = check_vendor(pciids)
    if len(not_ixgbe_vendor) > 0:
        log.error("VFD wont handle for this vendors: %s", not_ixgbe_vendor)
        sys.exit(1)

    if not is_vfio_pci_loaded():
        if load_vfio_pci_driver():
            log.info("Successfully loaded vfio-pci driver")
        else:
            log.error("unable to load vfio-pci driver")
            sys.exit(1)
    else:
        log.info("Already loaded vfio-pci driver")

    for pciid in pciids:
        status_list = driver_attach(pciid)
        if not status_list[1]:
            if status_list[0] == 0:
                if not unbind_pfs(pciid):
                    log.error("unable to unbind %s PF", pciid)
                    sys.exit(1)
                else:
                    log.info("Successfully unbind %s", pciid)
                    if not bind_pf_vfs(pciid):
                        log.error("unable to bind %s with vfio-pci", pciid)
                        sys.exit(1)
                    else:
                        log.info("Successfully bind %s", pciid)
        else:
            log.info("Already %s bind to vfio-pci driver", pciid)

    jobs = []
    for pciid in pciids:
        for vfid in get_vfids(pciid):
            return_list = driver_attach(vfid)
            if not return_list[1]:
                if return_list[0] == 0:
                    t = multiprocessing.Process(target=unbind_pfs, args=(vfid,))
                    jobs.append(t)
                    t.start()
    for job in jobs:
        job.join()

    for pciid in pciids:
        for vfid in get_vfids(pciid):
            return_list = driver_attach(vfid)
            if not return_list[1]:
                if return_list[0] == 0 or return_list[0] == 1:
                    bind_pf_vfs(vfid)
