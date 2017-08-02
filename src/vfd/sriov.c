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
				05 Aug 2016 - Changes to work with dpdk16.04.
				15 Aug 2016 - Changes to work with dpdk16.07.
				16 Aug 2016 - removed unused routines.
				07 Sep 2016 - Remvoed TAILQ macros as these seemed to be freeing a block of memory
					without discarding the pointer.
				20 Oct 2016 - Changes to support the dpdk 16.11 rc1 code.
				01 Nov 2016 - Correct queue drop enable bug (wrong ixgbe function invoked).
				10 Nov 2016 - Extend queue ready to support less than 32 configured VFs.
				31 Jan 2017 - Corrected error messages for untagged, mcast & bcast; added rc
					value to all failure msgs. Added calls to either directly refresh or queue a
					refresh on callbacks which didn't already have one. This is necessary for
					guest-guest across a back to back connection (no intermediate switch) and
					for properly resetting mcast flag (unknown what event is turning that off on
					the NIC).
				11 Feb 2017 - Changes to prevent packet loss which was occuring when the drop
					enable bit was set for VF queues. Fixed alignment on show output to handle
					wider fields.
				21 Mar 2017 - Ensure that looback is set on a port when reset/negotiate callbacks
					are dirven.
				06 Apr 2017 - Add set flowcontrol function, add mtu/jumbo confirmation msg to log.

	useful doc:
				 http://www.intel.com/content/dam/doc/design-guide/82599-sr-iov-driver-companion-guide.pdf
*/

#include "vfdlib.h"
#include "sriov.h"
#include "vfd_dcb.h"


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

/*
	Get_max_qpp returns the maximum number of queues which could be supported
	by a pool based on the current number of VFs configured for the PF.
*/
static int get_max_qpp( uint32_t port_id ) {
	struct rte_eth_dev_info dev_info;

	rte_eth_dev_info_get( port_id, &dev_info );

	if( dev_info.max_vfs >= 32 ) {				// set the max queues/pool based on the number of VFs which are configured
		return 2;
	} else {
		if( dev_info.max_vfs >= 16 ) {
			return 4;
		}
	}

	return 8;
}

/*
	Return the number of VFs for the port.
*/
static int get_num_vfs( uint32_t port_id ) {
	struct rte_eth_dev_info dev_info;

	rte_eth_dev_info_get( port_id, &dev_info );

	return dev_info.max_vfs;
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
set_vf_rate_limit(portid_t port_id, uint16_t vf, uint16_t rate, uint64_t q_msk)
{
	int diag;
	struct rte_eth_link link;

	if (q_msk == 0)
		return 0;


	rte_eth_link_get_nowait(port_id, &link);
	if (rate > link.link_speed) {
		bleat_printf( 0, "set_vf_rate: invalid rate value: %u bigger than link speed: %u", rate, link.link_speed);
		return 1;
	}
	diag = rte_eth_set_vf_rate_limit(port_id, vf, rate, q_msk);
	if (diag != 0) {
		bleat_printf( 0, "set_vf_rate: unable to set value %u: (%d) %s", rate, diag, strerror( -diag ) );
	}

	return diag;
}

/*
	Set VLAN tag on transmission.  If no tag is to be inserted, then a VLAN
	ID of 0 must be passed.
*/
void
tx_vlan_insert_set_on_vf(portid_t port_id, uint16_t vf_id, int vlan_id)
{
	int diag;

#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0) {
		diag = rte_pmd_bnxt_set_vf_vlan_insert( port_id, vf_id, vlan_id );
		if (diag >= 0) {
			if (vlan_id == 0) {
				struct vf_s *vf_cfg = suss_vf(port_id, vf_id);

				if (!vf_cfg->strip_stag)
					rte_pmd_bnxt_set_vf_vlan_stripq(port_id, vf_id, vlan_id);
			}
		}
	}
	else
#endif
		diag = rte_pmd_ixgbe_set_vf_vlan_insert( port_id, vf_id, vlan_id );

	if (diag < 0) {
		bleat_printf( 0, "set tx vlan insert on vf failed: port_pi=%d, vf_id=%d, vlan_id=%d) failed rc=%d", port_id, vf_id, vlan_id, diag );
	} else {
		bleat_printf( 3, "set tx vlan insert on vf successful: port=%d, vf=%d vlan=%d", port_id, vf_id, vlan_id );
	}
}


void
rx_vlan_strip_set_on_vf(portid_t port_id, uint16_t vf_id, int on)
{
	int diag;

#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0)
		diag = rte_pmd_bnxt_set_vf_vlan_stripq(port_id, vf_id, on);
	else
#endif
		diag = rte_pmd_ixgbe_set_vf_vlan_stripq(port_id, vf_id, on);
	if (diag < 0) {
		bleat_printf( 0, "set rx vlan strip on vf failed: port_pi=%d, vf_id=%d, on=%d) failed rc=%d", port_id, vf_id, on, diag );
	} else {
		bleat_printf( 3, "set rx vlan strip on vf successful: port=%d, vf_id=%d on/off=%d", port_id, vf_id, on );
	}
}


void
set_vf_allow_bcast(portid_t port_id, uint16_t vf_id, int on)
{
  int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_BROADCAST,(uint8_t) on);

	if (ret < 0) {
		bleat_printf( 0, "set allow bcast failed: port/vf %d/%d on/off=%d rc=%d", port_id, vf_id, on, ret );
	} else {
		bleat_printf( 3, "set allow bcast successful: port/vf %d/%d on/off=%d", port_id, vf_id, on );
	}
}


