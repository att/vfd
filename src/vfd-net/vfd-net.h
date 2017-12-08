

#ifndef __devexit_p
	#define __devexit_p(x) &(x)
#endif

#ifndef __devexit
#define __devexit
#endif


#define NL_PF_RESP_OK 		0x0
#define NL_PF_RESP_ERR 		0x1


#define NL_VF_STATS_RQ 		0x1
#define NL_VF_GETINFO_RQ 	0x2
#define NL_VF_SETMTU_RQ 	0x4
#define NL_VF_IFUP_RQ 		0x8
#define NL_VF_IFDOWN_RQ		0x10

#define NL_PF_RES_DEV_RQ	0x0
#define NL_VF_GET_DEV_RQ	0x100
#define NL_PF_ADD_DEV_RQ	0x400
#define NL_PF_DEL_DEV_RQ	0x800
#define NL_PF_UPD_DEV_RQ	0x1000

#define MAX_PF	16
#define MAX_VF	254

#define PCIADDR_LEN	12	

	
struct dev_stats
{
	__u32	rx_packets;		/* total packets received	*/
	__u32	tx_packets;		/* total packets transmitted	*/
	__u32	rx_bytes;		/* total bytes received 	*/
	__u32	tx_bytes;		/* total bytes transmitted	*/
	__u32	rx_errors;		/* bad packets received		*/
	__u32	tx_errors;		/* packet transmit problems	*/
	__u32	rx_dropped;		/* no space in linux buffers	*/
	__u32	tx_dropped;		/* no space available in linux	*/
	__u32	multicast;		/* multicast packets received	*/
	__u32	collisions;

	/* detailed rx_errors: */
	__u32	rx_length_errors;
	__u32	rx_over_errors;		/* receiver ring buff overflow	*/
	__u32	rx_crc_errors;		/* recved pkt with crc error	*/
	__u32	rx_frame_errors;	/* recv'd frame alignment error */
	__u32	rx_fifo_errors;		/* recv'r fifo overrun		*/
	__u32	rx_missed_errors;	/* receiver missed packet	*/

	/* detailed tx_errors */
	__u32	tx_aborted_errors;
	__u32	tx_carrier_errors;
	__u32	tx_fifo_errors;
	__u32	tx_heartbeat_errors;
	__u32	tx_window_errors;

	/* for cslip etc */
	__u32	rx_compressed;
	__u32	tx_compressed;
};


struct dev_info
{
	__u32	link_state;
	__u32	link_speed;
	__u32	link_duplex;
	__u32	mtu;
	__u32	n_queues;
	char * mac;
	struct dev_stats *stats;
};

struct vfd_nl_message {
	uint32_t req;
	uint32_t resp;
	uint32_t port;
	uint32_t vf;
	char * pciaddr;
	struct dev_info *info;
};






