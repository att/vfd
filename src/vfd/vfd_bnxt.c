
#include "vfd_bnxt.h"


#ifdef BNXT_SUPPORT

int  
vfd_bnxt_ping_vfs(uint8_t port_id, int16_t vf_id)
{
		/* TODO */
	bleat_printf( 0, "vfd_bnxt_ping_vfs(): not implemented for port=%d, vf=%d, qstart=%d ", port_id, vf_id );
	return 0;
	
	/*
	int diag = 0;
	int i;
	int vf_num = get_num_vfs( port_id );
	
	if (vf_id == -1)  // ping every VF
	{
		for (i = 0; i < vf_num; i++)
		{
			diag = rte_pmd_bnxt_ping_vfs(port_id, i);
			if (diag < 0) 
				bleat_printf( 0, "vfd_bnxt_ping_vfs failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, i, diag );
		}
	}
	else  // only specified
	{
		diag = rte_pmd_bnxt_ping_vfs(port_id, vf_id);
	}

	if (diag < 0) {
		bleat_printf( 0, "vfd_bnxt_ping_vfs failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "vfd_bnxt_ping_vfs successful: port_id=%d, vf_id=%d", port_id, vf_id);
	}
	return diag;
	*/
}


int 
vfd_bnxt_set_vf_mac_anti_spoof(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_bnxt_set_vf_mac_anti_spoof(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_mac_anti_spoof failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_mac_anti_spoof successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_bnxt_set_vf_vlan_anti_spoof(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	bleat_printf( 0, "vfd_bnxt_set_vf_vlan_anti_spoof(): not implemented for port=%d, vf=%d, qstart=%d, on/off=%d", port_id, vf_id, !!on );
	return 0;
	
	/*
	int diag = rte_pmd_bnxt_set_vf_vlan_anti_spoof(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_vlan_anti_spoof failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_vlan_anti_spoof successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}

	return diag;	
	*/
}


int 
vfd_bnxt_set_tx_loopback(uint8_t port_id, uint8_t on)
{
	int diag = rte_pmd_bnxt_set_tx_loopback(port_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_tx_loopback failed: port_pi=%d, on=%d) failed rc=%d", port_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_tx_loopback successful: port_id=%d, vf_id=%d", port_id, on);
	}
	
	return diag;	
}


int 
vfd_bnxt_set_vf_unicast_promisc(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	bleat_printf( 0, "vfd_bnxt_set_vf_unicast_promisc(): not implemented for port=%d, vf=%d, qstart=%d, on/off=%d", port_id, vf_id, !!on );
	return 0;
	
	/*
	int diag = rte_pmd_bnxt_set_vf_unicast_promisc(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_unicast_promisc failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_unicast_promisc successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;
	*/
}


int 
vfd_bnxt_set_vf_multicast_promisc(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	bleat_printf( 0, "vfd_bnxt_set_vf_multicast_promisc(): not implemented for port=%d, vf=%d, qstart=%d, on/off=%d", port_id, vf_id, !!on );
	return 0;
	
	/*
	int diag = rte_pmd_bnxt_set_vf_multicast_promisc(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_multicast_promisc failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_multicast_promisc successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;
	*/
}


int 
vfd_bnxt_set_vf_mac_addr(uint8_t port_id, uint16_t vf_id, struct ether_addr *mac_addr)
{
	int diag = rte_pmd_bnxt_set_vf_mac_addr(port_id, vf_id, mac_addr);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_mac_addr failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_mac_addr successful: port_id=%d, vf=%d", port_id, vf_id);
	}
	
	return diag;
}


int 
vfd_bnxt_set_vf_vlan_stripq(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_bnxt_set_vf_vlan_stripq(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_vlan_stripq failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_vlan_stripq successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_bnxt_set_vf_vlan_insert(uint8_t port_id, uint16_t vf_id, uint16_t vlan_id)
{
	int diag = rte_pmd_bnxt_set_vf_vlan_insert(port_id, vf_id, vlan_id);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_vlan_insert failed: port_pi=%d, vf_id=%d, vlan_id=%d) failed rc=%d", port_id, vf_id, vlan_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_vlan_insert successful: port_id=%d, vf=%d vlan_id=%d", port_id, vf_id, vlan_id);
	}
	
	return diag;	
}


