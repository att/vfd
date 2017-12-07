from tests import packet
import tests.helper_functions as hf
import pytest

@pytest.mark.valid_vlan_bcastT_mcastT_ucastF
def test_invalid_mac_valid_vlan(vf1_invalid_mac, vf1_valid_vlan):
    hf.read_sample_data('TARGET_VF1')
    pkt = hf.build_packet(dmac=vf1_invalid_mac, valid_vlan=int(vf1_valid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=5, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == None

@pytest.mark.valid_vlan_bcastT_mcastT_ucastF
def test_valid_macs_valid_vlan(vf1_mac, vf1_valid_vlan):
    hf.read_sample_data('TARGET_VF1')
    pkt = hf.build_packet(dmac=vf1_mac, valid_vlan=int(vf1_valid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=5, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == int(vf1_valid_vlan)

@pytest.mark.invalid_vlan_bcastT_mcastT_ucastF
def test_valid_macs_invalid_vlan(vf1_mac, vf1_invalid_vlan):
    hf.read_sample_data('TARGET_VF1')
    pkt = hf.build_packet(dmac=vf1_mac, valid_vlan=int(vf1_invalid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=5, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == None

@pytest.mark.valid_vlan_bcastT_mcastT_ucastF
def test_bcast_valid_vlan(vf1_valid_vlan):
    hf.read_sample_data('TARGET_VF1')
    pkt = hf.build_packet(dmac=hf.config['bcast_mac'], valid_vlan=int(vf1_valid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=8, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == int(vf1_valid_vlan)

@pytest.mark.invalid_vlan_bcastT_mcastT_ucastF
def test_bcast_invalid_vlan(vf1_invalid_vlan):
    hf.read_sample_data('TARGET_VF1')
    pkt = hf.build_packet(dmac=hf.config['bcast_mac'], valid_vlan=int(vf1_invalid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=8, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == None

@pytest.mark.valid_vlan_bcastT_mcastT_ucastF
def test_mcast_valid_vlan(vf1_valid_vlan):
    hf.read_sample_data('TARGET_VF1')
    pkt = hf.build_packet(dmac=hf.config['mcast_mac'], valid_vlan=int(vf1_valid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=8, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == int(vf1_valid_vlan)

@pytest.mark.invalid_vlan_bcastT_mcastT_ucastF
def test_mcast_invalid_vlan(vf1_invalid_vlan):
    hf.read_sample_data('TARGET_VF1')
    pkt = hf.build_packet(dmac=hf.config['mcast_mac'], valid_vlan=int(vf1_invalid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=5, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == None
