from tests.packet import Packet
from tests.parse_sample_data import section_map

# global dict to hold config
config = {}

def read_sample_data(target_vf):
    config['smac'] = section_map('PG')['mac']
    config['iface'] = section_map('PG')['iface']
    config['mcast_mac'] = section_map(target_vf)['mcast_mac']
    config['bcast_mac'] = section_map(target_vf)['bcast_mac']

def build_packet(**options):
    pkt = Packet(pkt_type='VLAN_UDP')
    pkt.config_layer('ether', {'dst': options['dmac'], 'src': config['smac']})
    pkt.config_layer('ipv4', {'dst': '1.1.1.1', 'src': '2.2.2.2'})
    pkt.config_layer('vlan', {'vlan': options['valid_vlan']})
    return pkt
