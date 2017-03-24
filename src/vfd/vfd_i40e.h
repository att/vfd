
#ifndef _VFD_I40E_H
#define _VFD_I40E_H


#include <rte_pmd_i40e.h>

/* Opcodes for VF-PF communication. These are placed in the v_opcode field
 * of the virtchnl_msg structure.
 */
enum i40e_virtchnl_ops {
/* The PF sends status change events to VFs using
 * the I40E_VIRTCHNL_OP_EVENT opcode.
 * VFs send requests to the PF using the other ops.
 */
	I40E_VIRTCHNL_OP_UNKNOWN = 0,
	I40E_VIRTCHNL_OP_VERSION = 1, /* must ALWAYS be 1 */
	I40E_VIRTCHNL_OP_RESET_VF = 2,
	I40E_VIRTCHNL_OP_GET_VF_RESOURCES = 3,
	I40E_VIRTCHNL_OP_CONFIG_TX_QUEUE = 4,
	I40E_VIRTCHNL_OP_CONFIG_RX_QUEUE = 5,
	I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES = 6,
	I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP = 7,
	I40E_VIRTCHNL_OP_ENABLE_QUEUES = 8,
	I40E_VIRTCHNL_OP_DISABLE_QUEUES = 9,
	I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS = 10,
	I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS = 11,
	I40E_VIRTCHNL_OP_ADD_VLAN = 12,
	I40E_VIRTCHNL_OP_DEL_VLAN = 13,
	I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE = 14,
	I40E_VIRTCHNL_OP_GET_STATS = 15,
	I40E_VIRTCHNL_OP_FCOE = 16,
	I40E_VIRTCHNL_OP_EVENT = 17, /* must ALWAYS be 17 */
#ifdef I40E_SOL_VF_SUPPORT
	I40E_VIRTCHNL_OP_GET_ADDNL_SOL_CONFIG = 19,
#endif
	I40E_VIRTCHNL_OP_CONFIG_RSS_KEY = 23,
	I40E_VIRTCHNL_OP_CONFIG_RSS_LUT = 24,
	I40E_VIRTCHNL_OP_GET_RSS_HENA_CAPS = 25,
	I40E_VIRTCHNL_OP_SET_RSS_HENA = 26,

};
	
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
int vfd_i40e_allow_untagged(uint8_t port, uint16_t vf_id, uint8_t on);
int vfd_i40e_set_vf_vlan_filter(uint8_t port, uint16_t vlan_id, uint64_t vf_mask, uint8_t on);
int vfd_i40e_get_vf_stats(uint8_t port, uint16_t vf_id, struct rte_eth_stats *stats);
int vfd_i40e_reset_vf_stats(uint8_t port, uint16_t vf_id);
int vfd_i40e_set_all_queues_drop_en(uint8_t port_id, uint8_t on);
void vfd_i40e_vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param);
uint32_t vfd_i40e_get_pf_spoof_stats(uint8_t port_id);
uint32_t vfd_i40e_get_vf_spoof_stats(uint8_t port_id, uint16_t vf_id);
int vfd_i40e_is_rx_queue_on(uint8_t port_id, uint16_t vf_id, int* mcounter);
void vfd_i40e_set_pfrx_drop(uint8_t port_id, int state );
void vfd_i40e_set_rx_drop(uint8_t port_id, uint16_t vf_id, int state);
void vfd_i40e_set_split_erop(uint8_t port_id, uint16_t vf_id, int state);
int vfd_i40e_get_split_ctlreg(uint8_t port_id, uint16_t vf_id);
int vfd_i40e_dump_all_vlans(uint8_t port_id);

#endif