int 
vfd_bnxt_set_vf_broadcast(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	
	bleat_printf( 0, "vfd_bnxt_set_vf_broadcast(): not implemented for port=%d, vf=%d, on/off=%d", port_id, vf_id, !!on );
	return 0;
	/*
	int diag = rte_pmd_bnxt_set_vf_broadcast(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_broadcas failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_broadcas successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
	*/
}


int 
vfd_bnxt_set_vf_vlan_tag(uint8_t port_id, uint16_t vf_id, uint8_t on)
{

	bleat_printf( 0, "vfd_bnxt_set_vf_vlan_tag(): not implemented for port=%d, vf=%d, on/off=%d", port_id, vf_id, !!on );
	return 0;

/*	
	int diag = rte_pmd_bnxt_set_vf_vlan_tag(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_vlan_tag failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_vlan_tag successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;		
*/
}


int 
vfd_bnxt_set_vf_vlan_filter(uint8_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on)
{
	bleat_printf( 0, "vfd_bnxt_set_vf_vlan_filter(): not implemented for port=%d, vf=%d, vlan_id=%d, on/off=%d", port_id, vlan_id, vf_mask, !!on );
	return 0;
	/*
	int diag = rte_pmd_bnxt_set_vf_vlan_filter(port_id, vlan_id, vf_mask, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_vlan_filter failed: port_pi=%d, vlan_id=%d) failed rc=%d", port_id, vlan_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_vlan_filter successful: port_id=%d, vlan_id=%d", port_id, vlan_id);
	}
	
	return diag;		
	*/
}


int 
vfd_bnxt_get_vf_stats(uint8_t port_id, uint16_t vf_id, __attribute__((__unused__)) struct rte_eth_stats *stats)
{
	bleat_printf( 0, "vfd_bnxt_get_vf_stats(): not implemented for port=%d, vf=%d, vf_id=%d", port_id, vf_id );
	return 0;
	
	/*
	int diag = rte_pmd_bnxt_get_vf_stats(port_id, vf_id, stats);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_stats failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_stats successful: port_id=%d, vf=%d on=%d", port_id, vf_id);
	}
	
	return diag;	
*/	
}


int 
vfd_bnxt_reset_vf_stats(uint8_t port_id, uint16_t vf_id)
{
	bleat_printf( 0, "vfd_bnxt_reset_vf_stats(): not implemented for port=%d, vf=%d, vf_id=%d", port_id, vf_id );
	return 0;
	/*
	int diag = rte_pmd_bnxt_reset_vf_stats(port_id, vf_id);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_reset_vf_stats failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_reset_vf_stats successful: port_id=%d, vf_id=%d", port_id, vf_id);
	}
	
	return diag;
	*/
}


