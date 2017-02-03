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
                    2016 8 Dec - fix to detrmine which iommu group pciid belong to and get the other pciid's in that iommu group.
                    2017 3 Feb - Add support to bind pf's to igb_uio.ko (dpdk)
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

# global varibale which will hold vfd.cfg as json data
data = {}

# max_vfs path
file_path = ""

# sometimes after binding pf to igb_uio still vfs exist
is_igb_uio = False

# logging
log = logging.getLogger('vfd_pre_start')

def setup_logging(logfile):
    handler = RotatingFileHandler(os.path.join(LOG_DIR, logfile), maxBytes=200000, backupCount=20)
    log_formatter = logging.Formatter('%(asctime)s  %(process)s %(levelname)s %(name)s [-] %(message)s')
    handler.setFormatter(log_formatter)
    log.setLevel(logging.INFO)
    log.addHandler(handler)

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

# get pciids from vfd.cfg
def get_config():
    global data
    with open(VFD_CONFIG) as data_file:
        try:
            data = json.load(data_file)
        except ValueError:
            log.error("%s is not a valid json", VFD_CONFIG)
            sys.exit(1)

# unbind pf and vf
def unbind_pfs_vfs(dev_id):
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

def get_vfs_path(dev_id):
    global file_path
    cmd = "find /sys -name *vfs | grep %s | grep max_vfs" % dev_id
    log.info(cmd)
    try:
        file_path = subprocess.check_output(cmd, shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

def create_vfs(dev_id, vfs):
    get_vfs_path(dev_id)
    global file_path
    if file_path:
        cmd = "echo '%s' > %s" % (str(vfs), file_path)
        log.info(cmd)
        try:
            if is_igb_uio and len(get_vfids(dev_id)):
                return True
            else:
                subprocess.check_call(cmd, shell=True)
                return True
        except subprocess.CalledProcessError:
            return False
    else:
        return False

# bind pf's to igb_uio
def bind_pf(dev_id):
    bind_cmd = 'dpdk_nic_bind --force -b igb_uio %s' % dev_id
    log.info(bind_cmd)
    try:
        subprocess.check_call(bind_cmd, shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

# bind vf's to vfio-pci
def bind_vf(dev_id):
    bind_cmd = 'dpdk_nic_bind --force -b vfio-pci %s' % dev_id
    log.info(bind_cmd)
    try:
        subprocess.check_call(bind_cmd, shell=True)
        return True
    except subprocess.CalledProcessError:
        return False

# check whether igb_uio driver is attached to pf
def driver_attach_pf(dev_id):
    index = 0
    cmd = 'lspci -k -s %s' % dev_id
    try:
        driver_name = subprocess.check_output(cmd, shell=True).splitlines()[2].split(':')[1].lstrip()
        if driver_name == 'igb_uio':
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

# get the pci cards in the group which must be attached to the igb_uio driver
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

def get_configured_vfs(pciids):
    vfd_count = 0
    for pciid in pciids:
        vfd_count = vfd_count + len(get_vfids(pciid))
    return vfd_count

if __name__ == '__main__':
    setup_logging('vfd_upstart.log')
    
    get_config()
    
    pciids = []
    for value in data['pciids']:
        if 'id' in value:
            pciids.append(value['id'])
        else:
            pciids.append(value)
            
    group_pciids = []
    for pciid in pciids:
        group_pciids = group_pciids + get_pciids_group(pciid)
        
    pciids = list(set(pciids) | set(group_pciids))
    log.info("pciids: %s", pciids)
    
    not_ixgbe_vendor = check_vendor(pciids)
    if len(not_ixgbe_vendor) > 0:
        log.error("VFD won't handle for this vendors: %s", not_ixgbe_vendor)
        sys.exit(1)
    
    if not is_uio_loaded():
        if load_uio():
            log.info("Successfully loaded uio driver")
        else:
            log.error("unable to load uio driver")
            sys.exit(1)
    else:
        log.info("Already loaded uio driver")
    
    if not is_igb_uio_loaded():
        log.error("please load dpdk igb_uio driver")
        sys.exit(1)
    
    if data["enable_qos"]:
        for pciid in pciids:
            if len(get_vfids(pciid)) < 0 and len(get_vfids(pciid)) > 31:
                log.error("qos supports (0-31) vfs per pf, pf %s: vf %s", pciid, str(len(get_vfids(pciid))))
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
        vfs_count = len(get_vfids(pciid))
        if vfs_count > 0:
            status_list = driver_attach_pf(pciid)
            if not status_list[1]:
                if status_list[0] == 0:
                    if not unbind_pfs_vfs(pciid):
                        log.error("unable to unbind %s PF", pciid)
                        sys.exit(1)
                    else:
                        log.info("Successfully unbind %s", pciid)
                        if not bind_pf(pciid):
                            log.error("unable to bind %s with igb_uio", pciid)
                            sys.exit(1)
                        else:
                            log.info("Successfully bind %s", pciid)
                            global is_igb_uio
                            is_igb_uio = True
                            if not create_vfs(pciid, vfs_count):
                                log.error("not able to create vfs for pf %s", pciid)
                                sys.exit(1)
                            else:
                                log.info("Successfully created vfs for pf: %s", pciid)
            else:
                log.info("Already %s bind to igb_uio driver", pciid)
        else:
            log.error("vfs are not created for pf : %s, vfs: %s", pciid, str(vfs_count))
            sys.exit(1)
            
    for pciid in pciids:
        jobs = []
        for vfid in get_vfids(pciid):
            return_list = driver_attach_vf(vfid)
            if not return_list[1]:
                if return_list[0] == 0:
                    t = multiprocessing.Process(target=unbind_pfs_vfs, args=(vfid,))
                    jobs.append(t)
                    t.start()
        for j in jobs:
            j.join()
            
    for pciid in pciids:
        for vfid in get_vfids(pciid):
            return_list = driver_attach_vf(vfid)
            if not return_list[1]:
                if return_list[0] == 0 or return_list[0] == 1:
                    t = multiprocessing.Process(target=bind_vf, args=(vfid,))
                    t.start()
