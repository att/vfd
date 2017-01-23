
		// .mq_mode        = ETH_MQ_RX_VMDQ_DCB,

#ifndef VFD_DCB_H
#define VFD_DCB_H

#include "sriov.h"

/* 
	Default dcb settings.  While pools here are set to 32, the actual number of queues per pool
	is 'forced' by the DPDK functions and is strictly based on the number of VFs which exist
	for a PF.  In fact, if the Number of VFs is >= 32, then the queues per pool value is set to 2,
	and if the number of VFs is < 16, then queues per pool are forced to 8 (like it or not). We may
	have to jump through some hoops should we ever wish to allow fewer than 31 VFs to be configured.
*/
static const struct rte_eth_conf eth_dcb_default = {
	.rxmode = {
		.mq_mode        = ETH_MQ_RX_VMDQ_DCB,	// both sr-iov and dcb support
		.split_hdr_size = 0,
		.header_split   = 0, 
		.hw_ip_checksum = 0, 
		.hw_vlan_filter = 0, 
		.jumbo_frame    = 0, 
	},
	.txmode = {
		.mq_mode =  ETH_MQ_TX_VMDQ_DCB,			// vmdq causes vt mode to be set which is what we want
	},
	.rx_adv_conf = {
		.vmdq_dcb_conf = {
			.nb_queue_pools = ETH_32_POOLS,		// it seems that this will be overridden based on number of vfs which exist
			.enable_default_pool = 0,
			.default_pool = 0,
			.nb_pool_maps = 0,					// up to 64
			.pool_map = {{0, 0},},				// {vlanID,pools}  pools is a bit mask(64) of which pool(s) the vlan id maps to
			.dcb_tc = {0},						// up to num-user-priorities; 'selects a queue in a pool' what ever thef that means
		},
		.dcb_rx_conf = {
			.nb_tcs = ETH_4_TCS,
			.dcb_tc = {0},
		},
		.vmdq_rx_conf = {
			.nb_queue_pools = ETH_32_POOLS,
			.enable_default_pool = 0,
			.default_pool = 0,
			.nb_pool_maps = 0,
			.pool_map = {{0, 0},},
		},
	},
	.tx_adv_conf = {
		.vmdq_dcb_tx_conf = {
			.nb_queue_pools = ETH_32_POOLS,
		 	.dcb_tc = {0},
		},
	},
};

// ------------- prototypes ----------------------------------------------

extern int vfd_dcb_config( sriov_port_t *pf );
int dcb_port_init( sriov_port_t *pf, __attribute__((__unused__)) struct rte_mempool *mbuf_pool);


#endif
