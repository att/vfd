// vi: sw=4 ts=4:
/*
	Mnemonic:	sriov.h 
	Abstract: 	Main header file for vfd.
				Original name was sriov daemon, so some references to that remain.

	Date:		February 2016
	Authors:	Alex Zelezniak (original code)
*/

#ifndef _SRIOV_H_
#define _SRIOV_H_


#define _GNU_SOURCE

#include <inttypes.h>


#include <time.h>
#include <math.h>

#include <stdarg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <inttypes.h>
#include <signal.h>
#include <syslog.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

// apt-get install libbsd-dev needed
//#include <sys/queue.h>

#include <rte_alarm.h>
#include <rte_common.h>
#include <rte_byteorder.h>
#include <rte_debug.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include <libconfig.h>

#include "../dpdk/drivers/net/ixgbe/base/ixgbe_mbx.h"

#define RX_RING_SIZE 64
#define TX_RING_SIZE 64
#define NUM_MBUFS 512
#define MBUF_SIZE (800 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 125


#define BURST_SIZE 32
#define MAX_VFS    254
#define MAX_PORTS  16
#define RESTORE_DELAY 2


#define RTE_PORT_ALL            (~(portid_t)0x0)
#define IXGBE_RXDCTL_VME        0x40000000 /* VLAN mode enable */

//#define STATS_FILE "/tmp/sriov_stats"


//#define timeval_to_ms(timeval)  (timeval.tv_sec * 1000) + (timeval.tv_usec / 1000)

#define TOGGLE(i) ((i+ 1) & 1)
//#define TV_TO_US(tv) ((tv)->tv_sec * 1000000 + (tv)->tv_usec)

#define IF_PORT_INFO "SRIOV_port_info"

#define simpe_atomic_swap(var, newval)  __sync_lock_test_and_set(&var, newval)
#define barrier()                       __sync_synchronize()




#define TRACE_EMERG       0, __FILE__, __LINE__       /* system is unusable */
#define TRACE_ALERT       1, __FILE__, __LINE__       /* action must be taken immediately */
#define TRACE_CRIT        2, __FILE__, __LINE__       /* critical conditions */
#define TRACE_ERROR       3, __FILE__, __LINE__       /* error conditions */
#define TRACE_WARNING     4, __FILE__, __LINE__       /* warning conditions */
#define TRACE_NORMAL      5, __FILE__, __LINE__       /* normal but significant condition */
#define TRACE_INFO        6, __FILE__, __LINE__       /* informational */
#define TRACE_DEBUG       7, __FILE__, __LINE__       /* debug-level messages */


typedef unsigned char __u8;
typedef unsigned int uint128_t __attribute__((mode(TI)));  


#define __UINT128__ 


#define MAX_VF_VLANS 64
#define MAX_VF_MACS  64

typedef uint8_t  lcoreid_t;
typedef uint8_t  portid_t;
typedef uint16_t queueid_t;
typedef uint16_t streamid_t;

#define MAX_QUEUE_ID ((1 << (sizeof(queueid_t) * 8)) - 1)

#define VFN2MASK(N) (1U << (N))


static const struct rte_eth_conf port_conf_default = {
	.rxmode = { 
		.max_rx_pkt_len = 9000,
		.jumbo_frame 		= 0, 
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 1, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.hw_vlan_strip  = 0, /**< VLAN strip enabled. */
		.hw_vlan_extend = 0, /**< Extended VLAN disabled. */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
		},
	.intr_conf = {
		.lsc = 1, /**< lsc interrupt feature enabled */
    //.lsc = 0, /**< lsc interrupt feature disabled */
	},
};


/**
 * The data structure associated with each port.
 */
