/*
**
** az
	useful doc:
	http://www.intel.com/content/dam/doc/design-guide/82599-sr-iov-driver-companion-guide.pdf
**
*/

#include "vfdlib.h"
#include "sriov.h"


#define RTE_PMD_PARAM_UNSET -1


struct rte_port *ports;


static inline
uint64_t RDTSC(void)
{
  unsigned int hi, lo;
  __asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
  return ((uint64_t)hi << 32) | lo;
}
	

int
xdigit(char c)
{
  unsigned d;
  d = (unsigned)(c-'0');

  if (d < 10)
    return (int)d;

  d = (unsigned)(c-'a');

  if (d < 6)
    return (int)(10+d);

  d = (unsigned)(c-'A');

  if (d < 6)
    return (int)(10+d);

  return -1;
}


void
ether_aton_r(const char *asc, struct ether_addr *addr)
{
  int i, val0, val1;

  for (i = 0; i < ETHER_ADDR_LEN; ++i){
    val0 = xdigit(*asc);
    asc++;

    if (val0 < 0)
      return;

    val1 = xdigit(*asc);
    asc++;
    if (val1 < 0)
      return;

    addr->addr_bytes[i] = (u_int8_t)((val0 << 4) + val1);

    if (i < ETHER_ADDR_LEN - 1){
      if (*asc != ':')
        return;
      asc++;
    }
  }
  if (*asc != '\0')
    return;
}


int
port_id_is_invalid(portid_t port_id, enum print_warning warning)
{

  traceLog(TRACE_DEBUG,"Port %d\n", port_id);

	if (port_id == (portid_t)RTE_PORT_ALL)
		return 0;

	if (port_id < RTE_MAX_ETHPORTS && ports[port_id].enabled)
		return 0;

	if( warning == ENABLED_WARN )
		bleat_printf( 2, "warn: Invalid port %d\n", port_id);

	return 1;
}


int
set_queue_rate_limit(portid_t port_id, uint16_t queue_idx, uint16_t rate)
{
	int diag;
	struct rte_eth_link link;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return 1;
	rte_eth_link_get_nowait(port_id, &link);
	if (rate > link.link_speed) {
		bleat_printf( 0, "error: Invalid rate value:%u bigger than link speed: %u",
			rate, link.link_speed);
		return 1;
	}
	diag = rte_eth_set_queue_rate_limit(port_id, queue_idx, rate);
	if (diag == 0)
		return diag;
	bleat_printf( 0, "error: rte_eth_set_queue_rate_limit for port_id=%d failed diag=%d",
		port_id, diag);
	return diag;
}



int
set_vf_rate_limit(portid_t port_id, uint16_t vf, uint16_t rate, uint64_t q_msk)
{
	int diag;
	struct rte_eth_link link;

	if (q_msk == 0)
		return 0;

	// main will only call for a valid port.
	//if (port_id_is_invalid(port_id, ENABLED_WARN))
	//	return 1;

	rte_eth_link_get_nowait(port_id, &link);
	if (rate > link.link_speed) {
		bleat_printf( 0, "set_vf_rate: invalid rate value: %u bigger than link speed: %u", rate, link.link_speed);
		//return 1;
	}
	diag = rte_eth_set_vf_rate_limit(port_id, vf, rate, q_msk);
	if (diag != 0) {
		bleat_printf( 0, "set_vf_rate: unable to set value %u: (%d) %s", rate, diag, strerror( -diag ) );
	
		//traceLog(TRACE_ERROR, "rte_eth_set_vf_rate_limit for port_id=%d failed diag=%d\n", port_id, diag);
	}

	return diag;
}



