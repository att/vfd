#!/usr/bin/env python
# vi: sw=4 ts=4:

"""
		Mnemonic:       vfd_pre_start.py
    	Abstract:       This script calls the 'dpdk_nic_bind' script to bind PF's and VF's to vfio-pci
    	Date:           7 April 2016
    	Author:         Dhanunjaya Naidu Ravada (dr3662@att.com)
    	Mod:            2016 7 Apr - Created script
"""

import subprocess
import json
import sys

VFD_CONFIG='/etc/vfd/vfd.cfg'
SYS_DIR="/sys/devices"

def is_vfio_pci_loaded():
	try:
		subprocess.check_call('lsmod | grep vfio_pci >/dev/null', shell=True)
		return True
	except subprocess.CalledProcessError:
		return False

def load_vfio_pci_driver():
	try:
		subprocess.check_call('modprobe vfio-pci')
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
	unbind_cmd = 'dpdk_nic_bind -u %s' % dev_id
	try:
		subprocess.check_call(unbind_cmd, shell=True)
		return True
	except subprocess.CalledProcessError:
		return False

def get_vfids(dev_id):
	cmd='find %s -name %s -type d | while read d; do echo "$d"; ls -l $d | grep virtfn| sed \'s!.*/!!\'; done' % (SYS_DIR, dev_id)
	vfids = subprocess.check_output(cmd, shell=True).split('\n')[1:]
	return filter(None, vfids)

def bind_pf_vfs(dev_id):
	bind_cmd = 'dpdk_nic_bind -b vfio-pci %s' % dev_id
	try:
		subprocess.check_call(bind_cmd, shell=True)
		return True
	except subprocess.CalledProcessError:
		return False

def driver_attach(dev_id):
	cmd = 'lspci -k -s %s' % dev_id
	driver_name = subprocess.check_output(cmd, shell=True).splitlines()[2].split(':')[1].lstrip()
	if driver_name == 'vfio-pci':
		return True
	else:
		return False

def main():
	pciids = []
	for value in get_pciids():
		if 'id' in value:
			pciids.append(value['id'])
		else:
			pciids = get_pciids()

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
			if not unbind_pfs(pciid):
				print "unable to bind %s PF" % pciid
			else:
				print "Successfully binded %s PF" % pciid

	for pciid in pciids:
		if not driver_attach(pciid):
			if not bind_pf_vfs(pciid):
				print "unable to bind %s with vfio-pci" % pciid
			for vfid in get_vfids(pciid):
				if not driver_attach(vfid):
					if not bind_pf_vfs(vfid):
						print "unbale to bind %s with vfio-pci" % vfid

if __name__ == '__main__':
	main()
