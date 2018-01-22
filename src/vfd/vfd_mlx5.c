
#include "sriov.h"
#include "vfd_mlx5.h"

#include <net/if.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
vfd_mlx5_get_ifname(uint16_t port_id, char *ifname)
{
	struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get(port_id, &dev_info);
	
	if (!if_indextoname(dev_info.if_index, ifname))
		return -1;

	return 0;
}

int
vfd_mlx5_get_num_vfs(uint16_t port_id)
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
vfd_mlx5_set_vf_link_status(uint16_t port_id, uint16_t vf_id, int status)
{
	char ifname[IF_NAMESIZE];
	char link_state[16]= "";
	char cmd[128] = "";
	int ret;

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

	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int 
vfd_mlx5_set_vf_mac_addr(uint16_t port_id, uint16_t vf_id, const char* mac, uint8_t on)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "echo \"%s %s\" > /sys/class/net/%s/device/sriov/%d/mac_list", on ? "add" : "rem", mac, ifname, vf_id);

	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int 
vfd_mlx5_set_vf_def_mac_addr(uint16_t port_id, uint16_t vf_id, const char* mac)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "ip link set %s vf %d mac %s", ifname, vf_id, mac);
	
	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int
vfd_mlx5_vf_mac_remove(uint16_t port_id, uint16_t vf_id)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "ip link set %s vf %d mac 00:00:00:00:00:00", ifname, vf_id);
	
	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int 
vfd_mlx5_set_vf_vlan_stripq(uint16_t port_id, uint16_t vf_id, uint8_t on)
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
vfd_mlx5_set_vf_vlan_insert(uint16_t port_id, uint16_t vf_id, uint16_t vlan_id)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	if (vlan_id) {
		sprintf(cmd, "echo rem 0 4095 > /sys/class/net/%s/device/sriov/%d/trunk", ifname, vf_id);
		ret = system(cmd);

		if (ret < 0) {
			//	printf("cmd exec returned %d\n", ret);
		}
	}

	sprintf(cmd, "echo '%d:0:802.1ad' > /sys/class/net/%s/device/sriov/%d/vlan", vlan_id, ifname, vf_id);
	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int 
vfd_mlx5_set_vf_cvlan_insert(uint16_t port_id, uint16_t vf_id, uint16_t vlan_id)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	if (vlan_id) {
		sprintf(cmd, "echo rem 0 4095 > /sys/class/net/%s/device/sriov/%d/trunk", ifname, vf_id);
		ret = system(cmd);

		if (ret < 0) {
			//	printf("cmd exec returned %d\n", ret);
		}
	}

	sprintf(cmd, "ip link set %s vf %d vlan %d", ifname, vf_id, vlan_id);
	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int
vfd_mlx5_set_vf_min_rate(uint16_t port_id, uint16_t vf_id, uint16_t rate)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "echo %d > /sys/class/net/%s/device/sriov/%d/min_tx_rate", rate, ifname, vf_id);
	
	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int
vfd_mlx5_set_vf_rate_limit(uint16_t port_id, uint16_t vf_id, uint16_t rate)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "ip link set %s vf %d rate %d", ifname, vf_id, rate);
	
	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int
vfd_mlx5_set_vf_mac_anti_spoof(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "ip link set %s vf %d spoofchk %s", ifname, vf_id, on ? "on" : "off");

	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}
	return 0;
}