void
set_vf_allow_mcast(portid_t port_id, uint16_t vf_id, int on)
{
	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_MULTICAST,(uint8_t) on);

	if (ret < 0) {
		bleat_printf( 0, "set allow mcast failed: port/vf %d/%d on/off=%d rc=%d", port_id, vf_id, on, ret );
	} else {
		bleat_printf( 3, "set allow mcast successful: port/vf %d/%d on/off=%d", port_id, vf_id, on );
	}
}


void
set_vf_allow_un_ucast(portid_t port_id, uint16_t vf_id, int on)
{
	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_HASH_UC,(uint8_t) on);

	if (ret < 0) {
		bleat_printf( 0, "set allow ucast failed: port/vf %d/%d on/off=%d rc=%d", port_id, vf_id, on, ret );
	} else {
		bleat_printf( 3, "set allow ucast successful: port/vf %d/%d on/off=%d", port_id, vf_id, on );
	}
}


void
set_vf_allow_untagged(portid_t port_id, uint16_t vf_id, int on)
{
  uint16_t rx_mode = 0;
  rx_mode |= ETH_VMDQ_ACCEPT_UNTAG;

  bleat_printf( 3, "set_vf_allow_untagged(): rx_mode = %d, on = %dn", rx_mode, on);

	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, rx_mode, (uint8_t) on);

	if (ret >= 0) {
		bleat_printf( 3, "set allow untagged failed: port/vf %d/%d on/off=%d rc=%d", port_id, vf_id, on, ret );
	} else {
		bleat_printf( 3, "set allow untagged successful: port/vf %d/%d on/off=%d", port_id, vf_id, on );
	}
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

	if (!is_valid_assigned_ether_addr(&mac_addr)) {
		bleat_printf(0, "Invalid MAC address in config file: port=%d vf=%d, mac=%s\n", (int)port_id, (int)vf, mac);
		return;
	}
#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0)
		diag = rte_pmd_bnxt_mac_addr_add(port_id, &mac_addr, vf);
	else
#endif
		diag = rte_eth_dev_mac_addr_add(port_id, &mac_addr, vf);
	if (diag < 0) {
		bleat_printf( 0, "set rx mac failed: port=%d vf=%d on/off=%d mac=%s rc=%d", (int)port_id, (int)vf, on, mac, diag );
	} else {
		bleat_printf( 3, "set rx mac successful: port=%d vf=%d on/off=%d mac=%s", (int)port_id, (int)vf, on, mac );
	}

}


void
set_vf_rx_vlan(portid_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on)
{
	int diag;

	diag = rte_eth_dev_set_vf_vlan_filter(port_id, vlan_id, vf_mask, on);
	if (diag < 0) {
		bleat_printf( 0, "set rx vlan filter failed: port=%d vlan=%d on/off=%d rc=%d", (int)port_id, (int) vlan_id, on, diag );
	} else {
		bleat_printf( 3, "set rx vlan filter successful: port=%d vlan=%d on/off=%d", (int)port_id, (int) vlan_id, on );
	}

}


void
set_vf_vlan_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on)
{
	int diag;

#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0)
		diag = rte_pmd_bnxt_set_vf_vlan_anti_spoof(port_id, vf, on);
	else
#endif
		diag = rte_pmd_ixgbe_set_vf_vlan_anti_spoof(port_id, vf, on);
	if (diag < 0) {
		bleat_printf( 0, "set vlan antispoof failed: port=%d vf=%d on/off=%d rc=%d", (int)port_id, (int)vf, on, diag );
	} else {
		bleat_printf( 3, "set vlan antispoof successful: port=%d vf=%d on/off=%d", (int)port_id, (int)vf, on );
	}

}


void
set_vf_mac_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on)
{
	int diag;

#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0)
		diag = rte_pmd_bnxt_set_vf_mac_anti_spoof(port_id, vf, on);
	else
#endif
		diag = rte_pmd_ixgbe_set_vf_mac_anti_spoof(port_id, vf, on);
	if (diag < 0) {
		bleat_printf( 0, "set mac antispoof failed: port=%d vf=%d on/off=%d rc=%d", (int)port_id, (int)vf, on, diag );
	} else {
		bleat_printf( 3, "set mac antispoof successful: port=%d vf=%d on/off=%d", (int)port_id, (int)vf, on );
	}

}

void
tx_set_loopback(portid_t port_id, u_int8_t on)
{
	int diag;

#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0)
		diag = rte_pmd_bnxt_set_tx_loopback(port_id, on);
	else
#endif
		diag = rte_pmd_ixgbe_set_tx_loopback(port_id, on);
	if (diag < 0) {
		bleat_printf( 0, "set tx loopback failed: port=%d on/off=%d rc=%d", (int)port_id, on, diag );
	} else {
		bleat_printf( 3, "set tx loopback successful: port=%d on/off=%d", (int)port_id, on );
	}
}

/*
	Returns the value of the split receive control register for the first queue
	of the port/vf pair.
*/
int get_split_ctlreg( portid_t port_id, uint16_t vf_id ) {
#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0) {
		bleat_printf( 0, "NOT reading split ctlreg on port=%d vf=%d", port_id, vf_id );
		return 0;
	}
