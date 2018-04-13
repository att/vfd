// vi: sw=4 ts=4 noet:
/*
	Mnemonic:	sriov.h
	Abstract: 	Main header file for vfd.
				Original name was sriov daemon, so some references to that remain.

	Date:		February 2016
	Authors:	Alex Zelezniak (original code)
				E. Scott Daniels

	Mods:		2016 18 Nov - Reorganised to group defs, structs, globals and protos 
					rather than to have them scattered.
				22 Mar 2017 - Set the jumbo frame flag in the default dev config.
					Fix comment in same initialisation.
				16 May 2017 - Add flow control flag constant.
				10 Oct 2017 - Change set_mirror proto.
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
#include <sys/resource.h>

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
#include <rte_bus_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>
#include <rte_spinlock.h>
#include <rte_version.h>

#if VFD_KERNEL
#include "vfd_nl.h"
#endif

#include <vfdlib.h>

#include "vfd_bnxt.h"
#include "vfd_ixgbe.h"
#include "vfd_i40e.h"
#include "vfd_mlx5.h"


// ---------------------------------------------------------------------------------------
#define SET_ON				1		// on/off parm constants
#define SET_OFF				0
#define FORCE				1

#define KEEP_DEFAULT		0		// keep default mac when clearing
#define RESET_DEFAULT		1		// reset default mac with a random address

#define VF_VAL_MCAST		0		// constants passed to get_vf_value()
#define VF_VAL_BCAST		1
#define VF_VAL_MSPOOF		2
#define VF_VAL_VSPOOF		3
#define VF_VAL_STRIPVLAN	4
#define VF_VAL_UNTAGGED		5
#define VF_VAL_UNUCAST		6
#define VF_VAL_STRIPCVLAN	7

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

#define TC_4PERQ_MODE	0		// bool flag passed to qos funcitons indicating 4 or 8 mode
#define TC_8PERQ_MODE	1

#define RTE_PORT_ALL            (~(portid_t)0x0)
#define IXGBE_RXDCTL_VME        0x40000000 /* VLAN mode enable */

#define VFD_NIANTIC		0x1
#define VFD_FVL25		0x2
#define VFD_BNXT		0x3
#define VFD_MLX5		0x4

#define VF_LINK_ON	1
#define VF_LINK_OFF	-1
#define VF_LINK_AUTO 0

#define TOGGLE(i) ((i+ 1) & 1)
//#define TV_TO_US(tv) ((tv)->tv_sec * 1000000 + (tv)->tv_usec)

#define IF_PORT_INFO "SRIOV_port_info"

#define simpe_atomic_swap(var, newval)  __sync_lock_test_and_set(&var, newval)
#define barrier()                       __sync_synchronize()

typedef char const* const_str;			// a pointer to a constant string
typedef unsigned char __u8;
typedef unsigned int uint128_t __attribute__((mode(TI))); 


#define __UINT128__

#define PFS_ONLY	1		// display only the PF stats (!PFS_ONLY displays VF stats too)
#define ALL_PFS		-1		// display stats for all PFs

#define MAX_VF_VLANS 64
#define MAX_VF_MACS  64
#define MAX_PF_VLANS 64		// vlan count across all PFs cannot exceed
#define MAX_PF_MACS  128	// mac count across all PFs cannot exceed

typedef uint8_t  lcoreid_t;
typedef uint16_t  portid_t;
typedef uint16_t queueid_t;
typedef uint16_t streamid_t;

#define MAX_QUEUE_ID ((1 << (sizeof(queueid_t) * 8)) - 1)

#define VFN2MASK(N) (1U << (N))

#define BUF_SIZE 1024

#define ENABLED		1
#define DISABLED	0
								// port flags
#define PF_LOOPBACK	0x01		// loopback is enabled
#define PF_OVERSUB	0x02		// allow qos oversubscription
#define PF_FC_ON	0x04		// turn flow control on for port
#define PF_PROMISC	0x08		// set promisc for the port when high


