from tests import packet
import tests.helper_functions as hf
import pytest

@pytest.mark.valid_vlan_bcastF_mcastF_ucastF
def test_mcast_valid_vlan(vf3_valid_vlan):
    hf.read_sample_data('TARGET_VF3')
    pkt = hf.build_packet(dmac=hf.config['mcast_mac'], valid_vlan=int(vf3_valid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=8, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == None

@pytest.mark.invalid_vlan_bcastF_mcastF_ucastF
def test_mcast_invalid_vlan(vf3_invalid_vlan):
    hf.read_sample_data('TARGET_VF3')
    pkt = hf.build_packet(dmac=hf.config['mcast_mac'], valid_vlan=int(vf3_invalid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=8, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == None
