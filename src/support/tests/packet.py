#!/usr/bin/python
# BSD LICENSE
#
# Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
        # LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
        # DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
Generic packet create, transmit and analyze module
Base on scapy(python program for packet manipulation)
"""

import os
import time
import sys
import re
import signal
import random
import subprocess
import shlex        # separate command line for pipe
from uuid import uuid4

from scapy.config import conf
conf.use_pcap = True

import struct
from socket import AF_INET6
from scapy.all import conf
from scapy.utils import wrpcap, rdpcap, hexstr
from scapy.layers.inet import Ether, IP, TCP, UDP, ICMP
from scapy.layers.inet6 import IPv6, IPv6ExtHdrRouting, IPv6ExtHdrFragment
from scapy.layers.l2 import Dot1Q, ARP, GRE
from scapy.layers.sctp import SCTP, SCTPChunkData
from scapy.sendrecv import sniff
from scapy.route import *
from scapy.packet import bind_layers, Raw
from scapy.sendrecv import sendp
from scapy.arch import get_if_hwaddr

PACKETGEN = "scapy"

LayersTypes = {
    "L2": ['ether', 'vlan', 'etag', '1588', 'arp', 'lldp'],
    # ipv4_ext_unknown, ipv6_ext_unknown
    "L3": ['ipv4', 'ipv4ihl', 'ipv6', 'ipv4_ext', 'ipv6_ext', 'ipv6_ext2', 'ipv6_frag'],
    "L4": ['tcp', 'udp', 'frag', 'sctp', 'icmp', 'nofrag'],
    "INNER L2": ['inner_mac', 'inner_vlan'],
    "PAYLOAD": ['raw']
}


# Saved back groud sniff process id
SNIFF_PIDS = {}

# Saved packet generator process id
# used in pktgen or tgen
PKTGEN_PIDS = {}


def sniff_packets(intf, count=0, timeout=5, filters=[]):
    """
    sniff all packets for certain port in certain seconds
    """
    param = ""
    direct_param = r"(\s+)\[ -(\w) in\|out\|inout \]"
    tcpdump_help = subprocess.check_output("tcpdump -h; echo 0",
                                           stderr=subprocess.STDOUT,
                                           shell=True)
    for line in tcpdump_help.split('\n'):
        m = re.match(direct_param, line)
        if m:
            param = "-" + m.group(2) + " in"

    if len(param) == 0:
        print "tcpdump not support direction chioce!!!"


    filter_cmd = get_filter_cmd(filters)

    sniff_cmd = 'tcpdump -i %(INTF)s %(FILTER)s %(IN_PARAM)s -w %(FILE)s'
    options = {'INTF': intf, 'COUNT': count, 'IN_PARAM': param,
               'FILE': '/tmp/sniff_%s.pcap' % intf,
               'FILTER': filter_cmd}
    if count:
        sniff_cmd += ' -c %(COUNT)d'
        cmd = sniff_cmd % options
    else:
        cmd = sniff_cmd % options

    args = shlex.split(cmd)

    pipe = subprocess.Popen(args)
    index = str(time.time())
    SNIFF_PIDS[index] = (pipe, intf, timeout)
    time.sleep(0.5)
    return index

def get_filter_cmd(filters=[]):
    """
    Return bpd formated filter string, only support ether layer now
    """
    filter_sep = " and "
    filter_cmds = ""
    for pktfilter in filters:
        filter_cmd = ""
        if pktfilter['layer'] == 'ether':
            if pktfilter['config'].keys()[0] == 'dst':
                dmac = pktfilter['config']['dst']
                filter_cmd = "ether dst %s" % dmac
            elif pktfilter['config'].keys()[0] == 'src':
                smac = pktfilter['config']['src']
                filter_cmd = "ether src %s" % smac
            elif pktfilter['config'].keys()[0] == 'type':
                eth_type = pktfilter['config']['type']
                eth_format = r"(\w+) (\w+)"
                m = re.match(eth_format, eth_type)
                if m:
                    type_hex = get_ether_type(m.group(2))
                    if type_hex == 'not support':
                        continue
                    if m.group(1) == 'is':
                        filter_cmd = 'ether[12:2] = %s' % type_hex
                    elif m.group(1) == 'not':
                        filter_cmd = 'ether[12:2] != %s' % type_hex

        if len(filter_cmds):
            if len(filter_cmd):
                filter_cmds += filter_sep
                filter_cmds += filter_cmd
        else:
            filter_cmds = filter_cmd

    if len(filter_cmds):
        return ' \'' + filter_cmds + '\' '
    else:
        return ""


def load_sniff_packets(index=''):
    pkts = []
    child_exit = False
    if index in SNIFF_PIDS.keys():
        pipe, intf, timeout = SNIFF_PIDS[index]
        time_elapse = int(time.time() - float(index))
        while time_elapse < timeout:
            if pipe.poll() is not None:
                child_exit = True
                break

            time.sleep(1)
            time_elapse += 1

        if not child_exit:
            pipe.send_signal(signal.SIGINT)
            pipe.wait()

        # wait pcap file ready
        time.sleep(1)
        try:
            cap_pkts = rdpcap("/tmp/sniff_%s.pcap" % intf)
            for pkt in cap_pkts:
                # packet gen should be scapy
                packet = Packet(tx_port=intf)
                packet.pktgen.assign_pkt(pkt)
                pkts.append(packet)
        except:
            pass

    return pkts

class scapy(object):
    SCAPY_LAYERS = {
        'ether': Ether(dst="ff:ff:ff:ff:ff:ff"),
        'vlan': Dot1Q(),
        'ipv4': IP(),
        'udp': UDP(),
        'tcp': TCP(),
        'icmp': ICMP(),
        'raw': Raw(),
        'inner_mac': Ether(),
        'inner_vlan': Dot1Q(),
    }

    def __init__(self):
        self.pkt = None
        pass

    def assign_pkt(self, pkt):
        self.pkt = pkt

    def add_layers(self, layers):
        self.pkt = None
        for layer in layers:
            if self.pkt is not None:
                self.pkt = self.pkt / self.SCAPY_LAYERS[layer]
            else:
                self.pkt = self.SCAPY_LAYERS[layer]

    def send_pkt(self, intf='', count=1):
        self.print_summary()
        if intf != '':
            # fix fortville can't receive packets with 00:00:00:00:00:00
            if self.pkt.getlayer(0).src == "00:00:00:00:00:00":
                self.pkt.getlayer(0).src = get_if_hwaddr(intf)
            sendp(self.pkt, iface=intf, count=count)

    def raw(self, pkt_layer, payload=None):
        if payload is not None:
            pkt_layer.load = ''
            for hex1, hex2 in payload:
                pkt_layer.load += struct.pack("=B", int('%s%s' % (hex1, hex2), 16))

    def ether(self, pkt_layer, dst="ff:ff:ff:ff:ff:ff", src="00:00:20:00:00:00", type=None):
        if pkt_layer.name != "Ethernet":
            return
        pkt_layer.dst = dst
        pkt_layer.src = src
        if type is not None:
            pkt_layer.type = type

    def print_summary(self):
        print "Send out pkt %s" % self.pkt.summary()

    def vlan(self, pkt_layer, vlan, prio=0, type=None):
        if pkt_layer.name != "802.1Q":
            return
        pkt_layer.vlan = int(vlan)
        pkt_layer.prio = prio
        if type is not None:
            pkt_layer.type = type

    def strip_vlan(self, element):
        value = None

        if self.pkt.haslayer('Dot1Q') is 0:
            return None

        if element == 'vlan':
            value = int(str(self.pkt[Dot1Q].vlan))
        return value

    def strip_layer2(self, element):
        value = None
        layer = self.pkt.getlayer(0)
        if layer is None:
            return None

        if element == 'src':
            value = layer.src
        elif element == 'dst':
            value = layer.dst
        elif element == 'type':
            value = layer.type

        return value

    def strip_layer3(self, element):
        value = None
        layer = self.pkt.getlayer(1)
        if layer is None:
            return None

        if element == 'src':
            value = layer.src
        elif element == 'dst':
            value = layer.dst
        else:
            value = layer.getfieldval(element)

        return value

    def ipv4(self, pkt_layer, frag=0, src="127.0.0.1", proto=None, tos=0, dst="127.0.0.1", chksum=None, len=None, version=4, flags=None, ihl=None, ttl=64, id=1, options=None):
        pkt_layer.frag = frag
        pkt_layer.src = src
        if proto is not None:
            pkt_layer.proto = proto
        pkt_layer.tos = tos
        pkt_layer.dst = dst
        if chksum is not None:
            pkt_layer.chksum = chksum
        if len is not None:
            pkt_layer.len = len
        pkt_layer.version = version
        if flags is not None:
            pkt_layer.flags = flags
        if ihl is not None:
            pkt_layer.ihl = ihl
        pkt_layer.ttl = ttl
        pkt_layer.id = id
        if options is not None:
            pkt_layer.options = options

class Packet(object):
    def_packet = {
                'UDP': {'layers': ['ether', 'ipv4', 'udp', 'raw'], 'cfgload': True},
        'VLAN_UDP': {'layers': ['ether', 'vlan', 'ipv4', 'udp', 'raw'], 'cfgload': True},
    }

    def __init__(self, **options):
        """
        pkt_type: description of packet type
                  defined in def_packet
        options: special option for Packet module
                 pkt_len: length of network packet
                 ran_payload: whether payload of packet is random
                 pkt_file:
                 pkt_gen: packet generator type
                          now only support scapy
        """
        self.pkt_layers = []
        self.pkt_len = 64
        self.pkt_opts = options

        self.pkt_type = "UDP"

        if 'pkt_type' in self.pkt_opts.keys():
            self.pkt_type = self.pkt_opts['pkt_type']

        if self.pkt_type in self.def_packet.keys():
            self.pkt_layers = self.def_packet[self.pkt_type]['layers']
            self.pkt_cfgload = self.def_packet[self.pkt_type]['cfgload']
            if "IPv6" in self.pkt_type:
                self.pkt_len = 128
        else:
            self._load_pkt_layers()

        if 'pkt_len' in self.pkt_opts.keys():
            self.pkt_len = self.pkt_opts['pkt_len']

        if 'pkt_file' in self.pkt_opts.keys():
            self.uni_name = self.pkt_opts['pkt_file']
        else:
            self.uni_name = '/tmp/' + str(uuid4()) + '.pcap'

        if 'pkt_gen' in self.pkt_opts.keys():
            if self.pkt_opts['pkt_gen'] == 'scapy':
                self.pktgen = scapy()
            else:
                print "Not support other pktgen yet!!!"
        else:
            self.pktgen = scapy()

        self._load_assign_layers()

    def _load_assign_layers(self):
        # assign layer
        self.assign_layers()

        # config special layer
        #self.config_def_layers()

        # handle packet options
        payload_len = self.pkt_len - len(self.pktgen.pkt) - 4

        # if raw data has not been configured and payload should configured
        if hasattr(self, 'configured_layer_raw') is False and self.pkt_cfgload is True:
            payload = []
            raw_confs = {}
            if 'ran_payload' in self.pkt_opts.keys():
                for loop in range(payload_len):
                    payload.append("%02x" % random.randrange(0, 255))
            else:
                for loop in range(payload_len):
                    payload.append('58')  # 'X'

            raw_confs['payload'] = payload
            self.config_layer('raw', raw_confs)

    def send_pkt(self, crb=None, tx_port='', auto_cfg=True, count=1):
        if tx_port == '':
            print "Invalid Tx interface"
            return

        self.tx_port = tx_port

        # check with port type
        if 'ixia' in self.tx_port:
            print "Not Support Yet"

        if crb is not None:
            self.pktgen.write_pcap(self.uni_name)
            crb.session.copy_file_to(self.uni_name)
            pcap_file = self.uni_name.split('/')[2]
            self.pktgen.send_pcap_pkt(
                crb=crb, file=pcap_file, intf=self.tx_port, count=count)
        else:
            self.pktgen.send_pkt(intf=self.tx_port, count=count)

    def check_layer_config(self, layer, config):
        """
        check the format of layer configuration
        every layer should has different check function
        """
        pass

    def assign_layers(self, layers=None):
        """
        assign layer for this packet
        maybe need add check layer function
        """
        if layers is not None:
            self.pkt_layers = layers

        for layer in self.pkt_layers:
            found = False
            l_type = layer.lower()

            for types in LayersTypes.values():
                if l_type in types:
                    found = True
                    break

            if found is False:
                self.pkt_layers.remove(l_type)
                print "INVAILD LAYER TYPE [%s]" % l_type.upper()

        self.pktgen.add_layers(self.pkt_layers)

    def config_layer(self, layer, config={}):
        """
        Configure packet assgined layer
        return the status of configure result
        """
        try:
            idx = self.pkt_layers.index(layer)
        except Exception as e:
            print "INVALID LAYER ID %s" % layer
            return False

        if self.check_layer_config(layer, config) is False:
            return False

        if 'inner' in layer:
            layer = layer[6:]

        pkt_layer = self.pktgen.pkt.getlayer(idx)
        layer_conf = getattr(self, "_config_layer_%s" % layer)
        setattr(self, 'configured_layer_%s' % layer, True)

        return layer_conf(pkt_layer, config)

    def _config_layer_raw(self, pkt_layer, config):
        return self.pktgen.raw(pkt_layer, **config)

    def _config_layer_ether(self, pkt_layer, config):
        return self.pktgen.ether(pkt_layer, **config)

    def _config_layer_vlan(self, pkt_layer, config):
        return self.pktgen.vlan(pkt_layer, **config)

    def _config_layer_ipv4(self, pkt_layer, config):
        return self.pktgen.ipv4(pkt_layer, **config)
