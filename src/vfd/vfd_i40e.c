
#include "vfd_i40e.h"



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
				bleat_printf( 0, "rte_pmd_i40e_ping_vfs failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, i, diag );
		}
	}
	else  // only specified
	{
		diag = rte_pmd_i40e_ping_vfs(port_id, vf_id);
	}

	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_ping_vfs failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
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
		bleat_printf( 0, "rte_pmd_i40e_set_vf_mac_anti_spoof failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
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
		bleat_printf( 0, "rte_pmd_i40e_set_vf_vlan_anti_spoof failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
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
		bleat_printf( 0, "rte_pmd_i40e_set_tx_loopback failed: port_pi=%d, on=%d) failed rc=%d", port_id, on, diag );
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
		bleat_printf( 0, "rte_pmd_i40e_set_vf_unicast_promisc failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
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
		bleat_printf( 0, "rte_pmd_i40e_set_vf_multicast_promisc failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_multicast_promisc successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;
}


int 
vfd_i40e_set_vf_mac_addr(uint8_t port_id, uint16_t vf_id, struct ether_addr *mac_addr)
{
	int diag = rte_pmd_i40e_set_vf_mac_addr(port_id, vf_id, mac_addr);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_mac_addr failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
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
		bleat_printf( 0, "rte_pmd_i40e_set_vf_vlan_stripq failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
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
		bleat_printf( 0, "rte_pmd_i40e_set_vf_vlan_insert failed: port_pi=%d, vf_id=%d, vlan_id=%d) failed rc=%d", port_id, vf_id, vlan_id, diag );
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
		bleat_printf( 0, "rte_pmd_i40e_set_vf_broadcas failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_broadcas successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_i40e_set_vf_vlan_tag(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_i40e_set_vf_vlan_tag(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_vlan_tag failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_vlan_tag successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;		
}


int 
vfd_i40e_set_vf_vlan_filter(uint8_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on)
{
	int diag = rte_pmd_i40e_set_vf_vlan_filter(port_id, vlan_id, vf_mask, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_vlan_filter failed: port_pi=%d, vlan_id=%d) failed rc=%d", port_id, vlan_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_vlan_filter successful: port_id=%d, vlan_id=%d", port_id, vlan_id);
	}
	
	return diag;		
}


int 
vfd_i40e_get_vf_stats(uint8_t port_id, uint16_t vf_id, struct rte_eth_stats *stats)
{
	int diag = rte_pmd_i40e_get_vf_stats(port_id, vf_id, stats);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_set_vf_stats failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_set_vf_stats successful: port_id=%d, vf=%d on=%d", port_id, vf_id);
	}
	
	return diag;			
}


int 
vfd_i40e_reset_vf_stats(uint8_t port_id, uint16_t vf_id)
{
	int diag = rte_pmd_i40e_reset_vf_stats(port_id, vf_id);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_i40e_reset_vf_stats failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_i40e_reset_vf_stats successful: port_id=%d, vf_id=%d", port_id, vf_id);
	}
	
	return diag;
}


int 
vfd_i40e_set_all_queues_drop_en(uint8_t port_id, uint8_t on)
{
	bleat_printf( 3, "vfd_i40e_set_all_queues_drop_en not implemented: port_id=%d, on=%d", port_id, on);

	return 0;	
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
	/* not implemented */
	bleat_printf( 3, "vfd_i40e_is_rx_queue_on not implemented: port_id=%d, vf=%d", port_id, vf_id);
	/* TODO */	
	
	return 0;
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
	bleat_printf( 3, "vfd_i40e_get_split_ctlreg not implemented: port_id=%d, on=%d", port_id, vf_id);
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
