from tests import packet
import tests.helper_functions as hf
import pytest

@pytest.mark.valid_vlan_bcastF_mcastT_ucastF
def test_bcast_valid_vlan(vf2_valid_vlan):
    hf.read_sample_data('TARGET_VF2')
    pkt = hf.build_packet(dmac=hf.config['bcast_mac'], valid_vlan=int(vf2_valid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=8, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == None

@pytest.mark.invalid_vlan_bcastF_mcastT_ucastF
def test_bcast_invalid_vlan(vf2_invalid_vlan):
    hf.read_sample_data('TARGET_VF2')
    pkt = hf.build_packet(dmac=hf.config['bcast_mac'], valid_vlan=int(vf2_invalid_vlan))
    inst = packet.sniff_packets(hf.config['iface'], timeout=8, filters=[{'layer': 'ether', 'config': {'type': '0x8100'}}])
    pkt.send_pkt(tx_port=hf.config['iface'], count=1)
    pkts = packet.load_sniff_packets(inst)
    vlan = None
    for pkt in pkts:
        vlan = pkt.pktgen.strip_vlan('vlan')
        break
    assert vlan == None
