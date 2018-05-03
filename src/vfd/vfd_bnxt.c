
#include "vfd_bnxt.h"



int  
vfd_bnxt_ping_vfs(uint16_t port_id, int16_t vf_id)
{
		/* TODO */
	bleat_printf( 0, "vfd_bnxt_ping_vfs(): not implemented for port=%d, vf=%d, qstart=%d ", port_id, vf_id );
	return 0;
}


int 
vfd_bnxt_set_vf_mac_anti_spoof(uint16_t port_id, uint16_t vf_id, uint8_t on)
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
vfd_bnxt_set_vf_vlan_anti_spoof(uint16_t port_id, uint16_t vf_id, uint8_t on)
{


	int diag = rte_pmd_bnxt_set_vf_vlan_anti_spoof(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_vlan_anti_spoof failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_vlan_anti_spoof successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}

	return diag;	
}


int 
vfd_bnxt_set_tx_loopback(uint16_t port_id, uint8_t on)
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
vfd_bnxt_set_vf_unicast_promisc(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_bnxt_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_HASH_UC,(uint8_t) on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_unicast_promisc failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_unicast_promisc successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;
}


int 
vfd_bnxt_set_vf_multicast_promisc(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_bnxt_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_MULTICAST,(uint8_t) on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_multicast_promisc failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_multicast_promisc successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;
}


/*
	This adds a MAC address to the Rx whitelist.  See vfd_bnxt_set_vf_default_mac_addr() 
	for adding a MAC as the default (guest visible) MAC address.
*/
int 
vfd_bnxt_set_vf_mac_addr(uint16_t port_id, uint16_t vf_id, struct ether_addr *mac_addr)
{
	int diag = rte_pmd_bnxt_mac_addr_add(port_id, mac_addr, vf_id );
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_mac_addr failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_mac_addr successful: port_id=%d, vf=%d", port_id, vf_id);
	}

	return diag;
}

/*
	Set the default rx MAC address for the pf/vf. This is the address that will be visible into the guest.
*/
int vfd_bnxt_set_vf_default_mac_addr( uint16_t port_id, uint16_t vf_id, struct ether_addr *mac_addr ) {
	int diag;

	diag  = rte_pmd_bnxt_set_vf_mac_addr(port_id, vf_id, mac_addr);

	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_default_mac_addr failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_default_mac_addr successful: port_id=%d, vf=%d", port_id, vf_id);
	}
	
	return diag;
}




int 
vfd_bnxt_set_vf_vlan_stripq(uint16_t port_id, uint16_t vf_id, uint8_t on)
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
vfd_bnxt_set_vf_vlan_insert(uint16_t port_id, uint16_t vf_id, uint16_t vlan_id)
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
vfd_bnxt_set_vf_broadcast(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_bnxt_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_BROADCAST,(uint8_t) on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_broadcas failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_broadcas successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_bnxt_set_vf_vlan_tag(uint16_t port_id, uint16_t vf_id, uint8_t on)
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
vfd_bnxt_set_vf_vlan_filter(uint16_t port_id, uint16_t vlan_id, __attribute__((__unused__)) uint64_t vf_mask,  __attribute__((__unused__)) uint8_t on)
{

	int diag = rte_pmd_bnxt_set_vf_vlan_filter(port_id, vlan_id, vf_mask, on);
	
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_vlan_filter failed: port_pi=%d, vlan_id=%d) failed rc=%d", port_id, vlan_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_vlan_filter successful: port_id=%d, vlan_id=%d", port_id, vlan_id);
	}
	
	return diag;		

}


int 
vfd_bnxt_get_vf_stats(uint16_t port_id, uint16_t vf_id, struct rte_eth_stats *stats)
{
	int diag = rte_pmd_bnxt_get_vf_stats(port_id, vf_id, stats);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_get_vf_stats failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_get_vf_stats successful: port_id=%d, vf=%d on=%d", port_id, vf_id);
	}
	
	return diag;	

}


