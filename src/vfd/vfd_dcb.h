
		// .mq_mode        = ETH_MQ_RX_VMDQ_DCB,

#ifndef VFD_DCB_H
#define VFD_DCB_H

#include "sriov.h"

/* 
	Default dcb settings.
*/
static const struct rte_eth_conf eth_dcb_default = {
	.rxmode = {
		.mq_mode        = ETH_MQ_RX_VMDQ_DCB,
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
			.nb_queue_pools = ETH_32_POOLS,
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

extern int vfd_dcb_config( uint8_t port );
extern int dcb_port_init( uint8_t port, __attribute__((__unused__)) struct rte_mempool *mbuf_pool );


#endif
