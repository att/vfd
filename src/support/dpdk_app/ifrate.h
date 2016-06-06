/*
**
** AZ 2015
**
*/

#ifndef __IFRATE_H_
#define __IFRATE_H_


#define _GNU_SOURCE

#include <inttypes.h>


#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <curses.h>

//#include <pthread.h>

#include <sys/param.h>
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/socket.h>

#include <time.h>
#include <math.h>

#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>



#include <sched.h>

#include <arpa/inet.h>

#include <net/if.h>
//#include <net/ethernet.h>
#include <linux/ethtool.h>
#include <linux/sockios.h>
#include <linux/pci.h>



#include <rte_common.h>
#include <rte_log.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_spinlock.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_pci.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ring.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>



#include "utils.h"

#define timeval_to_ms(timeval)  (timeval.tv_sec * 1000) + (timeval.tv_usec / 1000)


#define simpe_atomic_swap(var, newval)  __sync_lock_test_and_set(&var, newval)
#define barrier()                       __sync_synchronize()


#define ETHER_ADDR_LEN        6
#define	ETHERTYPE_ARP		    	0x0806
#define	ETHERTYPE_IP		    	0x0800
#define	ETHERTYPE_VLAN		    0x08100
#define	ETHERTYPE_DOT1AD      0x88A8
#define	ETHERTYPE_IPV6	    	0x86DD
#define	ETHERTYPE_MPLS	    	0x8847
#define ETHERTYPE_MPLS_MULTI	0x8848

#define US_PER_MS 1000

#define RX_RING_SIZE 128
#define TX_RING_SIZE 128

#define NUM_MBUFS 8191

#define MBUF_SIZE (1600 + sizeof(struct rte_mbuf) + RTE_PKTMBUF_HEADROOM)
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32


#define IF_PORT_INFO "IFRate_port_info"
#define SHMEM_STATS_DIR "/dev/shm/"
#define PCI_IRQ_DIR "/proc/irq/"


#define C_API


#define TOGGLE(i) ((i+ 1) & 1)
#define TV_TO_US(tv) ((tv)->tv_sec * 1000000 + (tv)->tv_usec)
unsigned int itvl_idx = 0;


#define ntohs_u(A) ((((u_short)(A) & 0xff00) >> 8) | \
(((u_short)(A) & 0x00ff) << 8))


WINDOW * mainwin;   // ncurses window


int print_ips = 1;


struct pkt_stats
{
  u_int64_t pkts_rx;
  u_int64_t pkts_tx; 
  u_int64_t bytes_rx;
  u_int64_t bytes_tx; 
  u_int64_t missed_tx; 
} __rte_cache_aligned;


struct port_s
{
  char name[5];
  u_int16_t core_id;
  struct ether_addr port_addr;
  struct ether_addr gw_addr;
  u_int64_t pkts_rx_before;
  u_int64_t pkts_tx_before;
  u_int64_t bytes_rx_before;
  u_int64_t bytes_tx_before;
  u_int16_t pps_rx;
  u_int16_t pps_tx;
  u_int16_t bps_rx;
  u_int16_t bps_tx;
  volatile struct pkt_stats pkt_stats;
};


struct port_s * port_stats;

struct ifrate_s
{
  struct port_s port_stats[2]; 
  volatile int	transmit;
  volatile u_int64_t idle_loops;
  volatile u_int64_t busy_loops;
  u_int64_t waist_cycles;
  volatile u_int64_t t_usefull;
} __rte_cache_aligned;


struct ifrate_s * ifrate_stats;


struct itvl_stats 
{
  //struct port_s port_stats[2];
  struct timeval tv;
  uint64_t usecs;
  uint64_t num_pkts;
  uint64_t num_bytes;
} itvl[2];


static u_int64_t p_size;



//typedef unsigned int uint128_t __attribute__((mode(TI)));  


struct ether_dot1q_header
{
  u_int8_t  dot1q_dhost[ETH_ALEN];      /* destination eth addr */
  u_int8_t  dot1q_shost[ETH_ALEN];      /* source ether addr    */
  u_int16_t dot1q_encap_type;
  u_int16_t dot1q_tag;
  u_int16_t ether_type;
};


struct v_tag
{
  u_int16_t tpid;
  u_int16_t vlan_id;
  u_int16_t ether_type;
};





static const struct rte_eth_conf port_conf_default = {
	.rxmode = { 
		.max_rx_pkt_len = 9000,
		.jumbo_frame 		= 1, 
		.header_split   = 0, /**< Header Split disabled */
    .hw_vlan_strip  = 0,
		.hw_ip_checksum = 0, /**< IP checksum offload disabled */
		.hw_vlan_filter = 0, /**< VLAN filtering disabled */
		.hw_strip_crc   = 0, /**< CRC stripped by hardware */
		},
   .txmode = {
		.mq_mode = ETH_MQ_TX_NONE,
	},
	.intr_conf = {
		//.lsc = 1, /**< lsc interrupt feature enabled */
    .lsc = 0, /**< lsc interrupt feature disabled */
	},
};



static const char * main_help =
	"ifrate\n"
	"Usage:\n"
  "  ifrate [options] -d <if-name>\n"
	"  Options:\n"
  "\t -c <mask> Processor affinity mask\n"
  "\t -m <mtu>  MTU size\n"
  "\t -y <MAC> \n"
  "\t -l <pciid of interface> \n"
  "\t -t transmit\n"
  "\t -k keep original dst mac\n"
  "\t -v <num>  Verbose (if num > 3 foreground) num - verbose level\n"
  "\t -s <num>  syslog facility 0-11 (log_kern - log_ftp) 16-23 (local0-local7) see /usr/include/sys/syslog.h\n"
  "\t -S strip VLAN\n"
  "\t -C change outer VLAN on TX\n"
  "\t -i insert VLAN on TX\n"
	"\t -h|?  Display this help screen\n";


int  				terminated = 0;
int  				restart = 0;           
static int	transmit = 0;
int         burst = BURST_SIZE;
int         strip = 0;
int         change_vlan = 0;
int         insert_vlan = 0;


static int keep_mac = 0;

static u_int32_t cpu_mask = 0;

static int mtu = 9000;
static int debug = 0;
extern int traceLevel;			  // NORMAL == 5 level, INFO == 6
extern int useSyslog;         // 0 send messages to stdout
extern int logFacility;       // LOG_LOCAL0

static char *pciid_l = NULL;
static char *gw_mac_l = NULL;

int     nb_ports;
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


struct link_status
{
  int link;
  int duplex;
  int speed;
} link_stat;

struct timeval startTime;
struct timeval endTime;

static void print_ethaddr(const char *name, const struct ether_addr *eth_addr, char *msg);
void waist_time(u_int64_t how_much);
void print_port_errors(struct rte_eth_stats et_stats, int col);
void print_packets(struct port_s s, int where, float delta_secs);
inline void ether_aton_r (const char *asc, struct ether_addr * addr);
inline uint128_t ntoh128_u(uint128_t * src);
static double timeDelta (struct timeval * now, struct timeval * before);
static void runIfrate(uint8_t port, unsigned nb_ports, int _mtu, unsigned long cpu_mask);
inline void gotpacket(struct rte_mbuf  *mb, int port);
static void lsi_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param);
void print_port_stats(struct rte_eth_stats et_stats);

int link_trace;

#endif