/*
	Called when a 'mailbox' message is received.  Examine and take action based on
	the message type.
*/
void
vfd_bnxt_vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param)
{
	struct rte_pmd_bnxt_mb_event_param *p = param;
	struct input *req_base = p->msg;
	uint16_t vf = p->vf_id;
	uint16_t mbox_type = rte_le_to_cpu_16(req_base->req_type);
	bool add_refresh = false;

	/* check & process VF to PF mailbox message */
	switch (mbox_type) {
		/* Allow */
		case HWRM_VER_GET:
		case HWRM_FUNC_RESET:
		case HWRM_FUNC_QCAPS:
		case HWRM_FUNC_QCFG:
		case HWRM_FUNC_DRV_RGTR:
		case HWRM_FUNC_DRV_UNRGTR:
		case HWRM_PORT_PHY_QCFG:
		case HWRM_QUEUE_QPORTCFG:
		case HWRM_VNIC_ALLOC:
		case HWRM_VNIC_FREE:
		case HWRM_VNIC_CFG:
		case HWRM_VNIC_QCFG:
		case HWRM_VNIC_RSS_CFG:
		case HWRM_RING_ALLOC:
		case HWRM_RING_FREE:
		case HWRM_RING_GRP_ALLOC:
		case HWRM_RING_GRP_FREE:
		case HWRM_VNIC_RSS_COS_LB_CTX_ALLOC:
		case HWRM_VNIC_RSS_COS_LB_CTX_FREE:
		case HWRM_STAT_CTX_ALLOC:
		case HWRM_STAT_CTX_FREE:
		case HWRM_STAT_CTX_CLR_STATS:
		case HWRM_CFA_L2_FILTER_FREE:
		case HWRM_TUNNEL_DST_PORT_FREE:
			p->retval = RTE_PMD_BNXT_MB_EVENT_PROCEED;
			add_refresh = true;
			break;

		/* Disallowed */
		case HWRM_FUNC_BUF_UNRGTR:
		case HWRM_FUNC_VF_VNIC_IDS_QUERY:
		case HWRM_FUNC_BUF_RGTR:
		case HWRM_PORT_PHY_CFG:
		case HWRM_CFA_L2_FILTER_ALLOC:
		case HWRM_CFA_L2_FILTER_CFG:
		case HWRM_CFA_L2_SET_RX_MASK:
		case HWRM_TUNNEL_DST_PORT_ALLOC:
		case HWRM_EXEC_FWD_RESP:
		case HWRM_REJECT_FWD_RESP:
		default:
			p->retval = RTE_PMD_BNXT_MB_EVENT_NOOP_NACK;     // VM should see failure
			break;

		/* Verify */
	}

	if (add_refresh)
		add_refresh_queue(port_id, vf);		// schedule a complete refresh when the queue goes hot
	else
		restore_vf_setings(port_id, vf);	// refresh all of our configuration back onto the NIC

	bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %d",
	             type, port_id, vf, p->retval, mbox_type);
}

int 
vfd_bnxt_set_all_queues_drop_en(uint8_t port_id, uint8_t on)
{
	bleat_printf( 3, "vfd_bntx_set_all_queues_drop_en not implemented: port_id=%d, on=%d", port_id, on);
	/* TODO */
	return 0;	
}


uint32_t 
vfd_bnxt_get_pf_spoof_stats(uint8_t port_id)
{
	bleat_printf( 3, "vfd_bnxt_get_pf_spoof_stats: port_id=%d", port_id);
	/* TODO */
	return 0;
}


uint32_t 
vfd_bnxt_get_vf_spoof_stats(uint8_t port_id, uint16_t vf_id)
{
	/* not implemented */
	bleat_printf( 3, "vfd_bnxt_get_vf_spoof_stats not implemented: port_id=%d, on=%d", port_id, vf_id);
	/* TODO */
	return 0;
}

int 
vfd_bnxt_is_rx_queue_on(uint8_t port_id, uint16_t vf_id, __attribute__((__unused__)) int* mcounter)
{
	/* not implemented */
	bleat_printf( 3, "vfd_bnxt_is_rx_queue_on not implemented: port_id=%d, on=%d", port_id, vf_id);
	/* TODO */	
	
	return 0;
}


void 
vfd_bnxt_set_rx_drop(uint8_t port_id, uint16_t vf_id, int state)
{
	/* TODO */
	bleat_printf( 0, "vfd_bnxt_set_rx_drop(): not implemented for port=%d, vf=%d, qstart=%d on/off=%d", port_id, vf_id, !!state );
}


void 
vfd_bnxt_set_split_erop(uint8_t port_id, uint16_t vf_id, int state)
{
	/* TODO */
	bleat_printf( 0, "vfd_bnxt_set_split_erop(): not implemented for port=%d, vf=%d, qstart=%d on/off=%d", port_id, vf_id, !!state );	
}

int 
vfd_bnxt_dump_all_vlans(uint8_t port_id)
{
	/* TODO */
	bleat_printf( 0, "vfd_bnxt_dump_all_vlans(): not implemented for port=%d", port_id );	
	return 0;
}

#endif