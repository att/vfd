from tests.parse_sample_data import section_map
import pytest

vf1_macs_list = [mac.strip() for mac in section_map('TARGET_VF1')['mac_list'].split(',')]
vf1_invalid_macs_list = [mac.strip() for mac in section_map('TARGET_VF1')['invalid_macs'].split(',')]
vf1_valid_vlans_list = [vlan.strip() for vlan in section_map('TARGET_VF1')['valid_vlans'].split(',')]
vf1_invalid_vlans_list = [vlan.strip() for vlan in section_map('TARGET_VF1')['invalid_vlans'].split(',')]

vf2_macs_list = [mac.strip() for mac in section_map('TARGET_VF2')['mac_list'].split(',')]
vf2_invalid_macs_list = [mac.strip() for mac in section_map('TARGET_VF2')['invalid_macs'].split(',')]
vf2_valid_vlans_list = [vlan.strip() for vlan in section_map('TARGET_VF2')['valid_vlans'].split(',')]
vf2_invalid_vlans_list = [vlan.strip() for vlan in section_map('TARGET_VF2')['invalid_vlans'].split(',')]

vf3_macs_list = [mac.strip() for mac in section_map('TARGET_VF3')['mac_list'].split(',')]
vf3_invalid_macs_list = [mac.strip() for mac in section_map('TARGET_VF3')['invalid_macs'].split(',')]
vf3_valid_vlans_list = [vlan.strip() for vlan in section_map('TARGET_VF3')['valid_vlans'].split(',')]
vf3_invalid_vlans_list = [vlan.strip() for vlan in section_map('TARGET_VF3')['invalid_vlans'].split(',')]

vf4_macs_list = [mac.strip() for mac in section_map('TARGET_VF4')['mac_list'].split(',')]
vf4_invalid_macs_list = [mac.strip() for mac in section_map('TARGET_VF4')['invalid_macs'].split(',')]
vf4_valid_vlans_list = [vlan.strip() for vlan in section_map('TARGET_VF4')['valid_vlans'].split(',')]
vf4_invalid_vlans_list = [vlan.strip() for vlan in section_map('TARGET_VF4')['invalid_vlans'].split(',')]

@pytest.fixture(params=vf1_macs_list)
def vf1_mac(request):
    return request.param

@pytest.fixture(params=vf1_invalid_macs_list)
def vf1_invalid_mac(request):
	return request.param

@pytest.fixture(params=vf1_valid_vlans_list)
def vf1_valid_vlan(request):
    return request.param

@pytest.fixture(params=vf1_invalid_vlans_list)
def vf1_invalid_vlan(request):
    return request.param

@pytest.fixture(params=vf2_macs_list)
def vf2_mac(request):
    return request.param

@pytest.fixture(params=vf2_invalid_macs_list)
def vf2_invalid_mac(request):
	return request.param

@pytest.fixture(params=vf2_valid_vlans_list)
def vf2_valid_vlan(request):
    return request.param

@pytest.fixture(params=vf2_invalid_vlans_list)
def vf2_invalid_vlan(request):
    return request.param

@pytest.fixture(params=vf3_macs_list)
def vf3_mac(request):
    return request.param

@pytest.fixture(params=vf3_invalid_macs_list)
def vf3_invalid_mac(request):
	return request.param

@pytest.fixture(params=vf3_valid_vlans_list)
def vf3_valid_vlan(request):
    return request.param

@pytest.fixture(params=vf3_invalid_vlans_list)
def vf3_invalid_vlan(request):
    return request.param

@pytest.fixture(params=vf4_macs_list)
def vf4_mac(request):
    return request.param

@pytest.fixture(params=vf4_invalid_macs_list)
def vf4_invalid_mac(request):
	return request.param

@pytest.fixture(params=vf4_valid_vlans_list)
def vf4_valid_vlan(request):
    return request.param

@pytest.fixture(params=vf4_invalid_vlans_list)
def vf4_invalid_vlan(request):
    return request.param
