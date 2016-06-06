#!/usr/bin/env python
# vi: sw=4 ts=4:

import socket
import argparse
import subprocess
import sys
import os
import dpkt
from dpkt.ip import IP
from dpkt.ethernet import Ethernet
from dpkt.arp import ARP

fname = "/tmp/" + str(os.getpid()) + ".pcap"

def parse_args():
    ap = argparse.ArgumentParser()
    ap.add_argument('-d', '--debug', action='store_true', default=False, help='Show debugging output')
    ap.add_argument('-i', '--iface', action='store', required=True, help='Interface to capture packets')
    ap.add_argument('-c', '--count', default=1, action='store', help='Count no of packets to capture')
    return ap.parse_args()

def mac_addr(address):
    return ':'.join('%02x' % ord(b) for b in address)

def capture_pkt(iface, count, debug):
    cmd = 'tcpdump -e -nnvXSs 1514 -c %s -i %s -w %s' % (count, iface, fname)
    if debug:
        print cmd
    try:
        subprocess.check_call(cmd, shell=True)
    except subprocess.CalledProcessError:
        print "subprocess module exception"
        sys.exit(0)

def mac_addr(address):
    return ':'.join('%02x' % ord(b) for b in address)

def ip_to_str(address):
    return socket.inet_ntop(socket.AF_INET, address)

def pkt_generator(iface, m_dst, m_src, i_dst, i_src, proto, dport, vlanid):
    cmd = None
    if vlanid and dport:
        cmd = 'mz %s -a %s -b %s -A %s -B %s -t %s "dp=%s" -Q %s' % (iface, m_src, m_dst, i_src, i_dst, proto, dport, vlanid)
        print cmd
    elif vlanid and not dport:
        cmd = 'mz %s -a %s -b %s -A %s -B %s -t %s -Q %s' % (iface, m_src, m_dst, i_src, i_dst, proto, vlanid)
        print cmd
    elif dport and not vlanid:
        cmd = 'mz %s -a %s -b %s -A %s -B %s -t %s "dp=%s"' % (iface, m_src, m_dst, i_src, i_dst, proto, dport)
        print cmd
    else:
        cmd = 'mz %s -a %s -b %s -A %s -B %s -t %s' % (iface, m_src, m_dst, i_src, i_dst, proto)
        print cmd
    try:
        subprocess.check_call(cmd, shell=True)
    except subprocess.CalledProcessError:
        print "'mz:' something failed while sending packets"


if __name__ == '__main__':
    args = parse_args()
    capture_pkt(args.iface, args.count, args.debug)
    f = open(fname)
    pcap = dpkt.pcap.Reader(f)
    for ts, buf in pcap:
        proto = dport = vlanid = None
        eth = dpkt.ethernet.Ethernet(buf)
        if eth.type != dpkt.ethernet.ETH_TYPE_IP:
            print 'Non IP Packet type not supported'
            continue
        ip = eth.data
        proto_data = ip.data
        do_not_fragment = bool(dpkt.ip.IP_DF)
        more_fragments = bool(dpkt.ip.IP_MF)
        fragment_offset = ip.off & dpkt.ip.IP_OFFMASK
        proto = type(proto_data).__name__.lower()
        if hasattr(proto_data, 'dport'):
            dport = proto_data.dport
        if hasattr(eth, 'vlanid'):
            vlanid = eth.vlanid
        m_src = mac_addr(eth.src)
        m_dst = mac_addr(eth.dst)
        i_src = ip_to_str(ip.src)
        i_dst = ip_to_str(ip.dst)
        if dport and vlanid:
            print "Resv: %s > %s | Eth Type: %s | (%s) > (%s) | protocol: %s | dport: %s | vlanid: %s" % (m_src, m_dst, eth.type, i_src, i_dst, proto, dport, vlanid)
        elif dport and not vlanid:
            print "Resv: %s > %s | Eth Type: %s | (%s) > (%s) | protocol: %s | dport: %s" % (m_src, m_dst, eth.type, i_src, i_dst, proto, dport)
        elif not dport and not vlanid:
            print "Resv: %s > %s | Eth Type: %s | (%s) > (%s) | protocol: %s" % (m_src, m_dst, eth.type, i_src, i_dst, proto)
        else:
            print "Resv: %s > %s | Eth Type: %s | (%s) > (%s) | protocol: %s | vlanid: %s" % (m_src, m_dst, eth.type, i_src, i_dst, proto, vlanid)
        pkt_generator(args.iface, m_src, m_dst, i_src, i_dst, proto, dport, vlanid)
    f.close()
    os.remove(fname)