struct rte_port {
	uint8_t                 enabled;    /**< Port enabled or not */
	struct rte_eth_dev_info dev_info;   /**< PCI info + driver name */
	struct rte_eth_conf     dev_conf;   /**< Port configuration. */
	struct ether_addr       eth_addr;   /**< Port ethernet address */
	struct rte_eth_stats    stats;      /**< Last port statistics */
	uint64_t                tx_dropped; /**< If no descriptor in TX ring */
	struct fwd_stream       *rx_stream; /**< Port RX stream, if unique */
	struct fwd_stream       *tx_stream; /**< Port TX stream, if unique */
	unsigned int            socket_id;  /**< For NUMA support */
	uint16_t                tx_ol_flags;/**< TX Offload Flags (TESTPMD_TX_OFFLOAD...). */
	uint16_t                tso_segsz;  /**< MSS for segmentation offload. */
	uint16_t                tx_vlan_id;/**< The tag ID */
	uint16_t                tx_vlan_id_outer;/**< The outer tag ID */
	void                    *fwd_ctx;   /**< Forwarding mode context */
	uint64_t                rx_bad_ip_csum; /**< rx pkts with bad ip checksum  */
	uint64_t                rx_bad_l4_csum; /**< rx pkts with bad l4 checksum */
	uint8_t                 tx_queue_stats_mapping_enabled;
	uint8_t                 rx_queue_stats_mapping_enabled;
	volatile uint16_t       port_status;    /**< port started or not */
	uint8_t                 need_reconfig;  /**< need reconfiguring port or not */
	uint8_t                 need_reconfig_queues; /**< need reconfiguring queues or not */
	uint8_t                 rss_flag;   /**< enable rss or not */
	uint8_t                 dcb_flag;   /**< enable dcb */
	struct rte_eth_rxconf   rx_conf;    /**< rx configuration */
	struct rte_eth_txconf   tx_conf;    /**< tx configuration */
	struct ether_addr       *mc_addr_pool; /**< pool of multicast addrs */
	uint32_t                mc_addr_nb; /**< nb. of addr. in mc_addr_pool */
	uint8_t                 slave_flag; /**< bonding slave port */
};


unsigned int itvl_idx;

struct itvl_stats 
{
  //struct port_s port_stats[2];
  struct timeval tv;
  uint64_t usecs;
  uint64_t num_pkts;
  uint64_t num_bytes;
} itvl[2];

  

struct vf_s
{
  int     num;
  int     last_updated;         
  /**
    *     no app m->ol_flags | PKT_TX_VLAN_PKT   |  app does m->ol_flags | PKT_TX_VLAN_PKT
    *     strip_stag  = 0 Y, 1 strip, 1 Y                                             | 0 NO, 1 Y, 1 Y
    *     insert_stag = 0 Y (q & qinq), xxx same as vlan filter (Y single tag only)   | 0 NO, 0 Y (q & qinq), xxx same as vlan filter (Y single tag only)
    *
   **/
  int     strip_stag;           
  int     insert_stag;          
  int     vlan_anti_spoof;      // if use VLAN filter then set VLAN anti spoofing
  int     mac_anti_spoof;       // set MAC anti spoofing when MAC filter is in use
  int     allow_bcast;
  int     allow_mcast;
  int     allow_un_ucast;
  int     allow_untagged;
  double  rate;
  int     link;                 /* -1 = down, 0 = mirror PF, 1 = up  */
  int     num_vlans;
  int     num_macs;
  int     vlans[MAX_VF_VLANS];
  char    macs[MAX_VF_MACS][18];
};


struct mirror_s
{
  int     vlan;
  int     vf_id;
};


struct sriov_port_s
{
  int     rte_port_number;
  char    name[64];
	char    pciid[64];
  int     last_updated;
  int     mtu;
  int     num_mirros;
  int     num_vfs;
  struct  mirror_s mirror[MAX_VFS];
  struct  vf_s vfs[MAX_VFS];
};


struct sriov_conf_c
{
  int     num_ports;
  struct sriov_port_s ports[MAX_PORTS];
} sriov_config;


struct reset_param_c
{
  uint32_t  port;
  uint32_t  vf;
};


struct sriov_conf_c running_config;


int rte_config_portmap[MAX_PORTS];

enum print_warning {
	ENABLED_WARN = 0,
	DISABLED_WARN
};

int port_id_is_invalid(portid_t port_id, enum print_warning warning);
int port_reg_off_is_invalid(portid_t port_id, uint32_t reg_off);


/**
 * Read/Write operations on a PCI register of a port.
 */
static inline uint32_t
port_pci_reg_read(portid_t port, uint32_t reg_off)
{
  
  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(port, &dev_info);
 
	void *reg_addr;
	uint32_t reg_v;

	reg_addr = (void *)
		((char *)dev_info.pci_dev->mem_resource[0].addr + reg_off);
	reg_v = *((volatile uint32_t *)reg_addr);
	return rte_le_to_cpu_32(reg_v);
}

#define port_id_pci_reg_read(pt_id, reg_off) \
	port_pci_reg_read(&ports[(pt_id)], (reg_off))

