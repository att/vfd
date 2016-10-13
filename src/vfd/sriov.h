// vi: sw=4 ts=4 noet:
/*
	Mnemonic:	sriov.h 
	Abstract: 	Main header file for vfd.
				Original name was sriov daemon, so some references to that remain.

	Date:		February 2016
	Authors:	Alex Zelezniak (original code)
				E. Scott Daniels
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
#include <rte_spinlock.h>

#include "../lib/dpdk/drivers/net/ixgbe/base/ixgbe_mbx.h"

#define RX_RING_SIZE 128
#define TX_RING_SIZE 64
#define NUM_MBUFS 512
#define MBUF_SIZE (800 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 125


#define BURST_SIZE 32
#define MAX_VFS    254
#define MAX_QUEUES	128			// max supported queues
#define MAX_PORTS  16
#define MAX_TCS		8			// max number of TCs possible
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
#define MAX_PF_VLANS 64		// vlan count across all PFs cannot exceed
#define MAX_PF_MACS  128	// mac count across all PFs cannot exceed

typedef uint8_t  lcoreid_t;
typedef uint8_t  portid_t;
typedef uint16_t queueid_t;
typedef uint16_t streamid_t;

#define MAX_QUEUE_ID ((1 << (sizeof(queueid_t) * 8)) - 1)

#define VFN2MASK(N) (1U << (N))

#define BUF_SIZE 1024

#define ENABLED		1
#define DISABLED	0
								// port flags
#define PF_LOOPBACK	0x01		// loopback is enabled
#define PF_OVERSUB	0x02

/*
	Provides a static port configuration struct with defaults.
*/
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
		.lsc = ENABLED, 		// < lsc interrupt feature enabled
	},
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

  

/*
	Manages information for a single virtual function (VF).
*/
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

	uid_t	owner;					// user id which 'owns' the VF (owner of the config file from stat())
	char*	start_cb;				// user commands driven just after initialisation and just before termination
	char*	stop_cb;
	uint8_t	tc_pctgs[MAX_TCS];		// percentage of the TC that the VF has been allocated (configured)
};


struct mirror_s
{
  int     vlan;
  int     vf_id;
};


/*
	Manages information for a single NIC port. Each port may have up to MAX_VFS configured.
*/
typedef struct sriov_port_s
{
	int		flags;					// PF_ constants
	int     rte_port_number;
	char    name[64];
	char    pciid[64];
	int     last_updated;
	int     mtu;
	int     num_mirrors;
	int		nvfs_config;			// actual number of configured vfs; could be less than max
	int		ntcs;					// number traffic clases (must be 4 or 8)
	//int		enable_loopback;		// allow VM-VM traffic looping back through the NIC
	int     num_vfs;
	struct  mirror_s mirror[MAX_VFS];
	struct  vf_s vfs[MAX_VFS];
	uint8_t	tc_pctgs[MAX_TCS];		// percentage of total bandwidth for each traffic class
	uint8_t	tc2bwg[MAX_TCS];		// maps TCs to bandwidth groups
} sriov_port_t;

/*
	Overall configuration anchor.
*/
typedef struct sriov_conf_c
{
  int     num_ports;						// number of ports actually used in ports
  struct sriov_port_s ports[MAX_PORTS];
} sriov_conf_t;


sriov_conf_t* running_config;		// global so that callbacks can access

int rte_config_portmap[MAX_PORTS];

enum print_warning {
	ENABLED_WARN = 0,
	DISABLED_WARN
};



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


void rx_vlan_strip_set_on_vf(portid_t port_id, uint16_t vf_id, int on);
void tx_vlan_insert_set_on_vf(portid_t port_id, uint16_t vf_id, int vlan_id);
int  rx_vft_set(portid_t port_id, uint16_t vlan_id, int on);
void init_port_config(void);

int get_split_ctlreg( portid_t port_id, uint16_t vf_id );
void set_queue_drop( portid_t port_id, int state );
void set_split_erop( portid_t port_id, uint16_t vf_id, int state );

void set_vf_allow_bcast(portid_t port_id, uint16_t vf_id, int on);
void set_vf_allow_mcast(portid_t port_id, uint16_t vf_id, int on);
void set_vf_allow_un_ucast(portid_t port_id, uint16_t vf_id, int on);
void set_vf_allow_untagged(portid_t port_id, uint16_t vf_id, int on);

void set_vf_rx_vlan(portid_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on);
void set_vf_rx_mac(portid_t port_id, const char* mac, uint32_t vf, uint8_t on);

void set_vf_vlan_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on);
void set_vf_mac_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on);

int set_vf_rate_limit(portid_t port_id, uint16_t vf, uint16_t rate, uint64_t q_msk);

void nic_stats_clear(portid_t port_id);
int nic_stats_display(uint8_t port_id, char * buff, int blen);
int vf_stats_display(uint8_t port_id, uint32_t pf_ari, int vf, char * buff, int bsize);
int dump_vlvf_entry(portid_t port_id);

int port_init(uint8_t port, struct rte_mempool *mbuf_pool);
void tx_set_loopback(portid_t port_id, u_int8_t on);

int terminated;				// set when a signal is received -- causes main loop to gracefully exit

int debug;
int traceLevel;			  // NORMAL == 5 level, INFO == 6  (deprecated)
int useSyslog;         // 0 send messages to stdout			(deprecated)
int logFacility;       // LOG_LOCAL0


char *prog_name;
char *fname;

int     n_ports;			// number of ports reported by hw/dpdk
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

void daemonize( char* pid_fname );
void detachFromTerminal( void );
void traceLog(int eventTraceLevel, const char * file, int line, const char * format, ...);
int readConfigFile(char *fname);
void dump_sriov_config( sriov_conf_t* config);
void dump_dev_info( int num_ports );
int update_ports_config(void);
int cmp_vfs (const void * a, const void * b);
void disable_default_pool(portid_t port_id);


void lsi_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param);
void vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param);
void restore_vf_setings(uint8_t port_id, int vf);

// callback validation support 
int valid_mtu( int port, int mtu );
int valid_vlan( int port, int vfid, int vlan );

// will keep PCI First VF offset and Stride here
uint16_t vf_offfset;
uint16_t vf_stride;

// this array holds # of spoffed packets per PF
uint32_t spoffed[MAX_PORTS];
/*
	Manages a reset for a port/vf pair. These are queued when a reset is received
	by callback/mbox message until the VF's queues are ready.
*/
struct rq_entry 
{
	struct rq_entry* next;		// link references
	struct rq_entry* prev;

	uint8_t	port_id;
	uint16_t vf_id;
	uint8_t enabled;
	int		mcounter;			// message counter so as not to flood the log
};

struct rq_entry *rq_list;		// reset queue list of VMs we are waiting on queue ready bits for

void add_refresh_queue(u_int8_t port_id, uint16_t vf_id);
void process_refresh_queue(void);
int is_rx_queue_on(portid_t port_id, uint16_t vf_id, int* mcounter );

// ------------ qos ------------
extern int enable_dcb_qos( sriov_port_t *port, int* pctgs, int tc8_mode, int option );


#endif /* _SRIOV_H_ */