#endif
	uint32_t reg_off = 0x01014; 	// split receive control regs (pg598)
	int queue;						// the first queue for the vf (TODO: expand this to accept a queue 0-max_qpp)

	queue = get_max_qpp( port_id ) * vf_id;

	if( queue >= 64 ) {
		reg_off = 0x0d014;			// high set of queues
	}

	reg_off += 0x40 * queue;		// step to the right spot for the given queue

	return (int) port_pci_reg_read( port_id, reg_off );
}

/*
	Set/reset the enable drop bit in the split receive control register. State is either 1 (on) or 0 (off).

	This bit must be set on for all queues to prevent head of line blocking in certain
	cases. The setting for a queue _is_ overridden by the drop enable setting (QDE)
	for the queue if it is set.
*/
void set_split_erop( portid_t port_id, uint16_t vf_id, int state ) {
#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0) {
		bleat_printf( 0, "NOT setting split receive drop on port=%d vf=%d to on/off=%d", port_id, vf_id, state );
		return;
	}
#endif
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

	bleat_printf( 4, "setting split receive drop for %d queues on port=%d vf=%d on/off=%d", qpvf, port_id, vf_id, state );

	for( ; qpvf > 0; qpvf-- ) {
		port_pci_reg_write( port_id, reg_off, reg_value );
		reg_off += 0x40;
	}
}

/*
	Set/reset the drop bit for all queues on the given VF.
*/
static void set_rx_drop(portid_t port_id, uint16_t vf_id, int state )
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
	bleat_printf( 0, "setting rx drop enable to %d for pf/vf %d/%d on/off=%d", state, port_id, vf_id, !!state );

	reg_off = 0x02f04;

	for( i = vf_id * q_num; i < (vf_id * q_num) + q_num; i++ ) {						// one bit per queue writes the right most three bits into the proper place
		reg_value = IXGBE_QDE_WRITE | (i << IXGBE_QDE_IDX_SHIFT) | (!!state);
		port_pci_reg_write( port_id, reg_off, reg_value );
	}
}

/*
	Set/reset the drop bit for PF queues on the given port.
	This will set the drop enable bit for all of the PF queues. It should be called only
	during initialisation, after the port has been initialised.
*/
extern void set_pfrx_drop(portid_t port_id, int state )
{
#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0) {
		//Dummy reading to avoid compilation error.
		get_num_vfs( port_id );	// PF queue starts just past last possible vf
		bleat_printf( 0, "NOT setting pfrx_drop on port=%d state=%d", port_id, !!state );
		return;
	}
#endif
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
		bleat_printf( 0, "internal mishap in set_pfrx_drop: qstart (%d) is out of range for nqueues=%d", qstart, q_num );
		return;
	}

	bleat_printf( 0, "setting pf drop enable for port %d qstart=%d on/off=%d", port_id, qstart, !!state );
	reg_off = 0x02f04;

	for( i = qstart; i < qstart + q_num; i++ ) {						// one bit per queue writes the right most three bits into the proper place
		reg_value = IXGBE_QDE_WRITE | (i << IXGBE_QDE_IDX_SHIFT) | (!!state);
		port_pci_reg_write( port_id, reg_off, reg_value );
	}
}


/*
	Set/reset the queue drop enable bit for all pools. State is either 1 (on) or 0 (off).

	CAUTION:
	This should proabaly not be used as setting the drop enable bit has the side effect of
	causing packet loss.  To avoid this, we slectively set the drop enable bit when we
	get a reset on a VF, and then clear the bit when the VF's queues go ready.

	This funciton probably should be deprecated!
*/
void set_queue_drop( portid_t port_id, int state ) {
	int		result = 0;
	
	bleat_printf( 0, "WARN: something is calling set_queue drop which may not be expected\n" );
#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0) {
		bleat_printf( 0, "NOT setting queue drop for port %d on all queues to: on/off=%d", port_id, !!state );
		return;
	}
#endif
	bleat_printf( 2, "setting queue drop for port %d on all queues to: on/off=%d", port_id, !!state );
#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0)
		result = rte_pmd_bnxt_set_all_queues_drop_en( port_id, !!state );			// (re)set flag for all queues on the port
	else
#endif
		result = rte_pmd_ixgbe_set_all_queues_drop_en( port_id, !!state );			// (re)set flag for all queues on the port
	if( result != 0 ) {
		bleat_printf( 0, "fail: unable to set drop enable for port %d on/off=%d: errno=%d", port_id, !state, -result );
	}

	/*
	 disable default pool to avoid DMAR errors when we get packets not destined to any VF
	*/
	disable_default_pool(port_id);
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
	Check to see if the NIC tx/rx queues are enabled for the pf/vf pair.
	Returns 1 if the queues are enabled.
*/

int
is_rx_queue_on(portid_t port_id, uint16_t vf_id, int* mcounter )
{
	uint32_t reg_off = 0;				// control register address
	uint32_t ctrl = 0;					// value read from nic register
	struct rte_eth_dev *pf_dev;
	struct rte_eth_dev_info dev_info;

	rte_eth_dev_info_get( port_id, &dev_info );
 	pf_dev = &rte_eth_devices[port_id];
#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0) {
		int queues = rte_pmd_bnxt_get_vf_rx_status(port_id, vf_id);
		if (queues > 0) {
			bleat_printf( 3, "%d queues active: port=%d vfid_id=%d)", queues, port_id, vf_id);
			return 1;
		}
	}
	else
