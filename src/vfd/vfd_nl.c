/*
	Mnemonic:	vfd_nl.c
	Abstract: 	Interface between vfd and vfd-net kernel module.
	Date:		October 2017
	Author:		Alex Zelezniak

*/

#include "sriov.h"
#include "vfd_nl.h"
#include "vfd_mlx5.h"


static __u32 seq;

int 
netlink_send(int s, struct cn_msg *msg)
{
	struct nlmsghdr *nlh;
	unsigned int size;
	int err;
	char buf[128];
	struct cn_msg *m;

	size = NLMSG_SPACE(sizeof(struct cn_msg) + msg->len);

	nlh = (struct nlmsghdr *)buf;
	nlh->nlmsg_seq = seq++;
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_type = NLMSG_DONE;
	nlh->nlmsg_len = size;
	nlh->nlmsg_flags = 0;

	m = NLMSG_DATA(nlh);
#if 0
	bleat_printf( 5, "%s: [%08x.%08x] len=%u, seq=%u, ack=%u.\n",
	       __func__, msg->id.idx, msg->id.val, msg->len, msg->seq, msg->ack);

#endif
	memcpy(m, msg, sizeof(*m) + msg->len);

	err = send(s, nlh, size, 0);
	if (err == -1)
		bleat_printf( 2, "Failed to send: %s [%d].\n",
			strerror(errno), errno);

	return err;
}


int nl_socket;


void 
netlink_init(void)
{
	static pthread_t nl_tid;	
	int ret;
	
	ret = pthread_create(&nl_tid, NULL, (void *)netlink_connect, NULL);	
	if (ret != 0) {
		bleat_printf( 0, "CRI: abort: cannot crate netlink_connect thread" );
		rte_exit(EXIT_FAILURE, "Cannot create netlink_connect thread\n");
	}

	ret = rte_thread_setname(nl_tid, "vfd-nl");	
	if (ret != 0) {
		bleat_printf( 2, "error: failed to set thread name: vfd-nl");
	}
	
	bleat_printf( 2, "netlink connect thread created" );
}

/*
	This is executed in it's own thread and is responsible for checking the
	netlink messages from kernel module. 

*/
void
netlink_connect(void)
{
	char buf[1024];
	int len;
	struct nlmsghdr *reply;
	struct sockaddr_nl l_local;
	struct cn_msg *data;
	time_t tm;
	struct pollfd pfd;
	bool send_msgs = true;
	int need_exit = 0;
	
	struct vfd_nl_message *msg_rq;
	
	

	memset(buf, 0, sizeof(buf));
	

	nl_socket = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
	if (nl_socket == -1) {
		bleat_printf( 2, "Failed to open nl socket\n");
		return ;
	}

	l_local.nl_family = AF_NETLINK;
	l_local.nl_groups = -1; /* bitmask of requested groups */
	l_local.nl_pid = 0;

	bleat_printf( 2, "nl subscribing to %u.%u\n", CN_VFD_IDX, CN_VFD_VAL);

	if (bind(nl_socket, (struct sockaddr *)&l_local, sizeof(struct sockaddr_nl)) == -1) {
		bleat_printf( 2, "nl bind failed\n");
		close(nl_socket);
		return;
	}

#if 0
	{
		int on = 0x57; /* Additional group number */
		setsockopt(nl_socket, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP, &on, sizeof(on));
	}
#endif
	
	
	
	while (!need_exit) {
		
		pfd.fd = nl_socket;

	
		pfd.events = POLLIN;
		pfd.revents = 0;
		switch (poll(&pfd, 1, -1)) {
			case 0:
				need_exit = 1;
				break;
			case -1:
				if (errno != EINTR) {
					need_exit = 1;
					break;
				}
				continue;
		}
		if (need_exit)
			break;

		memset(buf, 0, sizeof(buf));
		len = recv(nl_socket, buf, sizeof(buf), 0);
		if (len == -1) {
			bleat_printf( 2, "nl recv buf error\n");
			close(nl_socket);
			return;
		}
		reply = (struct nlmsghdr *)buf;

		switch (reply->nlmsg_type) {
			case NLMSG_ERROR:
				bleat_printf( 2,  "nl error message received.\n");
				break;
			case NLMSG_DONE:
				data = (struct cn_msg *)NLMSG_DATA(reply);

				time(&tm);
				bleat_printf( 5, "nl %.24s : [%x.%x] [%08u.%08u].\n",
					ctime(&tm), data->id.idx, data->id.val, data->seq, data->ack);
					
				msg_rq = (struct vfd_nl_message *) data->data;
			
				bleat_printf( 5, "nl Port: %d, VF: %d, REQ: %d\n", msg_rq->port, msg_rq->vf, msg_rq->req);

				break;
			default:
				break;
		}
		
		
		if (send_msgs) {
		
			switch(msg_rq->req) {
				
				case NL_VF_STATS_RQ:
					bleat_printf( 5, "nl GET Stats, Port: %d, VF: %d\n", msg_rq->port, msg_rq->vf);					
					device_message(msg_rq->port, msg_rq->vf, NL_VF_STATS_RQ, NL_PF_RESP_OK);
					break;
					
				case NL_VF_GET_DEV_RQ:
					bleat_printf( 5, "nl GET device list, Port: %d, VF: %d\n", msg_rq->port, msg_rq->vf);								
					get_all_devices();
					break;
					
				default:
					bleat_printf( 2, "nl Unknown request, Port: %d, VF: %d, REQ: %d\n", msg_rq->port, msg_rq->vf, msg_rq->req);
					device_message(msg_rq->port, msg_rq->vf, NL_PF_RES_DEV_RQ, NL_PF_RESP_ERR);
					break;
			}

		}

	}
	
	close(nl_socket);
	return;	
}



