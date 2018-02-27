
/* vfd-net
 *
 * Alex Z.
 *
 * Inspired by book "Linux Device Drivers" 
 * by Alessandro Rubini and Jonathan Corbet, 
 * published by O'Reilly & Associates.
 *
 */
 
 
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h> 
#include <linux/errno.h>  
#include <linux/types.h>  

#include <linux/in.h>
#include <linux/netdevice.h>   
#include <linux/etherdevice.h> 
#include <linux/connector.h>
#include <linux/version.h>

#include "vfd-net.h"


#define VFD_TIMEOUT 5   /* In jiffies */

static struct cb_id vfd_cn_id = { CN_NETLINK_USERS + 3, 0x456 };
static char vfd_net_name[] = "vfd-net";
const char vfd_net_version[] = "0.1";
static char vfd_net_copyright[] = "Copyright (c) 2017 AT&T.";

static struct sock *nls;

struct net_device *vfd_netdevs[MAX_PF][MAX_VF];

static int timeout = VFD_TIMEOUT;
module_param(timeout, int, 0);

MODULE_AUTHOR("AT&T");
MODULE_LICENSE("GPL");

	 
static void vfd_cleanup(void);
void vfd_init(struct net_device *dev);
static int add_vfd_net(int pf, int vf, char * bdf, int len);
static int delete_vfd_net(int pf, int vf);
static void update_vfd_net(void);



struct vfd_priv {
	struct net_device_stats stats;
	struct pci_dev *pci_dev;
	int status;
	struct sk_buff *skb;
	spinlock_t lock;
	struct net_device *netdev;
};


static int is_port_vf_valid(int port, int vf)
{
	if ((0 <= port && port < MAX_PF) && (0 <= vf && vf < MAX_VF))
		return 1;
	else
		return 0;
}


static void delete_all_vfd_devs(void)
{
	int x, y;
	
	for (x = 0; x < MAX_PF; x++) {
		
		for (y = 0; y < MAX_VF; y++) {
			
			if(vfd_netdevs[x][y] != NULL){
				
				delete_vfd_net(x, y);
			}
		}
	}
}