#define VFD_MAX_CPU	5			// CPU% threshold

/*
	Provides a static port configuration struct with defaults.
*/
static const struct rte_eth_conf port_conf_default = {
	.rxmode = {
		.max_rx_pkt_len = 9000,
		.jumbo_frame 		= ENABLED,
		.header_split   = 0, /**< Header Split disabled */
		.hw_ip_checksum = 1,		// enable hw to do the checksum
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.hw_vlan_strip  = 1, /**< VLAN strip enabled. */
		.hw_vlan_extend = 0, /**< Extended VLAN disabled. */
		.hw_strip_crc   = 1, /**< CRC stripped by hardware */
		},
		.txmode = {
			//.hw_vlan_insert_pvid = 1,
      .mq_mode = ETH_MQ_TX_NONE,
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
	int     strip_ctag;          
	int     strip_stag;          
	int     insert_stag;         
	int     insert_ctag;         
	int     vlan_anti_spoof;      // if use VLAN filter then set VLAN anti spoofing
	int     mac_anti_spoof;       // set MAC anti spoofing when MAC filter is in use
	int     allow_bcast;
	int     allow_mcast;
	int     allow_un_ucast;
	int     allow_untagged;
	double  rate;
	double  min_rate;
	int     link;                 /* -1 = down, 0 = mirror PF, 1 = up  */
	int     num_vlans;
	int     num_macs;
	int		first_mac;				// index of first mac in list (1 if VF has not changed their mac, 0 if they've pushed one down)
	int     vlans[MAX_VF_VLANS];
	char    macs[MAX_VF_MACS][18];	// human readable MAC strings
	int     rx_q_ready;
	int 	default_mac_set;

	uid_t	owner;					// user id which 'owns' the VF (owner of the config file from stat())
	char*	start_cb;				// user commands driven just after initialisation and just before termination
	char*	stop_cb;
	char*	config_name;			// name given in config file for delete confirmation
	uint8_t	qshares[MAX_TCS];		// percentage of each queue (TC) that has been set in the config for the vf
};


/*
	Represent a mirror added to the PF.
*/
struct mirror_s
{
	uint8_t	target;		// vf where trafiic is being sent
	int dir;			// traffic direction: in, out, both, off
	uint8_t	id;			// the id we assigned to the mirror (size limited by dpdk lib calls)
};


/*
	Manages information for a single NIC port. Each port may have up to MAX_VFS configured.
*/
typedef struct sriov_port_s
{
	int			flags;					// PF_ constants (enable loopback etc.)
	int     	rte_port_number;		// the real device number (as known by rte functions)
	char    	name[64];				// human readable name (probalby unused)
	char    	pciid[64];				// the ID of the device
	int     	last_updated;			// flags a change
	int     	mtu;
	int     	num_mirrors;
	int			nvfs_config;			// actual number of configured vfs; could be less than max
	int			ntcs;					// number traffic clases (must be 4 or 8)
	int     	num_vfs;					// number of VF spaces in the list used, NOT the total allocated on the port
	struct  	mirror_s mirrors[MAX_VFS];	// mirror info for each VF
	struct  	vf_s vfs[MAX_VFS];
	tc_class_t*	tc_config[MAX_TCS];		// configuration information (max/min lsp/gsp) for the TC	(set from config)
	int*		vftc_qshares;			// queue percentages arranged by vf/tc (computed with each add/del of a vf)
	uint8_t		tc2bwg[MAX_TCS];		// maps each TC to a bandwidth group (set from config info)
	
	// will keep PCI First VF offset and Stride here
	uint16_t vf_offset;
	uint16_t vf_stride;
} sriov_port_t;

/*
	Overall configuration anchor.
*/
typedef struct sriov_conf_c
{
	int     num_ports;						// number of ports actually used in ports array
	struct sriov_port_s ports[MAX_PORTS];	// ports; CAUTION: order may not be device id order
	rte_spinlock_t update_lock;				// we lock the config during update and deployment
	void*	mir_id_mgr;						// reference point for the id manager to allocate mirror ids
} sriov_conf_t;


enum print_warning {
	ENABLED_WARN = 0,
	DISABLED_WARN
};

struct pstat
{
  u_int64_t pcount;
  u_int64_t bcount;
  u_int64_t pcount_before;

  struct timeval startTime;
  struct timeval endTime;
};

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


// ----------- inline expansions ---------------------------------------------------------------------

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


// ---------------------- globals ------------------------------------------------------------------
const char* version;
sriov_conf_t* running_config;		// global so that callbacks can access
int port2config_map[MAX_PORTS];		// map hardware port number to our config array index

int terminated;				// set when a signal is received -- causes main loop to gracefully exit

int debug;
int traceLevel;			  // NORMAL == 5 level, INFO == 6  (deprecated)
int useSyslog;         // 0 send messages to stdout			(deprecated)
int logFacility;       // LOG_LOCAL0


char *prog_name;
char *fname;

int     n_ports;			// number of ports reported by hw/dpdk
struct ether_addr addr;

struct pstat st;
struct timeval startTime;
struct timeval endTime;


uint32_t spoffed[MAX_PORTS]; 		// # of spoffed packets per PF

struct rq_entry *rq_list;			// reset queue list of VMs we are waiting on queue ready bits for

// ---------------------- prototypes ------------------------------------------------------------------
void port_mtu_set(portid_t port_id, uint16_t mtu);

void rx_vlan_strip_set_on_vf(portid_t port_id, uint16_t vf_id, int on);
void rx_cvlan_strip_set_on_vf(portid_t port_id, uint16_t vf_id, int on);
void tx_vlan_insert_set_on_vf(portid_t port_id, uint16_t vf_id, int vlan_id);
void tx_cvlan_insert_set_on_vf(portid_t port_id, uint16_t vf_id, int vlan_id);
int  rx_vft_set(portid_t port_id, uint16_t vlan_id, int on);
void init_port_config(void);

int get_split_ctlreg( portid_t port_id, uint16_t vf_id );
int set_mirror( portid_t port_id, uint32_t vf, uint8_t id, uint8_t target, uint8_t direction );
int set_mirror_wrp( portid_t port_id, uint32_t vf, uint8_t id, uint8_t target, uint8_t direction );
void set_queue_drop( portid_t port_id, int state );
void set_split_erop( portid_t port_id, uint16_t vf_id, int state );

void set_vf_allow_bcast(portid_t port_id, uint16_t vf_id, int on);
void set_vf_allow_mcast(portid_t port_id, uint16_t vf_id, int on);
void set_vf_allow_un_ucast(portid_t port_id, uint16_t vf_id, int on);
void set_vf_allow_untagged(portid_t port_id, uint16_t vf_id, int on);

void set_vf_rx_vlan(portid_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on);
void set_vf_rx_mac(portid_t port_id, const char* mac, uint32_t vf, uint8_t on);
void set_vf_default_mac( portid_t port_id, const char* mac, uint32_t vf );

void set_vf_vlan_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on);
void set_vf_mac_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on);

