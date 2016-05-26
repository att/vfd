/*
	Mnemonic:	sriov.c
	Abstract: 	The direct interface between VFd and the DPDK library.
	Date:		February 2016
	Author:		Alex Zelezniak

	Mods:		06 May 2016 - Added some doc and changed port_init() to return rather
					than to exit.
				18 May 2016 - Verify vlan is configured for the port/vf before acking it; nak
					if it is not. 
				19 May 2016 - Added check for VF range in print function.
	useful doc:
				 http://www.intel.com/content/dam/doc/design-guide/82599-sr-iov-driver-companion-guide.pdf
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

	bleat_printf( 3,"Port %d", port_id);

	if (port_id == (portid_t)RTE_PORT_ALL)
		return 0;

	if (port_id < RTE_MAX_ETHPORTS && ports[port_id].enabled)
		return 0;

	if( warning == ENABLED_WARN )
		bleat_printf( 2, "warn: Invalid port %d", port_id);

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
	
		//bleat_printf( 0, "rte_eth_set_vf_rate_limit for port_id=%d failed diag=%d", port_id, diag);
	}

	return diag;
}



int
port_reg_off_is_invalid(portid_t port_id, uint32_t reg_off)
{
	uint64_t pci_len;

	if (reg_off & 0x3) {
		bleat_printf( 3, "Port register offset 0x%X not aligned on a 4-byte boundary", (unsigned)reg_off);
		return 1;
	}
	pci_len = ports[port_id].dev_info.pci_dev->mem_resource[0].len;
	if (reg_off >= pci_len) {
		bleat_printf( 3, "Port %d: register offset %u (0x%X) out of port PCI "
		       "resource (length=%"PRIu64")",
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
		bleat_printf( 3, "rx_vlan_strip_set_on_queue(port_pi=%d, queue_id=%d, on=%d) failed " "diag=%d", port_id, queue_id, on, diag);
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

	bleat_printf( 3, "tx_vlan_insert_set_on_vf: bar=0x%08X, vf_id=%d, vlan=%d", reg_off, vf_id, vlan_id);

	uint32_t ctrl = port_pci_reg_read(port_id, reg_off);

	bleat_printf( 3, "tx_vlan_insert_set_on_vf: read: bar=0x%08X, vf_id=%d, ctrl=0x%x", reg_off, vf_id, ctrl);


	if (vlan_id){
		ctrl = vlan_id;
		ctrl |= 0x40000000;
	} else {
		ctrl = 0;
	}

	port_pci_reg_write(port_id, reg_off, ctrl);

	bleat_printf( 3, "tx_insert_set_on_vf: set: bar=0x%08X, vfid_id=%d, ctrl=0x%08X", reg_off, vf_id, ctrl);
}


void
rx_vlan_strip_set_on_vf(portid_t port_id, uint16_t vf_id, int on)
{

  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(port_id, &dev_info);

  uint32_t queues_per_pool = dev_info.vmdq_queue_num / dev_info.max_vmdq_pools;

  uint32_t reg_off = 0x01028;						// receive descriptor control reg (pg527/597)

  reg_off += (0x40 * vf_id * queues_per_pool);

  bleat_printf( 3, "rx_vlan_strip_set_on_vf: bar=0x%08X, vf_id=%d, numq=%d)", reg_off, vf_id, queues_per_pool);

  uint32_t q;
  for(q = 0; q < queues_per_pool; ++q){

    reg_off += 0x40 * q;

    bleat_printf( 3, "rx_vlan_strip_set_on_vf: q=%d bar=0x%08X, vf_id=%d, on=%d", q, reg_off, vf_id, on);

    uint32_t ctrl = port_pci_reg_read(port_id, reg_off);

    bleat_printf( 3, "rx_vlan_strip_set_on_vf: read: q=%d bar=0x%08X, vf_id=%d, ctrl=0x%x", q, reg_off, vf_id, ctrl);


    if (on)
      ctrl |= IXGBE_RXDCTL_VME;				// vlan mode enable (strip flag)
    else
      ctrl &= ~IXGBE_RXDCTL_VME;

    port_pci_reg_write(port_id, reg_off, ctrl);    		// void -- no error to check

    bleat_printf( 3, "rx_vlan_strip_set_on_vf: set: q=%d bar=0x%08X, vfid_id=%d, ctrl=0x%08X)", q, reg_off, vf_id, ctrl);
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
		bleat_printf( 1, "rx_vlan_strip_set(port_pi=%d, on=%d) failed, diag=%d", port_id, on, diag);
	} else {
		bleat_printf( 3, "set vlan strip successful: %d: on/off=%d", port_id, on );
	}
}



void
set_vf_allow_bcast(portid_t port_id, uint16_t vf_id, int on)
{
  int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_BROADCAST,(uint8_t) on);
	
	if (ret < 0) {
    	bleat_printf( 1, "set_vf_allow_bcast(): bad VF receive mode parameter, return code = %d", ret);
	} else {
		bleat_printf( 3, "allow bcast successfully set for port/vf %d/%d on/off=%d", port_id, vf_id, on );
	}
}


void
set_vf_allow_mcast(portid_t port_id, uint16_t vf_id, int on)
{
	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_MULTICAST,(uint8_t) on);
	
	if (ret < 0) {
    	bleat_printf( 1, "set_vf_allow_mcast(): bad VF receive mode parameter, return code = %d", ret);
	} else {
		bleat_printf( 3, "allow mcast successfully set for port/vf %d/%d on/off=%d", port_id, vf_id, on );
	}
}


void
set_vf_allow_un_ucast(portid_t port_id, uint16_t vf_id, int on)
{
	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_HASH_UC,(uint8_t) on);
	
	if (ret < 0) {
    	bleat_printf( 1, "set_vf_allow_un_ucast(): bad VF receive mode parameter, return code = %dn", ret);
	} else {
		bleat_printf( 3, "allow un-ucast successfully set for port/vf %d/%d on/off=%d", port_id, vf_id, on );
	}
}


void
set_vf_allow_untagged(portid_t port_id, uint16_t vf_id, int on)
{
  uint16_t rx_mode = 0;
  rx_mode |= ETH_VMDQ_ACCEPT_UNTAG;

  bleat_printf( 3, "set_vf_allow_untagged(): rx_mode = %d, on = %dn", rx_mode, on);

	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, rx_mode, (uint8_t) on);
	
	if (ret < 0)
    	bleat_printf( 1, "set_vf_allow_untagged(): bad VF receive mode parameter, return code = %dn", ret);
}

/*
	Add one mac to the receive mac filter whitelist.  Only the traffic sent to the dest macs in the
	list will be passed to the VF.
	
*/
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
		bleat_printf( 0, "rte_eth_dev_mac_addr_add for port_id=%d failed " "diag=%d", port_id, diag);
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
		bleat_printf( 0, "rte_eth_dev_set_vf_vlan_filter for port_id=%d failed " "diag=%d", port_id, diag);
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
		bleat_printf( 0, "rte_eth_dev_set_vf_vlan_anti_spoof for port_id=%d failed " "diag=%d vf=%d", port_id, diag, vf);
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
		bleat_printf( 0, "rte_eth_dev_set_vf_mac_anti_spoof for port_id=%d failed " "diag=%d vf=%d", port_id, diag, vf);
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

