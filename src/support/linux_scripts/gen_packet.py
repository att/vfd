#!/usr/bin/env python
# vi: sw=4 ts=4:

import argparse
import re
import subprocess
import time

def parse_args():
	ap = argparse.ArgumentParser()
	ap.add_argument('-d', '--debug', action='store_true', default=False, help='Show debugging output')
	ap.add_argument('-f', '--filename', action='store', default="packet.conf", help="Path to data file")
	return ap.parse_args()

def gen_packet(pkt_num, iface, s_mac, d_mac, s_ip, d_ip, proto, dport, vlan_id):
	if dport == None and vlan_id != None:
		cmd = 'mz %s -a %s -b %s -A %s -B %s -t %s -P "Hello Test Packet %s" -Q %s' % (iface, s_mac, d_mac, s_ip, d_ip, proto, pkt_num, vlan_id)
	elif dport != None and vlan_id != None:
		cmd = 'mz %s -a %s -b %s -A %s -B %s -t %s "dp=%s" -P "Hello Test Packet %s" -Q %s' % (iface, s_mac, d_mac, s_ip, d_ip, proto, dport, pkt_num, vlan_id)
	else:
		cmd = 'mz %s -a %s -b %s -A %s -B %s -t %s -P "Hello Test Packet %s"' % (iface, s_mac, d_mac, s_ip, d_ip, proto, pkt_num)
	try:
		subprocess.check_call(cmd, shell=True)
		return True
	except subprocess.CalledProcessError:
		return False

def read_config(file_path):
	with open(file_path, 'r') as f:
		line_num = 0
		for file_entry in f.readlines():
			line_num += 1
			if re.match('#.*', file_entry):
				continue
			(iface, s_mac, d_mac, s_ip, d_ip, proto, dport, vlan_id) = file_entry.split(",")
			if args.debug:
				print iface, s_mac, d_mac, s_ip, d_ip, proto, dport, vlan_id
			pkt_num = 1
			for i in xrange(5):
				gen_packet(pkt_num, iface, s_mac, d_mac, s_ip, d_ip, proto, dport, vlan_id)
				time.sleep(1)
				pkt_num = pkt_num + 1

if __name__ == '__main__':
	args = parse_args()
	read_config(args.filename)
