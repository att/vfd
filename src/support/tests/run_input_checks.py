#!/usr/bin/python
# vi: sw=4 ts=4:

import os
import sys
from time import time, ctime
import argparse
import socket
import random
import json
import subprocess

VFD_CFG = "/var/lib/vfd/config"
pciid = ""
vfid = ""
filename = ""

def parse_args():
	ap = argparse.ArgumentParser()
	ap.add_argument('-p', '--pciid', action='store', required=True, help='Get the pciid from /etc/vfd/cfd.cfg')
	ap.add_argument('-v', '--vfid', action='store', required=True, help='virtual function number')
	return ap.parse_args()

def randomMAC():
	mac = [ 0x00, 0x16, 0x3e,
			random.randint(0x00, 0x7f),
			random.randint(0x00, 0xff),
			random.randint(0x00, 0xff) ]
	return ':'.join(map(lambda x: "%02x" % x, mac))

def gethostname():
	return socket.gethostname()

def generate_filename():
	global filename
	if os.path.isdir(VFD_CFG):
		filename = os.path.join(VFD_CFG, gethostname()) + '.json'
	else:
		print "%s doesn't exist" % VFD_CFG
		sys.exit(1)

def create_confFile(strip_stag, insert_stag, vlan_list, macs_list):
	global pciid, vfid, filename
	data = {}
	data['name'] = filename
	data['pciid'] = pciid
	data['vfid'] = vfid
	data['strip_stag'] = strip_stag
	data['insert_stag'] = insert_stag
	data['allow_bcast'] = True
	data['allow_mcast'] = True
	data['allow_un_ucast'] = True
	data['vlan_anti_spoof'] = True
	data['mac_anti_spoof'] = True
	data['vlans'] = vlan_list
	data['macs'] = macs_list
	with open(filename, 'w') as outfile:
		json.dump(data, outfile)
	
def delete_confFile(filename):
	if os.path.isfile(filename):
		os.remove(filename)

# Test when vlan_id is a range of values, strip_stag and insert_stag is true
def vlan_range_strip_insert_true():
	global filename
	strip_stag = True
	insert_stag = True
	vlan_list = random.sample(xrange(1, 4096), 5)
	macs_list = []
	macs_list.append(randomMAC())
	create_confFile(strip_stag, insert_stag, vlan_list, macs_list)
	cmd = 'iplex add %s' % os.path.splitext(os.path.basename(filename))[0]
	try:
		data = subprocess.check_output(cmd, shell=True)
		data = json.loads(data)
		if data['state'] != 'ERROR':
			return False
		return True
	except subprocess.CalledProcessError:
		return False

# Test when vlan_id is a range of values strip_stag and insert_stag is false
def vlan_range_strip_insert_false():
	global filename
	strip_stag = False
	insert_stag = False
	vlan_list = random.sample(xrange(1, 4096), 5)
	macs_list = []
	macs_list.append(randomMAC())
	create_confFile(strip_stag, insert_stag, vlan_list, macs_list)
	cmd = 'iplex add %s' % os.path.splitext(os.path.basename(filename))[0]
	try:
		data = subprocess.check_output(cmd, shell=True)
		data = json.loads(data)
		if data['state'] == 'ERROR':
			return False
		return True
	except subprocess.CalledProcessError:
		return False

# Test when vlan_id is single number, strip_stag and insert_stag is true
def vlan_number_strip_insert_true():
	global filename
	strip_stag = True
	insert_stag = True
	vlan_list = random.sample(xrange(1, 4096), 1)
	macs_list = []
	macs_list.append(randomMAC())
	create_confFile(strip_stag, insert_stag, vlan_list, macs_list)
	cmd = 'iplex add %s' % os.path.splitext(os.path.basename(filename))[0]
	try:
		data = subprocess.check_output(cmd, shell=True)
		data = json.loads(data)
		if data['state'] == 'ERROR':
			return False
		return True
	except subprocess.CalledProcessError:
		return False