int
port_reg_off_is_invalid(portid_t port_id, uint32_t reg_off)
{
	uint64_t pci_len;

	if (reg_off & 0x3) {
		traceLog(TRACE_DEBUG, "Port register offset 0x%X not aligned on a 4-byte "
		       "boundary\n",
		       (unsigned)reg_off);
		return 1;
	}
	pci_len = ports[port_id].dev_info.pci_dev->mem_resource[0].len;
	if (reg_off >= pci_len) {
		traceLog(TRACE_DEBUG, "Port %d: register offset %u (0x%X) out of port PCI "
		       "resource (length=%"PRIu64")\n",
		       port_id, (unsigned)reg_off, (unsigned)reg_off,  pci_len);
		return 1;
	}
	return 0;
}


void
rx_vlan_strip_set_on_queue(portid_t port_id, uint16_t queue_id, int on)
{
	int diag;

	diag = rte_eth_dev_set_vlan_strip_on_queue(port_id, queue_id, on);
	if (diag < 0) {
		traceLog(TRACE_DEBUG, "rx_vlan_strip_set_on_queue(port_pi=%d, queue_id=%d, on=%d) failed " "diag=%d\n", port_id, queue_id, on, diag);
	} else {
		bleat_printf( 3, "set vlan strip on queue successful: port=%d, q=%d on/off=%d", port_id, queue_id, on );
	}
	
}



/*
	Set VLAN tag on transmission.  If no tag is to be inserted, then a VLAN 
	ID of 0 must be passed.
*/
void
tx_vlan_insert_set_on_vf(portid_t port_id, uint16_t vf_id, int vlan_id)
{

	struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get(port_id, &dev_info);

	uint32_t reg_off = 0x08000;

	reg_off += 4 * vf_id;

	traceLog(TRACE_DEBUG, "tx_vlan_insert_set_on_vf: bar=0x%08X, vf_id=%d, vlan=%d", reg_off, vf_id, vlan_id);

	uint32_t ctrl = port_pci_reg_read(port_id, reg_off);

	traceLog(TRACE_DEBUG, "tx_vlan_insert_set_on_vf: read: bar=0x%08X, vf_id=%d, ctrl=0x%x", reg_off, vf_id, ctrl);


	if (vlan_id){
		ctrl = vlan_id;
		ctrl |= 0x40000000;
	} else {
		ctrl = 0;
	}

	port_pci_reg_write(port_id, reg_off, ctrl);

	traceLog(TRACE_DEBUG, "tx_insert_set_on_vf: set: bar=0x%08X, vfid_id=%d, ctrl=0x%08X", reg_off, vf_id, ctrl);
}


void
rx_vlan_strip_set_on_vf(portid_t port_id, uint16_t vf_id, int on)
{

  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(port_id, &dev_info);

  uint32_t queues_per_pool = dev_info.vmdq_queue_num / dev_info.max_vmdq_pools;

  uint32_t reg_off = 0x01028;

  reg_off += (0x40 * vf_id * queues_per_pool);

  traceLog(TRACE_DEBUG, "rx_vlan_strip_set_on_vf: bar=0x%08X, vf_id=%d, numq=%d)", reg_off, vf_id, queues_per_pool);

  uint32_t q;
  for(q = 0; q < queues_per_pool; ++q){

    reg_off += 0x40 * q;

    traceLog(TRACE_DEBUG, "rx_vlan_strip_set_on_vf: q=%d bar=0x%08X, vf_id=%d, on=%d", q, reg_off, vf_id, on);

    uint32_t ctrl = port_pci_reg_read(port_id, reg_off);

    traceLog(TRACE_DEBUG, "rx_vlan_strip_set_on_vf: read: q=%d bar=0x%08X, vf_id=%d, ctrl=0x%x", q, reg_off, vf_id, ctrl);


    if (on)
      ctrl |= IXGBE_RXDCTL_VME;				// vlan mode enable (strip flag)
    else
      ctrl &= ~IXGBE_RXDCTL_VME;

    port_pci_reg_write(port_id, reg_off, ctrl);    		// void -- no error to check

    traceLog(TRACE_DEBUG, "rx_vlan_strip_set_on_vf: set: q=%d bar=0x%08X, vfid_id=%d, ctrl=0x%08X)\n", q, reg_off, vf_id, ctrl);
  }
}