int 
vfd_bnxt_reset_vf_stats(uint16_t port_id, uint16_t vf_id)
{
	int diag = rte_pmd_bnxt_reset_vf_stats(port_id, vf_id);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_reset_vf_stats failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_reset_vf_stats successful: port_id=%d, vf_id=%d", port_id, vf_id);
	}
	
	return diag;

}


/*
	Called when a 'mailbox' message is received.  Examine and take action based on
	the message type.
*/
static bool verify_mac_address(uint16_t port_id, uint16_t vf, void *mac, void *mask)
{
	struct vf_s *vf_cfg = suss_vf(port_id, vf);
	struct ether_addr mac_addr;
	int i;

	if (vf_cfg == NULL)
		return false;

	/* Don't allow MAC masks */
	if (mask && memcmp(mask, "\xff\xff\xff\xff\xff\xff", 6))
		return false;

	/* TODO: This assumes that MAC anti-spoof isn't strict when there's no MACs configured */
	if (vf_cfg->num_macs == 0)
		return true;

	for (i=0; i<vf_cfg->num_macs; i++) {
		ether_aton_r(vf_cfg->macs[i], &mac_addr);
		if (memcmp(&mac_addr, mac, sizeof(mac_addr)) == 0)
			return true;
	}

	// must run in reverse order because of FV oddness
	for( i = vf_cfg->num_macs; i >= vf_cfg->first_mac; i-- ) {
		ether_aton_r(vf_cfg->macs[i], &mac_addr);
		if (memcmp(&mac_addr, mac, sizeof(mac_addr)) == 0)
			return true;
	}

	return false;
}

static void apply_rx_restrictions(uint16_t port_id, uint16_t vf, struct hwrm_cfa_l2_set_rx_mask_input *mi)
{
	struct vf_s *vf_cfg = suss_vf(port_id, vf);

	/* Can't find the config, disallow all traffic */
	if (vf_cfg == NULL) {
		mi->mask &= ~(HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_MCAST |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS);
		return;
	}
	if (!vf_cfg->allow_bcast) {
		mi->mask &= ~(HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS);
	}
	if (!vf_cfg->allow_mcast) {
		mi->mask &= ~(HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS);
		mi->mask |= HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_MCAST;
		mi->num_mc_entries = 0;
	}
	if (!vf_cfg->allow_un_ucast)
		mi->mask &= ~HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS;
}


