
#include "sriov.h"

int  
vfd_ixgbe_ping_vfs( __attribute__((__unused__)) uint16_t port_id,  __attribute__((__unused__)) int16_t vf_id)
{
	int diag = 0;
	int i;

	int vf_num = get_num_vfs( port_id );
	if (vf_id < 0)  // ping every VF
	{
		for (i = 0; i < vf_num; i++)
		{
			diag = rte_pmd_ixgbe_ping_vf(port_id, i);
			if (diag < 0) 
				bleat_printf( 0, "vfd_ixgbe_ping_vfs failed: (port_pi=%d, vf_id=%d) failed rc=%d", port_id, i, diag );
		}
	}
	else  // only specified
	{
		diag = rte_pmd_ixgbe_ping_vf(port_id, vf_id);
	}
	
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_ping_vfs failed: (port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_ping_vfs successful: port_id=%d, vf_id=%d", port_id, vf_id);
	}
	
	return 0;
}


int 
vfd_ixgbe_set_vf_mac_anti_spoof(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_ixgbe_set_vf_mac_anti_spoof(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_vf_mac_anti_spoof failed: (port_id=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_vf_mac_anti_spoof successful: port_id=%d, vf_id=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_ixgbe_set_vf_vlan_anti_spoof(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_ixgbe_set_vf_vlan_anti_spoof(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_vf_vlan_anti_spoof failed: (port_id=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_vf_vlan_anti_spoof successful: port_id=%d, vf_id=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_ixgbe_set_tx_loopback(uint16_t port_id, uint8_t on)
{
	int diag = rte_pmd_ixgbe_set_tx_loopback(port_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_tx_loopback failed: (port_id=%d, on=%d) failed rc=%d", port_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_tx_loopback successful: port_id=%d, on=%d", port_id, on);
	}
	
	return diag;	
}


int 
vfd_ixgbe_set_vf_unicast_promisc(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_ixgbe_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_HASH_UC,(uint8_t) on);
	if (diag < 0) {
		bleat_printf( 0, "vfd_ixgbe_set_vf_unicast_promisc failed: (port_id=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "vfd_ixgbe_set_vf_unicast_promisc successful: port_id=%d, vf_id=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;
}


int 
vfd_ixgbe_set_vf_multicast_promisc(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_ixgbe_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_MULTICAST,(uint8_t) on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_vf_multicast_promisc failed: (port_id=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_vf_multicast_promisc successful: port_id=%d, vf_id=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;
}


/*
	Add a MAC address to the white list. When more than one address is added to the list
	AND a default has not been set with a call to vfd_ixgbe_set_vf_default_mac_addr(),
	it seems the niantic picks one to report to the driver when the VF user requests
	the address; it is not clear how this is determined. The call to the set default
	should be made after all calls to this function have been made. 
*/
int 
vfd_ixgbe_set_vf_mac_addr(uint16_t port_id, uint16_t vf_id, struct ether_addr *mac_addr)
{
 	int diag = rte_eth_dev_mac_addr_add( port_id, mac_addr, vf_id );			// add to whitelist
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_vf_mac_addr failed: (port_id=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_vf_mac_addr successful: port_id=%d, vf_id=%d", port_id, vf_id);
	}
	
	return diag;
}

/*
	Set the 'default' MAC address for the VF. This is different than the set_vf_rx_mac() function
	inasmuch as the address should be what the driver reports to a DPDK application when the 
	MAC address is 'read' from the device.
*/
int vfd_ixgbe_set_vf_default_mac_addr( portid_t port_id, uint16_t vf, struct ether_addr *mac_addr ) {
	int state;

	state =  rte_pmd_ixgbe_set_vf_mac_addr( port_id, vf, mac_addr );
	if( state < 0 ) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_vf_default_mac_addr failed: (port_id=%d, vf_id=%d) failed rc=%d", port_id, vf, state );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_vf_default_mac_addr successful: port_id=%d, vf_id=%d", port_id, vf );
	}

	return state;
}


int 
vfd_ixgbe_set_vf_vlan_stripq(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_ixgbe_set_vf_vlan_stripq(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_vf_vlan_stripq failed: (port_=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_vf_vlan_stripq successful: port_id=%d, vf_id=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_ixgbe_set_vf_vlan_insert(uint16_t port_id, uint16_t vf_id, uint16_t vlan_id)
{
	int diag = rte_pmd_ixgbe_set_vf_vlan_insert(port_id, vf_id, vlan_id);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_vf_vlan_insert failed: (port_pi=%d, vf_id=%d, vlan_id=%d) failed rc=%d", port_id, vf_id, vlan_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_vf_vlan_insert successful: port_id=%d, vf_id=%d vlan_id=%d", port_id, vf_id, vlan_id);
	}

	return diag;	
}


int 
vfd_ixgbe_set_vf_broadcast(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	int diag = rte_pmd_ixgbe_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_BROADCAST,(uint8_t) on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_vf_broadcas failed: (port_id=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_vf_broadcas successful: port_id=%d, vf_id=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;	
}


int 
vfd_ixgbe_allow_untagged(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	
	uint16_t rx_mode = 0;
  rx_mode |= ETH_VMDQ_ACCEPT_UNTAG;

	int diag = rte_pmd_ixgbe_set_vf_rxmode(port_id, vf_id, rx_mode,(uint8_t) on);
	if (diag < 0) {
		bleat_printf( 0, "vfd_ixgbe_set_vf_vlan_tag failed: (port_id=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "vfd_ixgbe_set_vf_vlan_tag successful: port_id=%d, vf_id=%d on=%d", port_id, vf_id, on);
	}
	
	return diag;		
}


int 
vfd_ixgbe_set_vf_vlan_filter(uint16_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on)
{
	int diag = rte_pmd_ixgbe_set_vf_vlan_filter(port_id, vlan_id, vf_mask, on);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_vf_vlan_filter failed: (port_id=%d, vlan_id=%d) failed rc=%d", port_id, vlan_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_vf_vlan_filter successful: port_id=%d, vlan_id=%d", port_id, vlan_id);
	}
	
	return diag;		
}


int 
vfd_ixgbe_get_vf_stats(uint16_t port_id, uint16_t vf_id, struct rte_eth_stats *stats)
{
	int diag = 0;
	/* not implemented in DPDK yet */
	//diag = rte_pmd_ixgbe_get_vf_stats(port_id, vf_id, stats);


	if(vf_id > 31 ) {
		return -1;
	}

  stats->ipackets = port_pci_reg_read(port_id, IXGBE_PVFGPRC(vf_id));
	uint64_t	rx_ol = port_pci_reg_read(port_id, IXGBE_PVFGORC_LSB(vf_id));
	uint64_t	rx_oh = port_pci_reg_read(port_id, IXGBE_PVFGORC_MSB(vf_id));
	stats->ibytes = (rx_oh << 32) |	rx_ol;		// 36 bit only counter

	stats->opackets = port_pci_reg_read(port_id, IXGBE_PVFGPTC(vf_id));
	uint64_t	tx_ol = port_pci_reg_read(port_id, IXGBE_PVFGOTC_LSB(vf_id));
	uint64_t	tx_oh = port_pci_reg_read(port_id, IXGBE_PVFGOTC_MSB(vf_id));
	stats->obytes = (tx_oh << 32) |	tx_ol;		// 36 bit only counter

	
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_get_vf_stats failed: (port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_get_vf_stats successful: port_id=%d, vf_id=%d", port_id, vf_id);
	}
	
	return diag;			
}



int 
vfd_ixgbe_reset_vf_stats(uint16_t port_id, uint16_t vf_id)
{
	int diag = 0;
	
	/* not implemented in DPDK yet */
	//int diag = rte_pmd_ixgbe_reset_vf_stats(port_id, vf_id);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_reset_vf_stats failed: (port_pi=%d, vf_id=%d) failed rc=%d", port_id, vf_id, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_reset_vf_stats successful: port_id=%d, vf_id=%d", port_id, vf_id);
	}
	
	return diag;
}


int 
vfd_ixgbe_set_vf_rate_limit(uint16_t port_id, uint16_t vf_id, uint16_t tx_rate, uint64_t q_msk)
{
	int diag = rte_pmd_ixgbe_set_vf_rate_limit(port_id, vf_id, tx_rate, q_msk);
	if (diag < 0) {
		bleat_printf( 0, "rte_pmd_ixgbe_set_vf_rate_limit failed: (port_id=%d, vf_id=%d, tx_rate=%d) failed rc=%d", port_id, vf_id, tx_rate, diag );
	} else {
		bleat_printf( 3, "rte_pmd_ixgbe_set_vf_rate_limit successful: port_id=%d, vf_id=%d, tx_rate=%d", port_id, vf_id, tx_rate);
	}
	
	return diag;			
}


int 
vfd_ixgbe_set_all_queues_drop_en(uint16_t port_id, uint8_t on)
{
	int diag = rte_pmd_ixgbe_set_all_queues_drop_en(port_id, on);
	if (diag < 0) {
		bleat_printf( 0, "vfd_ixgbe_set_all_queues_drop_en failed: (port=%d, on=%d) failed rc=%d", port_id, on, diag );
	} else {
		bleat_printf( 3, "vfd_ixgbe_set_all_queues_drop_en successful: port_id=%d, on=%d", port_id, on);
	}
	
	return diag;	
}


/*
	Called when a 'mailbox' message is received.  Examine and take action based on
	the message type. (Somewhere after dpdk 17.05 data was inserted but is generally
	NULL (hard set in their code).
*/
int
vfd_ixgbe_vf_msb_event_callback(uint16_t port_id, enum rte_eth_event_type type, void *data, void* param ) {

	struct rte_pmd_ixgbe_mb_event_param *p;
	uint16_t vf;
	uint16_t mbox_type;
	uint32_t *msgbuf;
	unsigned char addr_len;				// length of address in callback message
	int add_refresh = 0;				// some actions require refresh only if something done
	char	wbuf[128];
	int i;

	struct ether_addr *new_mac;

	RTE_SET_USED(data);

	p = (struct rte_pmd_ixgbe_mb_event_param*) param;
	if( p == NULL ) {									// yes this has happened
		bleat_printf( 2, "callback driven with null pointer data=%p", data );
		return 0;
	}

	vf = p->vfid;
	mbox_type = p->msg_type;
	msgbuf = (uint32_t *) p->msg;

	bleat_printf( 3, "ixgbe: processing callback starts: pf/vf=%d/%d, evtype=%d mbtype=%d", port_id, vf, type, mbox_type);

	/* check & process VF to PF mailbox message */
	switch (mbox_type) {
		case IXGBE_VF_RESET:
			bleat_printf( 1, "reset event received: port=%d", port_id );

			p->retval = RTE_PMD_IXGBE_MB_EVENT_NOOP_ACK;				/* noop & ack */

			add_refresh_queue(port_id, vf);
			break;

		case IXGBE_VF_SET_MAC_ADDR:
			bleat_printf( 1, "setmac event approved for: port=%d", port_id );
			p->retval = RTE_PMD_IXGBE_MB_EVENT_PROCEED;    						// do what's needed

			new_mac = (struct ether_addr *) (&msgbuf[1]);

			snprintf( wbuf, sizeof( wbuf ), "%02x:%02x:%02x:%02x:%02x:%02x", new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3], new_mac->addr_bytes[4], new_mac->addr_bytes[5] );

			if( ! push_mac( port_id, vf, wbuf ) ) {								// push onto the head of our list
				bleat_printf( 1, "guest attempt to push mac address fails: %s: (sending nack)", wbuf );
				p->retval = RTE_PMD_IXGBE_MB_EVENT_NOOP_NACK;     				// guest should see failure
			} else {
				bleat_printf( 1, "guest attempt to push mac address successful: %s", wbuf );
			}
	
			add_refresh_queue(port_id, vf);
			break;

		case IXGBE_VF_SET_MULTICAST:
			bleat_printf( 1, "set multicast event received: port=%d", port_id );
			p->retval = RTE_PMD_IXGBE_MB_EVENT_PROCEED;    /* do what's needed */

			new_mac = (struct ether_addr *) (&msgbuf[1]);
			bleat_printf( 3, "multicast mac set, pf %u vf %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
					(uint) port_id,
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);

			add_refresh_queue( port_id, vf );		// schedule a complete refresh when the queue goes hot

			break;

		case IXGBE_VF_SET_VLAN:
			// NOTE: we _always_ approve this.  This is the VMs setting of what will be an 'inner' vlan ID and thus we don't care
			if( valid_vlan( port_id, vf, (int) msgbuf[1] )) {
				bleat_printf( 1, "vlan set event approved: port=%d vf=%d vlan=%d (responding noop-ack)", port_id, vf, (int) msgbuf[1] );
				p->retval = RTE_PMD_IXGBE_MB_EVENT_NOOP_ACK;     // good rc to VM while not changing anything
			} else {
				bleat_printf( 1, "vlan set event rejected; vlan not not configured: port=%d vf=%d vlan=%d (responding noop-ack)", port_id, vf, (int) msgbuf[1] );
				p->retval = RTE_PMD_IXGBE_MB_EVENT_NOOP_NACK;     // VM should see failure
			}

			add_refresh_queue( port_id, vf );		// schedule a complete refresh when the queue goes hot

			//bleat_printf( 3, "setting vlan id = %d", p[1]);
			break;

		case IXGBE_VF_SET_LPE:
			bleat_printf( 1, "set lpe event received %d %d", port_id, (int) msgbuf[1]  );
			if( valid_mtu( port_id, (int) msgbuf[1] ) ) {
				bleat_printf( 1, "mtu set event approved: port=%d vf=%d mtu=%d", port_id, vf, (int) msgbuf[1]  );
				p->retval = RTE_PMD_IXGBE_MB_EVENT_PROCEED;
			} else {
				bleat_printf( 1, "mtu set event rejected: port=%d vf=%d mtu=%d", port_id, vf, (int) msgbuf[1] );
				p->retval = RTE_PMD_IXGBE_MB_EVENT_NOOP_NACK;     /* noop & nack */
			}
			
			restore_vf_setings(port_id, vf);
			set_fc_on( port_id, !FORCE );							// enable flow control if allowed (force off)
			tx_set_loopback( port_id, suss_loopback( port_id ) );	// enable loopback if set (could be reset if link was down)
			add_refresh_queue( port_id, vf );						// schedule a complete refresh when the queue goes hot
			break;

		case IXGBE_VF_SET_MACVLAN:
			addr_len =  msgbuf[0];				//this is a length, ensure that it is valid (0 or 6, anything else is junk)

			p->retval =  RTE_PMD_IXGBE_MB_EVENT_NOOP_NACK;    						// assume no good and default to nack
			switch( addr_len ) {
				case 0:							// we'll accept this and respond proceed
					// TODO -- should we consider this a reset and clear all values?
					bleat_printf( 1, "set macvlan event has no length; ignoring: pf/vf=%d/%d", port_id, vf );
					p->retval = RTE_PMD_IXGBE_MB_EVENT_PROCEED;			// if no addresses, let them go on, but no refresh
					break;

				case 6:							// valid mac address len
					new_mac = (struct ether_addr *) (&msgbuf[1]);
					snprintf( wbuf, sizeof( wbuf ), "%02x:%02x:%02x:%02x:%02x:%02x", new_mac->addr_bytes[0], new_mac->addr_bytes[1],
							new_mac->addr_bytes[2], new_mac->addr_bytes[3], new_mac->addr_bytes[4], new_mac->addr_bytes[5] );
		
					for( i = 0; i < 6; i++ ) {					// check to see if mac is all 0s
						if( new_mac->addr_bytes[i] ) {
							break;
						}
					}
		
					if( i >= 6 ) {												// all 0s -- assume reset (don't save the 0s)
						bleat_printf( 1, "set macvlan event received with address of 0s: clearing all but default MAC: pf/vf=%d/%d", port_id, vf );
						clear_macs( port_id, vf, KEEP_DEFAULT );
						p->retval = RTE_PMD_IXGBE_MB_EVENT_PROCEED;
					} else {
						if( add_mac( port_id, vf, wbuf ) ) {					// add to the VF's mac list, if not there and if room on both pf and vf
							bleat_printf( 1, "set macvlan event received: pf/vf=%d/%d %s (responding proceed)", port_id, vf, wbuf );
							p->retval = RTE_PMD_IXGBE_MB_EVENT_PROCEED;
							add_refresh = 1;
						} else {
							bleat_printf( 1, "set macvlan event: add to vfd table rejected: pf/vf=%d/%d %s (responding nop+nak)", port_id, vf, wbuf );
							break;
						}
					}
					break;

				default:
					bleat_printf( 1, "set macvlan event received, bad address length: %u pf/vf=%d/%d (responding nop+nak)", addr_len, port_id, vf );
					p->retval =  RTE_PMD_IXGBE_MB_EVENT_NOOP_NACK;    						/* something rejected so noop & nack */
					break;
			}


			if( add_refresh ) {
				add_refresh_queue( port_id, vf );								// schedule a complete refresh when the queue goes hot
			}

			break;

		case IXGBE_VF_API_NEGOTIATE:
			bleat_printf( 1, "set negotiate event received: port=%d (responding proceed)", port_id );
			p->retval =  RTE_PMD_IXGBE_MB_EVENT_PROCEED;   /* do what's needed */
			
			set_fc_on( port_id, !FORCE );									// enable flow control if allowed
			restore_vf_setings(port_id, vf);							// these must happen now, do NOT queue it. if not immediate guest-guest may hang
			tx_set_loopback( port_id, suss_loopback( port_id ) );		// enable loopback if set (could be reset if link goes down)
			break;

		case IXGBE_VF_GET_QUEUES:
			bleat_printf( 1, "get queues event received: port=%d (responding proceed)", port_id );
			p->retval =  RTE_PMD_IXGBE_MB_EVENT_PROCEED;   /* do what's needed */

			add_refresh_queue( port_id, vf );		// schedule a complete refresh when the queue goes hot
			break;
	
		case IXGBE_VF_UPDATE_XCAST_MODE:
			bleat_printf( 1, "update xcast mode event received: port=%d (responding proceed)", port_id );
			p->retval =  RTE_PMD_IXGBE_MB_EVENT_PROCEED;   /* do what's needed */

			add_refresh_queue( port_id, vf );		// schedule a complete refresh when the queue goes hot
			break;

		default:
			bleat_printf( 1, "unknown event request received: port=%d (responding nop+nak)", port_id );
			p->retval = RTE_PMD_IXGBE_MB_EVENT_NOOP_NACK;     /* noop & nack */

			restore_vf_setings(port_id, vf);		// refresh all of our configuration back onto the NIC
			break;
	}

	bleat_printf( 3, "ixgbe: processing callback finished: %d, pf/vf=%d/%d, rc=%d mbtype=%d", type, port_id, vf, p->retval, mbox_type);

	return 0;   // CAUTION:  as of 2017/07/05 it seems this value is ignored by dpdk, but it might not alwyas be
}



uint32_t 
vfd_ixgbe_get_pf_spoof_stats(uint16_t port_id)
{
	bleat_printf( 3, "vfd_ixgbe_get_pf_spoof_stats: port_id=%d", port_id);
	return port_pci_reg_read(port_id, 0x08780);
}


uint32_t 
vfd_ixgbe_get_vf_spoof_stats(__attribute__((__unused__)) uint16_t port_id, __attribute__((__unused__)) uint16_t vf_id)
{
	/* not implemented */
	bleat_printf( 3, "vfd_ixgbe_get_vf_spoof_stats not implemented: port_id=%d, vf_id=%d", port_id, vf_id);
	return 0;
}

void 
vfd_ixgbe_disable_default_pool(uint16_t port_id)
{
	uint32_t ctrl = port_pci_reg_read(port_id, IXGBE_VT_CTL);
	ctrl |= IXGBE_VT_CTL_DIS_DEFPL;
	bleat_printf( 3, "vfd_ixgbe_disable_default_pool bar=0x%08X, port=%d ctrl=0x%08x ", IXGBE_VT_CTL, port_id, ctrl);
	port_pci_reg_write( port_id, IXGBE_VT_CTL, ctrl);	
}


int 
vfd_ixgbe_is_rx_queue_on(uint16_t port_id, uint16_t vf_id, int* mcounter)
{
	int queue;						// queue to set (0-max-queues)
	uint32_t reg_off;				// control register address
	uint32_t ctrl;					// value read from nic register
	uint32_t queues_per_pool = 8;	// maximum number of queues that could be assigned to a pool (based on total VFs configured)

	struct rte_eth_dev *pf_dev;
	struct rte_eth_dev_info dev_info;

	rte_eth_dev_info_get( port_id, &dev_info );
 	pf_dev = &rte_eth_devices[port_id];

	reg_off = 0x01028;							// default to 'low' range (receive descriptor control reg (pg527/597))
	queues_per_pool = get_max_qpp( port_id );	// set the max possible queues per pool; controls layout at offset
	queue = vf_id * queues_per_pool;			// compute the offset which is based on the max/pool
	if( queue > 127 ) {
		bleat_printf( 2, "warn: can't check rx_queue_on q out of range: port=%d q=%d vfid_id=%d", port_id, queue, vf_id );
		return 0;								// error -- vf is out of range for the number of queues/pool
	} else {
		if( queue > 63 ) {
			reg_off = 0x0D028;					// must use the 'high' area
			queue -= 64;						// this now becomes the offset into the dcb space
		}
	}

	reg_off += queue * 0x40;					// each block of info is x40 wide (per datasheet)

	ctrl = port_pci_reg_read(port_id, reg_off);
	bleat_printf( 5, "is_queue_en: offset=0x%08X, port=%d q=%d vfid_id=%d, ctrl=0x%08X)", reg_off, port_id, queue, vf_id, ctrl);

	if( ctrl & 0x2000000) {
  		bleat_printf( 3, "first queue active: offset=0x%08X, port=%d vfid_id=%d, q=%d ctrl=0x%08X)", reg_off, port_id, vf_id, queue, ctrl);
		return 1;
	} else {
		if( mcounter != NULL ) {
			if( (*mcounter % 100 ) == 0 ) {
  				bleat_printf( 4, "is_queue_en: still pending: first queue not active: bar=0x%08X, port=%d vfid_id=%d, ctrl=0x%08x q/pool=%d", reg_off, port_id, vf_id, ctrl, (int) RTE_ETH_DEV_SRIOV(pf_dev).nb_q_per_pool);
			}
			(*mcounter)++;
		}
		return 0;
	}	
}


void 
vfd_ixgbe_set_pfrx_drop(uint16_t port_id, int state)
{
	uint16_t qstart;			// point where the queue starts (1 past the last VF)
	int          i;
	uint32_t reg_off;
	uint32_t reg_value;          // value to write into the register
	int q_num;

	q_num = get_max_qpp( port_id );				// number of queues per vf on this port
	if( q_num > 8 ) {
		bleat_printf( 0, "internal mishap in set_pfrx_drop: qpp is out of range: %d", q_num );
		return;							// panic in the face of potential disaster and do nothing
	}

	qstart = get_num_vfs( port_id ) * q_num;	// PF queue starts just past last possible vf
	if( qstart > (128 - q_num) ) {
		bleat_printf( 0, "internal mishap in vfd_ixgbe_set_pfrx_drop(): qstart (%d) is out of range for nqueues=%d", qstart, q_num );
		return;
	}

	bleat_printf( 0, "vfd_ixgbe_set_pfrx_drop(): setting pf drop enable for port %d qstart=%d on/off=%d", port_id, qstart, !!state );
	reg_off = 0x02f04;

	for( i = qstart; i < qstart + q_num; i++ ) {						// one bit per queue writes the right most three bits into the proper place
		reg_value = IXGBE_QDE_WRITE | (i << IXGBE_QDE_IDX_SHIFT) | (!!state);
		port_pci_reg_write( port_id, reg_off, reg_value );
	}	
}


void 
vfd_ixgbe_set_rx_drop(uint16_t port_id, uint16_t vf_id, int state)
{
	int          i;
	uint32_t reg_off;
	uint32_t reg_value;          // value to write into the register
	int q_num;

	q_num = get_max_qpp( port_id );		// number of queues per vf on this port
	if( q_num > 8 ) {
		bleat_printf( 0, "internal mishap in set_rx_drop: qpp is out of range: %d", q_num );
		return;							// panic in the face of potential disaster and do nothing
	}
	bleat_printf( 0, "vfd_ixgbe_set_rx_drop() to %d for pf/vf %d/%d on/off=%d", state, port_id, vf_id, !!state );

	reg_off = 0x02f04;

	for( i = vf_id * q_num; i < (vf_id * q_num) + q_num; i++ ) {						// one bit per queue writes the right most three bits into the proper place
		reg_value = IXGBE_QDE_WRITE | (i << IXGBE_QDE_IDX_SHIFT) | (!!state);
		port_pci_reg_write( port_id, reg_off, reg_value );
	}
}


void 
vfd_ixgbe_set_split_erop(uint16_t port_id, uint16_t vf_id, int state)
{
	uint32_t reg_off = 0x01014; 							// split receive control regs (pg598)
	uint32_t reg_value;
	uint32_t qpvf;					// number of queues per vf

	if( vf_id > 63  ) {				// this offset good only for 0-63; we won't support 64-127
		return;
	}

	qpvf = (uint32_t) get_max_qpp( port_id );

	reg_off += 0x40 * vf_id;

	reg_value =  port_pci_reg_read( port_id, reg_off );

	if( state ) {
		reg_value |= IXGBE_SRRCTL_DROP_EN;								// turn on the enable bit
	} else {
		reg_value &= ~IXGBE_SRRCTL_DROP_EN;								// turn off the enable bit
	}

	bleat_printf( 4, "vfd_ixgbe_set_split_erop(): setting split receive drop for %d queues on port=%d vf=%d on/off=%d", qpvf, port_id, vf_id, state );

	for( ; qpvf > 0; qpvf-- ) {
		port_pci_reg_write( port_id, reg_off, reg_value );
		reg_off += 0x40;
	}	
}


int 
vfd_ixgbe_get_split_ctlreg(uint16_t port_id, uint16_t vf_id)
{
	uint32_t reg_off = 0x01014; 	// split receive control regs (pg598)
	int queue;						// the first queue for the vf (TODO: expand this to accept a queue 0-max_qpp)

	queue = get_max_qpp( port_id ) * vf_id;

	if( queue >= 64 ) {
		reg_off = 0x0d014;			// high set of queues
	}

	reg_off += 0x40 * queue;		// step to the right spot for the given queue

	return (int) port_pci_reg_read( port_id, reg_off );	
}


int 
vfd_ixgbe_dump_all_vlans(uint16_t port_id)
{
	uint32_t res;
	uint32_t ix;
	uint32_t count = 0;

	for (ix = 1; ix < IXGBE_VLVF_ENTRIES; ix++) {
		res = port_pci_reg_read(port_id, IXGBE_VLVF(ix));
		if( 0 != (res & 0xfff))
		{
			count++;
			printf("VLAN ID = %d\n", res & 0xfff);
		}
	}

	return count;
}

