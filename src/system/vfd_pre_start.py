#!/usr/bin/env python
# vi: sw=4 ts=4:

"""
    Mnemonic:       vfd_pre_start.py
    Abstract:       This script calls the 'dpdk_nic_bind' script to bind PF's and VF's to vfio-pci
    Date:           April 2016
    Author:         Dhanunjaya Naidu Ravada (dr3662@att.com)
    Mod:            2016 7 Apr - Created script
                    2016 8 Apr - fix to index out of bound error
                    2016 22 Apr - remove unloading ixgbevf driver
                    2016 30 May - wait for vf's to create
"""

import subprocess
import json
import sys
import logging
from logging.handlers import RotatingFileHandler
import os
import time

VFD_CONFIG = '/etc/vfd/vfd.cfg'
SYS_DIR = "/sys/devices"
LOG_DIR = '/var/log/vfd'
    
# global pciids list
pciids = []
# global group pciids list
group_pciids = []
# To catch index error
index = 0


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
        return Flase

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
    unbind_cmd = 'dpdk_nic_bind --force -u %s' % dev_id
    log.info(unbind_cmd)
    try:
        msg = subprocess.check_output(unbind_cmd, shell=True)
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
    bind_cmd = 'dpdk_nic_bind --force -b vfio-pci %s' % dev_id
    log.info(bind_cmd)
    try:
        subprocess.check_call(bind_cmd, shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

# check whether vfio-pci driver is attached to pf or vf
def driver_attach(dev_id):
    global index
    index = 0
    cmd = 'lspci -k -s %s' % dev_id
    try:
        driver_name = subprocess.check_output(cmd, shell=True).splitlines()[2].split(':')[1].lstrip()
        if driver_name == 'vfio-pci':
            return True
        return False
    except IndexError:
        index = 1
        return False

# get the pci cards in the group which must be attached to the vfio-pci driver
def get_pciids_group(dev_id):
    global group_pciids
    group_num = None
    cmd = "find /sys/kernel/iommu_groups -type l|grep %s | awk -F/ '{print $(NF-2)}'" % dev_id
    group_num = subprocess.check_output(cmd, shell=True)
    if group_num != None:
        cmd = "find /sys/kernel/iommu_groups -type l|grep groups.%s" % group_num
        list_pciids = subprocess.check_output(cmd, shell=True)
        for pciid in list_pciids.splitlines():
            group_pciids.append(pciid.split('/')[-1])

# get the vendor details
def check_vendor():
    global pciids
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

def get_configured_vfs(pciids):
    vfd_count = 0
    for pciid in pciids:
        vfd_count = vfd_count + len(get_vfids(pciid))
    return vfd_count

def main():
    global pciids
    global group_pciids
    global index

    for value in get_pciids():
        if 'id' in value:
            pciids.append(value['id'])
        else:
            pciids.append(value)

    for pciid in pciids:
        get_pciids_group(pciid)

    pciids = list(set(pciids) | set(group_pciids))
    log.info("pciids: %s", pciids)
    
    vfs_count = get_configured_vfs(pciids)
    if vfs_count == 0:
        log.error("It seems VF's are not Created, check 'dpdk_nic_bind --st'")
        sys.exit(1)

    not_ixgbe_vendor = check_vendor()
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
        if not driver_attach(pciid):
            if index == 0:
                if not unbind_pfs(pciid):
                    log.error("unable to unbind %s PF", pciid)
                    sys.exit(1)
                else:
                    log.info("Successfully unbind %s", pciid)
        else:
            log.info("Already %s bind to vfio-pci driver", pciid)

    for pciid in pciids:
        if not bind_pf_vfs(pciid):
            log.error("unable to bind %s with vfio-pci", pciid)
            sys.exit(1)
        else:
            log.info("Successfully bind %s", pciid)
        for vfid in get_vfids(pciid):
            if not driver_attach(vfid):
                if index == 0:
                    if not unbind_pfs(vfid):
                        log.error("unable to unbind %s VF", vfid)
                        sys.exit(1)
                    else:
                        log.info("Successfully unbind %s", vfid)
                    if not bind_pf_vfs(vfid):
                        log.error("unbale to bind %s with vfio-pci", vfid)
                        sys.exit(1)
                    else:
                        log.info("Successfully bind %s", vfid)
                if index == 1:
                    if not bind_pf_vfs(vfid):
                        log.error("unbale to bind %s with vfio-pci", vfid)
                        sys.exit(1)
                    else:
                        log.info("Successfully bind %s", vfid)
            else:
                log.info("Already %s bind to vfio-pci driver", vfid)

if __name__ == '__main__':
    setup_logging('vfd_upstart.log')
    main()