static void vfd_stats_callback(struct cn_msg *msg, struct netlink_skb_parms *nsp)
{
	struct vfd_nl_message *vfd_msg;	
	struct vfd_priv *priv = NULL;
	spinlock_t *lock;
	
	pr_debug("%s: %lu: idx=%x, val=%x, seq=%u, ack=%u, len=%d\n",
	        __func__, jiffies, msg->id.idx, msg->id.val,
	        msg->seq, msg->ack, msg->len);			
	
	vfd_msg = (struct vfd_nl_message *) msg->data;
		
	
	if(vfd_msg->resp != NL_PF_RESP_OK) {
		pr_warning("ERROR: PF = %u, VF = %u, RQ = %u, RESP = %u\n", vfd_msg->port, vfd_msg->vf, vfd_msg->req, vfd_msg->resp);
		return;
	} else {
		pr_debug("PF = %u, VF = %u, RQ = %u, RESP = %u\n", vfd_msg->port, vfd_msg->vf, vfd_msg->req, vfd_msg->resp);
	}

	if (is_port_vf_valid(vfd_msg->port, vfd_msg->vf))
	{
		switch (vfd_msg->req) {
		case NL_VF_STATS_RQ:
		
			priv = netdev_priv(vfd_netdevs[vfd_msg->port][vfd_msg->vf]);	
			
			lock = &priv->lock;
			spin_lock(lock);
			
			if (priv) {
				pr_debug("PF = %u, VF = %u, STATE %x\n", vfd_msg->port, vfd_msg->vf, vfd_msg->info->link_state);

				priv->stats.rx_packets	= vfd_msg->info->stats->rx_packets;
				priv->stats.tx_packets	= vfd_msg->info->stats->tx_packets;
				priv->stats.rx_bytes	= vfd_msg->info->stats->rx_bytes;
				priv->stats.tx_bytes	= vfd_msg->info->stats->tx_bytes;
				priv->stats.rx_errors	= vfd_msg->info->stats->rx_errors;
				priv->stats.tx_errors	= vfd_msg->info->stats->tx_errors;
				priv->stats.rx_dropped	= vfd_msg->info->stats->rx_dropped;
				
				priv->netdev->flags &= ~IFF_BROADCAST & ~IFF_MULTICAST;
				
				memcpy(priv->netdev->dev_addr, vfd_msg->info->mac, ETH_ALEN);	

	
				if (vfd_msg->info->link_state) {
					priv->netdev->state = __LINK_STATE_PRESENT;
					priv->netdev->flags |= IFF_UP | IFF_LOWER_UP | IFF_RUNNING; 
					
					netif_carrier_on(priv->netdev);
					netif_tx_wake_all_queues(priv->netdev);
					
				} else {
					priv->netdev->flags &= ~IFF_UP & ~IFF_RUNNING & ~IFF_LOWER_UP;
					netif_carrier_off(priv->netdev);
					netif_stop_queue(priv->netdev);
				}
				
				pr_debug("FLAGS = %x, STATE = %x\n", priv->netdev->flags, (uint32_t) priv->netdev->state);
			}
			else
				pr_debug("*priv == NULL PF = %u, VF = %u\n", vfd_msg->port, vfd_msg->vf);
			
			spin_unlock(lock);
			
			break;
						
		case NL_PF_ADD_DEV_RQ:
			pr_debug("Add device: Port: %d, VF: %d\n", vfd_msg->port, vfd_msg->vf);; 		
			add_vfd_net(vfd_msg->port, vfd_msg->vf, vfd_msg->pciaddr, PCIADDR_LEN); 			
			break;

		case NL_PF_DEL_DEV_RQ:
			pr_debug("Delete device: Port: %d, VF: %d\n", vfd_msg->port, vfd_msg->vf);
			delete_vfd_net(vfd_msg->port, vfd_msg->vf);	
			break;	
			
		case NL_PF_UPD_DEV_RQ:
			pr_debug("Update device list: Port: %d, VF: %d\n", vfd_msg->port, vfd_msg->vf);
			update_vfd_net(); 			
			break;	

		case NL_PF_RES_DEV_RQ:
			pr_debug("Remove all devices Port: %d, VF: %d\n", vfd_msg->port, vfd_msg->vf);
			delete_all_vfd_devs(); 			
			break;				
		
		default:
			pr_warning("unknown msg: PF = %u, VF = %u, REQ = %u, RESP = %u\n", vfd_msg->port, vfd_msg->vf, vfd_msg->req, vfd_msg->resp );
		}
		
			
	} else {
		pr_warning("Invalid responce: PF = %u, VF = %u\n", vfd_msg->port, vfd_msg->vf);
	}
	
}


static u32 vfd_nl_rq_counter;
static void send_nl_request(int port, int vf, uint32_t req)
{
	struct cn_msg *m;

	struct vfd_nl_message msg;

	m = kzalloc(sizeof(*m) + sizeof(msg), GFP_ATOMIC);
	if ( m ) {

		memcpy(&m->id, &vfd_cn_id, sizeof(m->id));
		m->seq = vfd_nl_rq_counter;
		m->len = sizeof(msg);

		msg.port = port;
		msg.vf = vf;
		msg.req = req;
		
		pr_debug("%s: %d: %d: %d: %d\n", __func__,  msg.port, msg.vf, msg.req, m->len);

		memcpy(m + 1, (const void *) &msg, m->len);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0)
		cn_netlink_send(m, 0, GFP_ATOMIC);
#else
		cn_netlink_send(m, 0, 0, GFP_ATOMIC);
#endif
		kfree(m);
	}
	
	/*
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
			pr_debug("nl error message received.\n");
			break;
		case NLMSG_DONE:
			data = (struct cn_msg *)NLMSG_DATA(reply);

			time(&tm);
			pr_debug("nl %.24s : [%x.%x] [%08u.%08u].\n",
				ctime(&tm), data->id.idx, data->id.val, data->seq, data->ack);
				
			msg_rq = (struct vfd_nl_message *) data->data;
		
			pr_debug("nl Port: %d, VF: %d, REQ: %d\n", msg_rq->port, msg_rq->vf, msg_rq->req);

			break;
		default:
			break;
	}
	*/

	
	vfd_nl_rq_counter++;
}


