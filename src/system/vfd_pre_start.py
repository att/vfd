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
                2017 20 Feb - update script to bind pfs to igb_uio
                2017 23 Feb - update script to bind pfs to vfio-pci in older AIC env
                2017 23 Jun - update script to bind pfs to vfio-pci in all env
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

# global variable which will hold pciid path
file_path = ""

# logging
log = logging.getLogger('vfd_pre_start')

def setup_logging(logfile):
    handler = RotatingFileHandler(os.path.join(LOG_DIR, logfile), maxBytes=200000, backupCount=20)
    log_formatter = logging.Formatter('%(asctime)s  %(process)s %(levelname)s %(name)s %(lineno)d [-] %(message)s')
    handler.setFormatter(log_formatter)
    log.setLevel(logging.INFO)
    log.addHandler(handler)

def is_pci_stub_loaded():
    try:
        subprocess.check_call('lsmod | grep pci_stub >/dev/null', shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

def load_pci_stub():
    try:
        subprocess.check_call('modprobe pci-stub', shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

def is_uio_loaded():
    try:
        subprocess.check_call('lsmod | grep uio >/dev/null', shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

def load_uio():
    try:
        subprocess.check_call('modprobe uio', shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

def is_igb_uio_loaded():
    try:
        subprocess.check_call('lsmod | grep igb_uio >/dev/null', shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

def load_igb_uio():
    pass

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

# get config data as json object
def get_config():
    with open(VFD_CONFIG) as data_file:
        try:
            data = json.load(data_file)
        except ValueError:
            log.error("%s is not a valid json", VFD_CONFIG)
            sys.exit(1)
    return data

# get pciids from vfd.cfg (depricated)
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

# unbind vfs
def unbind_vfs(vfs_list):
    unbind_cmd = "dpdk_nic_bind --force -u %s" % ' '.join(vfs_list)
    try:
        subprocess.check_call(unbind_cmd, shell=True)
        log.info(unbind_cmd)
        return True
    except subprocess.CalledProcessError:
        return False

def get_vfids(dev_id):
    cmd='find %s -name %s -type d | while read d; do echo "$d"; ls -l $d | grep virtfn| sed \'s!.*/!!\'; done' % (SYS_DIR, dev_id)
    vfids = subprocess.check_output(cmd, shell=True).split('\n')[1:]
    return filter(None, vfids)

# bind pf's to igb_uio
def bind_pf(dev_id, pf_driver):
    bind_cmd = "dpdk_nic_bind --force -b %s %s" % (pf_driver, dev_id)
    try:
        subprocess.check_call(bind_cmd, shell=True)
        log.info(bind_cmd)
        return True
    except subprocess.CalledProcessError:
        return False

# bind vf's to vfio-pci
def bind_vfs(vfs_list):
    bind_cmd = "dpdk_nic_bind --force -b vfio-pci %s" % ' '.join(vfs_list)
    try:
        subprocess.check_call(bind_cmd, shell=True)
        log.info(bind_cmd)
        return True
    except subprocess.CalledProcessError:
        return False

# check whether pf driver is attached to pf
def driver_attach_pf(dev_id, pf_driver):
    index = 0
    cmd = 'lspci -k -s %s' % dev_id
    try:
        driver_name = subprocess.check_output(cmd, shell=True).splitlines()[2].split(':')[1].lstrip()
        if driver_name == pf_driver:
            return [index, True]
        return [index, False]
    except IndexError:
        index = 1
        return [index, False]

# check whether vfio-pci driver is attached to vf
def driver_attach_vf(dev_id):
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
    not_intel_broadcom_vendor = []
    for pciid in pciids:
        cmd = "lspci -vm -s %s" % pciid
        try:
            vendor_name = subprocess.check_output(cmd, shell=True).splitlines()[2].split(':')[1].lstrip()
            if vendor_name == 'Intel Corporation' or vendor_name == "Broadcom Corporation":
                continue
            else:
                not_intel_broadcom_vendor.append(pciid)
        except IndexError:
            log.error("Not able to find valid vendor %s", pciid)
            sys.exit(1)
    return not_intel_broadcom_vendor

def get_vfs_path(dev_id):
    global file_path
    cmd = "find /sys -name max_vfs | grep %s" % dev_id
    log.info(cmd)
    try:
        file_path = subprocess.check_output(cmd, shell=True)
        log.info(file_path)
        return True
    except subprocess.CalledProcessError:
        log.info(file_path)
        return False

def create_vfs(dev_id, vfs):
    global file_path
    get_vfs_path(dev_id)
    cmd = "echo %s > %s" % (str(vfs), file_path)
    log.info(cmd)
    try:
        subprocess.check_call(cmd, shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

def reset_vfs(dev_id):
    global file_path
    get_vfs_path(dev_id)
    vfs = 0
    cmd = "echo %s > %s" % (str(vfs), file_path)
    log.info(cmd)
    try:
        subprocess.check_call(cmd, shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

def main():
    setup_logging('vfd_upstart.log')

    data = get_config()
    pciids_obj = []
    if 'pciids' in data:
        pciids_obj = data['pciids']

    real_pciids = []
    for value in pciids_obj:
        if 'id' in value:
            real_pciids.append(value['id'])

    group_pciids = []
    for pciid in real_pciids:
        group_pciids = group_pciids + get_pciids_group(pciid)

    # include iommu group pciids to bind
    group_pciids = list(set(real_pciids) | set(group_pciids))
    log.info("pciids: %s", group_pciids)

    not_intel_broadcom_vendor = check_vendor(real_pciids)
    if len(not_intel_broadcom_vendor) > 0:
        log.error("VFD wont handle for this vendors: %s", not_intel_broadcom_vendor)
        sys.exit(1)

    if not is_uio_loaded():
        if load_uio():
            log.info("Successfully loaded uio driver")
        else:
            log.error("unable to load uio driver")
            sys.exit(1)

    if not is_igb_uio_loaded():
        log.error("please load dpdk igb_uio driver")
        sys.exit(1)

    if not is_vfio_pci_loaded():
        if load_vfio_pci_driver():
            log.info("Successfully loaded vfio-pci driver")
        else:
            log.error("unable to load vfio-pci driver")
            sys.exit(1)

    if not is_pci_stub_loaded():
        if load_pci_stub():
            log.info("Successfully loaded pci-stub driver")
        else:
            log.error("unable to load pci-stub driver")
            sys.exit(1)
    
    pf_driver = "vfio-pci"
    vf_driver = "vfio-pci"
    for obj in pciids_obj:
        if 'pf_driver' in obj:
            pf_driver = obj['pf_driver']
        if 'vf_driver' in obj:
            vf_driver = obj['vf_driver']

        pciid = None
        if 'id' in obj:
            pciid = obj['id']

        vfs_count = None
        if 'vfs_count' in obj:
            vfs_count = obj['vfs_count']

        if pciid in real_pciids and pciid in group_pciids:
            group_pciids.remove(pciid)
            status_list = driver_attach_pf(pciid, pf_driver)
            if status_list[1] and (vfs_count == len(get_vfids(pciid))):
                if not bind_pf(pciid, pf_driver):
                    log.error("unable to bind %s with %s", pciid, pf_driver)
                    sys.exit(1)
                else:
                    log.info("Successfully bind to %s", pciid)
            elif status_list[1] and (len(get_vfids(pciid)) < vfs_count):
                if reset_vfs(pciid):
                    if not create_vfs(pciid, vfs_count):
                        log.error("not able to create vfs for pf %s", pciid)
                        sys.exit(1)
                    else:
                        log.info("Successfully created vfs for pf: %s", pciid)
                else:
                    log.error("not able to reset vfs for pf %s", pciid)
                    sys.exit(1)  
                if not bind_pf(pciid, pf_driver):
                    log.error("unable to bind %s with %s", pciid, pf_driver)
                    sys.exit(1)
                else:
                    log.info("Successfully bind to %s", pciid)
            elif not status_list[1] and (vfs_count == len(get_vfids(pciid))):
                if not bind_pf(pciid, pf_driver):
                    log.error("unable to bind %s with %s", pciid, pf_driver)
                    sys.exit(1)
                else:
                    log.info("Successfully bind to %s", pciid)
    for group_pciid in group_pciids:
        status_list = driver_attach_pf(pciid, pf_driver)
        if not status_list[1]:
            if status_list[0] == 0:
                if not bind_pf(pciid, pf_driver):
                    log.error("unable to bind %s with %s", pciid, pf_driver)
                    sys.exit(1)
                else:
                    log.info("Successfully bind %s", pciid)

if __name__ == "__main__":
    main()