void
rx_vlan_strip_set(portid_t port_id, int on)
{
	int diag;
	int vlan_offload;


	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);

	if (on)
		vlan_offload |= ETH_VLAN_STRIP_OFFLOAD;
	else
		vlan_offload &= ~ETH_VLAN_STRIP_OFFLOAD;

	diag = rte_eth_dev_set_vlan_offload(port_id, vlan_offload);
	if (diag < 0) {
		traceLog(TRACE_INFO, "rx_vlan_strip_set(port_pi=%d, on=%d) failed, diag=%d\n", port_id, on, diag);
	} else {
		bleat_printf( 3, "set vlan strip successful: %d: on/off=%d", port_id, on );
	}
}



void
set_vf_allow_bcast(portid_t port_id, uint16_t vf_id, int on)
{
  int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_BROADCAST,(uint8_t) on);
	
	if (ret < 0) {
    	traceLog(TRACE_INFO, "set_vf_allow_bcast(): bad VF receive mode parameter, return code = %d \n", ret);
	} else {
		bleat_printf( 3, "allow bcast successfully set for port/vf %d/%d on/off=%d", port_id, vf_id, on );
	}
}


void
set_vf_allow_mcast(portid_t port_id, uint16_t vf_id, int on)
{
	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_MULTICAST,(uint8_t) on);
	
	if (ret < 0) {
    	traceLog(TRACE_INFO, "set_vf_allow_mcast(): bad VF receive mode parameter, return code = %d \n", ret);
	} else {
		bleat_printf( 3, "allow mcast successfully set for port/vf %d/%d on/off=%d", port_id, vf_id, on );
	}
}


void
set_vf_allow_un_ucast(portid_t port_id, uint16_t vf_id, int on)
{
	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_HASH_UC,(uint8_t) on);
	
	if (ret < 0) {
    	traceLog(TRACE_INFO, "set_vf_allow_un_ucast(): bad VF receive mode parameter, return code = %d \n", ret);
	} else {
		bleat_printf( 3, "allow un-ucast successfully set for port/vf %d/%d on/off=%d", port_id, vf_id, on );
	}
}


void
set_vf_allow_untagged(portid_t port_id, uint16_t vf_id, int on)
{
  uint16_t rx_mode = 0;
  rx_mode |= ETH_VMDQ_ACCEPT_UNTAG;

  traceLog(TRACE_DEBUG, "set_vf_allow_untagged(): rx_mode = %d, on = %d \n", rx_mode, on);

	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, rx_mode, (uint8_t) on);
	
	if (ret < 0)
    	traceLog(TRACE_INFO, "set_vf_allow_untagged(): bad VF receive mode parameter, return code = %d \n", ret);
}



void
set_vf_rx_mac(portid_t port_id, const char* mac, uint32_t vf,  __attribute__((__unused__)) uint8_t on)
{
	int diag;
  struct ether_addr mac_addr;
  ether_aton_r(mac, &mac_addr);

	diag = rte_eth_dev_mac_addr_add(port_id, &mac_addr, vf);
	if (diag == 0) {
		bleat_printf( 3, "set rx mac successful: port=%d vf=%d on/off=%d mac=%s", (int)port_id, (int)vf, on, mac );
	} else {
		bleat_printf( 0, "rte_eth_dev_mac_addr_add for port_id=%d failed " "diag=%d\n", port_id, diag);
	}

}


void
set_vf_rx_vlan(portid_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on)
{
	int diag;

	diag = rte_eth_dev_set_vf_vlan_filter(port_id, vlan_id, vf_mask, on);
	if (diag == 0) {
		bleat_printf( 3, "set vlan filter successful: port=%d vlan=%d on/off=%d", (int)port_id, (int) vlan_id, on );
	} else {
		bleat_printf( 0, "rte_eth_dev_set_vf_vlan_filter for port_id=%d failed " "diag=%d\n", port_id, diag);
	}

}