/*
	Check the state of the split receive control register
*/
int get_split_ctlreg( portid_t port_id, uint16_t vf_id ) {
	
	uint32_t reg_off = 0x01014; 							// split receive control regs (pg598)

	if( vf_id > 63  ) {				// this offset good only for 0-63; we won't support 64-127
		return 0;
	}

	reg_off += (0x40 * vf_id );
	
	return (int) port_pci_reg_read( port_id, reg_off );
}

/*
	Set/reset the enable drop bit in the split receive control register. State is either 1 (on) or 0 (off).
*/
void set_split_erop( portid_t port_id, uint16_t vf_id, int state ) {
	
	uint32_t reg_off = 0x01014; 							// split receive control regs (pg598)
	uint32_t reg_value;

	if( vf_id > 63  ) {				// this offset good only for 0-63; we won't support 64-127
		return;
	}

	reg_off += (0x40 * vf_id );
	
	reg_value =  port_pci_reg_read( port_id, reg_off );

	if( state ) {
		reg_value |= IXGBE_SRRCTL_DROP_EN;								// turn on the enable bit
	} else {
		reg_value &= ~IXGBE_SRRCTL_DROP_EN;								// turn off the enable bit
	}

	bleat_printf( 2, "setting split receive drop for port %d vf %d to %d", port_id, vf_id, state );
	port_pci_reg_write( port_id, reg_off, reg_value );
}