static void send_stats_request(int port, int vf)
{
	send_nl_request(port, vf, NL_VF_STATS_RQ);
}


static void send_get_dev_list_request(void)
{
	send_nl_request(0, 0, NL_VF_GET_DEV_RQ);
}


static int vfd_nl_connect_init(void)
{
	int err;

	err = cn_add_callback(&vfd_cn_id, vfd_net_name, vfd_stats_callback);
	if (err)
		goto err_out;


	pr_debug("callback initialized with id={%u.%u}\n",
		vfd_cn_id.idx, vfd_cn_id.val);

    err_out:
	if (nls && nls->sk_socket)
		sock_release(nls->sk_socket);

	return err;
}

		 


/*
*  adds net_device name will use supplied pf, vf parameters
   and bdf "0000:02:10.1" will be used as an alias
   
   returns 0 if everything is ok > 0 othervise
  
*/

/**
 * Add net_device.
 *
 * @param pf
 *   The port identifier.
 * @param vf
 *   VF id.
 * @param bdf
 *   PCI BDF in 0000:05:02:0 format used as an alias
 *	 no alias creaed if bdf == NULL
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *vf* or *bdf* invalid.
 */
static int add_vfd_net(int pf, int vf, char * bdf, int len)
{
	char *alias;
	struct net_device *netdev;
	struct vfd_priv *priv;
	int result, ret = -ENOMEM;
	spinlock_t *lock;
	
	char devname[12];

	
	if (vf == MAX_VF - 1)
		sprintf(devname, "vfd_p%d", pf);
	else
		sprintf(devname, "vfd_p%df%d", pf, vf);
	
	
	if (vfd_netdevs[pf][vf] != NULL){
		pr_warning("vfd-net: add_vfd_net: port: %d, vf: %d already exist\n", pf, vf);
		return 0;
	}
		
	/* Allocate the devices */	
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,17,0)
	netdev = alloc_netdev(sizeof(struct vfd_priv), devname,
			vfd_init);
#else
	netdev = alloc_netdev(sizeof(struct vfd_priv), devname,
			NET_NAME_UNKNOWN, vfd_init);
#endif


	if (netdev == NULL) {
		pr_err("vfd-net: alloc_netdev error: port: %d, vf: %d %s %d\n", pf, vf, __FUNCTION__, __LINE__);	
		goto out;
	}

 
	if(netdev->ifalias){
		kfree(netdev->ifalias); 
		netdev->ifalias = NULL; 
	}


	if(bdf) {
		alias = krealloc(netdev->ifalias, len + 1, GFP_KERNEL); 
		if (!alias){
			pr_err("krealloc error: port: %d, vf: %d, %s %d\n", pf, vf, __FUNCTION__, __LINE__ );	
			ret = -ENOMEM;
			goto out; 
		}
		
		netdev->ifalias = alias; 
		memcpy(netdev->ifalias, bdf, len); 
		netdev->ifalias[len] = 0; 
		
		pr_debug("ALIAS = %s\n", netdev->ifalias);
	}
	
	
	ret = -ENODEV;

	priv = netdev_priv(netdev);
	lock = &priv->lock;
	spin_lock(lock);
	
	priv = netdev_priv(netdev);
	priv->netdev = netdev;		
	vfd_netdevs[pf][vf] = netdev;
	
	spin_unlock(lock);
		
	pr_info("adding vfd_net: %s\n", netdev->name);
	pr_debug("vfd-net: Storing netdev, port: %d, vf: %d\n", pf, vf);	
				
	result = register_netdev(netdev);
	
	if (result)
		pr_err("vfd-net: error %i registering device \"%s\"\n",
			result, netdev->name);
	else
		ret = 0;
			
			
    out:
	
	
	if (ret) {
		free_netdev(netdev);
		if(netdev->ifalias) 
			kfree(netdev->ifalias);
		
		vfd_netdevs[pf][vf] = NULL;
	}
	
	return ret;
}


