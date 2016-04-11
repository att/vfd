#!/usr/bin/env python
# vi: sw=4 ts=4:

"""
		Mnemonic:       vfd_pre_start.py
    	Abstract:       This script calls the 'dpdk_nic_bind' script to bind PF's and VF's to vfio-pci
    	Date:           7 April 2016
    	Author:         Dhanunjaya Naidu Ravada (dr3662@att.com)
    	Mod:            2016 7 Apr - Created script
    					2016 8 Apr - fix to index out of bound error
"""

import subprocess
import json
import sys

VFD_CONFIG='/etc/vfd/vfd.cfg'
SYS_DIR="/sys/devices"

# global pciids list
pciids = []
# global group pciids list
group_pciids = []
# To catch index error
index = 0

def is_vfio_pci_loaded():
	try:
		subprocess.check_call('lsmod | grep vfio_pci >/dev/null', shell=True)
		return True
	except subprocess.CalledProcessError:
		return False

def load_vfio_pci_driver():
	try:
		subprocess.check_call('modprobe vfio-pci', shell=True)
		return True
	except subprocess.CalledProcessError:
		return Flase

def is_ixgbevf_loaded():
	try:
		subprocess.check_call('lsmod | grep ixgbevf >/dev/null', shell=True)
		return True
	except subprocess.CalledProcessError:
		return False

def unload_ixgbevf_driver():
	try:
		subprocess.check_call('rmmod ixgbevf', shell=True)
		return True
	except subprocess.CalledProcessError:
		return False

def get_pciids():
	with open(VFD_CONFIG) as data_file:
		try:
			data = json.load(data_file)
		except ValueError:
			print VFD_CONFIG + " is not a valid json"
			sys.exit(1)
	return data['pciids']

def unbind_pfs(dev_id):
	unbind_cmd = 'dpdk_nic_bind -u --force %s' % dev_id
	try:
		msg = subprocess.check_output(unbind_cmd, shell=True)
		if "Routing" in msg:
			return False
		return True
	except subprocess.CalledProcessError:
		return False

def get_vfids(dev_id):
	cmd='find %s -name %s -type d | while read d; do echo "$d"; ls -l $d | grep virtfn| sed \'s!.*/!!\'; done' % (SYS_DIR, dev_id)
	vfids = subprocess.check_output(cmd, shell=True).split('\n')[1:]
	return filter(None, vfids)

def bind_pf_vfs(dev_id):
	bind_cmd = 'dpdk_nic_bind -b --force vfio-pci %s' % dev_id
	try:
		subprocess.check_call(bind_cmd, shell=True)
		return True
	except subprocess.CalledProcessError:
		return False

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

def main():
	global pciids
	global group_pciids
	global index

	for value in get_pciids():
		if 'id' in value:
			pciids.append(value['id'])
		else:
			pciids = get_pciids()

	for pciid in pciids:
		get_pciids_group(pciid)

	pciids = list(set(pciids) | set(group_pciids))
	print "pciids: " + ','.join(pciids)

	if is_ixgbevf_loaded():
		if not unload_ixgbevf_driver():
			print "unable to unload the driver [rmmod ixgbevf]"
			sys.exit(1)

	if not is_vfio_pci_loaded():
		if load_vfio_pci_driver():
			print "Successfully loaded driver [modprobe vfio-pci]"
		else:
			print "Unable to load driver [modprobe vfio-pci]"
			sys.exit(1)

	for pciid in pciids:
		if not driver_attach(pciid):
			if index == 0:
				if not unbind_pfs(pciid):
					print "unable to bind %s PF" % pciid
					sys.exit(1)
				else:
					print "Successfully binded %s PF" % pciid

	for pciid in pciids:
		if not bind_pf_vfs(pciid):
			print "unable to bind %s with vfio-pci" % pciid
		for vfid in get_vfids(pciid):
			if not driver_attach(vfid):
				if index == 1:
					if not bind_pf_vfs(vfid):
						print "unbale to bind %s with vfio-pci" % vfid

if __name__ == '__main__':
	main()
