
#include "sriov.h"

int  
vfd_i40e_ping_vfs(uint8_t port_id, int16_t vf_id)
{
	int diag = 0;
	int i;
	int vf_num = get_num_vfs( port_id );
	
	if (vf_id == -1)  // ping every VF
	{
		for (i = 0; i < vf_num; i++)
		{
			diag = rte_pmd_i40e_ping_vfs(port_id, i);
			if (diag < 0) 
				bleat_printf( 0, "rte_pmd_i40e_ping_vfs failed: (port_pi=%d, vf_id=%d) failed rc=%d", port_id, i, diag );
		}
	}
	else  // only specified
	{
		diag = rte_pmd_i40e_ping_vfs(port_id, vf_id);
	}

	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_ping_vfs failed: (port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_ping_vfs successful: port_id=%d, vf_id=%d", port_id, vf_id);
	}
	
	return diag;
}


int 
vfd_i40e_set_vf_mac_anti_spoof(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_i40e_set_vf_mac_anti_spoof(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_mac_anti_spoof failed: (port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_mac_anti_spoof successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_i40e_set_vf_vlan_anti_spoof(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_i40e_set_vf_vlan_anti_spoof(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_vlan_anti_spoof failed: (port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_vlan_anti_spoof successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_i40e_set_tx_loopback(uint8_t port_id, uint8_t on)
{
	int diag = rte_pmd_i40e_set_tx_loopback(port_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_tx_loopback failed: (port_pi=%d, on=%d) failed rc=%d", port_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_tx_loopback successful: port_id=%d, vf_id=%d", port_id, on);
	}
	
	return diag;	
}


int 
vfd_i40e_set_vf_unicast_promisc(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_i40e_set_vf_unicast_promisc(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_unicast_promisc failed: (port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_unicast_promisc successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;
}


int 
vfd_i40e_set_vf_multicast_promisc(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_i40e_set_vf_multicast_promisc(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_multicast_promisc failed: (port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_multicast_promisc successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;
}


int 
vfd_i40e_set_vf_mac_addr(uint8_t port_id, uint16_t vf_id,  __attribute__((__unused__)) struct ether_addr *mac_addr)
{
	int diag = 0;
	diag = rte_pmd_i40e_set_vf_mac_addr(port_id, vf_id, mac_addr);
		
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_mac_addr failed: (port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_mac_addr successful: port_id=%d, vf=%d", port_id, vf_id);
	}
	
	return diag;
}


int 
vfd_i40e_set_vf_vlan_stripq(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_i40e_set_vf_vlan_stripq(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_vlan_stripq failed: (port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_vlan_stripq successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_i40e_set_vf_vlan_insert(uint8_t port_id, uint16_t vf_id, uint16_t vlan_id)
{
	int diag = rte_pmd_i40e_set_vf_vlan_insert(port_id, vf_id, vlan_id);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_vlan_insert failed: (port_pi=%d, vf_id=%d, vlan_id=%d) failed rc=%d", port_id, vf_id, vlan_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_vlan_insert successful: port_id=%d, vf=%d vlan_id=%d", port_id, vf_id, vlan_id);
	}
	
	return diag;	
}


int 
vfd_i40e_set_vf_broadcast(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_i40e_set_vf_broadcast(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_broadcast failed: (port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_broadcast successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_i40e_allow_untagged(uint8_t port_id, uint16_t vf_id, uint8_t on)
{	
	int diag = 0;
	//int diag = rte_pmd_i40e_set_vf_vlan_untag_drop(port_id, vf_id, !on);  // don't allow untagged
	if (diag < 0) {
		bleat_printf( 0, "vfd_i40e_allow_untagged failed: (port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "vfd_i40e_allow_untagged successful: (port_id=%d, vf=%d on=%d)", port_id, vf_id, on);
	}
	
	return diag;		
}


int 
vfd_i40e_set_vf_vlan_filter(uint8_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on)
{
	int diag = rte_pmd_i40e_set_vf_vlan_filter(port_id, vlan_id, vf_mask, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_vlan_filter failed: (port_pi=%d, vlan_id=%d, vf_mask=%d) failed rc=%d", port_id, vlan_id, vf_mask, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_vlan_filter successful: (port_id=%d, vlan_id=%d, vf_mask=%d", port_id, vlan_id, vf_mask);
	}
	
	return diag;		
}


int 
vfd_i40e_get_vf_stats(uint8_t port_id, uint16_t vf_id, struct rte_eth_stats *stats)
{
	int diag = rte_pmd_i40e_get_vf_stats(port_id, vf_id, stats);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_stats failed: (port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_stats successful: (port_id=%d, vf=%d)", port_id, vf_id);
	}
	
	
	
	//printf("dropped: stats->oerrors: %15"PRIu64"\n", port_pci_reg_read(port_id, 0x00344000));
	
	//printf("dropped: stats->oerrors: %d\n", port_pci_reg_read(port_id, 0x00074000));
	
	return diag;			
}


int 
vfd_i40e_reset_vf_stats(uint8_t port_id, uint16_t vf_id)
{
	int diag = rte_pmd_i40e_reset_vf_stats(port_id, vf_id);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_reset_vf_stats failed: (port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_reset_vf_stats successful: (port_id=%d, vf_id=%d)", port_id, vf_id);
	}
	
	return diag;
}


int 
vfd_i40e_set_all_queues_drop_en(uint8_t port_id, uint8_t on)
{
	bleat_printf( 3, "vfd_i40e_set_all_queues_drop_en not implemented: port_id=%d, on=%d", port_id, on);

	return 0;	
}


/*
	Called when a 'mailbox' message is received.  Examine and take action based on
	the message type.
	
	
	on load i40evf kernel driver:
	I40E_VIRTCHNL_OP_VERSION
	I40E_VIRTCHNL_OP_GET_VF_RESOURCES
	I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP
	
	on ifconfig dev_name ip/mask
	I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS ucast mac
	I40E_VIRTCHNL_OP_ADD_VLAN 0 
	I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES
	I40E_VIRTCHNL_OP_ENABLE_QUEUES
	I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS ipv6 mcast mac 33 33 ff 
	
	
	
	on ifconfig dev_name up
	I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS ucast mac
	I40E_VIRTCHNL_OP_ADD_VLAN 0
	I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES
	I40E_VIRTCHNL_OP_ENABLE_QUEUES
	I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS ipv6 mcast mac 33 33 ff 


	on ifconfig dev_name down
	I40E_VIRTCHNL_OP_DISABLE_QUEUES
	I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS ucast mac
	I40E_VIRTCHNL_OP_DEL_VLAN 0

	on ifconfig dev_name mtu 9000
	I40E_VIRTCHNL_OP_RESET_VF
	I40E_VIRTCHNL_OP_GET_VF_RESOURCES
	I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP

	
  unbind driver/rmmpd i40evf
	I40E_VIRTCHNL_OP_RESET_VF
	
	bind vfio-pci driver
	Notting
	
	unbinf vfio-pci
	Nothing
	
	load dpdk app
	I40E_VIRTCHNL_OP_RESET_VF
	I40E_VIRTCHNL_OP_VERSION
	I40E_VIRTCHNL_OP_GET_VF_RESOURCES
	I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES
	I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP
	I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS ucast mac
	I40E_VIRTCHNL_OP_ENABLE_QUEUES
	I40E_VIRTCHNL_OP_ENABLE_QUEUES
	I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE
	
	unload dpdk app
	I40E_VIRTCHNL_OP_DISABLE_QUEUES
	I40E_VIRTCHNL_OP_DISABLE_QUEUES
	I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS ucasst mac
	I40E_VIRTCHNL_OP_RESET_VF

*/

struct i40e_virtchnl_promisc_info {
	u16 vsi_id;
	u16 flags;
};

#define I40E_FLAG_VF_UNICAST_PROMISC	0x00000001
#define I40E_FLAG_VF_MULTICAST_PROMISC	0x00000002


int
vfd_i40e_vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *data, void *param) {

	struct rte_pmd_ixgbe_mb_event_param *p;
	uint16_t vf;
	uint16_t mbox_type;
	uint32_t *msgbuf;
	struct ether_addr *new_mac;

	p = (struct rte_pmd_ixgbe_mb_event_param*) param;
	if( p == NULL ) {
		return 0;
	}
	vf = p->vfid;
	mbox_type = p->msg_type;
	msgbuf = (uint32_t *) p->msg;

	RTE_SET_USED(data);

	//AZprintf("------------------- MBOX port: %d, vf: %d, configured: %d box_type: %d-------------------\n", port_id, vf, running_config->ports[port_id].vfs[vf].num_vlans, mbox_type );
			

	/* check & process VF to PF mailbox message */
	switch (mbox_type) {
		case I40E_VIRTCHNL_OP_RESET_VF:
			bleat_printf( 1, "reset event received: port=%d", port_id );

			rte_spinlock_lock( &running_config->update_lock );
			running_config->ports[port_id].vfs[vf].rx_q_ready = 0;		// set queue ready flag off
			rte_spinlock_unlock( &running_config->update_lock );
			
			set_vf_allow_untagged(port_id, vf, 0);
			
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			
			bleat_printf( 3, "Port: %d, VF: %d, OUT: %d, _T: %s ",
				port_id, vf, p->retval, "I40E_VIRTCHNL_OP_RESET_VF");
			break;

		case I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS:
			bleat_printf( 1, "setmac event received: port=%d", port_id );
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;    						// do what's needed
			bleat_printf( 3, "Port: %d, VF: %d, OUT: %d, _T: %s ",
				port_id, vf, p->retval, "I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS");

			new_mac = (struct ether_addr *) (&msgbuf[1]);

			if (is_valid_assigned_ether_addr(new_mac)) {
				bleat_printf( 3, "setting ucast mac, vf %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);
			} else if (is_multicast_ether_addr(new_mac)){
				bleat_printf( 3, "setting mcast mac, vf %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);				
			} else {
				bleat_printf( 3, "setting mac, vf %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);				
			}
			break;

		case I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS:
			bleat_printf( 1, "setmac event received: port=%d", port_id );
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;    						// do what's needed			
			bleat_printf( 3, "Port: %d, VF: %d, OUT: %d, _T: %s ",
				port_id, vf, p->retval, "I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS");

			new_mac = (struct ether_addr *) (&msgbuf[1]);

			if (is_valid_assigned_ether_addr(new_mac)) {
				bleat_printf( 3, "deleting ucast mac, vf %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);
			} else if (is_multicast_ether_addr(new_mac)){
				bleat_printf( 3, "deleting mcast mac, vf %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);				
			} else {
				bleat_printf( 3, "deleting mac, vf %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);				
			}
			break;

		case I40E_VIRTCHNL_OP_ADD_VLAN:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_ADD_VLAN");
			if (0 == (int) msgbuf[1]){
				bleat_printf( 1, "vlan set event approved: port=%d vf=%d vlan=%d (responding ack)", port_id, vf, (int) msgbuf[1] );
				p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;     // allow to set VLAN 0
			} else if ( valid_vlan( port_id, vf, (int) msgbuf[1] )) {
				bleat_printf( 1, "vlan set event approved: port=%d vf=%d vlan=%d (responding ack)", port_id, vf, (int) msgbuf[1] );
				p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;     // good rc to VM while not changing anything
			} else {
				bleat_printf( 1, "vlan set event rejected; vlan not not configured: port=%d vf=%d vlan=%d (responding noop-ack)", port_id, vf, (int) msgbuf[1] );
				p->retval = RTE_PMD_I40E_MB_EVENT_NOOP_NACK;     // VM should see failure
			}
			break;
			
		case I40E_VIRTCHNL_OP_DEL_VLAN:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_DEL_VLAN");
			if (0 == (int) msgbuf[1]){
				bleat_printf( 1, "vlan delete event approved: port=%d vf=%d vlan=%d (responding ack)", port_id, vf, (int) msgbuf[1] );
				p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;     // allow to set VLAN 0
			} else if( valid_vlan( port_id, vf, (int) msgbuf[1] )) {
				bleat_printf( 1, "vlan delete event approved: port=%d vf=%d vlan=%d (responding ack)", port_id, vf, (int) msgbuf[1] );
				p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;     // good rc to VM while not changing anything
			} else {
				bleat_printf( 1, "vlan delete event rejected; vlan not not configured: port=%d vf=%d vlan=%d (responding noop-ack)", port_id, vf, (int) msgbuf[1] );
				p->retval = RTE_PMD_I40E_MB_EVENT_NOOP_NACK;     // VM should see failure
			}
			break;			
			
		case I40E_VIRTCHNL_OP_UNKNOWN:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_UNKNOWN");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_VERSION:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_VERSION");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_GET_VF_RESOURCES:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_GET_VF_RESOURCES");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_CONFIG_TX_QUEUE:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_CONFIG_TX_QUEUE");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_CONFIG_RX_QUEUE:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_CONFIG_RX_QUEUE");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_ENABLE_QUEUES:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_ENABLE_QUEUES");
			
			rte_spinlock_lock( &running_config->update_lock );
			running_config->ports[port_id].vfs[vf].rx_q_ready = 1;		// set queue ready flag on
			rte_spinlock_unlock( &running_config->update_lock );			
			
			add_refresh_queue(port_id, vf);
					
			// return NACK when VF isnt configured
			if (running_config->ports[port_id].vfs[vf].num_vlans < 1) 
				p->retval = RTE_PMD_I40E_MB_EVENT_NOOP_NACK;
			else
				p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			
			break;
		case I40E_VIRTCHNL_OP_DISABLE_QUEUES:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_DISABLE_QUEUES");
			
			rte_spinlock_lock( &running_config->update_lock );
			running_config->ports[port_id].vfs[vf].rx_q_ready = 0;		// set queue ready flag off
			rte_spinlock_unlock( &running_config->update_lock );		
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE");
								
			// return allowed promisc modes based on specified in config
			struct i40e_virtchnl_promisc_info *promisc = (struct i40e_virtchnl_promisc_info *)p->msg;
						
			if (running_config->ports[port_id].vfs[vf].allow_un_ucast) {
				promisc->flags &= I40E_FLAG_VF_UNICAST_PROMISC;
				bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "UCAST PROM ENABLE");
			} else {
				promisc->flags &= ~I40E_FLAG_VF_UNICAST_PROMISC;
				bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "UCAST PROM DISABLE");
			}
			
			if (running_config->ports[port_id].vfs[vf].allow_mcast) {
				promisc->flags &= I40E_FLAG_VF_MULTICAST_PROMISC;
				bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "MCAST PROM ENABLE");
			} else {
				promisc->flags &= ~I40E_FLAG_VF_MULTICAST_PROMISC;
				bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "MCAST PROM DISABLE");
			}
			
			
			
			bleat_printf(3, "Port: %d, VF: %d, _T: %s PCI: %s, PORT # %d", port_id, vf, "-----------------", running_config->ports[port_id].pciid, running_config->ports[port_id].rte_port_number);

			add_refresh_queue(port_id, vf);
			
			// return NACK when VF isn't configured
			if (running_config->ports[port_id].vfs[vf].num_vlans < 1) {
				p->retval = RTE_PMD_I40E_MB_EVENT_NOOP_NACK;
				bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "PROM VF NOT CONFIGURED");
			} else {
				p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			}

			break;
		case I40E_VIRTCHNL_OP_GET_STATS:
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED; 		// VF's driver is getting stats every 2 sec
			break;
		case I40E_VIRTCHNL_OP_FCOE:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_FCOE");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_EVENT:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_EVENT");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_CONFIG_RSS_KEY:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_CONFIG_RSS_KEY");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_CONFIG_RSS_LUT:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_CONFIG_RSS_LUT");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_GET_RSS_HENA_CAPS:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_GET_RSS_HENA_CAPS");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;
		case I40E_VIRTCHNL_OP_SET_RSS_HENA:
			bleat_printf(3, "Port: %d, VF: %d, _T: %s", port_id, vf, "I40E_VIRTCHNL_OP_SET_RSS_HENA");
			p->retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
			break;			
			
		default:
			bleat_printf( 1, "unknown  event request received: port=%d (responding nop+nak)", port_id );
			p->retval = RTE_PMD_I40E_MB_EVENT_NOOP_NACK;     /* noop & nack */
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, MBOX_TYPE: %d", type, port_id, vf, p->retval, mbox_type);
			break;
	}

	return 0;   // CAUTION:  as of 2017/07/05 it seems this value is ignored by dpdk, but it might not alwyas be
}

uint32_t 
vfd_i40e_get_pf_spoof_stats(uint8_t port_id)
{
	bleat_printf( 3, "vfd_i40e_get_pf_spoof_stats: port_id=%d", port_id);
	
	/* TODO */
	//return port_pci_reg_read(port_id, 0x08780);
	return 0;
}


uint32_t 
vfd_i40e_get_vf_spoof_stats(uint8_t port_id, uint16_t vf_id)
{
	/* not implemented */
	bleat_printf( 3, "vfd_i40e_get_vf_spoof_stats not implemented: port_id=%d, on=%d", port_id, vf_id);
	/* TODO */
	return 0;
}


int 
vfd_i40e_is_rx_queue_on(uint8_t port_id, uint16_t vf_id, __attribute__((__unused__)) int* mcounter)
{

	bleat_printf( 5, "vfd_i40e_is_rx_queue_on:  port=%d  vfid_id=%d, on=%d)",  port_id, vf_id, running_config->ports[port_id].vfs[vf_id].rx_q_ready);

	if( running_config->ports[port_id].vfs[vf_id].rx_q_ready) {
  		bleat_printf( 3, "i40e queue active:  port=%d vfid_id=%d)", port_id, vf_id);
		return 1;
	} else {
		if( mcounter != NULL ) {
			if( (*mcounter % 100 ) == 0 ) {
  				bleat_printf( 4, "is_queue_en: still pending: queue not active: port=%d vfid_id=%d, vf=%d", port_id, vf_id);
			}
			(*mcounter)++;
		}
		return 0;
	}	
}


void 
vfd_i40e_set_pfrx_drop(uint8_t port_id,  __attribute__((__unused__)) int state)
{
	/* TODO */
	bleat_printf( 0, "vfd_i40e_set_pfrx_drop(): not implementede for port %d", port_id);
}


void 
vfd_i40e_set_rx_drop(uint8_t port_id, uint16_t vf_id, __attribute__((__unused__)) int state)
{
	/* TODO */
	bleat_printf( 0, "vfd_i40e_set_pfrx_drop(): not implemented for port=%d, vf=%d", port_id, vf_id);
}


void 
vfd_i40e_set_split_erop(uint8_t port_id, uint16_t vf_id, __attribute__((__unused__)) int state)
{
	/* TODO */
	bleat_printf( 0, "vfd_i40e_set_split_erop(): not implemented for port=%d, vf=%d", port_id, vf_id );	
}


int 
vfd_i40e_get_split_ctlreg(uint8_t port_id, uint16_t vf_id)
{
	/* not implemented */
	bleat_printf( 3, "vfd_i40e_get_split_ctlreg not implemented: port_id=%d, vd_id=%d", port_id, vf_id);
	/* TODO */	
	
	return 0;
}

int 
vfd_i40e_dump_all_vlans(uint8_t port_id)
{
	/* TODO */
	bleat_printf( 0, "vfd_i40e_dump_all_vlans(): not implemented for port=%d", port_id );	
	return 0;
}