void
set_vf_vlan_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on)
{
	int diag;

	diag = rte_eth_dev_set_vf_vlan_anti_spoof(port_id, vf, on);
	if (diag == 0) {
		bleat_printf( 3, "set vlan antispoof successful: port=%d vf=%d on/off=%d", (int)port_id, (int)vf, on );
	} else {
		bleat_printf( 0, "rte_eth_dev_set_vf_vlan_anti_spoof for port_id=%d failed " "diag=%d vf=%d\n", port_id, diag, vf);
	}

}


void
set_vf_mac_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on)
{
	int diag;

	diag = rte_eth_dev_set_vf_mac_anti_spoof(port_id, vf, on);
	if (diag == 0) {
		bleat_printf( 3, "set mac antispoof successful: port=%d vf=%d on/off=%d", (int)port_id, (int)vf, on );
	} else {
		bleat_printf( 0, "rte_eth_dev_set_vf_mac_anti_spoof for port_id=%d failed " "diag=%d vf=%d\n", port_id, diag, vf);
	}

}

void
tx_set_loopback(portid_t port_id, u_int8_t on)
{
	uint32_t ctrl = port_pci_reg_read(port_id, IXGBE_PFDTXGSWC);
	if (on) 
		ctrl |= IXGBE_PFDTXGSWC_VT_LBEN;
	else
		ctrl &= ~IXGBE_PFDTXGSWC_VT_LBEN;
	
	port_pci_reg_write(port_id, IXGBE_PFDTXGSWC, ctrl);
}


int 
is_rx_queue_on(portid_t port_id, uint16_t vf_id)
{
	/* check if first queue in the pool is active */
	
	struct rte_eth_dev *pf_dev = &rte_eth_devices[port_id];
  uint32_t queues_per_pool = RTE_ETH_DEV_SRIOV(pf_dev).nb_q_per_pool;
	queues_per_pool = 2;
	
  uint32_t reg_off = 0x01028;
  
  reg_off += (0x40 * vf_id * queues_per_pool);
	
  uint32_t ctrl = port_pci_reg_read(port_id, reg_off);

  traceLog(TRACE_DEBUG, "RX_QUEUS_ENA, bar=0x%08X, vfid_id=%d, ctrl=0x%08X)\n", reg_off, vf_id, ctrl);
	
	if( ctrl & 0x2000000)
		return 1;
	else
		return 0;
}


static rte_spinlock_t rte_refresh_q_lock = RTE_SPINLOCK_INITIALIZER;

void 
add_refresh_queue(u_int8_t port_id, uint16_t vf_id)
{
	
	struct rq_entry *refresh_item;
	
	//printf("add_refresh_queue\n");
	
	/* look for refresh request and update enabled status if already there */ 
	rte_spinlock_lock(&rte_refresh_q_lock);
	TAILQ_FOREACH(refresh_item, &rq_head, rq_entries) {
		if (refresh_item->port_id == port_id && refresh_item->vf_id == vf_id){
			if (!refresh_item->enabled)
				refresh_item->enabled = is_rx_queue_on(port_id, vf_id);
			
			rte_spinlock_unlock(&rte_refresh_q_lock);
			return;
		}
	}
	
	rte_spinlock_unlock(&rte_refresh_q_lock);
	
	refresh_item = malloc(sizeof(*refresh_item));
	if (refresh_item == NULL) 
		rte_exit(EXIT_FAILURE, "add_refresh_queue(): Can not allocate memory\n");

	refresh_item->port_id = port_id;
	refresh_item->vf_id = vf_id;
	refresh_item->enabled = is_rx_queue_on(port_id, vf_id);
	
	
	rte_spinlock_lock(&rte_refresh_q_lock);
	TAILQ_INSERT_TAIL(&rq_head, refresh_item, rq_entries);	
	rte_spinlock_unlock(&rte_refresh_q_lock);
}