void
device_message(int port, int vf, int req, int resp)
{
	int len;
	char buf[1024];
	char pciaddr[PCIADDR_LEN + 1];
	struct vfd_nl_message *msg_rq;
	struct cn_msg *data;
	struct nlmsghdr *reply;
	struct rte_eth_link link;
	struct rte_eth_dev_info dev_info;
	
	uint8_t bytes[6];
	int values[6];
	int i;
	struct vf_s* vfp;
	struct ether_addr e_addr;

	memset(buf, 0, sizeof(buf));
	reply = (struct nlmsghdr *)buf;
	
	data = (struct cn_msg *)NLMSG_DATA(reply);
	
	data = (struct cn_msg *)buf;

	data->id.idx = CN_VFD_IDX;
	data->id.val = CN_VFD_VAL;
	data->seq++;
	data->ack = 0;
	
	msg_rq = (struct vfd_nl_message *) data->data;
	
	msg_rq->info = malloc(sizeof(struct dev_info));
	msg_rq->info->stats = malloc(sizeof(struct dev_stats));
	
	msg_rq->info->mac = malloc(6);
	
	msg_rq->pciaddr = malloc(PCIADDR_LEN);
	
	
	if(req == NL_VF_STATS_RQ) {
		struct rte_eth_stats rt_stats;
		
		if(vf == MAX_VFS - 1) {
			rte_eth_stats_get(port, &rt_stats);		//PF			
			rte_eth_link_get_nowait(port, &link);
			
			msg_rq->info->link_state = link.link_status;
			msg_rq->info->link_speed = link.link_speed;
			msg_rq->info->link_duplex = link.link_duplex;
			
		} else {
			get_vf_stats(port, vf, &rt_stats);		//VF
			
			int mcounter = 0;
			if(is_rx_queue_on(port, vf, &mcounter ))
				msg_rq->info->link_state = 1;
			else
				msg_rq->info->link_state = 0;
		}
		
		
		msg_rq->info->stats->rx_packets	= rt_stats.ipackets;
		msg_rq->info->stats->tx_packets	= rt_stats.opackets;
		msg_rq->info->stats->rx_bytes		= rt_stats.ibytes;
		msg_rq->info->stats->tx_bytes		= rt_stats.obytes;
		msg_rq->info->stats->rx_errors	= rt_stats.ierrors;
		msg_rq->info->stats->tx_errors	= rt_stats.oerrors;
		msg_rq->info->stats->rx_dropped	= rt_stats.rx_nombuf;
		//msg_rq->info->stats.tx_dropped	= rt_stats.
		//msg_rq->info->stats.multicast	= rt_stats.
		//msg_rq->info->stats.collisions	= rt_stats.
		
							
				
		if(vf < MAX_VFS - 1) {  
			// VF
			vfp = suss_vf( port, vf );
				
			len = sscanf( vfp->macs[0], "%x:%x:%x:%x:%x:%x",
				&values[0], &values[1], &values[2], &values[3], &values[4], &values[5]);
					
			if (len == 6) {
				// convert to uint8_t 
				for( i = 0; i < 6; ++i )
					bytes[i] = (uint8_t) values[i];	
				
				memcpy(msg_rq->info->mac,  bytes, 6);				
			} else {
				bleat_printf( 3, "nl: incorrect MAC: %s\n", vfp->macs[0]);
				memset(msg_rq->info->mac,  0, 6);	
			}
		} else {
			//PF
			rte_eth_macaddr_get(port, &e_addr);
			memcpy(msg_rq->info->mac, (char *) &e_addr, 6);
		}

	} else if (req == NL_PF_ADD_DEV_RQ) {
		rte_eth_dev_info_get(port, &dev_info);
			
		if(vf == MAX_VFS - 1) {  
			// PF
			snprintf(pciaddr, sizeof( pciaddr ), "%04X:%02X:%02X.%01X", 
					dev_info.pci_dev->addr.domain,
					dev_info.pci_dev->addr.bus,
					dev_info.pci_dev->addr.devid,
					dev_info.pci_dev->addr.function);
					
			strncpy(msg_rq->pciaddr, pciaddr, PCIADDR_LEN);
		} else {
			
			// VF
			uint32_t pf_ari = dev_info.pci_dev->addr.bus << 8 | dev_info.pci_dev->addr.devid << 3 | dev_info.pci_dev->addr.function;
			struct sriov_port_s *p = &running_config->ports[port];	
			uint32_t new_ari = pf_ari + p->vf_offset + (vf * p->vf_stride);
			
			int domain = 0;
			int bus = (new_ari >> 8) & 0xff;
			int devid = (new_ari >> 3) & 0x1f;
			int function = new_ari & 0x7;
			
			snprintf(pciaddr, sizeof( pciaddr ), "%04X:%02X:%02X.%01X", 
					domain,
					bus,
					devid,
					function);
					
			strncpy(msg_rq->pciaddr, pciaddr, PCIADDR_LEN);
		}
	
		bleat_printf( 5, "nl: PCI ADDR: %s\n", pciaddr);
	}
	

	
	msg_rq->port = port;
	msg_rq->vf = vf;
					
	msg_rq->req = req;
	msg_rq->resp = resp;
	
	memcpy (data->data, msg_rq, sizeof(struct vfd_nl_message));
	data->len = sizeof(struct vfd_nl_message);
	
	len = netlink_send(nl_socket, data);
	
	bleat_printf( 5, "nl messages have been sent to %08x.%08x, link = %08x, data len = %d, msg len = %d\n", data->id.idx, data->id.val, msg_rq->info->link_state, data->len, len);
		
	free(msg_rq->info->stats);
	free(msg_rq->info->mac);
	free(msg_rq->pciaddr);
	free(msg_rq->info);		
}




