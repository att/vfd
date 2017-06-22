
#ifndef _VFD_MLX5_H
#define _VFD_MLX5_H




// ------------- prototypes ----------------------------------------------
int vfd_mlx5_get_ifname(uint8_t port_id, char *ifname);

int vfd_mlx5_set_vf_mac_addr(uint8_t port, uint16_t vf_id, const char *mac_addr);
int vfd_mlx5_vf_mac_remove(uint8_t port_id, uint16_t vf_id);
int vfd_mlx5_set_vf_vlan_stripq(uint8_t port, uint16_t vf, uint8_t on);
int vfd_mlx5_set_vf_vlan_insert(uint8_t port, uint16_t vf_id, uint16_t vlan_id);
int vfd_mlx5_set_vf_mac_anti_spoof(uint8_t port_id, uint16_t vf_id, uint8_t on);
int vfd_mlx5_set_vf_rate_limit(uint8_t port_id, uint16_t vf_id, uint16_t rate);
int vfd_mlx5_set_vf_link_status(uint8_t port_id, uint16_t vf_id, int status);
uint32_t vfd_mlx5_get_pf_spoof_stats(uint8_t port_id);
int vfd_mlx5_get_num_vfs(uint8_t port_id);
int vfd_mlx5_get_vf_stats(uint8_t port_id, uint16_t vf_id, struct rte_eth_stats *stats);
uint64_t vfd_mlx5_get_vf_sysfs_counter(char *ifname, const char *counter,  uint16_t vf_id);

#endif