#endif
	{
		int queue;						// queue to set (0-max-queues)
		uint32_t queues_per_pool = 8;	// maximum number of queues that could be assigned to a pool (based on total VFs configured)
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
		}
	}

	if( mcounter != NULL ) {
		if( (*mcounter % 100 ) == 0 ) {
  			bleat_printf( 4, "is_queue_en: still pending: first queue not active: bar=0x%08X, port=%d vfid_id=%d, ctrl=0x%08x q/pool=%d", reg_off, port_id, vf_id, ctrl, (int) RTE_ETH_DEV_SRIOV(pf_dev).nb_q_per_pool);
		}
		(*mcounter)++;
	}
	return 0;
}

/*
	Drop packets which are not directed to any of VF's
	instead of sending them to default pool. This helps
	prevent DMAR errors in the system log.
*/
void
disable_default_pool(portid_t port_id)
{
#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0) {
		bleat_printf( 0, "NOT disabling default pool on port=%d\n", port_id);
		return;
	}
#endif
	uint32_t ctrl = port_pci_reg_read(port_id, IXGBE_VT_CTL);
	ctrl |= IXGBE_VT_CTL_DIS_DEFPL;
	bleat_printf( 3, "disabling default pool bar=0x%08X, port=%d ctrl=0x%08x ", IXGBE_VT_CTL, port_id, ctrl);
	port_pci_reg_write( port_id, IXGBE_VT_CTL, ctrl);
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
	for( refresh_item = rq_list; refresh_item != NULL; refresh_item = refresh_item->next ) {
		if (refresh_item->port_id == port_id && refresh_item->vf_id == vf_id){
			if (!refresh_item->enabled) {
				refresh_item->enabled = is_rx_queue_on(port_id, vf_id, &refresh_item->mcounter );
			}

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
	refresh_item->prev = NULL;
	bleat_printf( 2, "adding refresh to queue for %d/%d", port_id, vf_id );

	rte_spinlock_lock(&rte_refresh_q_lock);
	
	set_rx_drop( refresh_item->port_id, refresh_item->vf_id, SET_ON );		// set the drop enable flag (emulate kernel driver)

	refresh_item->next = rq_list;						// push on the head of the list, order is unimportant
	rq_list = refresh_item;
	if( refresh_item->next ) {
		refresh_item->next->prev = refresh_item;
	}
	rte_spinlock_unlock(&rte_refresh_q_lock);
}

/*
	If a queued block for port/vf exists, mark it enabled. This is a hack.
	There are observed cases where the VF tx/rx queues never show ready. This
	function will force the pending reset to be dispatchable regardless of
	what the state of the NIC is.  This funciton is called when we receive a
	mailbox message which we interpret as meaning that the device is up and
	in a 'ready' state.
DEPRECATED
static void enable_refresh_queue(u_int8_t port_id, uint16_t vf_id)
{
	struct rq_entry *refresh_item;

	bleat_printf( 3, "enable is looking for: %d %d", port_id, vf_id );
	rte_spinlock_lock(&rte_refresh_q_lock);
	XXTAILQ_FOREACH(refresh_item, &rq_head, rq_entries) {
		if (refresh_item->port_id == port_id && refresh_item->vf_id == vf_id){
			bleat_printf( 2, "enabling %d/%d", port_id, vf_id );
			refresh_item->enabled = 1;
		}
	}
	rte_spinlock_unlock(&rte_refresh_q_lock);
	return;
}
*/


/*
	This is executed in it's own thread and is responsible for checking the
	queue of pending resets. When a pending reset becomes 'enabled' then
	the following happen:
		- restore_vf_settings() executed for the VF
		- drop enable bit is CLEARED for all of the VF's queues.
		- the block is removed from the queue
*/
void
process_refresh_queue(void)
{
	struct rq_entry* next_item;		// pointer makes delete and free safe in loop

	while(1) {

		usleep(200000);
		struct rq_entry *refresh_item;

		rte_spinlock_lock(&rte_refresh_q_lock);
		for( refresh_item = rq_list; refresh_item != NULL; refresh_item = next_item ) {
			next_item = refresh_item->next;			// if we delete we need this to go forward

			//printf("checking the queue:  PORT: %d, VF: %d, Enabled: %d\n", refresh_item->port_id, refresh_item->vf_id, refresh_item->enabled);
			/* check if item's q is enabled, update VF and remove item from queue */
			if(refresh_item->enabled){
				bleat_printf( 2, "refresh item enabled: updating VF: %d", refresh_item->vf_id);

				restore_vf_setings(refresh_item->port_id, refresh_item->vf_id);		// refresh all of our configuration back onto the NIC

				bleat_printf( 3, "refresh_queue: clearing enable queue drop for %d/%d", refresh_item->port_id, refresh_item->vf_id );
				set_rx_drop( refresh_item->port_id, refresh_item->vf_id, SET_OFF );

				if( refresh_item->prev ) {
					refresh_item->prev->next = refresh_item->next;
				} else {
					rq_list = refresh_item->next;						// when the head
				}
				if( refresh_item->next ) {
					refresh_item->next->prev = refresh_item->prev;
				}
				memset( refresh_item, 0, sizeof( *refresh_item ) );
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

#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0) {
		if (rte_pmd_bnxt_get_tx_drop_count(port_id, &spoffed[port_id]))
			spoffed[port_id] = UINT64_MAX;
	}
	else
#endif
		spoffed[port_id] += port_pci_reg_read(port_id, 0x08780);


	char status[5];
	if(!link.link_status)
		stpcpy(status, "DOWN");
	else
		stpcpy(status, "UP  ");

		//" %6s %6"PRIu16" %6"PRIu16" %15"PRIu64" %15"PRIu64" %15"PRIu64" %15"PRIu64" %15"PRIu64" %15"PRIu64" %15"PRIu64" %15"PRIu32" %lld\n",
	return snprintf( buff, bsize, " %6s  %6d %6d %15lld %15lld %15lld %15lld %15lld %15lld %15d %15"PRIu64"\n",
		status,
		(int) link.link_speed,
		(int) link.link_duplex,
		(long long) stats.ipackets,
		(long long) stats.ibytes,
		(long long) stats.ierrors,
		(long long) stats.imissed,
		(long long) stats.opackets,
		(long long) stats.obytes,
		(int) stats.oerrors,
		spoffed[port_id]
	);
}

/*
*	prints VF statistics
	Returns number of characters placd into buff, or -1 if error (vf not in use
	or out of range).  The parm ivf is the virtual function number which is maintained
	as integer in our datstructs allowing -1 to indicate an uninstalled/delted VF.
	It is converted to uint32 for calculations here.
*/
int
vf_stats_display(uint8_t port_id, uint32_t pf_ari, int ivf, char * buff, int bsize)
{
	uint32_t vf;
	uint32_t	rx_pkts = 0;
	uint32_t	tx_pkts = 0;
	uint64_t	rx_octets = 0;
	uint64_t	tx_octets = 0;
#ifdef BNXT_SUPPORT
	bool is_bnxt = (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0);
#endif

	if( ivf < 0 || ivf > 31 ) {
		return -1;
	}

	vf = (uint32_t) ivf;						// unsinged for rest

	uint32_t new_ari;
	struct rte_pci_addr vf_pci_addr;


	new_ari = pf_ari + vf_offfset[port_id] + (vf * vf_stride[port_id]);

	vf_pci_addr.domain = 0;
	vf_pci_addr.bus = (new_ari >> 8) & 0xff;
	vf_pci_addr.devid = (new_ari >> 3) & 0x1f;
	vf_pci_addr.function = new_ari & 0x7;

#ifdef BNXT_SUPPORT
	uint64_t	tx_errors = 0;
	uint64_t	vf_spoffed = 0;
	if (is_bnxt) {
		struct rte_eth_stats stats;

		if (rte_pmd_bnxt_get_vf_tx_drop_count(port_id, vf, &vf_spoffed))
			vf_spoffed = UINT64_MAX;
		if (!rte_pmd_bnxt_get_vf_stats(port_id, vf, &stats)) {
			rx_pkts = stats.ipackets;
			tx_pkts = stats.opackets;
			tx_octets = stats.obytes;
			rx_octets = stats.ibytes;
			tx_errors = stats.oerrors;
			//if(rx_pkts_g[vf] != rx_pkts) {
				//rx_on_g[vf] = 1;
				//rx_pkts_g[vf] = rx_pkts;
			//}
		}
	}
	else
#endif
	{
		rx_pkts = port_pci_reg_read(port_id, IXGBE_PVFGPRC(vf));
		uint64_t	rx_ol = port_pci_reg_read(port_id, IXGBE_PVFGORC_LSB(vf));
		uint64_t	rx_oh = port_pci_reg_read(port_id, IXGBE_PVFGORC_MSB(vf));
		rx_octets = (rx_oh << 32) |	rx_ol;		// 36 bit only counter

		tx_pkts = port_pci_reg_read(port_id, IXGBE_PVFGPTC(vf));
		uint64_t	tx_ol = port_pci_reg_read(port_id, IXGBE_PVFGOTC_LSB(vf));
		uint64_t	tx_oh = port_pci_reg_read(port_id, IXGBE_PVFGOTC_MSB(vf));
		tx_octets = (tx_oh << 32) |	tx_ol;		// 36 bit only counter
	}

	char status[5];
#ifdef BNXT_SUPPORT
	if (is_bnxt) {
		if (rte_pmd_bnxt_get_vf_rx_status(port_id, vf) <= 0)
			stpcpy(status, "DOWN");
		else
	    		stpcpy(status, "UP  ");
	}
	else
#endif
	{
		int mcounter = 0;
		if(!is_rx_queue_on(port_id, vf, &mcounter ))
			stpcpy(status, "DOWN");
		else
			stpcpy(status, "UP  ");
	}

#ifdef BNXT_SUPPORT
	return 	snprintf(buff, bsize, "%2s %6d    %04X:%02X:%02X.%01X %6s %30"PRIu32" %15"PRIu64" %47"PRIu32" %15"PRIu64" %15"PRIu64" %15"PRIu64"\n",
				"vf", vf, vf_pci_addr.domain, vf_pci_addr.bus, vf_pci_addr.devid, vf_pci_addr.function, status,
				rx_pkts, rx_octets, tx_pkts, tx_octets, tx_errors, vf_spoffed);
#else
	return 	snprintf(buff, bsize, "%2s %6d    %04X:%02X:%02X.%01X %6s %30"PRIu32" %15"PRIu64" %47"PRIu32" %15"PRIu64"\n",
				"vf", vf, vf_pci_addr.domain, vf_pci_addr.bus, vf_pci_addr.devid, vf_pci_addr.function, status,
				rx_pkts, rx_octets, tx_pkts, tx_octets);
#endif
}


/*
	prints extended PF statistics
	rx_size_64_packets: 0
	rx_size_65_to_127_packets: 0
	rx_size_128_to_255_packets: 0
	rx_size_256_to_511_packets: 0
	rx_size_512_to_1023_packets: 0
	rx_size_1024_to_max_packets: 0
	tx_size_64_packets: 0
	tx_size_65_to_127_packets: 0
	tx_size_128_to_255_packets: 0
	tx_size_256_to_511_packets: 0
	tx_size_512_to_1023_packets: 0
	tx_size_1024_to_max_packets: 0

	eturns number of characters placed into buff.
*/
int
port_xstats_display(uint8_t port_id, char * buff, int bsize)
{
	struct rte_eth_xstat *xstats;
	int cnt_xstats, idx_xstat;
	struct rte_eth_xstat_name *xstats_names;

	cnt_xstats = rte_eth_xstats_get_names(port_id, NULL, 0);
	if (cnt_xstats  < 0) {
		bleat_printf( 0, "fail: unable to get count of xstats for port: %d", port_id);
		return 0;
	}

	xstats_names = malloc(sizeof(struct rte_eth_xstat_name) * cnt_xstats);
	if (xstats_names == NULL) {
		bleat_printf( 0, "fail: unable to allocate memory for xstat names for port: %d", port_id);
		return 0;
	}
	
	if (cnt_xstats != rte_eth_xstats_get_names(port_id, xstats_names, cnt_xstats)) {
		bleat_printf( 0, "fail: unable to get xstat names for port: %d", port_id);
		free(xstats_names);
		return 0;
	}

	xstats = malloc(sizeof(struct rte_eth_xstat) * cnt_xstats);
	if (xstats == NULL) {
		bleat_printf( 0, "fail: unable to allocate memory for xstat for port: %d", port_id);
		free(xstats_names);
		return 0;
	}
	
	if (cnt_xstats != rte_eth_xstats_get(port_id, xstats, cnt_xstats)) {
		bleat_printf( 0, "fail: unable to get xstat for port: %d", port_id);
		free(xstats_names);
		free(xstats);
		return 0;
	}

  int lw = 0;
	for (idx_xstat = 0; idx_xstat < cnt_xstats; idx_xstat++)
		if (strncmp(xstats_names[idx_xstat].name, "rx_size_", 8) == 0 || strncmp(xstats_names[idx_xstat].name, "tx_size_", 8) == 0)
			lw += snprintf(buff + lw, bsize - lw, "%s: %"PRIu64"\n", xstats_names[idx_xstat].name, xstats[idx_xstat].value);		

	free(xstats_names);
	free(xstats);
	
	return lw;
}


/*
  dumps all LAN ID's configured
  to be used for debugging
  or to check if number of vlans doesn't exceed MAX (64)
*/
int
dump_vlvf_entry(portid_t port_id)
{
#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port_id].driver->pci_drv.driver.name, "net_bnxt") == 0) {
		return 0;
	}
#endif
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

	This is the basic, non-dcb, port initialisation.

	If hw_strip_crc is false, the default will be overridden.
*/
int
port_init(uint8_t port, __attribute__((__unused__)) struct rte_mempool *mbuf_pool, int hw_strip_crc, sriov_port_t *pf )
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1;
	const uint16_t tx_rings = 1;
	int retval;
	uint16_t q;

	port_conf.rxmode.max_rx_pkt_len = pf->mtu;
	port_conf.rxmode.jumbo_frame = pf->mtu >= 1500;

	if (port >= rte_eth_dev_count()) {
		bleat_printf( 0, "CRI: abort: port >= rte_eth_dev_count");
		return 1;
	}

	bleat_printf( 2, "port %d max_mtu=%d jumbo=%d", (int) port, (int) port_conf.rxmode.max_rx_pkt_len, (int) port_conf.rxmode.jumbo_frame );

	if( !hw_strip_crc ) {
		bleat_printf( 2, "hardware crc stripping is now disabled for port %d", port );
		port_conf.rxmode.hw_strip_crc = 0;
	}

	// Configure the Ethernet device.
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0) {
		bleat_printf( 0, "CRI: abort: cannot configure port %u, retval %d", port, retval);
		return 1;
	}

	rte_eth_dev_callback_register(port, RTE_ETH_EVENT_INTR_LSC, lsi_event_callback, NULL);
#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[port].driver->pci_drv.driver.name, "net_bnxt") == 0)
		rte_eth_dev_callback_register(port, RTE_ETH_EVENT_VF_MBOX, bnxt_vf_msb_event_callback, NULL);
	else
#endif
		rte_eth_dev_callback_register(port, RTE_ETH_EVENT_VF_MBOX, ixgbe_vf_msb_event_callback, NULL);

	// Allocate and set up 1 RX queue per Ethernet port.
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0) {
			bleat_printf( 0, "CRI: abort: cannot setup rx queue, port %u", port);
			return 1;
		}
	}

	// Allocate and set up 1 TX queue per Ethernet port.
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE, rte_eth_dev_socket_id(port), NULL);
		if (retval < 0) {
			bleat_printf( 0, "CRI: abort: cannot setup tx queue, port %u", port);
			return 1;
		}
	}


	// Start the Ethernet port.
	retval = rte_eth_dev_start(port);
	if (retval < 0) {
		bleat_printf( 0, "CRI: abort: cannot start port %u", port);
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

/*
	EXPERIMANTAL --
	Set the flow control config when not in qos.

	Force should be set when calling during initialisation and not running in qos mode.
	It causes us to track that we are allowed to reset the flag if called without
	force on renegotiate callbacks.
*/
extern void set_fcc( portid_t pf, int force ) {
	static int allowed = 0;			// allows to safely call for reset

	uint32_t val;
	uint32_t cval = 0;				// current value read from nic
	uint32_t offset;
	uint32_t mask;

#ifdef BNXT_SUPPORT
	if (strcmp(rte_eth_devices[pf].driver->pci_drv.driver.name, "net_bnxt") == 0) {
		return;
	}
#endif
	if( force ) {
		allowed = 1;
	} else {
		if( ! allowed ) {
			return;
		}
	}

	offset = 0x03d00;		// FCCFG.TFCE=10b
	mask = 0xffffffe7;
	cval = port_pci_reg_read( pf, offset );
	val = 1 << 3;												// 01b (bits 3,4)  flow control on when not in dcb (match ixgbe driver)
	port_pci_reg_write( pf, offset, (cval & mask) | val );		// flip on our bits, and set
	bleat_printf( 1, "tfce %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );

	// priority flow control enable should be set only when in dcb mode
	offset = 0x04294;    	// MFLCN.RPFCE=1b RFCE=0b
	val = 0x0a;				// match the ixgbe driver setting
	mask =0xfffffff0;
	cval = port_pci_reg_read( pf, offset );
	port_pci_reg_write( pf, offset, (cval & mask) | val );		// flip on our bits, and set
	bleat_printf( 1, "mflcn %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );
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
  //AZrte_eth_dev_ping_vfs(port_id, -1);
}



/*
	Called when a 'mailbox' message is received.  Examine and take action based on
	the message type.
*/
#ifdef BNXT_SUPPORT
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
	if (!vf_cfg->allow_bcast) {
		mi->mask &= ~(HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS);
	}
	if (!vf_cfg->allow_mcast) {
		mi->mask &= ~(HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_BCAST |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_ALL_MCAST |
		    HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS);
		mi->mask = HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_MCAST;
		mi->num_mc_entries = 0;
	}
	if (!vf_cfg->allow_un_ucast)
		mi->mask &= ~HWRM_CFA_L2_SET_RX_MASK_INPUT_MASK_PROMISCUOUS;
}

void
bnxt_vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param)
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
		case HWRM_STAT_CTX_CLR_STATS:
		case HWRM_FUNC_CLR_STATS:
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
#endif


/*
	Called when a 'mailbox' message is received.  Examine and take action based on
	the message type.
*/
void
ixgbe_vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param) {

	struct rte_pmd_ixgbe_mb_event_param *p = (struct rte_pmd_ixgbe_mb_event_param*) param;
  uint16_t vf = p->vfid;
	uint16_t mbox_type = p->msg_type;
	uint32_t *msgbuf = (uint32_t *) p->msg;

	struct ether_addr *new_mac;

	/* check & process VF to PF mailbox message */
	switch (mbox_type) {
		case IXGBE_VF_RESET:
			bleat_printf( 1, "reset event received: port=%d", port_id );

			p->retval = RTE_PMD_IXGBE_MB_EVENT_NOOP_ACK;				/* noop & ack */
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, p->retval, "IXGBE_VF_RESET");

			add_refresh_queue(port_id, vf);
			break;

		case IXGBE_VF_SET_MAC_ADDR:
			bleat_printf( 1, "setmac event received: port=%d", port_id );
			p->retval = RTE_PMD_IXGBE_MB_EVENT_PROCEED;    						// do what's needed
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, p->retval, "IXGBE_VF_SET_MAC_ADDR");

			new_mac = (struct ether_addr *) (&msgbuf[1]);


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
			bleat_printf( 1, "set multicast event received: port=%d", port_id );
			p->retval = RTE_PMD_IXGBE_MB_EVENT_PROCEED;    /* do what's needed */
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ",
				type, port_id, vf, p->retval, "IXGBE_VF_SET_MULTICAST");

			new_mac = (struct ether_addr *) (&msgbuf[1]);

			if (is_valid_assigned_ether_addr(new_mac)) {
				bleat_printf( 3, "setting mac, vf %u, MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
					" %02" PRIx8 " %02" PRIx8 " %02" PRIx8,
					(uint32_t)vf,
					new_mac->addr_bytes[0], new_mac->addr_bytes[1],
					new_mac->addr_bytes[2], new_mac->addr_bytes[3],
					new_mac->addr_bytes[4], new_mac->addr_bytes[5]);
			}

			add_refresh_queue( port_id, vf );		// schedule a complete refresh when the queue goes hot

			break;

		case IXGBE_VF_SET_VLAN:
			// NOTE: we _always_ approve this.  This is the VMs setting of what will be an 'inner' vlan ID and thus we don't care
			if( valid_vlan( port_id, vf, (int) msgbuf[1] )) {
				bleat_printf( 1, "vlan set event approved: port=%d vf=%d vlan=%d (responding noop-ack)", port_id, vf, (int) msgbuf[1] );
				//*((int*) param) = RTE_ETH_MB_EVENT_PROCEED;
				p->retval = RTE_PMD_IXGBE_MB_EVENT_NOOP_ACK;     // good rc to VM while not changing anything
			} else {
				bleat_printf( 1, "vlan set event rejected; vlan not not configured: port=%d vf=%d vlan=%d (responding noop-ack)", port_id, vf, (int) msgbuf[1] );
				p->retval = RTE_PMD_IXGBE_MB_EVENT_NOOP_NACK;     // VM should see failure
			}

			add_refresh_queue( port_id, vf );		// schedule a complete refresh when the queue goes hot

			//bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ", type, port_id, vf, *(uint32_t*) param, "IXGBE_VF_SET_VLAN");
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
			set_fcc( port_id, 0 );									// reset flow-control if allowed
			tx_set_loopback( port_id, suss_loopback( port_id ) );		// enable loopback if set (could be reset if link was down)
			add_refresh_queue( port_id, vf );		// schedule a complete refresh when the queue goes hot
			break;

		case IXGBE_VF_SET_MACVLAN:
			bleat_printf( 1, "set macvlan event received: port=%d (responding nop+nak)", port_id );
			p->retval =  RTE_PMD_IXGBE_MB_EVENT_NOOP_NACK;    /* noop & nack */
			bleat_printf( 3, "type: %d, port: %d, vf: %d, out: %d, _T: %s ", type, port_id, vf, p->retval, "IXGBE_VF_SET_MACVLAN");
			bleat_printf( 3, "setting mac_vlan = %d", msgbuf[1] );

			add_refresh_queue( port_id, vf );		// schedule a complete refresh when the queue goes hot
			break;

		case IXGBE_VF_API_NEGOTIATE:
			bleat_printf( 1, "set negotiate event received: port=%d (responding proceed)", port_id );
			p->retval =  RTE_PMD_IXGBE_MB_EVENT_PROCEED;   /* do what's needed */
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ", type, port_id, vf, p->retval, "IXGBE_VF_API_NEGOTIATE");
			
			set_fcc( port_id, 0 );									// reset flow-control if allowed
			restore_vf_setings(port_id, vf);							// these must happen now, do NOT queue it. if not immediate guest-guest may hang
			tx_set_loopback( port_id, suss_loopback( port_id ) );		// enable loopback if set (could be reset if link goes down)
			break;

		case IXGBE_VF_GET_QUEUES:
			bleat_printf( 1, "get queues  event received: port=%d (responding proceed)", port_id );
			p->retval =  RTE_PMD_IXGBE_MB_EVENT_PROCEED;   /* do what's needed */
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, _T: %s ", type, port_id, vf, p->retval, "IXGBE_VF_GET_QUEUES");

			add_refresh_queue( port_id, vf );		// schedule a complete refresh when the queue goes hot
			break;

		default:
			bleat_printf( 1, "unknown  event request received: port=%d (responding nop+nak)", port_id );
			p->retval = RTE_PMD_IXGBE_MB_EVENT_NOOP_NACK;     /* noop & nack */
			bleat_printf( 3, "Type: %d, Port: %d, VF: %d, OUT: %d, MBOX_TYPE: %d", type, port_id, vf, p->retval, mbox_type);

			restore_vf_setings(port_id, vf);		// refresh all of our configuration back onto the NIC
			break;
	}

	bleat_printf( 3, "callback type: %d, Port: %d, VF: %d, OUT: %d, _T: %d", type, port_id, vf, p->retval, mbox_type);
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

	signal(SIGCHLD, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	if((childpid = fork()) < 0) {
		bleat_printf( 0, "err: daemonise cannot fork process (errno = %d)", errno);
	} else {
		bleat_printf( 1, "daemonise: after fork() in %s (%d)", childpid ? "parent" : "child", childpid);
		if(!childpid) {
			// child
			bleat_printf( 1, "daemonise: child has started" );
			detachFromTerminal();
			if( pid_fname != NULL ) {
				save_pid( pid_fname );
			}
		} else {
			bleat_printf( 1, "daemonise: child process running, parent process exiting: child pid=%d", childpid );
			exit(EXIT_SUCCESS);
		}
	}
}