# Test when vlan_id is single number, strip_stag and insert_stag is false
def vlan_number_strip_insert_false():
	global filename
	strip_stag = False
	insert_stag = False
	vlan_list = random.sample(xrange(1, 4096), 1)
	macs_list = []
	macs_list.append(randomMAC())
	create_confFile(strip_stag, insert_stag, vlan_list, macs_list)
	cmd = 'iplex add %s' % os.path.splitext(os.path.basename(filename))[0]
	try:
		data = subprocess.check_output(cmd, shell=True)
		data = json.loads(data)
		if data['state'] == 'ERROR':
			return False
		return True
	except subprocess.CalledProcessError:
		return False

# Test invalid VF
def invalidVF():
	global filename
	strip_stag = True
	insert_stag = True
	vlan_list = random.sample(xrange(1, 4096), 1)
	macs_list = []
	macs_list.append(randomMAC())
	create_confFile(strip_stag, insert_stag, vlan_list, macs_list)
	cmd = 'iplex add %s' % os.path.splitext(os.path.basename(filename))[0]
	try:
		data = subprocess.check_output(cmd, shell=True)
		data = json.loads(data)
		if data['state'] != 'ERROR':
			return False
		return True
	except subprocess.CalledProcessError:
		return False

# Test when mac addresses are duplicates
def mac_dupplicates():
	global filename
	strip_stag = True
	insert_stag = True
	vlan_list = random.sample(xrange(1, 4096), 1)
	macs_list = ["a6:65:2e:28:ec:f6", "90:b1:1c:0c:cb:cc", "90:b1:1c:0c:cb:cc"]
	create_confFile(strip_stag, insert_stag, vlan_list, macs_list)
	cmd = 'iplex add %s' % os.path.splitext(os.path.basename(filename))[0]
	try:
		data = subprocess.check_output(cmd, shell=True)
		data = json.loads(data)
		if data['state'] != 'ERROR':
			return False
		return True
	except subprocess.CalledProcessError:
		return False

# Test when duplicate vlan_ids
def vlan_dupplicates():
	global filename
	strip_stag = False
	insert_stag = False
	vlan_list = [11, 12, 13, 11]
	macs_list = []
	macs_list.append(randomMAC())
	create_confFile(strip_stag, insert_stag, vlan_list, macs_list)
	cmd = 'iplex add %s' % os.path.splitext(os.path.basename(filename))[0]
	try:
		data = subprocess.check_output(cmd, shell=True)
		data = json.loads(data)
		if data['state'] != 'ERROR':
			return False
		return True
	except subprocess.CalledProcessError:
		return False

# vlan should not exceed 64 on VF and across PF
# mac's should not exceed 64 on VF and across PF the max is 128
def macs_64_vlans_64():
	global filename
	strip_stag = False
	insert_stag = False
	vlan_list = random.sample(xrange(1, 4096),65)
	macs_list = []
	count = 0
	while True:
		mac = randomMAC()
		if mac not in macs_list:
			if count <= 64:
				macs_list.append(mac)
				count = count + 1
			else:
				break
	create_confFile(strip_stag, insert_stag, vlan_list, macs_list)
	cmd = 'iplex add %s' % os.path.splitext(os.path.basename(filename))[0]
	try:
		data = subprocess.check_output(cmd, shell=True)
		data = json.loads(data)
		if data['state'] != 'ERROR':
			return False
		return True
	except subprocess.CalledProcessError:
		return False

def invalid_vlans(vlan_id):
	global filename
	strip_stag = True
	insert_stag = True
	vlan_list = [vlan_id]
	macs_list = []
	macs_list.append(randomMAC())
	create_confFile(strip_stag, insert_stag, vlan_list, macs_list)
	cmd = 'iplex add %s' % os.path.splitext(os.path.basename(filename))[0]
	try:
		data = subprocess.check_output(cmd, shell=True)
		data = json.loads(data)
		if data['state'] != 'ERROR':
			return False
		return True
	except subprocess.CalledProcessError:
		return False