/*
	Set/reset the queue drop enable bit for all pools. State is either 1 (on) or 0 (off).
*/
void set_queue_drop( portid_t port_id, int state ) {
	int 		i;
	uint32_t reg_off;
	uint32_t reg_value;							// value to write into the register


	reg_off = 0x02f04; 							// PF queue drop enable register (pg728)
	
	bleat_printf( 2, "setting queue drop for port %d on all queues to: %d", port_id, (state & 0x01) );
	for( i = 0; i < 128; i++ ) {
		reg_value = IXGBE_QDE_WRITE | (i << IXGBE_QDE_IDX_SHIFT) | (state & 0x01);

		port_pci_reg_write( port_id, reg_off, reg_value );
	}
}

// --------------- pending reset support ----------------------------------------------------------------------
/*
	The refresh queue is where VFd manages queued reset requests. When we receive
	a mailbox message to reset, we must wait for the tx/rx queues on the virtual
	device to show ready before we can actually reset them. These functions
	are used to manage the queued reset requests until it is ok to actually
	execute them.

	When a reset is received, the device is checkecked and if not ready the reset
	is added to the queue. If there is already a reset queued, the new one is ignored.
	Periodically we test each device associated with a reset on our queue to see if
	the tx/rx queues are ready and if they are we allow the reset to happen.
*/
/*
	Check to see if the NIC tx/rx queues are on for the pf/vf pair.
	Returns 1 if the queues are "ready".
*/
int
is_rx_queue_on(portid_t port_id, uint16_t vf_id, int* mcounter )
{
	/* check if first queue in the pool is active */
	
	struct rte_eth_dev *pf_dev = &rte_eth_devices[port_id];
  uint32_t queues_per_pool = RTE_ETH_DEV_SRIOV(pf_dev).nb_q_per_pool;
	queues_per_pool = 2;											// if we don't have RSS or DCB enabled number of queues is 2 per pool ?
	
  uint32_t reg_off = 0x01028; 							// receive descriptor control reg (pg527/597)

  reg_off += (0x40 * vf_id * queues_per_pool);
	
  uint32_t ctrl = port_pci_reg_read(port_id, reg_off);

  bleat_printf( 5, "RX_QUEUS_ENA, bar=0x%08X, port=%d vfid_id=%d, ctrl=0x%08X)", reg_off, port_id, vf_id, ctrl);		// these happen too frequently if a VM goes away; only if really verbose
	
	if( ctrl & 0x2000000) {
  		bleat_printf( 3, "first queue active: bar=0x%08X, port=%d vfid_id=%d, ctrl=0x%08X)", reg_off, port_id, vf_id, ctrl);
		return 1;
	} else {
		if( (*mcounter % 100 ) == 0 ) {
  			bleat_printf( 4, "still pending: first queue not active: bar=0x%08X, port=%d vfid_id=%d, ctrl=0x%08x q/pool=%d", reg_off, port_id, vf_id, ctrl, (int) RTE_ETH_DEV_SRIOV(pf_dev).nb_q_per_pool);
		}
		(*mcounter)++;
		return 0;
	}
}


static rte_spinlock_t rte_refresh_q_lock = RTE_SPINLOCK_INITIALIZER;