int
vfd_bnxt_vf_msb_event_callback(uint16_t port_id, enum rte_eth_event_type type, void *data, void *param)
{
	struct rte_pmd_bnxt_mb_event_param *p;
	struct input *req_base;
	uint16_t vf;
	uint16_t mbox_type;
	bool add_refresh = false;
	bool restore = false;

	p = param;
	if( p == NULL ) {
		return 0;
	}

	req_base = p->msg;
	if( req_base == NULL ) {
		return 0;
	}

	vf = p->vf_id;
	mbox_type = rte_le_to_cpu_16(req_base->req_type);

	RTE_SET_USED(data);

	/* check & process VF to PF mailbox message */
	switch (mbox_type) {
		/* Allow and trigger a refresh */
		case HWRM_FUNC_VF_CFG:
		{
			struct hwrm_func_vf_cfg_input *vcfg = p->msg;

			if (vcfg->enables & rte_cpu_to_le_32(HWRM_FUNC_VF_CFG_INPUT_ENABLES_DFLT_MAC_ADDR)) {
				if (verify_mac_address(port_id, vf, vcfg->dflt_mac_addr, NULL))
					p->retval = RTE_PMD_BNXT_MB_EVENT_PROCEED;
				else
					p->retval = RTE_PMD_BNXT_MB_EVENT_NOOP_NACK;
			}
			else {
				p->retval = RTE_PMD_BNXT_MB_EVENT_PROCEED;
			}
			add_refresh = true;
			break;
		}
		case HWRM_CFA_L2_FILTER_ALLOC:
		{
			struct hwrm_cfa_l2_filter_alloc_input *l2a = p->msg;

			if (verify_mac_address(port_id, vf, l2a->l2_addr, l2a->enables & HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK ? l2a->l2_addr_mask : NULL))
				p->retval = RTE_PMD_BNXT_MB_EVENT_PROCEED;
			else
				p->retval = RTE_PMD_BNXT_MB_EVENT_NOOP_NACK;
			add_refresh = true;
			break;
		}
		case HWRM_CFA_L2_SET_RX_MASK:
		{
			struct hwrm_cfa_l2_set_rx_mask_input *mi = p->msg;
			apply_rx_restrictions(port_id, vf, mi);
			add_refresh = true;
			p->retval = RTE_PMD_BNXT_MB_EVENT_PROCEED;
			break;
		}

		case HWRM_VNIC_CFG:								// simple allow w/ refresh cases
		case HWRM_FUNC_RESET:
		case HWRM_VNIC_PLCMODES_CFG:
		case HWRM_TUNNEL_DST_PORT_ALLOC:
		case HWRM_TUNNEL_DST_PORT_FREE:
		case HWRM_VNIC_TPA_CFG:
		case HWRM_VNIC_RSS_CFG:
		case HWRM_CFA_L2_FILTER_FREE:
		case HWRM_VNIC_ALLOC:
		case HWRM_VNIC_FREE:
		case HWRM_VNIC_RSS_COS_LB_CTX_ALLOC:
		case HWRM_VNIC_RSS_COS_LB_CTX_FREE:
		case HWRM_CFA_L2_FILTER_CFG:
		case HWRM_CFA_TUNNEL_FILTER_ALLOC:
		case HWRM_CFA_TUNNEL_FILTER_FREE:
		case HWRM_TUNNEL_DST_PORT_QUERY:
			add_refresh = true;
			p->retval = RTE_PMD_BNXT_MB_EVENT_PROCEED;
			break;
			
		/* Allow */
		case HWRM_VER_GET:
		case HWRM_FUNC_DRV_RGTR:
		case HWRM_FUNC_DRV_UNRGTR:
		case HWRM_FUNC_QCAPS:
		case HWRM_FUNC_QCFG:
		case HWRM_FUNC_QSTATS:
		case HWRM_FUNC_DRV_QVER:
		case HWRM_PORT_PHY_QCFG:
		case HWRM_PORT_MAC_QCFG:
		case HWRM_PORT_PHY_QCAPS:
		case HWRM_QUEUE_QPORTCFG:
		case HWRM_QUEUE_PFCENABLE_QCFG:
		case HWRM_QUEUE_PRI2COS_QCFG:
		case HWRM_QUEUE_COS2BW_QCFG:
		case HWRM_RING_ALLOC:
		case HWRM_RING_FREE:
		case HWRM_RING_CMPL_RING_QAGGINT_PARAMS:
		case HWRM_RING_CMPL_RING_CFG_AGGINT_PARAMS:
		case HWRM_RING_RESET:
		case HWRM_RING_GRP_ALLOC:
		case HWRM_RING_GRP_FREE:
		case HWRM_VNIC_QCFG:
		case HWRM_VNIC_RSS_QCFG:
		case HWRM_VNIC_PLCMODES_QCFG:
		case HWRM_VNIC_QCAPS:
		case HWRM_STAT_CTX_ALLOC:
		case HWRM_STAT_CTX_FREE:
		//case 0xc8:
			p->retval = RTE_PMD_BNXT_MB_EVENT_PROCEED;
			break;

		/* Disallowed */
		case HWRM_FUNC_BUF_UNRGTR:
		case HWRM_FUNC_GETFID:
		case HWRM_FUNC_VF_ALLOC:
		case HWRM_FUNC_VF_FREE:
		case HWRM_FUNC_CFG:
		case HWRM_FUNC_VF_RESC_FREE:
		case HWRM_FUNC_VF_VNIC_IDS_QUERY:
		case HWRM_FUNC_BUF_RGTR:
		case HWRM_PORT_PHY_CFG:
		case HWRM_PORT_MAC_CFG:
		case HWRM_PORT_QSTATS:
		case HWRM_PORT_LED_CFG:
		case HWRM_PORT_LED_QCFG:
		case HWRM_PORT_LED_QCAPS:
		case HWRM_QUEUE_CFG:
		case HWRM_QUEUE_PFCENABLE_CFG:
		case HWRM_QUEUE_PRI2COS_CFG:
		case HWRM_QUEUE_COS2BW_CFG:
		case HWRM_PORT_LPBK_QSTATS:
		case HWRM_CFA_NTUPLE_FILTER_ALLOC:
		case HWRM_CFA_NTUPLE_FILTER_FREE:
		case HWRM_CFA_NTUPLE_FILTER_CFG:
		case HWRM_FW_RESET:
		case HWRM_FW_QSTATUS:
		case HWRM_EXEC_FWD_RESP:
		case HWRM_REJECT_FWD_RESP:
		case HWRM_FWD_RESP:
		case HWRM_FWD_ASYNC_EVENT_CMPL:
		case HWRM_TEMP_MONITOR_QUERY:
		case HWRM_WOL_FILTER_ALLOC:
		case HWRM_WOL_FILTER_FREE:
		case HWRM_WOL_FILTER_QCFG:
		case HWRM_WOL_REASON_QCFG:
		case HWRM_DBG_DUMP:
		case HWRM_NVM_VALIDATE_OPTION:
		case HWRM_NVM_FLUSH:
		case HWRM_NVM_GET_VARIABLE:
		case HWRM_NVM_SET_VARIABLE:
		case HWRM_NVM_INSTALL_UPDATE:
		case HWRM_NVM_MODIFY:
		case HWRM_NVM_VERIFY_UPDATE:
		case HWRM_NVM_GET_DEV_INFO:
		case HWRM_NVM_ERASE_DIR_ENTRY:
		case HWRM_NVM_MOD_DIR_ENTRY:
		case HWRM_NVM_FIND_DIR_ENTRY:
		case HWRM_NVM_GET_DIR_ENTRIES:
		case HWRM_NVM_GET_DIR_INFO:
		case HWRM_NVM_RAW_DUMP:
		case HWRM_NVM_READ:
		case HWRM_NVM_WRITE:
		case HWRM_NVM_RAW_WRITE_BLK:
		case HWRM_FUNC_CLR_STATS:
		case HWRM_STAT_CTX_CLR_STATS:
		default:
			p->retval = RTE_PMD_BNXT_MB_EVENT_NOOP_NACK;     // VM should see failure
			break;

		/* Verify */
	}

	if (add_refresh)
		add_refresh_queue(port_id, vf);		// schedule a complete refresh when the queue goes hot
	if (restore)
		restore_vf_setings(port_id, vf);	// refresh all of our configuration back onto the NIC

	bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %d",
	             type, port_id, vf, p->retval, mbox_type);

	return 0;   // CAUTION:  as of 2017/07/05 it seems this value is ignored by dpdk, but it might not alwyas be
}