static inline void
port_pci_reg_write(portid_t port, uint32_t reg_off, uint32_t reg_v)
{
  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(port, &dev_info);
  
	void *reg_addr;

	reg_addr = (void *)
		((char *)dev_info.pci_dev->mem_resource[0].addr + reg_off);
	*((volatile uint32_t *)reg_addr) = rte_cpu_to_le_32(reg_v);
}

#define port_id_pci_reg_write(pt_id, reg_off, reg_value) \
	port_pci_reg_write(&ports[(pt_id)], (reg_off), (reg_value))


void port_mtu_set(portid_t port_id, uint16_t mtu);

void rx_vlan_strip_set(portid_t port_id, int on);
void rx_vlan_strip_set_on_queue(portid_t port_id, uint16_t queue_id, int on);
void rx_vlan_strip_set_on_vf(portid_t port_id, uint16_t vf_id, int on);
void rx_vlan_insert_set_on_vf(portid_t port_id, uint16_t vf_id, int vlan_id);

void rx_vlan_filter_set(portid_t port_id, int on);
void rx_vlan_all_filter_set(portid_t port_id, int on);
int  rx_vft_set(portid_t port_id, uint16_t vlan_id, int on);
void vlan_extend_set(portid_t port_id, int on);
void vlan_tpid_set(portid_t port_id, uint16_t tp_id);
void tx_vlan_set(portid_t port_id, uint16_t vlan_id);
void tx_qinq_set(portid_t port_id, uint16_t vlan_id, uint16_t vlan_id_outer);
void tx_vlan_reset(portid_t port_id);
void tx_vlan_pvid_set(portid_t port_id, uint16_t vlan_id, int on);

int set_queue_rate_limit(portid_t port_id, uint16_t queue_idx, uint16_t rate);

void dev_set_link_up(portid_t pid);
void dev_set_link_down(portid_t pid);
void init_port_config(void);


int start_port(portid_t pid);
void stop_port(portid_t pid);
void close_port(portid_t pid);


int port_is_started(portid_t port_id);
void set_vf_allow_bcast(portid_t port_id, uint16_t vf_id, int on);
void set_vf_allow_mcast(portid_t port_id, uint16_t vf_id, int on);
void set_vf_allow_un_ucast(portid_t port_id, uint16_t vf_id, int on);
void set_vf_allow_untagged(portid_t port_id, uint16_t vf_id, int on);

void set_vf_rx_vlan(portid_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on);
void set_vf_rx_mac(portid_t port_id, const char* mac, uint32_t vf, uint8_t on);

void set_vf_vlan_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on);
void set_vf_mac_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on);

int set_vf_rate_limit(portid_t port_id, uint16_t vf, uint16_t rate, uint64_t q_msk);


void get_ethertype_filter(uint8_t port_id, uint16_t index);

void nic_stats_clear(portid_t port_id);
//void nic_stats_display(uint8_t port_id);
int nic_stats_display(uint8_t port_id, char * buff, int blen);
int port_init(uint8_t port, struct rte_mempool *mbuf_pool);
void restore_vf_setings_cb(__rte_unused void *param);


int terminated;
int restart;           


u_int32_t cpu_mask;

int debug;
int traceLevel;			  // NORMAL == 5 level, INFO == 6
int useSyslog;         // 0 send messages to stdout
int logFacility;       // LOG_LOCAL0


char *prog_name;
char *fname;

int     n_ports;
struct ether_addr addr;

struct pstat
{
  u_int64_t pcount;
  u_int64_t bcount;
  u_int64_t pcount_before;

  struct timeval startTime;
  struct timeval endTime;
};

struct pstat st;

struct timeval startTime;
struct timeval endTime;


void ether_aton_r(const char *asc, struct ether_addr * addr);
int xdigit(char c);
 
void print_port_errors(struct rte_eth_stats et_stats, int col);

double timeDelta (struct timeval * now, struct timeval * before);
void runIfrate(uint8_t port, unsigned n_ports, unsigned long cpu_mask);

void daemonize(void);
void detachFromTerminal(void);
void traceLog(int eventTraceLevel, const char * file, int line, const char * format, ...);
int readConfigFile(char *fname);
void dump_sriov_config(struct sriov_conf_c config);
int update_ports_config(void);


void lsi_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param);
void vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param);
void restore_vf_setings(uint8_t port_id, int vf);
int check_mcast_mbox(uint32_t * mb);


#endif /* _SRIOV_H_ */
