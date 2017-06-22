
#include "sriov.h"
#include "vfd_mlx5.h"

#include <net/if.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
vfd_mlx5_get_ifname(uint8_t port_id, char *ifname)
{
	struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get(port_id, &dev_info);
	
	if (!if_indextoname(dev_info.if_index, ifname))
		return -1;

	return 0;
}

int
vfd_mlx5_get_num_vfs(uint8_t port_id)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";
	char data[8];
	int val;
	FILE *fp;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "cat /sys/class/net/%s/device/mlx5_num_vfs", ifname);

	fp = popen(cmd, "r");
	if (fp == NULL)
		return 0;
	
	while (fgets(data, 8, fp) != NULL);
	val = atoi(data);
	
	pclose(fp);

	return val;
}

int
vfd_mlx5_set_vf_link_status(uint8_t port_id, uint16_t vf_id, int status)
{
	char ifname[IF_NAMESIZE];
	char link_state[16]= "";
	char cmd[128] = "";

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;
	
			
	switch (status) {
		case VF_LINK_ON:
			strcpy(link_state, "enable");
			break;
		case VF_LINK_OFF:
			strcpy(link_state, "disable");
			break;
		case VF_LINK_AUTO:
			strcpy(link_state, "auto");
			break;
		default:
			return -1;
	}	
	
	sprintf(cmd, "ip link set %s vf %d state %s", ifname, vf_id, link_state);
	
	system(cmd);

	return 0;
}

int 
vfd_mlx5_set_vf_mac_addr(uint8_t port_id, uint16_t vf_id, const char* mac)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "ip link set %s vf %d mac %s", ifname, vf_id, mac);
	
	system(cmd);

	return 0;
}

int
vfd_mlx5_vf_mac_remove(uint8_t port_id, uint16_t vf_id)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "ip link set %s vf %d mac 00:00:00:00:00:00", ifname, vf_id);
	
	system(cmd);

	return 0;
}

int 
vfd_mlx5_set_vf_vlan_stripq(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	char ifname[IF_NAMESIZE];
	char vf_id_s[8] = "";

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(vf_id_s, "%d", vf_id);

	if (on)
		return 0;
	
	return 0;
}


int 
vfd_mlx5_set_vf_vlan_insert(uint8_t port_id, uint16_t vf_id, uint16_t vlan_id)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	//if (vlan_id == 0)
	//	strcpy(vlan_id_s, "4095");
	//else 
	//	sprintf(vlan_id_s, "%d", vlan_id);
	//
	//sprintf(vf_id_s, "%d", vf_id);

	//ip link add link <ifname> name <ifname.vlan_id> type vlan id <vlan_id>
	//execl("/usr/sbin/ip", "ip", "link", "set", ifname, "vf", vf_id_s, "vlan", vlan_id_s, NULL);
	
	sprintf(cmd, "ip link set %s vf %d vlan %d", ifname, vf_id, vlan_id);

	system(cmd);

	return 0;
}

int
vfd_mlx5_set_vf_rate_limit(uint8_t port_id, uint16_t vf_id, uint16_t rate)
{
	char ifname[IF_NAMESIZE];
	char vf_id_s[8] = "";
	char rate_s[32] = "";

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(vf_id_s, "%d", vf_id);
	sprintf(rate_s, "%d", rate);

	//We don't support q_mask but rather applying it on all vf queues.
	//execl("/usr/sbin/ip", "ip", "link", "set", ifname, "vf", vf_id_s, "rate", rate_s, NULL);
	
	return 0;
}

int
vfd_mlx5_set_vf_mac_anti_spoof(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "ip link set %s vf %d spoofchk %s", ifname, vf_id, on ? "on" : "off");

	system(cmd);

	return 0;
}

uint32_t
vfd_mlx5_get_pf_spoof_stats(uint8_t port_id)
{
	char ifname[IF_NAMESIZE];
	//FILE *fp;
	//char data[64];
	uint32_t val = 0;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	/*fp = popen("ethtool -S <ifname> | grep <counter_name> | cut -c32-", "r");
	if (fp == NULL)
		return 0;
	
	while (fgets(data, 64, fp) != NULL);
	val = atoi(data);
	
	if (pclose(fp) == -1)
		printf("fp close error\n");*/
	return val;
}

uint64_t
vfd_mlx5_get_vf_sysfs_counter(char *ifname, const char *counter,  uint16_t vf_id)
{
	FILE *fp;
	uint64_t val;
	char cmd[128] = "";
	char data[64] = "";

	sprintf(cmd, "cat /sys/class/net/%s/device/sriov/%d/stats | grep %s | grep -o '[0-9]*'", ifname, vf_id, counter);

	fp = popen(cmd, "r");
	if (fp == NULL)
		return 0;
	
	while (fgets(data, 64, fp) != NULL);
	val = strtoll(data, NULL, 10);
	
	pclose(fp);

	return val;
}

int
vfd_mlx5_get_vf_stats(uint8_t port_id, uint16_t vf_id, struct rte_eth_stats *stats)
{
	char ifname[IF_NAMESIZE];
	
	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	stats->ipackets = vfd_mlx5_get_vf_sysfs_counter(ifname, "rx_packets", vf_id);
	stats->opackets = vfd_mlx5_get_vf_sysfs_counter(ifname, "tx_packets", vf_id);
	stats->ibytes = vfd_mlx5_get_vf_sysfs_counter(ifname, "rx_bytes", vf_id);
	stats->obytes = vfd_mlx5_get_vf_sysfs_counter(ifname, "tx_bytes", vf_id);
	stats->ipackets = vfd_mlx5_get_vf_sysfs_counter(ifname, "rx_packets", vf_id);
	stats->oerrors = 0;
	
	return 0;
}