/*
	Add a reset event to our queue.  We will pop it and update the nic
	when the pf/vf queues are ready. If a reset for the pf/vf is already on
	the queue then we do nothing.
*/
void
add_refresh_queue(u_int8_t port_id, uint16_t vf_id)
{
	
	struct rq_entry *refresh_item;
	
	/* look for refresh request and update enabled status if already there */
	rte_spinlock_lock(&rte_refresh_q_lock);
	TAILQ_FOREACH(refresh_item, &rq_head, rq_entries) {
		if (refresh_item->port_id == port_id && refresh_item->vf_id == vf_id){
			if (!refresh_item->enabled)
				refresh_item->enabled = is_rx_queue_on(port_id, vf_id, &refresh_item->mcounter );
			
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
	refresh_item->mcounter = 0;
	refresh_item->enabled = is_rx_queue_on(port_id, vf_id, &refresh_item->mcounter );
	bleat_printf( 2, "adding refresh to queue for %d/%d", port_id, vf_id );
	
	rte_spinlock_lock(&rte_refresh_q_lock);
	TAILQ_INSERT_TAIL(&rq_head, refresh_item, rq_entries);	
	rte_spinlock_unlock(&rte_refresh_q_lock);
}

/*
	If a queued block for port/vf exists, mark it enabled. This is a hack.
	There are observed cases where the VF tx/rx queues never show ready. This
	function will force the pending reset to be dispatchable regardless of
	what the state of the NIC is.  This funciton is called when we receive a
	mailbox message which we interpret as meaning that the device is up and
	in a 'ready' state.
*/
static void enable_refresh_queue(u_int8_t port_id, uint16_t vf_id)
{
	struct rq_entry *refresh_item;
	
	bleat_printf( 3, "enable is looking for: %d %d", port_id, vf_id );
	rte_spinlock_lock(&rte_refresh_q_lock);
	TAILQ_FOREACH(refresh_item, &rq_head, rq_entries) {
		if (refresh_item->port_id == port_id && refresh_item->vf_id == vf_id){
			bleat_printf( 2, "enabling %d/%d", port_id, vf_id );
			refresh_item->enabled = 1;
		}
	}
	rte_spinlock_unlock(&rte_refresh_q_lock);
	return;
}


/*
	This is executed in it's own thread and is responsible for checking the
	queue of pending resets. If a pending reset becomes 'enabled' then
	the reset is 'executed' by invoking the restore_vf_setings() function
	and the reset is removed from our queue.
*/
void
process_refresh_queue(void)
{
	while(1) {
		
		usleep(200000);
		struct rq_entry *refresh_item;
		
		rte_spinlock_lock(&rte_refresh_q_lock);
		TAILQ_FOREACH(refresh_item, &rq_head, rq_entries){
			
			//printf("checking the queue:  PORT: %d, VF: %d, Enabled: %d\n", refresh_item->port_id, refresh_item->vf_id, refresh_item->enabled);
			/* check if item's q is enabled, update VF and remove item from queue */
			if(refresh_item->enabled){
				bleat_printf( 2, "refresh item enabled: updating VF: %d", refresh_item->vf_id);

				restore_vf_setings(refresh_item->port_id, refresh_item->vf_id);
				
				TAILQ_REMOVE(&rq_head, refresh_item, rq_entries);
				free(refresh_item);
			} 
			else
			{
				refresh_item->enabled = is_rx_queue_on(refresh_item->port_id, refresh_item->vf_id, &refresh_item->mcounter );
				//printf("updating item:  PORT: %d, VF: %d, Enabled: %d\n", refresh_item->port_id, refresh_item->vf_id, refresh_item->enabled);
			}			
		}
		
		rte_spinlock_unlock(&rte_refresh_q_lock);
	}
}

// -----------------------------------------------------------------------------------------------------------------------------

/*
	Return the link speed for the indicated port
*/
/*
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
	bleat_printf( 3, "\n  NIC statistics for port %d cleared", port_id);
}


int
nic_stats_display(uint8_t port_id, char * buff, int bsize)
{
	struct rte_eth_stats stats;
  struct rte_eth_link link;
  rte_eth_link_get_nowait(port_id, &link);
	rte_eth_stats_get(port_id, &stats);

	spoffed[port_id] += port_pci_reg_read(port_id, 0x08780);
	
			
  char status[5];
  if(!link.link_status)
    stpcpy(status, "DOWN");
  else
    stpcpy(status, "UP  ");

  return snprintf(buff, bsize, "    %s %10"PRIu16" %10"PRIu16" %10"PRIu64" %10"PRIu64" %10"PRIu64" %10"PRIu64" %10"PRIu64" %10"PRIu64" %10"PRIu64" %10"PRIu32"\n",
    status, link.link_speed, link.link_duplex, stats.ipackets, stats.ibytes, stats.ierrors, stats.imissed, stats.opackets, stats.obytes, stats.oerrors, spoffed[port_id]);
}

/*
*	prints VF statistics
	Returns number of characters placd into buff, or -1 if error (vf not in use
	or out of range).  The parm ivf is the virtual function number which is maintained
	as integer in our datstructs allowing -1 to indicate an uninstalled/delted VF.
	It is converted to uint32 for calculations here. 
* 
*/
int 
vf_stats_display(uint8_t port_id, uint32_t pf_ari, int ivf, char * buff, int bsize)
{
	uint32_t vf;

	if( ivf < 0 || ivf > 31 ) {
		return -1;
	}

	vf = (uint32_t) ivf;						// unsinged for rest
	
	uint32_t new_ari;
	struct rte_pci_addr vf_pci_addr;
	

	new_ari = pf_ari + vf_offfset + (vf * vf_stride);
	
	vf_pci_addr.domain = 0;
	vf_pci_addr.bus = (new_ari >> 8) & 0xff;
	vf_pci_addr.devid = (new_ari >> 3) & 0x1f;
	vf_pci_addr.function = new_ari & 0x7;

				
	uint32_t	rx_pkts = port_pci_reg_read(port_id, IXGBE_PVFGPRC(vf));
	uint64_t	rx_ol = port_pci_reg_read(port_id, IXGBE_PVFGORC_LSB(vf));
	uint64_t	rx_oh = port_pci_reg_read(port_id, IXGBE_PVFGORC_MSB(vf));
	uint64_t	rx_octets = (rx_oh << 32) |	rx_ol;		// 36 bit only counter
	
	uint32_t	tx_pkts = port_pci_reg_read(port_id, IXGBE_PVFGPTC(vf));
	uint64_t	tx_ol = port_pci_reg_read(port_id, IXGBE_PVFGOTC_LSB(vf));
	uint64_t	tx_oh = port_pci_reg_read(port_id, IXGBE_PVFGOTC_MSB(vf));
	uint64_t	tx_octets = (tx_oh << 32) |	tx_ol;		// 36 bit only counter
	
	
	char status[5];
	int mcounter = 0;
  if(!is_rx_queue_on(port_id, vf, &mcounter ))
    stpcpy(status, "DOWN");
  else
    stpcpy(status, "UP  ");

	return 	snprintf(buff, bsize, "%s   %4d    %04X:%02X:%02X.%01X    %s %32"PRIu32" %10"PRIu64" %32"PRIu32" %10"PRIu64"\n",
				"vf",
				vf,
				vf_pci_addr.domain, 
				vf_pci_addr.bus, 
				vf_pci_addr.devid, 
				vf_pci_addr.function,
				status,
				rx_pkts, 
				rx_octets, 
				tx_pkts, 
				tx_octets);
}


/*
  dumps all LAN ID's configured 
  to be used for debugging  
  or to check if number of vlans doesn't exceed MAX (64)
*/
int 
dump_vlvf_entry(portid_t port_id)
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


/*
	Initialise a device (port).
	Return 0 if there were no errors, 1 otherwise.  The calling programme should
	not continue if this function returns anything but 0.
*/
int
port_init(uint8_t port, __attribute__((__unused__)) struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count()) {
		bleat_printf( 0, "CRI: abort: port >= rte_eth_dev_count");
   		//exit(EXIT_FAILURE);
		return 1;
	}


	// Configure the Ethernet device.
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0) {
		bleat_printf( 0, "CRI: abort: can not configure port %u, retval %d", port, retval);
   		//exit(EXIT_FAILURE);
		return 1;
	}


  rte_eth_dev_callback_register(port, RTE_ETH_EVENT_INTR_LSC, lsi_event_callback, NULL);

  rte_eth_dev_callback_register(port, RTE_ETH_EVENT_VF_MBOX, vf_msb_event_callback, NULL);


	// Allocate and set up 1 RX queue per Ethernet port.
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0) {
			bleat_printf( 0, "CRI: abort: can not setup rx queue, port %u", port);
   			//exit(EXIT_FAILURE);
			return 1;
		}
	}

	// Allocate and set up 1 TX queue per Ethernet port.
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE, rte_eth_dev_socket_id(port), NULL);
		if (retval < 0) {
			bleat_printf( 0, "CRI: abort: can not setup tx queue, port %u", port);
   			//exit(EXIT_FAILURE);
			return 1;
		}
	}


	// Start the Ethernet port.
	retval = rte_eth_dev_start(port);
	if (retval < 0) {
		bleat_printf( 0, "CRI: abort: can not start port %u", port);
  		//exit(EXIT_FAILURE);
		return 1;
	}
	

	// Display the port MAC address.
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	bleat_printf( 3,  "port_init: port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "",
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

	bleat_printf( 3, "Event type: %s", type == RTE_ETH_EVENT_INTR_LSC ? "LSC interrupt" : "unknown event");
	rte_eth_link_get_nowait(port_id, &link);
	if (link.link_status) {
		bleat_printf( 3, "Port %d Link Up - speed %u Mbps - %s",
				port_id, (unsigned)link.link_speed,
			(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
				("full-duplex") : ("half-duplex"));
	} else
		bleat_printf( 3, "Port %d Link Down", port_id);

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
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_RESET");
				
			add_refresh_queue(port_id, vf);
			break;

		case IXGBE_VF_SET_MAC_ADDR:
			bleat_printf( 1, "setmac event received: port=%d", port_id );
			*(int*) param = RTE_ETH_MB_EVENT_PROCEED;    						// do what's needed
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_MAC_ADDR");
			
			struct ether_addr *new_mac = (struct ether_addr *)(&p[1]);
			
			if (is_valid_assigned_ether_addr(new_mac)) {
				bleat_printf( 3, "setting mac, vf %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
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
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_MULTICAST");

			new_mac = (struct ether_addr *)(&p[1]);

			if (is_valid_assigned_ether_addr(new_mac)) {
				bleat_printf( 3, "setting mcast, vf %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);
			}
			break;

		case IXGBE_VF_SET_VLAN:
			// NOTE: we _always_ approve this.  This is the VMs setting of what will be an 'inner' vlan ID and thus we don't care
			if( valid_vlan( port_id, vf, (int) p[1] ) ) {
				bleat_printf( 1, "vlan set event approved: port=%d vf=%d vlan=%d (responding noop-ack)", port_id, vf, (int) p[1] );
				//*((int*) param) = RTE_ETH_MB_EVENT_PROCEED;
				*(int*) param = RTE_ETH_MB_EVENT_NOOP_ACK;     // good rc to VM while not changing anything 
			} else {
				bleat_printf( 1, "vlan set event rejected; vlan not not configured: port=%d vf=%d vlan=%d (responding noop-ack)", port_id, vf, (int) p[1] );
				*(int*) param = RTE_ETH_MB_EVENT_NOOP_NACK;     // VM should see failure
			}

			//bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ", type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_VLAN");
			//bleat_printf( 3, "setting vlan id = %d", p[1]);
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

			//bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ", type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_LPE");
			//bleat_printf( 3, "setting mtu = %d", p[1]);
			break;

		case IXGBE_VF_SET_MACVLAN:
			bleat_printf( 1, "set macvlan event received: port=%d (responding nop+nak)", port_id );
			*(int*) param =  RTE_ETH_MB_EVENT_NOOP_NACK;    /* noop & nack */
			bleat_printf( 3, "type: %d, port: %d, vf: %d, out: %d, _T: %s ", type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_MACVLAN");
			bleat_printf( 3, "setting mac_vlan = %d", p[1]);
			//bleat_printf( 3, "calling enable with: %d %d", port_id, vf );

			// ### this is a hack, but until we see a queue ready everywhere/everytime we assume we can enable things when we see this message
			enable_refresh_queue( port_id, vf );
			break;

		case IXGBE_VF_API_NEGOTIATE:
			bleat_printf( 1, "set negotiate event received: port=%d (responding proceed)", port_id );
			*(int*) param =  RTE_ETH_MB_EVENT_PROCEED;   /* do what's needed */
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_API_NEGOTIATE");
			break;

		case IXGBE_VF_GET_QUEUES:
			bleat_printf( 1, "get queues  event received: port=%d (responding proceed)", port_id );
			*(int*) param =  RTE_ETH_MB_EVENT_PROCEED;   /* do what's needed */
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_GET_QUEUES");
			break;

		default:
			bleat_printf( 1, "unknown  event request received: port=%d (responding nop+nak)", port_id );
			*(int*) param = RTE_ETH_MB_EVENT_NOOP_NACK;     /* noop & nack */
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, MBOX_TYPE: %d",
				type, port_id, vf, *(uint32_t*) param, mbox_type);
			break;
	}

  bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %d",
      type, port_id, vf, *(uint32_t*) param, mbox_type);
  /*
  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(port_id, &dev_info);


	bleat_printf( 3, "driver_name = %s", dev_info.driver_name);
	bleat_printf( 3, "if_index = %d", dev_info.if_index);
	bleat_printf( 3, "min_rx_bufsize = %d", dev_info.min_rx_bufsize);
	bleat_printf( 3, "max_rx_pktlen = %d", dev_info.max_rx_pktlen);
	bleat_printf( 3, "max_rx_queues = %d", dev_info.max_rx_queues);
	bleat_printf( 3, "max_tx_queues = %d", dev_info.max_tx_queues);
	bleat_printf( 3, "max_mac_addrs = %d", dev_info.max_mac_addrs);
	bleat_printf( 3, "max_hash_mac_addrs = %d", dev_info.max_hash_mac_addrs);
	// Maximum number of hash MAC addresses for MTA and UTA.
	bleat_printf( 3, "max_vfs = %d", dev_info.max_vfs);
	bleat_printf( 3, "max_vmdq_pools = %d", dev_info.max_vmdq_pools);
	bleat_printf( 3, "rx_offload_capa = %d", dev_info.rx_offload_capa);
	bleat_printf( 3, "reta_size = %d", dev_info.reta_size);
	// Device redirection table size, the total number of entries.
	bleat_printf( 3, "hash_key_size = %d", dev_info.hash_key_size);
	///Bit mask of RSS offloads, the bit offset also means flow type
	bleat_printf( 3, "flow_type_rss_offloads = %lu", dev_info.flow_type_rss_offloads);
	bleat_printf( 3, "vmdq_queue_base = %d", dev_info.vmdq_queue_base);
	bleat_printf( 3, "vmdq_queue_num = %d", dev_info.vmdq_queue_num);
	bleat_printf( 3, "vmdq_pool_base = %d", dev_info.vmdq_pool_base);
  */
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
    bleat_printf( 0, "INIT: Can not fork process (errno = %d)", errno);
  else {
#ifdef DEBUG
    bleat_printf( 1, "DEBUG: after fork() in %s (%d)",
	       childpid ? "parent" : "child", childpid);
#endif
    if(!childpid) {
      // child
      bleat_printf( 1, "INIT: Starting Tcap daemon");
      detachFromTerminal();
		if( pid_fname != NULL ) {
			save_pid( pid_fname );
		}
    }
    else {
      // parent
      bleat_printf( 1, "INIT: Parent process exits");
      exit(EXIT_SUCCESS);
    }
  }
}

/*
	Called when a dump request is received from iplex. Writes general things about each port
	into the log.
*/
void dump_dev_info( int num_ports  ) {
	int i;
	struct rte_eth_dev_info dev_info;

	for( i = 0; i < num_ports; i++ ) {
		rte_eth_dev_info_get( i, &dev_info );
	
		bleat_printf( 0, "port=%d driver_name = %s", i, dev_info.driver_name);
		bleat_printf( 0, "port=%d if_index = %d", i, dev_info.if_index);
		bleat_printf( 0, "port=%d min_rx_bufsize = %d", i, dev_info.min_rx_bufsize);
		bleat_printf( 0, "port=%d max_rx_pktlen = %d", i, dev_info.max_rx_pktlen);
		bleat_printf( 0, "port=%d max_rx_queues = %d", i, dev_info.max_rx_queues);
		bleat_printf( 0, "port=%d max_tx_queues = %d", i, dev_info.max_tx_queues);
		bleat_printf( 0, "port=%d max_mac_addrs = %d", i, dev_info.max_mac_addrs);
		bleat_printf( 0, "port=%d max_hash_mac_addrs = %d", i, dev_info.max_hash_mac_addrs);

		// Maximum number of hash MAC addresses for MTA and UTA.
		bleat_printf( 0, "port=%d max_vfs = %d", i, dev_info.max_vfs);
		bleat_printf( 0, "port=%d max_vmdq_pools = %d", i, dev_info.max_vmdq_pools);
		bleat_printf( 0, "port=%d rx_offload_capa = %d", i, dev_info.rx_offload_capa);
		bleat_printf( 0, "port=%d reta_size = %d", i, dev_info.reta_size);

		// Device redirection table size, the total number of entries.
		bleat_printf( 0, "port=%d hash_key_size = %d", i, dev_info.hash_key_size);

		///Bit mask of RSS offloads, the bit offset also means flow type
		bleat_printf( 0, "port=%d flow_type_rss_offloads = %lu", i, dev_info.flow_type_rss_offloads);
		bleat_printf( 0, "port=%d vmdq_queue_base = %d", i, dev_info.vmdq_queue_base);
		bleat_printf( 0, "port=%d vmdq_queue_num = %d", i, dev_info.vmdq_queue_num);
		bleat_printf( 0, "port=%d vmdq_pool_base = %d", i, dev_info.vmdq_pool_base);
	}
}



