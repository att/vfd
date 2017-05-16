
#include "vfd_bnxt.h"


#ifdef BNXT_SUPPORT

int  
vfd_bnxt_ping_vfs(uint8_t port_id, int16_t vf_id)
{
		/* TODO */
	bleat_printf( 0, "vfd_bnxt_ping_vfs(): not implemented for port=%d, vf=%d, qstart=%d ", port_id, vf_id );
	return 0;
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


	int diag = rte_pmd_bnxt_set_vf_vlan_anti_spoof(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_vlan_anti_spoof failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_vlan_anti_spoof successful: port_id=%d, vf=%d on=%d", port_id, vf_id, on);
	}

	return diag;	
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
vfd_bnxt_set_vf_vlan_filter(uint8_t port_id, uint16_t vlan_id, __attribute__((__unused__)) uint64_t vf_mask,  __attribute__((__unused__)) uint8_t on)
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
vfd_bnxt_get_vf_stats(uint8_t port_id, uint16_t vf_id, struct rte_eth_stats *stats)
{
	int diag = rte_pmd_bnxt_get_vf_stats(port_id, vf_id, stats);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_bnxt_set_vf_stats failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_bnxt_set_vf_stats successful: port_id=%d, vf=%d on=%d", port_id, vf_id);
	}
	
	return diag;	

}


int 
vfd_bnxt_reset_vf_stats(uint8_t port_id, uint16_t vf_id)
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
static bool verify_mac_address(uint8_t port_id, uint16_t vf, void *mac, void *mask)
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

	return false;
}


static void apply_rx_restrictions(uint8_t port_id, uint16_t vf, struct hwrm_cfa_l2_set_rx_mask_input *mi)
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
	if (!vf_cfg->allow_bcast)
		mi->mask &= ~HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST;
	if (!vf_cfg->allow_mcast)
		mi->mask &= ~(HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST);
	if (!vf_cfg->allow_un_ucast)
		mi->mask &= ~HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS;
}

void
vfd_bnxt_vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param)
{
	struct rte_pmd_bnxt_mb_event_param *p = param;
	struct input *req_base = p->msg;
	uint16_t vf = p->vf_id;
	uint16_t mbox_type = rte_le_to_cpu_16(req_base->req_type);
	bool add_refresh = false;
	bool restore = false;

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

		case HWRM_VNIC_CFG:
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
		/* Allow */
		case HWRM_VER_GET:
		case HWRM_FUNC_DRV_RGTR:
		case HWRM_FUNC_DRV_UNRGTR:
		case HWRM_FUNC_QCAPS:
		case HWRM_FUNC_QCFG:
		case HWRM_FUNC_QSTATS:
		case HWRM_FUNC_CLR_STATS:
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
		case HWRM_STAT_CTX_CLR_STATS:
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
}



int 
vfd_bnxt_allow_untagged(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	/* not implemented */
	bleat_printf( 3, "vfd_bnxt_allow_untagged not implemented: port_id=%d, vf_id=%d, on=%d", port_id, vf_id, on);
	return 0;	
}


int 
vfd_bnxt_set_all_queues_drop_en(uint8_t port_id, uint8_t on)
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
	int queues = rte_pmd_bnxt_get_vf_rx_status(port_id, vf_id);
	
	if (queues > 0) {
		bleat_printf( 3, "%d queues active: port=%d vfid_id=%d)", queues, port_id, vf_id);
		return 1;
	}

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