uint32_t
vfd_mlx5_get_pf_spoof_stats(uint16_t port_id)
{
	char ifname[IF_NAMESIZE];
	//FILE *fp;
	//char data[64];
	uint32_t val = 0;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

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

uint64_t
vfd_mlx5_get_vf_ethtool_counter(char *ifname, const char *counter)
{
	FILE *fp;
	uint64_t val;
	char cmd[128] = "";
	char data[64] = "";

	sprintf(cmd, "ethtool -S %s | grep %s | cut -c%d-", ifname, counter, (int)strlen(counter) + 2);

	fp = popen(cmd, "r");
	if (fp == NULL)
		return 0;
	
	while (fgets(data, 64, fp) != NULL);
	val = strtoll(data, NULL, 10);
	
	pclose(fp);

	return val;
}

uint64_t
vfd_mlx5_get_vf_spoof_stats(uint16_t port_id, uint16_t vf_id)
{
	char ifname[IF_NAMESIZE];

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	return vfd_mlx5_get_vf_sysfs_counter(ifname, "tx_dropped", vf_id);;
}

int
vfd_mlx5_get_vf_stats(uint16_t port_id, uint16_t vf_id, struct rte_eth_stats *stats)
{
	char ifname[IF_NAMESIZE];
	
	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	stats->ipackets = vfd_mlx5_get_vf_sysfs_counter(ifname, "rx_packets", vf_id);
	stats->opackets = vfd_mlx5_get_vf_sysfs_counter(ifname, "tx_packets", vf_id);
	stats->ibytes = vfd_mlx5_get_vf_sysfs_counter(ifname, "rx_bytes", vf_id);
	stats->obytes = vfd_mlx5_get_vf_sysfs_counter(ifname, "tx_bytes", vf_id);
	stats->ipackets = vfd_mlx5_get_vf_sysfs_counter(ifname, "rx_packets", vf_id);
	stats->ierrors = vfd_mlx5_get_vf_sysfs_counter(ifname, "rx_dropped", vf_id);
	stats->oerrors = vfd_mlx5_get_vf_sysfs_counter(ifname, "tx_dropped", vf_id);
	
	return 0;
}

int
vfd_mlx5_pf_vf_offset(char *pciid)
{
	char cmd[128] = "";
	char data[8] = "";
	FILE *fp;

	sprintf(cmd, "lspci -s %s -vv | grep -o 'VF offset: [0-9]*' | grep -o '[0-9]*'", pciid);
	fp = popen(cmd, "r");
	if (fp == NULL)
		return 0;
	
	while (fgets(data, 8, fp) != NULL);

	pclose(fp);

	if (!strlen(data))
		return 0;

	return atoi(data);
}

int
vfd_mlx5_set_prio_trust(uint16_t port_id)
{
	char ifname[IF_NAMESIZE];
	char cmd[128] = "";
	int ret;
	
	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "mlnx_qos -i %s --trust=pcp", ifname);

	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int
vfd_mlx5_set_qos_pf(uint16_t port_id, tc_class_t **tc_config, uint8_t ntcs)
{
	char ifname[IF_NAMESIZE];
	char cmd[256] = "";
	struct mlx5_tc_cfg tc_cfg[MAX_TCS];
	struct rte_eth_link link;
	int i, ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	if (ntcs != 8) {
		printf("mlx5 devices don't support 4 TC configuration\n");
		return -2;
	}

	memset(tc_cfg, 0, sizeof(tc_cfg));

	rte_eth_link_get_nowait(port_id, &link);

	for(i=0; i< ntcs; i++) {
		strcpy(tc_cfg[i].policy, (tc_config[i]->flags & TCF_LNK_STRICTP) ? "strict" : "ets");
		tc_cfg[i].min_bw = (tc_config[i]->flags & TCF_LNK_STRICTP) ? 0 : tc_config[i]->min_bw;
		if (tc_config[i]->max_bw < 100) {
			tc_cfg[i].max_bw = (tc_config[i]->max_bw * link.link_speed) / 100;
			if (tc_cfg[i].max_bw < 1000)
				tc_cfg[i].max_bw = 1000;

			//move to Gbps
			tc_cfg[i].max_bw =  tc_cfg[i].max_bw / 1000;
		}
	}
		

	sprintf(cmd, "mlnx_qos -i %s -s %s,%s,%s,%s,%s,%s,%s,%s -t %d,%d,%d,%d,%d,%d,%d,%d", ifname, tc_cfg[0].policy, tc_cfg[1].policy,
			tc_cfg[2].policy, tc_cfg[3].policy, tc_cfg[4].policy, tc_cfg[5].policy, tc_cfg[6].policy, tc_cfg[7].policy,
			tc_cfg[0].min_bw, tc_cfg[1].min_bw, tc_cfg[2].min_bw, tc_cfg[3].min_bw, tc_cfg[4].min_bw, tc_cfg[5].min_bw,
			tc_cfg[6].min_bw, tc_cfg[7].min_bw);

	ret = system(cmd);

	//set rate limiters
	
	sprintf(cmd, "mlnx_qos -i %s -r %d,%d,%d,%d,%d,%d,%d,%d", ifname, tc_cfg[0].max_bw, tc_cfg[1].max_bw, tc_cfg[2].max_bw,
				tc_cfg[3].max_bw, tc_cfg[4].max_bw, tc_cfg[5].max_bw, tc_cfg[6].max_bw, tc_cfg[7].max_bw);

	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int
vfd_mlx5_set_vf_vlan_filter(uint16_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on)
{
	char ifname[IF_NAMESIZE];
	char cmd[256] = "";
	int vf_num;
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	vf_num = ffs(vf_mask) - 1;

	sprintf(cmd, "echo %s %d %d > /sys/class/net/%s/device/sriov/%d/trunk", on ? "add" : "rem", vlan_id, vlan_id, ifname, vf_num);

	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int 
vfd_mlx5_set_vf_promisc(uint16_t port_id, uint16_t vf_id, uint8_t on)
{
	char ifname[IF_NAMESIZE];
	char cmd[256] = "";
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "echo \"%s\" > /sys/class/net/%s/device/sriov/%d/trust", on ? "ON" : "OFF", ifname, vf_id);
	bleat_printf( 0, "allow_mcast cmd: %s", cmd );

	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int
vfd_mlx5_set_mirror( portid_t port_id, uint32_t vf, uint8_t target, uint8_t direction )
{
	char ifname[IF_NAMESIZE];
	char cmd_in[256] = "";
	char cmd_eg[256] = "";
	int ret;

	if( target > MAX_VFS ) {
		bleat_printf( 0, "mirror not set: target vf out of range: %d", (int) target );
		return -1;
	}

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	switch( direction ) {
		case MIRROR_OFF:
			sprintf(cmd_in, "echo %s %d > /sys/class/net/%s/device/sriov/%d/ingress_mirr",
				"rem", vf, ifname, target);
			sprintf(cmd_eg, "echo %s %d > /sys/class/net/%s/device/sriov/%d/egress_mirr",
				"rem", vf, ifname, target);
			break;
		case MIRROR_IN:
			sprintf(cmd_in, "echo %s %d > /sys/class/net/%s/device/sriov/%d/ingress_mirr",
				"rem", vf, ifname, target);
			sprintf(cmd_eg, "echo %s %d > /sys/class/net/%s/device/sriov/%d/egress_mirr",
				"add", vf, ifname, target);
			break;
		case MIRROR_OUT:
			sprintf(cmd_in, "echo %s %d > /sys/class/net/%s/device/sriov/%d/ingress_mirr",
				"add", vf, ifname, target);
			sprintf(cmd_eg, "echo %s %d > /sys/class/net/%s/device/sriov/%d/egress_mirr",
				"rem", vf, ifname, target);
			break;
		case MIRROR_ALL:
			sprintf(cmd_in, "echo %s %d > /sys/class/net/%s/device/sriov/%d/ingress_mirr",
				"add", vf, ifname, target);
			sprintf(cmd_eg, "echo %s %d > /sys/class/net/%s/device/sriov/%d/egress_mirr",
				"add", vf, ifname, target);
			break;

	}

	ret = system(cmd_in);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	ret = system(cmd_eg);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}

int
vfd_mlx5_set_vf_tcqos( portid_t port_id, uint32_t vf, uint8_t tc, uint32_t rate )
{
	char ifname[IF_NAMESIZE];
	char cmd[256] = "";
	int ret;

	if (vfd_mlx5_get_ifname(port_id, ifname))
		return -1;

	sprintf(cmd, "echo %d %d > /sys/class/net/%s/device/sriov/%d/min_tx_tc_rate",
		tc, rate, ifname, vf);

	ret = system(cmd);

	if (ret < 0) {
	//	printf("cmd exec returned %d\n", ret);
	}

	return 0;
}