def invalid_macs(mac):
	global filename
	strip_stag = True
	insert_stag = True
	vlan_list = [10]
	macs_list = [mac]
	create_confFile(strip_stag, insert_stag, vlan_list, macs_list)
	cmd = 'iplex add %s' % os.path.splitext(os.path.basename(filename))[0]
	try:
		data = subprocess.check_output(cmd, shell=True)
		data = json.loads(data)
		if data['state'] != 'ERROR':
			return False
		return True
	except subprocess.CalledProcessError:
		return False

def deleteVF(filename):
	if os.path.isfile(filename):
		cmd = 'iplex delete %s >/dev/null' % os.path.splitext(os.path.basename(filename))[0]
		try:
			subprocess.check_call(cmd, shell=True)
			return True
		except subprocess.CalledProcessError:
			return False

def echo_pass(msg):
	global filename
	print msg
	print "PASSED"
	deleteVF(filename)

def echo_fail(msg):
	global filename
	print msg
	print "FAILED"
	deleteVF(filename)

def is_vfd_running():
	cmd = "iplex ping >/dev/null"
	try:
		subprocess.check_call(cmd, shell=True)
		return True
	except subprocess.CalledProcessError:
		return False

if __name__ == '__main__':
	args = parse_args()
	pciid = args.pciid
	vfid = args.vfid
	invalid_vf = False
	if int(args.vfid) < 0 or int(args.vfid) > 31:
		invalid_vf = True
	if not is_vfd_running():
		sys.exit(1)
	generate_filename()
	print "Test start: ", ctime(time())
	if invalid_vf:
		if invalidVF():
			echo_pass("'Test invalid VFID as string'")
		else:
			echo_fail("'Test invalid VFID as string'")
	vfid = int(args.vfid)
	if invalidVF():
		echo_pass("'Test invalid VFID as integer'")
		delete_confFile(filename+'-')
		sys.exit(1)
	if vlan_range_strip_insert_true():
		echo_pass("'Test when vlan_id is a range of values, strip_stag and insert_stag is true'")
	else:
		echo_fail("'Test when vlan_id is a range of values, strip_stag and insert_stag is true'")
	if vlan_range_strip_insert_false():
		echo_pass("'Test when vlan_id is a range of values, strip_stag and insert_stag is false'")
	else:
		echo_fail("'Test when vlan_id is a range of values, strip_stag and insert_stag is false'")
	if vlan_number_strip_insert_true():
		echo_pass("'Test when vlan_id is single number, strip_stag and insert_stag is true'")
	else:
		echo_fail("'Test when vlan_id is single number, strip_stag and insert_stag is true'")
	if vlan_number_strip_insert_false():
		echo_pass("'Test when vlan_id is single number, strip_stag and insert_stag is false'")
	else:
		echo_fail("'Test when vlan_id is single number, strip_stag and insert_stag is false'")
	if mac_dupplicates():
		echo_pass("'Test when mac addreses are duplicates'")
	else:
		echo_fail("'Test when mac addreses are duplicates'")
	if vlan_dupplicates():
		echo_pass("'Test when duplicate vlan_ids'")
	else:
		echo_fail("'Test when duplicate vlan_ids'")
	if macs_64_vlans_64():
		echo_pass("'Test not more than 64 macs and 64 vlans on a VF'")
	else:
		echo_fail("'Test not more than 64 macs and 64 vlans on a VF'")
	invalid_vlan_list = [-1, 0, 4096]
	for vlan in invalid_vlan_list:
		if invalid_vlans(vlan):
			echo_pass("'Test invalid vlan %s'" % vlan)
		else:
			echo_fail("'Test invalid vlan %s'" % vlan)
	invalid_macs_list = ["00:12", "11:22:gg:fh:aa:bb"]
	for mac in invalid_macs_list:
		if invalid_macs(mac):
			echo_pass("'Test invalid mac %s'" % mac)
		else:
			echo_fail("'Test invalid mac %s'" % mac)
	deleteVF(filename)
	delete_confFile(filename+'-')
	print "Test done: ", ctime(time())