int 
vfd_bnxt_allow_untagged(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	/* not implemented */
	bleat_printf( 3, "vfd_bnxt_allow_untagged not implemented: port_id=%d, vf_id=%d, on=%d", port_id, vf_id, on);
	return 0;	
}


int 
vfd_bnxt_set_all_queues_drop_en(uint16_t port_id, uint8_t on)
{
	int diag  = rte_pmd_bnxt_set_all_queues_drop_en( port_id, on );			

	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_all_queues_drop_en failed: port_pi=%d, vf_id=%d) failed rc=%d", port_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_all_queues_drop_en successful: port_id=%d, vf_id=%d", port_id, on);
	}
	return diag;	
}


uint32_t
vfd_bnxt_get_pf_spoof_stats(uint16_t port_id)
{
	uint64_t spoffed = 0;
	bleat_printf( 3, "vfd_bnxt_get_pf_spoof_stats: port_id=%d", port_id);

	struct rte_eth_xstat *xstats;
	int cnt_xstats, idx_xstat;
	struct rte_eth_xstat_name *xstats_names;

	/* Get count */
	cnt_xstats = rte_eth_xstats_get_names(port_id, NULL, 0);
	if (cnt_xstats  < 0) {
		printf("Error: Cannot get count of xstats\n");
		return 0;
	}

	/* Get id-name lookup table */
	xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * cnt_xstats);
	if (xstats_names == NULL) {
		printf("Cannot allocate memory for xstats lookup\n");
		return 0;
	}
	if (cnt_xstats != rte_eth_xstats_get_names(
		port_id, xstats_names, cnt_xstats)) {
		printf("Error: Cannot get xstats lookup\n");
		free(xstats_names);
		return 0;
	}

	/* Get stats themselves */
	xstats = malloc(sizeof(struct rte_eth_xstat) * cnt_xstats);
	if (xstats == NULL) {
		printf("Cannot allocate memory for xstats\n");
		free(xstats_names);
		return 0;
	}
	if (cnt_xstats != rte_eth_xstats_get(port_id, xstats, cnt_xstats)) {
		printf("Error: Unable to get xstats\n");
		free(xstats_names);
		free(xstats);
		return 0;
	}

	/* Display xstats */
	for (idx_xstat = 0; idx_xstat < cnt_xstats; idx_xstat++) {
		if (memcmp(&xstats_names[idx_xstat].name, "tx_drop_pkts",
		    strlen("tx_drop_pkts")) == 0)
			spoffed = xstats[idx_xstat].value;
	}
	free(xstats_names);
	free(xstats);
	return spoffed;
}