void
get_all_devices(void)
{
	int i, y;
	sriov_conf_t * sriov_config = running_config;
	
	for (i = 0; i < sriov_config->num_ports; i++){

		device_message(i, MAX_VFS - 1, NL_PF_ADD_DEV_RQ, NL_PF_RESP_OK);  // indicates PF
		
		for (y = 0; y < sriov_config->ports[i].num_vfs; y++){
			if( sriov_config->ports[i].vfs[y].num >= 0 ) {
	
				device_message(sriov_config->ports[i].rte_port_number, sriov_config->ports[i].vfs[y].num, NL_PF_ADD_DEV_RQ, NL_PF_RESP_OK);
				
			} else {
				bleat_printf( 2, "get_all_devices: port %d index %d is not configured", i, y );
			}
		}
	}

}



/*
*	returns port number assotiated 
	with PCI address provided
*/
int
get_port_by_pci(const char * pciaddr)
{
	
	int i, ret;
	
	struct rte_eth_dev_info dev_info;
	struct rte_pci_addr s_pci_addr, d_pci_addr;
	
	ret = eal_parse_pci_DomBDF(pciaddr, &s_pci_addr);
	
	if (ret < 0) {
		bleat_printf( 2, "get_port_by_pci: ret=%d, pciaddr=%d", ret, pciaddr);
		return (-1);
	}
	
	for( i = 0; i < running_config->num_ports; ++i ) {

		rte_eth_dev_info_get( running_config->ports[i].rte_port_number, &dev_info );				// must use port number that we mapped during initialisation
		
		d_pci_addr = dev_info.pci_dev->addr;

		if (!rte_eal_compare_pci_addr(&d_pci_addr, &s_pci_addr)) {
			bleat_printf( 5, "port found: Port=%d, pciaddr=%s", running_config->ports[i].rte_port_number, pciaddr);
			return (running_config->ports[i].rte_port_number);
		}		
	}
	
	return (-1);
}


/*
*	returns VF stats based on provided 
	PF PCIADDR and VF number
*/
int
get_vf_stats(int port_id, int vf, struct rte_eth_stats *stats)
{
	int result = -1;
	uint dev_type = get_nic_type(port_id);
	
	switch (dev_type) {
		case VFD_NIANTIC:
			result = vfd_ixgbe_get_vf_stats(port_id, vf, stats);
			stats->oerrors = 0;
			break;
			
		case VFD_FVL25:		
			result = vfd_i40e_get_vf_stats(port_id, vf, stats);
			break;

		case VFD_BNXT:
			result = vfd_bnxt_get_vf_stats(port_id, vf, stats);
			break;
			
		case VFD_MLX5:
			result = vfd_mlx5_get_vf_stats(port_id, vf, stats);
			break;

		default:
			bleat_printf( 2, "get_vf_stats: unknown device type: %u, port: %u", port_id, dev_type);
			break;	
	}	
	return result;
}






