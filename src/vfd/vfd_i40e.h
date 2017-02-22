
#ifndef VFD_I40E_H
#define VFD_I40E_H

#include <rte_pmd_i40e.h>

#include "sriov.h"


// ------------- prototypes ----------------------------------------------

int vfd_i40e_ping_vfs(uint8_t port, int16_t vf);
int vfd_i40e_set_vf_mac_anti_spoof(uint8_t port, uint16_t vf_id, uint8_t on); 
int vfd_i40e_set_vf_vlan_anti_spoof(uint8_t port, uint16_t vf_id, uint8_t on);
int vfd_i40e_set_tx_loopback(uint8_t port, uint8_t on);
int vfd_i40e_set_vf_unicast_promisc(uint8_t port, uint16_t vf_id, uint8_t on);
int vfd_i40e_set_vf_multicast_promisc(uint8_t port, uint16_t vf_id, uint8_t on);
int vfd_i40e_set_vf_mac_addr(uint8_t port, uint16_t vf_id, struct ether_addr *mac_addr);
int vfd_i40e_set_vf_vlan_stripq(uint8_t port, uint16_t vf, uint8_t on);
int vfd_i40e_set_vf_vlan_insert(uint8_t port, uint16_t vf_id, uint16_t vlan_id);
int vfd_i40e_set_vf_broadcast(uint8_t port, uint16_t vf_id, uint8_t on);
int vfd_i40e_set_vf_vlan_tag(uint8_t port, uint16_t vf_id, uint8_t on);
int vfd_i40e_set_vf_vlan_filter(uint8_t port, uint16_t vlan_id, uint64_t vf_mask, uint8_t on);
int vfd_i40e_get_vf_stats(uint8_t port, uint16_t vf_id, struct rte_eth_stats *stats);
int vfd_i40e_reset_vf_stats(uint8_t port, uint16_t vf_id);


int vfd_i40e_set_all_queues_drop_en(uint8_t port_id, uint8_t on);
uint32_t vfd_i40e_get_pf_spoof_stats(uint8_t port_id);
uint32_t vfd_i40e_get_vf_spoof_stats(uint8_t port_id, uint16_t vf_id);
int vfd_i40e_is_rx_queue_on(uint8_t port_id, uint16_t vf_id, int* mcounter);
void vfd_i40e_set_pfrx_drop(uint8_t port_id, int state );
void vfd_i40e_set_rx_drop(uint8_t port_id, uint16_t vf_id, int state);
void vfd_i40e_set_split_erop(uint8_t port_id, uint16_t vf_id, int state);
int vfd_i40e_get_split_ctlreg(uint8_t port_id, uint16_t vf_id);
int vfd_i40e_dump_all_vlans(uint8_t port_id);

#endif