uint32_t
vfd_bnxt_get_vf_spoof_stats(uint16_t port_id, uint16_t vf_id)
{
	uint64_t vf_spoffed = 0;
	bleat_printf( 3, "vfd_bnxt_get_vf_spoof_stats not implemented: port_id=%d, on=%d", port_id, vf_id);
	rte_pmd_bnxt_get_vf_tx_drop_count(port_id, vf_id, &vf_spoffed);

	return vf_spoffed;
}

int 
vfd_bnxt_is_rx_queue_on(uint16_t port_id, uint16_t vf_id, __attribute__((__unused__)) int* mcounter)
{
	int queues = rte_pmd_bnxt_get_vf_rx_status(port_id, vf_id);
	
	if (queues > 0) {
		bleat_printf( 3, "%d queues active: port=%d vfid_id=%d)", queues, port_id, vf_id);
		return 1;
	}

	return 0;
}


void 
vfd_bnxt_set_rx_drop(uint16_t port_id, uint16_t vf_id, int state)
{
	/* TODO */
	bleat_printf( 0, "vfd_bnxt_set_rx_drop(): not implemented for port=%d, vf=%d, qstart=%d on/off=%d", port_id, vf_id, !!state );
}


void 
vfd_bnxt_set_split_erop(uint16_t port_id, uint16_t vf_id, int state)
{
	/* TODO */
	bleat_printf( 0, "vfd_bnxt_set_split_erop(): not implemented for port=%d, vf=%d, qstart=%d on/off=%d", port_id, vf_id, !!state );	
}

int 
vfd_bnxt_dump_all_vlans(uint16_t port_id)
{
	/* TODO */
	bleat_printf( 0, "vfd_bnxt_dump_all_vlans(): not implemented for port=%d", port_id );	
	return 0;
}