/*
	Dump a bunch of port stats to the log
*/
static void dump_port_stats( int port  ) {
	struct rte_eth_stats stats;
	int i;

	memset( &stats, -1, sizeof( stats ) );
	rte_eth_stats_get( port, &stats );

	bleat_printf( 0, "port=%d success rx pkts:    %lld", port, (long long) stats.ipackets );
	bleat_printf( 0, "port=%d success tx pkts:    %lld", port, (long long) stats.opackets );
	bleat_printf( 0, "port=%d rx missed:    %lld", port, (long long) stats.imissed );
	bleat_printf( 0, "port=%d rx errors:    %lld", port, (long long) stats.ierrors );
	bleat_printf( 0, "port=%d tx errors:    %lld", port, (long long) stats.oerrors );
	bleat_printf( 0, "port=%d rx no mbufs:  %lld", port, (long long) stats.rx_nombuf );

	for( i = 0; i < RTE_ETHDEV_QUEUE_STAT_CNTRS; i++ ) {
		bleat_printf( 0, "port=%d q_ipackets[%02d]:  %lld", port, i, stats.q_ipackets[i] );
		bleat_printf( 0, "port=%d q_opackets[%02d]:  %lld", port, i, stats.q_opackets[i] );
		bleat_printf( 0, "port=%d q_errors[%02d]:  %lld", port, i, stats.q_errors[i] );
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

		dump_port_stats( i  );
	}

}