void
process_refresh_queue(void)
{
	while(1) {
		
		usleep(200000);
		struct rq_entry *refresh_item;;
		
		rte_spinlock_lock(&rte_refresh_q_lock);
		TAILQ_FOREACH(refresh_item, &rq_head, rq_entries){
			
			//printf("checking the queue:  PORT: %d, VF: %d, Enabled: %d\n", refresh_item->port_id, refresh_item->vf_id, refresh_item->enabled);
			/* check if item's q is enabled, update VF and remove item from queue */
			if(refresh_item->enabled){
				printf("updating VF: %d\n", refresh_item->vf_id);

				restore_vf_setings(refresh_item->port_id, refresh_item->vf_id);
				
				TAILQ_REMOVE(&rq_head, refresh_item, rq_entries);
				free(refresh_item);
			}   
			else
			{
				refresh_item->enabled = is_rx_queue_on(refresh_item->port_id, refresh_item->vf_id);
				//printf("updating item:  PORT: %d, VF: %d, Enabled: %d\n", refresh_item->port_id, refresh_item->vf_id, refresh_item->enabled);
			}			
		}
		
		//printf("Nothing to update\n");
		rte_spinlock_unlock(&rte_refresh_q_lock);
	}
}


/*
	Return the link speed for the indicated port
int nic_value_speed( uint8_t id ) {
	struct rte_eth_link link;

	rte_eth_link_get_nowait( id, &link );
	return (int) link.link_speed;
}
*/

void
nic_stats_clear(portid_t port_id)
{

	rte_eth_stats_reset(port_id);
	traceLog(TRACE_DEBUG, "\n  NIC statistics for port %d cleared\n", port_id);
}


int
nic_stats_display(uint8_t port_id, char * buff, int bsize)
{
	struct rte_eth_stats stats;
  struct rte_eth_link link;
  rte_eth_link_get_nowait(port_id, &link);
	rte_eth_stats_get(port_id, &stats);

  char status[5];
  if(!link.link_status)
    stpcpy(status, "DOWN");
  else
    stpcpy(status, "UP  ");

  return snprintf(buff, bsize, "        %s %10"PRIu16" %10"PRIu16" %10"PRIu64" %10"PRIu64" %10"PRIu64" %10"PRIu64" %10"PRIu64" %10"PRIu64" %10"PRIu64"\n",
    status, link.link_speed, link.link_duplex, stats.ipackets, stats.ibytes, stats.ierrors, stats.imissed, stats.opackets, stats.obytes, stats.oerrors);
}

int
port_init(uint8_t port, __attribute__((__unused__)) struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count()) {
			traceLog(TRACE_ERROR, "port >= rte_eth_dev_count\n");
   		exit(EXIT_FAILURE);
	}


	// Configure the Ethernet device.
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0) {
			traceLog(TRACE_ERROR, "Can not configure port %u, retval %d\n", port, retval);
   		exit(EXIT_FAILURE);
	}


  rte_eth_dev_callback_register(port, RTE_ETH_EVENT_INTR_LSC, lsi_event_callback, NULL);

  rte_eth_dev_callback_register(port, RTE_ETH_EVENT_VF_MBOX, vf_msb_event_callback, NULL);


	// Allocate and set up 1 RX queue per Ethernet port.
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0) {
			traceLog(TRACE_ERROR, "Can not setup rx queue, port %u\n", port);
   		exit(EXIT_FAILURE);
		}
	}

	// Allocate and set up 1 TX queue per Ethernet port.
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE, rte_eth_dev_socket_id(port), NULL);
		if (retval < 0) {
			traceLog(TRACE_ERROR, "Can not setup tx queue, port %u\n", port);
   		exit(EXIT_FAILURE);
		}
	}


	// Start the Ethernet port.
	retval = rte_eth_dev_start(port);
	if (retval < 0) {
		traceLog(TRACE_ERROR, "Can not start port %u\n", port);
  	exit(EXIT_FAILURE);
	}
	

	// Display the port MAC address.
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	traceLog(TRACE_DEBUG,  "Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);

	// Enable RX in promiscuous mode for the Ethernet device.
	rte_eth_promiscuous_enable(port);

	return 0;
}


