
#ifndef _VFD_IXGBE_H
#define _VFD_IXGBE_H


#include <rte_pmd_ixgbe.h>
#include <drivers/net/ixgbe/base/ixgbe_mbx.h>


// ------------- prototypes ----------------------------------------------

int vfd_ixgbe_ping_vfs( portid_t port, int16_t vf);
int vfd_ixgbe_set_vf_mac_anti_spoof( portid_t port, uint16_t vf_id, uint8_t on);
int vfd_ixgbe_set_vf_vlan_anti_spoof( portid_t port, uint16_t vf_id, uint8_t on);
int vfd_ixgbe_set_tx_loopback( portid_t port, uint8_t on);
int vfd_ixgbe_set_vf_unicast_promisc( portid_t port, uint16_t vf_id, uint8_t on);
int vfd_ixgbe_set_vf_multicast_promisc( portid_t port, uint16_t vf_id, uint8_t on);
int vfd_ixgbe_set_vf_mac_addr( portid_t port, uint16_t vf_id, struct ether_addr *mac_addr);
int vfd_ixgbe_set_vf_default_mac_addr(  portid_t port_id, uint16_t vf, struct ether_addr *mac_addr );
int vfd_ixgbe_set_vf_vlan_stripq( portid_t port, uint16_t vf, uint8_t on);
int vfd_ixgbe_set_vf_vlan_insert( portid_t port, uint16_t vf_id, uint16_t vlan_id);
int vfd_ixgbe_set_vf_broadcast( portid_t port, uint16_t vf_id, uint8_t on);
int vfd_ixgbe_allow_untagged( portid_t port, uint16_t vf_id, uint8_t on);
int vfd_ixgbe_set_vf_vlan_filter( portid_t port, uint16_t vlan_id, uint64_t vf_mask, uint8_t on);
int vfd_ixgbe_get_vf_stats( portid_t port, uint16_t vf_id, struct rte_eth_stats *stats);
int vfd_ixgbe_reset_vf_stats( portid_t port, uint16_t vf_id);
int vfd_ixgbe_set_vf_rate_limit( portid_t port_id, uint16_t vf_id, uint16_t tx_rate, uint64_t q_msk);
int vfd_ixgbe_set_all_queues_drop_en( portid_t port, uint8_t on);

// 18.0x int vfd_ixgbe_vf_msb_event_callback(uint16_t port_id, enum rte_eth_event_type type, void *data, void* param );
int vfd_ixgbe_vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *data, void* param );


uint32_t vfd_ixgbe_get_pf_spoof_stats( portid_t port_id);
uint32_t vfd_ixgbe_get_vf_spoof_stats( portid_t port_id, uint16_t vf_id);

void vfd_ixgbe_disable_default_pool( portid_t port_id);
int vfd_ixgbe_is_rx_queue_on( portid_t port_id, uint16_t vf_id, int* mcounter);


void vfd_ixgbe_set_pfrx_drop( portid_t port_id, int state );
void vfd_ixgbe_set_rx_drop( portid_t port_id, uint16_t vf_id, int state);
void vfd_ixgbe_set_split_erop( portid_t port_id, uint16_t vf_id, int state);
int vfd_ixgbe_get_split_ctlreg( portid_t port_id, uint16_t vf_id);
int vfd_ixgbe_dump_all_vlans( portid_t port_id);

#endif