int set_vf_rate_limit(portid_t port_id, uint16_t vf, uint16_t rate, uint64_t q_msk);
int set_vf_min_rate(portid_t port_id, uint16_t vf, uint16_t rate, uint64_t q_msk);
int set_vf_link_status(portid_t port_id, uint16_t vf, int status);

void nic_stats_clear(portid_t port_id);
int nic_stats_display(uint16_t port_id, char * buff, int blen);
int vf_stats_display(uint16_t port_id, uint32_t pf_ari, int vf, char * buff, int bsize);
int port_xstats_display(uint16_t port_id, char * buff, int bsize);
int dump_all_vlans(portid_t port_id);
void ping_vfs(portid_t port_id, int vf);

int port_init(uint16_t port, struct rte_mempool *mbuf_pool, int hw_strip_crc, sriov_port_t *pf );
void tx_set_loopback(portid_t port_id, u_int8_t on);

void ether_aton_r(const char *asc, struct ether_addr * addr);
int xdigit(char c);

void print_port_errors(struct rte_eth_stats et_stats, int col);

double timeDelta (struct timeval * now, struct timeval * before);

void daemonize( char* pid_fname );
void detachFromTerminal( void );
void traceLog(int eventTraceLevel, const char * file, int line, const char * format, ...);
int readConfigFile(char *fname);
void dump_sriov_config( sriov_conf_t* config);
void dump_dev_info( int num_ports );
int update_ports_config(void);
int cmp_vfs (const void * a, const void * b);
void disable_default_pool(portid_t port_id);