void
lsi_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param)
{
	struct rte_eth_link link;

	RTE_SET_USED(param);

	traceLog(TRACE_DEBUG, "Event type: %s\n", type == RTE_ETH_EVENT_INTR_LSC ? "LSC interrupt" : "unknown event");
	rte_eth_link_get_nowait(port_id, &link);
	if (link.link_status) {
		traceLog(TRACE_DEBUG, "Port %d Link Up - speed %u Mbps - %s\n\n",
				port_id, (unsigned)link.link_speed,
			(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
				("full-duplex") : ("half-duplex"));
	} else
		traceLog(TRACE_DEBUG, "Port %d Link Down\n\n", port_id);

  // notify every VF about link status change
  rte_eth_dev_ping_vfs(port_id, -1);
}

int
check_mcast_mbox(uint32_t * mb)
{
  //#define IXGBE_VFMAILBOX_SIZE	16 /* 16 32 bit words - 64 bytes */
  uint32_t mbox[IXGBE_VFMAILBOX_SIZE];
  RTE_SET_USED(mb);

  RTE_SET_USED(mbox);

  return 0;
}

/*
	Called when a 'mailbox' message is received.  Examine and take action based on
	the message type.
*/
void
vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param) {
	uint32_t *p = (uint32_t*) param;
	uint16_t vf = p[0] & 0xffff;
	uint16_t mbox_type = (p[0] >> 16) & 0xffff;


	/* check & process VF to PF mailbox message */
	switch (mbox_type) {
		case IXGBE_VF_RESET:
			bleat_printf( 1, "reset event received: port=%d", port_id );

			*(int*) param = RTE_ETH_MB_EVENT_NOOP_ACK;     /* noop & ack */
			traceLog(TRACE_DEBUG, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_RESET");
				
			add_refresh_queue(port_id, vf);
			break;

		case IXGBE_VF_SET_MAC_ADDR:
			bleat_printf( 1, "setmac event received: port=%d", port_id );
			*(int*) param = RTE_ETH_MB_EVENT_PROCEED;    /* do what's needed */
			//*(int*) param = RTE_ETH_MB_EVENT_NOOP_ACK;     /* noop & ack */
			traceLog(TRACE_DEBUG, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_MAC_ADDR");
			
			struct ether_addr *new_mac = (struct ether_addr *)(&p[1]);
			
			if (is_valid_assigned_ether_addr(new_mac)) {
				traceLog(TRACE_DEBUG, "SETTING MAC, VF %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);
			}
			
			add_refresh_queue(port_id, vf);
			break;

		case IXGBE_VF_SET_MULTICAST:
			bleat_printf( 1, "setmulticast event received: port=%d", port_id );
			*(int*) param = RTE_ETH_MB_EVENT_PROCEED;    /* do what's needed */
			//*(int*) param = RTE_ETH_MB_EVENT_NOOP_ACK;     /* noop & ack */
			traceLog(TRACE_DEBUG, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_MULTICAST");

			new_mac = (struct ether_addr *)(&p[1]);

			if (is_valid_assigned_ether_addr(new_mac)) {
				traceLog(TRACE_DEBUG, "SETTING MCAST, VF %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);
			}
			break;

		case IXGBE_VF_SET_VLAN:
			// NOTE: we _always_ approve this.  This is the VMs setting of what will be an 'inner' vlan ID and thus we don't care
			bleat_printf( 1, "vlan set event approved: port=%d vf=%d vlan=%d (responding proceed)", port_id, vf, (int) p[1] );
			*((int*) param) = RTE_ETH_MB_EVENT_PROCEED;

			//traceLog(TRACE_DEBUG, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ", type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_VLAN");
			//traceLog(TRACE_DEBUG, "SETTING VLAN ID = %d\n", p[1]);
			break;

		case IXGBE_VF_SET_LPE:
			bleat_printf( 1, "set mtu event received %d %d", port_id, (int) p[1] );
			if( valid_mtu( port_id, (int) p[1] ) ) {
				bleat_printf( 1, "mtu set event approved: port=%d vf=%d mtu=%d", port_id, vf, (int) p[1] );
				*((int*) param) = RTE_ETH_MB_EVENT_PROCEED;
			} else {
				bleat_printf( 1, "mtu set event rejected: port=%d vf=%d mtu=%d", port_id, vf, (int) p[1] );
				*((int*) param) = RTE_ETH_MB_EVENT_NOOP_NACK;     /* noop & nack */
			}

			//traceLog(TRACE_DEBUG, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ", type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_LPE");
			//traceLog(TRACE_DEBUG, "SETTING MTU = %d\n", p[1]);
			break;

		case IXGBE_VF_SET_MACVLAN:
			bleat_printf( 1, "set macvlan event received: port=%d (responding nop+nak)", port_id );
			*(int*) param =  RTE_ETH_MB_EVENT_NOOP_NACK;    /* noop & nack */
			traceLog(TRACE_DEBUG, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ", type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_MACVLAN");
			traceLog(TRACE_DEBUG, "SETTING MAC_VLAN = %d\n", p[1]);
			break;

		case IXGBE_VF_API_NEGOTIATE:
			bleat_printf( 1, "set negotiate event received: port=%d (responding proceed)", port_id );
			*(int*) param =  RTE_ETH_MB_EVENT_PROCEED;   /* do what's needed */
			traceLog(TRACE_DEBUG, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_API_NEGOTIATE");
			break;

		case IXGBE_VF_GET_QUEUES:
			bleat_printf( 1, "get queues  event received: port=%d (responding proceed)", port_id );
			*(int*) param =  RTE_ETH_MB_EVENT_PROCEED;   /* do what's needed */
			traceLog(TRACE_DEBUG, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_GET_QUEUES");
			break;

		default:
			bleat_printf( 1, "unknown  event request received: port=%d (responding nop+nak)", port_id );
			*(int*) param = RTE_ETH_MB_EVENT_NOOP_NACK;     /* noop & nack */
			traceLog(TRACE_DEBUG, "Type: %d, Port: %d, VF: %d, OUT: %d, MBOX_TYPE: %d\n",
				type, port_id, vf, *(uint32_t*) param, mbox_type);
			break;
	}

  traceLog(TRACE_DEBUG, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %d\n",
      type, port_id, vf, *(uint32_t*) param, mbox_type);
  /*
  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(port_id, &dev_info);


	traceLog(TRACE_DEBUG, "driver_name = %s\n", dev_info.driver_name);
	traceLog(TRACE_DEBUG, "if_index = %d\n", dev_info.if_index);
	traceLog(TRACE_DEBUG, "min_rx_bufsize = %d\n", dev_info.min_rx_bufsize);
	traceLog(TRACE_DEBUG, "max_rx_pktlen = %d\n", dev_info.max_rx_pktlen);
	traceLog(TRACE_DEBUG, "max_rx_queues = %d\n", dev_info.max_rx_queues);
	traceLog(TRACE_DEBUG, "max_tx_queues = %d\n", dev_info.max_tx_queues);
	traceLog(TRACE_DEBUG, "max_mac_addrs = %d\n", dev_info.max_mac_addrs);
	traceLog(TRACE_DEBUG, "max_hash_mac_addrs = %d\n", dev_info.max_hash_mac_addrs);
	// Maximum number of hash MAC addresses for MTA and UTA.
	traceLog(TRACE_DEBUG, "max_vfs = %d\n", dev_info.max_vfs);
	traceLog(TRACE_DEBUG, "max_vmdq_pools = %d\n", dev_info.max_vmdq_pools);
	traceLog(TRACE_DEBUG, "rx_offload_capa = %d\n", dev_info.rx_offload_capa);
	traceLog(TRACE_DEBUG, "reta_size = %d\n", dev_info.reta_size);
	// Device redirection table size, the total number of entries.
	traceLog(TRACE_DEBUG, "hash_key_size = %d\n", dev_info.hash_key_size);
	///Bit mask of RSS offloads, the bit offset also means flow type
	traceLog(TRACE_DEBUG, "flow_type_rss_offloads = %lu\n", dev_info.flow_type_rss_offloads);
	traceLog(TRACE_DEBUG, "vmdq_queue_base = %d\n", dev_info.vmdq_queue_base);
	traceLog(TRACE_DEBUG, "vmdq_queue_num = %d\n", dev_info.vmdq_queue_num);
	traceLog(TRACE_DEBUG, "vmdq_pool_base = %d\n", dev_info.vmdq_pool_base);
  */
}



/*
	This is now a wrapper which invokes bleat_printf to allow for dynamic log level
	changes while running. Tracelog calls should eventually be converted to bleat
	calls directly, but this is low priority.
*/
void
traceLog(int eventTraceLevel, const char * file, int line, const char * format, ...)
{
	int bleat_level = 0;			// assume the worst -- message will be printed
	va_list va_ap;
	char buf[256], out_buf[256];
	char extra_msg[10];

	*extra_msg = 0;
	switch( eventTraceLevel ) {
		case 7:						// TRACE_xxx are macros, not values, so they cant be used here
			bleat_level = 3;
			break;

		case 6:
			bleat_level = 2;
			break;

		case 5:	
			bleat_level = 1;
			break;

		case 3:
      		strcpy(extra_msg, "ERR: ");
			break;

		case 4:
      		strcpy(extra_msg, "WRN: ");
			break;

		default:			
      		strcpy(extra_msg, "CRI: ");
			break;		// all others considered to be always print
	}

    va_start (va_ap, format);
    memset(buf, 0, sizeof(buf));
    vsnprintf(buf, sizeof(buf) - 1, format, va_ap);

    while(buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';

    if(traceLevel > 6)
      snprintf(out_buf, sizeof(out_buf), "[%s:%d] %s%s", file, line, extra_msg, buf);
    else
      snprintf(out_buf, sizeof(out_buf), "%s%s", extra_msg, buf);

  	va_end(va_ap);

	bleat_printf( bleat_level, "%s", out_buf );
}


/*
	Writes the current pid into the named file as a newline terminated string.
	Returns true on success.
*/
static int save_pid( char* fname ) {
	int fd;
	char buf[100];
	int	len;
	int rc = 0;

	if( (fd = open( fname, O_CREAT|O_TRUNC|O_WRONLY, 0644 )) >= 0 ) {
		len = snprintf( buf, sizeof( buf ), "%d\n", getpid()  );
		if( write( fd, buf, len ) == len ) {
			rc = 1;
		}
		close( fd );
	}

	return rc;
}

void
detachFromTerminal(void)
{
  setsid();  // detach from the terminal

  fclose(stdin);
	dup2( 1, 2 );				// dup stdout to stderr rather than closing so we get rte messages that appear on stdout

  umask(0); 					// clear any inherited file mode creation mask

  setvbuf( stderr, (char *)NULL, _IOLBF, 0);
}


void
daemonize(  char* pid_fname )
{
  int childpid;

  //signal(SIGHUP, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  if((childpid = fork()) < 0)
    traceLog(TRACE_ERROR, "INIT: Can not fork process (errno = %d)", errno);
  else {
#ifdef DEBUG
    traceLog(TRACE_INFO, "DEBUG: after fork() in %s (%d)",
	       childpid ? "parent" : "child", childpid);
#endif
    if(!childpid) {
      // child
      traceLog(TRACE_INFO, "INIT: Starting Tcap daemon");
      detachFromTerminal();
		if( pid_fname != NULL ) {
			save_pid( pid_fname );
		}
    }
    else {
      // parent
      traceLog(TRACE_INFO, "INIT: Parent process exits");
      exit(EXIT_SUCCESS);
    }
  }
}




