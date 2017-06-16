
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
vfd_mlx5_set_vf_mac_addr(uint8_t port_id, uint16_t vf_id, const char* mac)
{
	char ifname[IF_NAMESIZE];
	char vf_id_s[4] = "";

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(vf_id_s, "%d", vf_id);

	//TODO: set vf link down or unbind driver
	execl("/usr/sbin/ip", "ip", "link", "set", ifname, "vf", vf_id_s, "mac", mac, NULL);
	//TODO: set vf link up or bind driver

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
	//Enabled/Disabled with vlan insertion command.
	printf("%d", on);
	
	return 0;
}


int 
vfd_mlx5_set_vf_vlan_insert(uint8_t port_id, uint16_t vf_id, uint16_t vlan_id)
{
	char ifname[IF_NAMESIZE];
	char vlan_id_s[32] = "";
	char vf_id_s[8] = "";

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	if (vlan_id == 0)
		strcpy(vlan_id_s, "4095");
	else 
		sprintf(vlan_id_s, "%d", vlan_id);
	
	sprintf(vf_id_s, "%d", vf_id);
	//ip link add link <ifname> name <ifname.vlan_id> type vlan id <vlan_id>
	execl("/usr/sbin/ip", "ip", "link", "set", ifname, "vf", vf_id_s, "vlan", vlan_id_s, NULL);

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
	execl("/usr/sbin/ip", "ip", "link", "set", ifname, "vf", vf_id_s, "rate", rate_s, NULL);
	
	return 0;
}

int
vfd_mlx5_set_vf_mac_anti_spoof(uint8_t port_id, uint16_t vf_id, uint8_t on)
{
	char ifname[IF_NAMESIZE];
	char vf_id_s[32] = "";

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(vf_id_s, "%d", vf_id);

	execl("/usr/sbin/ip", "ip", "link", "set", ifname, "vf", vf_id_s, "spoofchk", on ? "on" : "off", NULL);

	return 0;
}

uint32_t
vfd_mlx5_get_pf_spoof_stats(uint8_t port_id)
{
	char ifname[IF_NAMESIZE];
	FILE *fp;
	char data[64];
	uint32_t val;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	fp = popen("ethtool -S <ifname> | grep <counter_name> | cut -c32-", "r");
	if (fp == NULL)
		return 0;
	
	while (fgets(data, 64, fp) != NULL);
	val = atoi(data);
	
	if (pclose(fp) == -1)
		printf("fp close error\n");
	return val;
}

/*int
vfd_mlx5_get_vf_stats(uint8_t port_id, uint16_t vf_id, struct rte_eth_stats *stats)
{
	return 0
}*/