int lsi_event_callback(uint16_t port_id, enum rte_eth_event_type type, void *param, void* data );
//int lsi_event_callback(uint16_t port_id, enum rte_eth_event_type type, void *param, void *ret_param);
void restore_vf_setings(uint16_t port_id, int vf);

// callback validation support
int valid_mtu( int port, int mtu );
int valid_vlan( int port, int vfid, int vlan );
int get_vf_setting( int portid, int vf, int what );
int suss_loopback( int port );

struct sriov_port_s *suss_port( int portid );
struct vf_s *suss_vf( int port, int vfid );
struct mirror_s*  suss_mirror( int port, int vfid );


void add_refresh_queue(u_int8_t port_id, uint16_t vf_id);
void process_refresh_queue(void);
int is_rx_queue_on(portid_t port_id, uint16_t vf_id, int* mcounter );

int vfd_update_nic( parms_t* parms, sriov_conf_t* conf );
int vfd_init_fifo( parms_t* parms );
//int is_valid_mac_str( char* mac );
char*  gen_stats( sriov_conf_t* conf, int pf_only, int pf );
int get_nic_type(portid_t port_id);
int get_mac_antispoof( portid_t port_id );
int get_max_qpp( uint32_t port_id );
int get_num_vfs( uint32_t port_id );
void discard_pf_traffic( portid_t portid );

void log_port_state( struct sriov_port_s* port, const_str msg );

// ---- mac support ---------------------------------------
extern int mac_init( void );
extern int add_mac( int port, int vfid, char* mac );
extern int can_add_mac( int port, int vfid, char* mac );
extern int clear_macs( int port, int vfid, int assign_random );
extern int push_mac( int port, int vfid, char* mac );
extern int set_macs( int port, int vfid );

//-- testing --
extern void set_fc_on( portid_t pf, int force );
extern void set_fd_off( portid_t port_id );
extern void set_rx_pbsize( portid_t port_id );

//------- queue support -------------------------
void set_pfrx_drop(portid_t port_id, int state );

// --- tools --------------------------------------------
extern int stricmp(const char *s1, const char *s2);


// ---- new qos, merge up after initial testing ----
void gen_port_qshares( sriov_port_t *port );
int check_qs_oversub( struct sriov_port_s* port, uint8_t *qshares );
int check_qs_spread( struct sriov_port_s* port, uint8_t* qshares );

// --- qos hard coded nic funcitons that need to move to dpdk
void qos_set_credits( portid_t pf, int mtu, int* rates, int tc8_mode );
extern void qos_enable_arb( portid_t pf );
extern void qos_set_tdplane( portid_t pf, uint8_t* pctgs, uint8_t *bwgs, int ntcs, int mtu );
extern void qos_set_txpplane( portid_t pf, uint8_t* pctgs, uint8_t *bwgs, int ntcs, int mtu );
extern void mlx5_set_vf_tcqos( sriov_port_t *port, uint32_t link_speed );

void chk_cpu_usage( char* msg_type, double threshold );

//------- these are hacks in the dpdk library and we  must find a good way to rid ourselves of them ------
struct rth_eth_dev;
extern void ixgbe_configure_dcb(struct rte_eth_dev *dev);
extern void scott_ixgbe_configure_dcb_pctgs( struct rte_eth_dev *dev );


#endif /* _SRIOV_H_ */