/**
 * Add net_device.
 *
 * @param pf
 *   The port identifier.
 * @param vf
 *   VF id.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *vf* or *bdf* invalid.
 */
static int delete_vfd_net(int pf, int vf)
{
	struct net_device *netdev;
	//spinlock_t *lock;
	//struct vfd_priv *priv;
	
	if (pf < 0 || pf > MAX_PF || vf < 0 || vf > MAX_VF) {
		pr_err("vfd_net: invalid vfd_net, pf: %d, vf: %d\n", pf, vf);
		return -EINVAL;
	}
	
	netdev = vfd_netdevs[pf][vf];
				
	if (!netdev) {
		pr_err("vfd_net: pf: %d, vf: %d doesn't exist\n", pf, vf);
		return -ENOMEM;
	}

	//priv = netdev_priv(netdev);
	//lock = &priv->lock;
	//spin_lock(lock);
	
	unregister_netdev(netdev);
				
	if(netdev->ifalias){
		kfree(netdev->ifalias);
		netdev->ifalias = NULL;
	}
				
	pr_info("removing vfd_net: %s\n", netdev->name);
	
	
	free_netdev(netdev);
	
	vfd_netdevs[pf][vf] = NULL;
	
	//spin_unlock(lock);
		
	return 0;
}


static void vfd_tx_timeout(struct net_device *dev);


int vfd_open(struct net_device *dev)
{
	memcpy(dev->dev_addr, "\0SNUL0", ETH_ALEN);		
	dev->dev_addr[ETH_ALEN-1]++; /* \0SNUL1 */
	
	netif_start_queue(dev);
	return 0;
}


int vfd_release(struct net_device *dev)
{
    netif_stop_queue(dev); /* can't transmit any more */
	return 0;
}

	  
int vfd_config(struct net_device *dev, struct ifmap *map)
{
	return 0;
}




int vfd_tx(struct sk_buff *skb, struct net_device *dev)
{
	return NETDEV_TX_OK;
}


void vfd_tx_timeout (struct net_device *dev)
{
	return;
}




int vfd_ioctl(struct net_device *dev, struct ifreq *rq, int cmd)
{
	printk("vfd_ioctl, cmd: %d\n", cmd);
	return 0;
}


struct net_device_stats *vfd_stats(struct net_device *dev)
{
	struct vfd_priv *priv = netdev_priv(dev);
	int port, vf, fields;
	struct pci_dev *pdev;
	
	//sscanf(dev->name, "vfd%dp%df%d", &bus, &port, &vf);
	fields = sscanf(dev->name, "vfd_p%df%d", &port, &vf);
	
	if(fields == 1)
		vf = MAX_VF - 1;	// PF
	
	pdev = priv->pci_dev;
	
	
	if (pdev)
		pr_debug("Getting Stats: %s, %d, %d, %s\n", dev->name, port, vf, dev_name(&pdev->dev));
	else
		pr_debug("Getting stats: %s, %d, %d, %d\n", dev->name, port, vf, netif_carrier_ok(dev));
	
	send_stats_request(port, vf);

	return &priv->stats;
}


/*
 * This function is called to fill up an eth header, since arp is not
 * available on the interface
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0))
int vfd_rebuild_header(struct sk_buff *skb)
{
	struct ethhdr *eth = (struct ethhdr *) skb->data;
	struct net_device *dev = skb->dev;
    
	memcpy(eth->h_source, dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest, dev->dev_addr, dev->addr_len);
	eth->h_dest[ETH_ALEN-1]   ^= 0x01;   
	return 0;
}
#endif /* < 4.1.0  */


