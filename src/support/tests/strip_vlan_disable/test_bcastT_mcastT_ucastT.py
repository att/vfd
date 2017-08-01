from tests import packet
import tests.helper_functions as hf
import pytest

@pytest.mark.valid_vlan_bcastT_mcastT_ucastT
def test_invalid_mac_valid_vlan(vf4_invalid_mac, vf4_valid_vlan):
    hf.read_sample_data('TARGET_VF4')
    pkt = hf.build_packet(dmac=vf4_invalid_mac, valid_vlan=int(vf4_valid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=8, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    # Here we dont recv packet because antispoff rules enabled, "iplex show all" will show the incremented spoof counter
    assert vlan == None

@pytest.mark.invalid_vlan_bcastT_mcastT_ucastT
def test_invalid_mac_invalid_vlan(vf4_invalid_mac, vf4_invalid_vlan):
    hf.read_sample_data('TARGET_VF4')
    pkt = hf.build_packet(dmac=vf4_invalid_mac, valid_vlan=int(vf4_invalid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=8, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == None
