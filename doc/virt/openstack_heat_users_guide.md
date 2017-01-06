VFd Openstack HEAT Integration User Guide
-----------------------------------------

The patches provided in the virt/openstack-mos-kilo directory of the
VFd source tree provide patches for AT&T's version of Openstack
(called AIC). AIC is based on Mirantis OpenStack (MOS), and so, these
patches should work with vanilla Openstack with one notable exception.
Vanilla Openstack uses ML2 plugins for VF/vNIC config but since AIC
uses a monolithic contrail plugin for as its neutron network provider,
a ML2 approach is not feasible. Instead, the patches modify nova's
vif.py directly to create VFd VF config files and invoke the iplex
command to allow VFd to configure them.

Using these patches, the tenant can configure VFd VFs through
attributes specified through HEAT, and attached to a neutron port
definition.

The first step is to create a neutron provider network and subnet that
is based on the physnet that is associated with the SR-IOV NIC (see
the associated configuration guide on how to set up the physnet-NIC
association when setting up OpenStack).

	resources:
		my_net:
			type: OS::Neutron::ProviderNet
			properties:
				provider:network_type: flat
					name: "my_net"
					network_type: flat
					physical_network: physnet

	    my_subnet:
			type: OS::Neutron::Subnet
			properties:
				name: "my_subnet"
			    network: "my_net"
					
Then, neutron ports can be created against this subnet. VFd per-VF
parameters are specified in the port definition as follows.

	heat_template_version: 2015-04-30
	resources:
		server1_port:
			type: OS::Neutron::Port
			properties:
				name: test_port
				network_id: my_subnet
				ATT_VF_VLAN_FILTER: [100]
				ATT_VF_MAC_FILTER: ['aa:aa:aa:aa:aa:aa']
				ATT_VF_VLAN_STRIP: true
				ATT_VF_BROADCAST_ALLOW: true
				ATT_VF_UNKNOWN_MULTICAST_ALLOW: true
				ATT_VF_UNKNOWN_UNICAST_ALLOW: true
				ATT_VF_INSERT_STAG: true
				ATT_VF_LINK_STATUS: "auto"
				ATT_VF_MAC_ANTI_SPOOF_CHECK: true
				ATT_VF_VLAN_ANTI_SPOOF_CHECK: true
				binding:vnic_type: direct
 
See the VFd users guide in the doc/operations/vf_config.md of the VFd
source tree for documentation on the parameter values accepted by VFd.