int vfd_header(struct sk_buff *skb, struct net_device *dev,
                unsigned short type, const void *daddr, const void *saddr,
                unsigned len)
{
	struct ethhdr *eth = (struct ethhdr *)skb_push(skb,ETH_HLEN);

	eth->h_proto = htons(type);
	memcpy(eth->h_source, saddr ? saddr : dev->dev_addr, dev->addr_len);
	memcpy(eth->h_dest,   daddr ? daddr : dev->dev_addr, dev->addr_len);
	eth->h_dest[ETH_ALEN-1]   ^= 0x01;   
	return (dev->hard_header_len);
}




/*
 * The "change_mtu" method is usually not needed.
 * If you need it, it must be like this.
 */
int vfd_change_mtu(struct net_device *dev, int new_mtu)
{
	unsigned long flags;
	struct vfd_priv *priv = netdev_priv(dev);
	spinlock_t *lock = &priv->lock;
    
	
	pr_debug("Setting new MTU: %d\n", new_mtu);
	/* check ranges */
	if ((new_mtu < 68) || (new_mtu > 1500))
		return -EINVAL;
	
	/*
	 * Do anything you need, and the accept the value
	 */
	spin_lock_irqsave(lock, flags);
	dev->mtu = new_mtu;
	spin_unlock_irqrestore(lock, flags);
	return 0; /* success */
}



static const struct header_ops vfd_header_ops = {
    .create  = vfd_header,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0))
    .rebuild = vfd_rebuild_header
#endif /* < 4.1.0  */
};


static const struct net_device_ops vfd_netdev_ops = {
	.ndo_open            = vfd_open,
	.ndo_stop            = vfd_release,
	.ndo_start_xmit      = vfd_tx,
	.ndo_do_ioctl        = vfd_ioctl,
	.ndo_set_config      = vfd_config,
	.ndo_get_stats       = vfd_stats,
	.ndo_change_mtu      = vfd_change_mtu,
	.ndo_tx_timeout      = vfd_tx_timeout
};



void vfd_init(struct net_device *dev)
{
	struct vfd_priv *priv;

	ether_setup(dev); /* assign some of the fields */
	dev->watchdog_timeo = timeout;
	dev->netdev_ops = &vfd_netdev_ops;
	dev->header_ops = &vfd_header_ops;

	dev->flags           |= IFF_NOARP;
	dev->features        |= NETIF_F_HW_CSUM;

	priv = netdev_priv(dev);

	memset(priv, 0, sizeof(struct vfd_priv));
	spin_lock_init(&priv->lock);
}


static void update_vfd_net(void)
{
	delete_all_vfd_devs();
	send_get_dev_list_request();
}


static void vfd_cleanup(void)
{
	int x, y;
	
	for (x = 0; x < MAX_PF; x++) {
		
		for (y = 0; y < MAX_VF; y++) {
			
			if(vfd_netdevs[x][y] != NULL){
				
				delete_vfd_net(x, y);
			}
		}
	}
				
	cn_del_callback(&vfd_cn_id);	
	pr_info("deleting callback:\n");
	
	if (nls && nls->sk_socket) {
		sock_release(nls->sk_socket);
		pr_info("nl socket released:\n");
	}
	return;
}


static int __init vfd_net_mod_init(void)
{
	printk(KERN_INFO "%s: - version %s\n", vfd_net_name, vfd_net_version);
	printk(KERN_INFO "%s\n", vfd_net_copyright);

	vfd_nl_connect_init();

	update_vfd_net();
	 
	return 0;
}


static void __exit vfd_net_mod_exit(void)
{
	vfd_cleanup();
}


module_init(vfd_net_mod_init);
module_exit(vfd_net_mod_exit);

