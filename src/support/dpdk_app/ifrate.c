/*
**
** az
**
*/

#include "ifrate.h"


char * prog_name;


int traceLevel = 8;			  // NORMAL == 5 level, INFO == 6
int useSyslog = 0;        // 0 send messages to stdout
int logFacility = 128;    // LOG_LOCAL0



static inline int xdigit (char c) 
{
  unsigned d;
  d = (unsigned)(c-'0');
  
  if (d < 10)
    return (int)d;
  
  d = (unsigned)(c-'a');
  
  if (d < 6) 
    return (int)(10+d);
  
  d = (unsigned)(c-'A');
  
  if (d < 6) 
    return (int)(10+d);
  
  return -1;
}

inline void ether_aton_r (const char *asc, struct ether_addr * addr)
{
  int i, val0, val1;
  
  for (i = 0; i < ETHER_ADDR_LEN; ++i) 
  {
    val0 = xdigit(*asc);
    asc++;
    
    if (val0 < 0)
      return;

    val1 = xdigit(*asc);
    asc++;
    if (val1 < 0)
      return;

    addr->addr_bytes[i] = (u_int8_t)((val0 << 4) + val1);

    if (i < ETHER_ADDR_LEN - 1) 
    {
      if (*asc != ':')
        return;
      asc++;
    }
  }
  if (*asc != '\0')
    return;    
}




 static void print_ethaddr(const char *name, const struct ether_addr *eth_addr, char *msg)
 {
	sprintf (msg, "%s%02X:%02X:%02X:%02X:%02X:%02X", name,
		eth_addr->addr_bytes[0],
		eth_addr->addr_bytes[1],
		eth_addr->addr_bytes[2],
		eth_addr->addr_bytes[3],
		eth_addr->addr_bytes[4],
		eth_addr->addr_bytes[5]);
 }

 
static inline void lock(int mutex)
{
  while(simpe_atomic_swap(mutex, 1)) {}
}


static inline uint64_t RDTSC(void)
{
  unsigned int hi, lo;
  __asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
  return ((uint64_t)hi << 32) | lo;
}
	  


// static rte_atomic32_t cb_count;

// static void test_multi_cb(void *arg)
// {
	// rte_atomic32_inc(&cb_count);
	// printf("In %s - arg = %p\n", __func__, arg);
  // print_periodic_stats();
  // rte_eal_alarm_set(1000 * 1000, test_multi_cb, (void *)1);
// }


static void sig_int(int sig)
{
  terminated = 1;
  restart = 0;

  int portid;
  if (sig == SIGINT) 
  {
		for (portid = 0; portid < nb_ports; portid++) 
    {
			rte_eth_dev_close(portid);
		}
	}
  
  
  
  static int called = 0;
  
  if(sig == 1) called = sig;

  if(called) return; else called = 1;
  
  traceLog(TRACE_NORMAL, "Received Interrupt signal\n");
}




static void sig_usr(int sig)
{
  terminated = 1;
  restart = 1;
    
  static int called = 0;
  
  if(sig == 1) called = sig;

  if(called) return; else called = 1;
  
  traceLog(TRACE_NORMAL, "Restarting ifrate, port: %s\n", pciid_l);
}


static void sig_hup(int sig)
{
  terminated = 1;
  restart = 1;

  static int called = 0;
  
  if(sig == 1) 
    called = sig;

  if(called) 
    return; 
  else called = 1;

  traceLog(TRACE_NORMAL, "Received HUP signal\n");
}



// Time difference in millisecond

double timeDelta(struct timeval * now, struct timeval * before)
{
  time_t delta_seconds;
  time_t delta_microseconds;

  //compute delta in second, 1/10's and 1/1000's second units

  delta_seconds      = now -> tv_sec  - before -> tv_sec;
  delta_microseconds = now -> tv_usec - before -> tv_usec;

  if(delta_microseconds < 0)
  {
    // manually carry a one from the seconds field
    delta_microseconds += 1000000;  // 1e6 
    -- delta_seconds;
  }
  return((double)(delta_seconds * 1000) + (double)delta_microseconds/1000);
}







static inline int port_init(uint8_t port, struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;
  struct rte_eth_dev_info dev_info;
  
		printf("------------------------------------------------------- 1 --------------------------------------------------------------\n");
  
	
	if (port >= rte_eth_dev_count())
	{
			traceLog(TRACE_ERROR, "port >= rte_eth_dev_count\n");
   		exit(EXIT_FAILURE);
	}
  

  rte_eth_dev_info_get(port, &dev_info);

  
  
  if(strip)
    port_conf.rxmode.hw_vlan_strip = 1;
  
	// Configure the Ethernet device.
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0)
	{
			traceLog(TRACE_ERROR, "Can not configure port %u, retval %d\n", port, retval);
   		exit(EXIT_FAILURE);
	}
	
	printf("------------------------------------------------------- 3 --------------------------------------------------------------\n");

  rte_eth_dev_callback_register(port, RTE_ETH_EVENT_INTR_LSC, lsi_event_callback, NULL);

	// Allocate and set up 1 RX queue per Ethernet port. 
	for (q = 0; q < rx_rings; q++) 
  {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0)
		{
			traceLog(TRACE_ERROR, "Can not setup rx queue, port %u\n", port);
   		exit(EXIT_FAILURE);
		}
	}

	// Allocate and set up 1 TX queue per Ethernet port.
	for (q = 0; q < tx_rings; q++) 
  {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE, rte_eth_dev_socket_id(port), NULL);
		if (retval < 0)
		{
			traceLog(TRACE_ERROR, "Can not setup tx queue, port %u\n", port);
   		exit(EXIT_FAILURE);
		}
	}

	// Start the Ethernet port. 
	retval = rte_eth_dev_start(port);
	if (retval < 0)
	{
		traceLog(TRACE_ERROR, "Can not start port %u\n", port);
  	exit(EXIT_FAILURE);
	}

	// Display the port MAC address. 
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);


  ifrate_stats->port_stats[port].port_addr = addr;
  
  
  printf("GW %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)port,
			ifrate_stats->port_stats[port].gw_addr.addr_bytes[0], ifrate_stats->port_stats[port].gw_addr.addr_bytes[1],
			ifrate_stats->port_stats[port].gw_addr.addr_bytes[2], ifrate_stats->port_stats[port].gw_addr.addr_bytes[3],
			ifrate_stats->port_stats[port].gw_addr.addr_bytes[4], ifrate_stats->port_stats[port].gw_addr.addr_bytes[5]);

	// Enable RX in promiscuous mode for the Ethernet device. 
	rte_eth_promiscuous_enable(port);
  
  

  int diag = rte_eth_dev_set_vlan_offload(port, ETH_VLAN_FILTER_OFFLOAD | ETH_VLAN_STRIP_OFFLOAD);
	if (diag < 0)
		printf("rx_vlan_strip_set(port_pi=%d) failed, diag=%d\n", (int)port, diag);

	return 0;
}


static void lsi_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param)
{
	struct rte_eth_link link;

	RTE_SET_USED(param);
	RTE_SET_USED(type);
	
	link_trace = 1;  // will use it if needed to print link down only once

	//traceLog(TRACE_NORMAL, "Event type: %s\n", type == RTE_ETH_EVENT_INTR_LSC ? "LSC interrupt" : "unknown event");
	
	rte_eth_link_get_nowait(port_id, &link);
	
	if (link.link_status) 
	{
		//printf("Port %d Link Up - speed %u Mbps - %s\n\n",  port_id, (unsigned)link.link_speed, (link.link_duplex == ETH_LINK_FULL_DUPLEX) ? ("full-duplex") : ("half-duplex"));
    
    traceLog(TRACE_WARNING, "link is UP: Port: %d, speed %u, duplex %u\n", port_id, (unsigned) link.link_speed, link.link_duplex);
    
    link_trace = 1;
	} 
	else
	{
		   
   // traceLog(TRACE_NORMAL, "link is Down: Port: %s, speed %u, duplex %u\n", portname, (unsigned) link.link_speed, link.link_duplex);
  
    if(link_trace)  traceLog(TRACE_WARNING, "link is down, Port: %d\n", port_id);
    
    link_trace = 0;
	}	
}


inline void gotpacket(struct rte_mbuf  *mb, int __attribute__((__unused__)) port)
{

  st.bcount += mb->pkt_len;
  st.pcount++;
  
  
  char msg[256];
  char ip_buf[16];
  
	uint8_t *ptr;
	
	struct ether_hdr *eth_hdr;

  
	//struct ipv6_hdr *ipv6_h;
	//uint8_t l2_len  = sizeof(struct ether_hdr);

	//uint128_t srcaddr;      // Source IP Address
  //uint128_t dstaddr;      // Destination IP Address
 // u_int16_t srcport;      // TCP/UDP source port number
 // u_int16_t dstport;      // TCP/UDP destination port number
  //u_int8_t  tcp_flags;    // Cumulative OR of tcp flags
 // u_int8_t  proto;        // IP protocol, 
 // u_int8_t  tos;          // IP Type-of-Service
  //u_int8_t  version;      // Protocol version
  //u_int32_t mpls_label;
  
  
  u_int16_t vlan_id;
  u_short tpid;

	eth_hdr = rte_pktmbuf_mtod(mb, struct ether_hdr *);
  ptr = (uint8_t *) eth_hdr;
  u_short eth_type = ntohs(eth_hdr->ether_type);

  
  
  // put address of right port as source and change dest
  
  if(!keep_mac)
  {
    eth_hdr->d_addr = eth_hdr->s_addr;
    eth_hdr->s_addr = addr;
  }
  
  
  /* Enable VLAN tag insertion through TXD */
  
  if(insert_vlan) {
    printf("Inserting PKT_TX_VLAN_PKT\n");
    mb->ol_flags = mb->ol_flags | PKT_TX_VLAN_PKT;
  }

  
  
  if (!print_ips) return;
  
  struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get(port, &dev_info);

  printf("------------------------------------------------------------------------------------------------------------------------------\n");
  
  printf("VLAN TCI: %d, VLAN TCI-OUTER: %d\n", mb->vlan_tci , mb->vlan_tci_outer);
  //mb->vlan_tci_outer = 20;
    
  struct ether_addr addr_t;
	rte_eth_macaddr_get(port, &addr_t);
	printf("Port: %u, MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8
			   ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ", ",
			(unsigned)port,
			addr_t.addr_bytes[0], addr_t.addr_bytes[1],
			addr_t.addr_bytes[2], addr_t.addr_bytes[3],
			addr_t.addr_bytes[4], addr_t.addr_bytes[5]);


  printf("Driver Name: %s, Index %d, Pkts rx: %lu, ", dev_info.driver_name, dev_info.if_index, st.pcount);
  
  printf("PCI: %04X:%02X:%02X.%01X, Max VF's: %d\n\n", dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus , dev_info.pci_dev->addr.devid , dev_info.pci_dev->addr.function, dev_info.max_vfs);

    
  //printf("Ether type: %04X\n", eth_type);


  struct ether_dot1q_header *e1qh;

 
  int vlan_h = 0;

  if (ETHERTYPE_VLAN == eth_type || ETHERTYPE_DOT1AD == eth_type) 
  {
    vlan_h = 1;
    
  	e1qh = (struct ether_dot1q_header *) ptr;

    tpid = htons(e1qh->dot1q_encap_type);
   
    vlan_id = htons(e1qh->dot1q_tag) & 0xfff;
  	eth_type = ntohs(e1qh->ether_type);
    
    print_ethaddr("", &eth_hdr->s_addr, msg);
    printf("%s > ", msg);
    print_ethaddr("", &eth_hdr->d_addr, msg);
    printf("%s", msg);
    
    
    
    // change vlan id
    
    if(change_vlan)
      e1qh->dot1q_tag += 10;
    
    
    
    printf(" | TPID: %04X, VLAN ID: %04X |", tpid, vlan_id);
    
    ptr += sizeof(*e1qh);
    struct v_tag * v;
    
    while (ETHERTYPE_VLAN == eth_type || ETHERTYPE_DOT1AD == eth_type)
    {
      ptr -= 2;
      
      v = (struct v_tag *) ptr;
      
      tpid = ntohs(v->tpid);
      eth_type = ntohs(v->ether_type);
      vlan_id = htons(v->vlan_id) & 0xfff;

      printf(" TPID: %04X, VLAN ID: %04X |", tpid, vlan_id);
      
      ptr += sizeof(*v);
      
    }
  }   
  
  if(ETHERTYPE_IP == eth_type || ETHERTYPE_IPV6 == eth_type) 
  {  
    struct iphdr * ip_h;
    
    if (!vlan_h)
    {
      ptr += sizeof(*eth_hdr);
      ip_h = (struct iphdr *) ptr;

      print_ethaddr("", &eth_hdr->s_addr, msg);
      printf("%s > ", msg);
      print_ethaddr("", &eth_hdr->d_addr, msg);
      printf("%s", msg);
      printf(" | Eth Type: %04X | ", eth_type);

      printf("(%s) > ", _intoaV4(ip_h->saddr, ip_buf, 16));
      printf("(%s)\n", _intoaV4(ip_h->daddr, ip_buf, 16));  
    }
    else
    {
      ip_h = (struct iphdr *) ptr;

      printf(" Eth Type: %04X | ", eth_type);
      printf("(%s) > ", _intoaV4(ip_h->saddr, ip_buf, 16));   
      printf("(%s)\n", _intoaV4(ip_h->daddr, ip_buf, 16)); 
    }
  } 

  printf("\n");

  p_size = mb->pkt_len;  // add CRC  (unsigned) mb->data_len ?
	
	
	
	
	
	struct rte_eth_stats et_stats;	
      
  rte_eth_stats_get(port, &et_stats);
  print_port_stats(et_stats);

}

void print_port_stats(struct rte_eth_stats et_stats)
{
	printf("\n");
  printf( "rx packets:\t" "%02" PRIx64 "\n", et_stats.ipackets);
  printf("rx bytes:\t" "%02" PRIx64 "\n", et_stats.ibytes);
  printf( "tx packets:\t" "%02" PRIx64 "\n", et_stats.opackets);
  printf( "tx bytes:\t" "%02" PRIx64 "\n", et_stats.obytes);
  printf( "rx error:\t" "%02" PRIx64 "\n", et_stats.ierrors);
  printf( "tx error:\t" "%02" PRIx64 "\n", et_stats.oerrors);
	printf("\n");
}


//return new ip address instead of changing value of var pointed by address supplied as parameter
inline uint128_t ntoh128_u(uint128_t * src)
{
	uint128_t ret_ip;
	__u8 * s = (__u8 * )src;
	__u8 * d = (__u8 * ) &ret_ip;
	
	int i;
	for (i = 0; i < 16; i++)
		d[15 - i] = s[i];	
		
   return ret_ip;
}


uint64_t ticks_before, ticks_now, t_usefull, t_begin, t_end;

void runIfrate(uint8_t port, unsigned nb_ports, int _mtu, unsigned long cmask)
{ 
  terminated = 0;
 
  st.bcount = 0;
  st.pcount = 0;
  st.pcount_before = 0;

  traceLog(TRACE_NORMAL, "mtu %d, cmask %u\n", _mtu, cmask);

  for (port = 0; port < nb_ports; port++)
  {
		if (rte_eth_dev_socket_id(port) > 0 &&
				rte_eth_dev_socket_id(port) !=
						(int)rte_socket_id())
			printf("WARNING, port %u is on remote NUMA node to "
					"polling thread.\n\tPerformance will "
					"not be optimal.\n", port);
    ifrate_stats->port_stats[port].core_id = rte_lcore_id();
  }     

	printf("\nCore %u forwarding packets. [Ctrl+C to quit]\n", rte_lcore_id());


  memset(itvl, 0, sizeof(struct itvl_stats) * 2);
	gettimeofday(&itvl[0].tv, NULL);
	gettimeofday(&itvl[1].tv, NULL);
  itvl_idx = TOGGLE(0);

  restart = 0;

  
  //rte_eal_alarm_set(1000 * 1000, print_periodic_stats, (void *)1);
 
  //record start time here
  gettimeofday(&st.startTime, NULL);
  
  
  while(!terminated)
	{
		for (port = 0; port < nb_ports; port++) 
    {
			// Get burst of RX packets, from first port of pair.
			struct rte_mbuf *bufs[burst];
      uint16_t nb_rx = rte_eth_rx_burst(port, 0, bufs, burst);
			if (unlikely(nb_rx == 0))
      {
        ++ifrate_stats->idle_loops;
				continue;
      }
      
      ++ifrate_stats->busy_loops;

      t_begin = rte_get_tsc_cycles();
      
      // loop here for a while
     //AZ if(ifrate_stats->waist_cycles > 0)
     //AZ   waist_time(ifrate_stats->waist_cycles);
      
     // waist_cycles = rte_get_tsc_cycles() - t_begin;
      
      ifrate_stats->port_stats[port].pkt_stats.pkts_rx += nb_rx;
      
      int x;
      for (x = 0; x < nb_rx; x++)
      {
        gotpacket(bufs[x], port);

        //ifrate_stats->port_stats[port].pkt_stats.bytes_rx += bufs[x]->pkt_len + 8;
        //ifrate_stats->port_stats[port ^ 1].pkt_stats.bytes_tx += bufs[x]->pkt_len + 8;
      }
      
      uint16_t nb_tx = 0;
      
      if (transmit == 1)
        nb_tx = rte_eth_tx_burst(port, 0, bufs, nb_rx);
      
      
      //printf("N = %d\n", nb_tx);

      if (unlikely(nb_tx < nb_rx))
      {
        do 
        {
          rte_pktmbuf_free(bufs[nb_tx]);
        } while (++nb_tx < nb_rx);
      }
  

      t_end = rte_get_tsc_cycles();
      ifrate_stats->t_usefull += (t_end - t_begin);
		}
    //uint64_t t1 = RDTSC();   
	}
}


void waist_time(u_int64_t how_much)
{
  rte_delay_us(how_much);
}




int main(int argc, char **argv)
{
  int  opt;
  opterr = 0;
 
 	struct rte_mempool *mbuf_pool;

  prog_name = strdup(argv[0]);
   
  pciid_l = strdup("0000:07:10.0");
  
  gw_mac_l = strdup("00:00:5e:00:01:00");
  

	//int i;

//	for( i = 0; i < argc; i++)
//		printf("ARGV[%d] = %s\n", i, argv[i]);

	int ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");
	argc -= ret;
	argv += ret;

  // Parse command line options
  while ( (opt = getopt(argc, argv, "htkSCiv:c:m:l:s:k:y:b:")) != -1)
  {
    switch (opt)
    {

    case 'c':
      cpu_mask = atoi(optarg);
      break;
      
    case 'b':
      burst = atoi(optarg);
      break;
      
     case 'y':
      gw_mac_l = strdup(optarg);
      break;
      
      
    case 'm':
      mtu = atoi(optarg);
      break;

    case 'v':
      traceLevel = atoi(optarg);

      if(traceLevel > 6)
      {
       useSyslog = 0;
       debug = 1;
      }
     break;

    case 'l':
      pciid_l = strdup(optarg);
      break;    
      
    case 'k':
      keep_mac = 1;
      break;

    case 's':
      logFacility = (atoi(optarg) << 3);
      break;
    
    case 't':
      transmit = 1;
      break;
      
    case 'S':
      strip = 1;
      break;

    case 'C':
      change_vlan = 1;
      break; 

    case 'i':
      insert_vlan = 1;
      break;       

    case 'h':
    case '?':
      printf("%s\n", main_help);
      exit(EXIT_FAILURE);
      break;
    }
  }


/*
  argc -= optind;
  argv += optind;
  optind = 0;
*/
/*
	argc = 14;
	
	char **cli_argv = (char**)malloc(argc * sizeof(char*));

  
  for(i = 0; i < argc; i ++)
  {

    cli_argv[i] = (char*)malloc(20 * sizeof(char));
  }

  sprintf(cli_argv[0], "ifrate");
  
  sprintf(cli_argv[1], "-c");
  sprintf(cli_argv[2], "%#02x", cpu_mask);
  
  //sprintf(cli_argv[1], "-l");
  //sprintf(cli_argv[2], "%#02x", cpu_mask);
  
  sprintf(cli_argv[3], "-n");
  sprintf(cli_argv[4], "4");
  sprintf(cli_argv[5], "-w");
  sprintf(cli_argv[6], "%s", pciid_l);
  sprintf(cli_argv[7], "-m");
  sprintf(cli_argv[8], "32");
  sprintf(cli_argv[9], "--file-prefix");
  sprintf(cli_argv[10], "%s", pciid_l);
  sprintf(cli_argv[11], "--log-level");
  //sprintf(cli_argv[12], "%d", traceLevel);
  sprintf(cli_argv[12], "%d", 8);
	sprintf(cli_argv[13], "%s", "--no-huge");
  
 // sprintf(cli_argv[13], "-w");
 // sprintf(cli_argv[14], "%s", pciid_r);
  
  //sprintf(cli_argv[15], "--proc-type");        // Type of this process (primary|secondary|auto)
  //sprintf(cli_argv[16], "%s", "primary");

	

	//rte_set_log_level(RTE_LOG_WARNING);
  
	//if(!debug) daemonize();
    */
			
	// init EAL 
	//int ret = rte_eal_init(argc, cli_argv);
	
	/*
	int ret = rte_eal_init(argc, argv);

		
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	
	*/
	
	rte_set_log_type(RTE_LOGTYPE_PMD && RTE_LOGTYPE_PORT, 0);
	
	traceLog(TRACE_INFO, "LOG LEVEL = %d, LOG TYPE = %d\n", rte_get_log_level(), rte_log_cur_msg_logtype());

    
	rte_set_log_level(8);


  // Check that there is an even number of ports to send/receive on. 
	nb_ports = rte_eth_dev_count();
	//if (nb_ports < 2 || (nb_ports & 1))
	//	rte_exit(EXIT_FAILURE, "Error: number of ports must be even\n");

  
  traceLog(TRACE_NORMAL, "nb_ports = %d\n", nb_ports);

  
  nb_ports = 1;
 
  
  static struct ether_addr  gw1;
 
  ether_aton_r(gw_mac_l, &gw1);


	// Creates a new mempool in memory to hold the mbufs.
	mbuf_pool = rte_pktmbuf_pool_create("ifrate", NUM_MBUFS * nb_ports,
                      MBUF_CACHE_SIZE,
                      0, 
                      RTE_MBUF_DEFAULT_BUF_SIZE,
                      rte_socket_id());
	
		   printf("------------------------------------------------------ 22 -------------------------------------------------------------------\n");
			 
	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
	
		   printf("------------------------------------------------------ 23 -------------------------------------------------------------------\n");

	/* Initialize all ports. */
  u_int16_t portid;
	for (portid = 0; portid < nb_ports; portid++)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);
   

	   printf("------------------------------------------------------ 2 -------------------------------------------------------------------\n");
   
   
  int port = 0; 
  struct rte_eth_dev_info dev_info;
	rte_eth_dev_info_get(0, &dev_info);

  printf("------------------------------------------------------------------------------------------------------------------------------\n");
    
  //struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	printf("Port: %u, MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8
			   ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ", ",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);


  printf("Driver Name: %s, Index %d, Pkts rx: %lu, ", dev_info.driver_name, dev_info.if_index, st.pcount);
  
  printf("PCI: %04X:%02X:%02X.%01X, Max VF's: %d\n\n", dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus , dev_info.pci_dev->addr.devid , dev_info.pci_dev->addr.function, dev_info.max_vfs);

  


  struct sigaction sa;

  sa.sa_handler = sig_int;
  sigaction(SIGINT, &sa, NULL);

  sa.sa_handler = sig_int;
  sigaction(SIGTERM, &sa, NULL);

  sa.sa_handler = sig_int;
  sigaction(SIGABRT, &sa, NULL);

  sa.sa_handler = sig_hup;
  sigaction(SIGHUP, &sa, NULL);
  
  sa.sa_handler = sig_usr;
  sigaction(SIGUSR1, &sa, NULL);    
  
  gettimeofday(&st.startTime, NULL);

  traceLog(TRACE_NORMAL, "starting ifrate loop\n");
  


  runIfrate(2, nb_ports, mtu, cpu_mask);
 
  gettimeofday(&st.endTime, NULL);
  traceLog(TRACE_NORMAL, "Duration %.f sec\n", timeDelta(&st.endTime, &st.startTime));
  traceLog(TRACE_NORMAL, "Total packets: %d, Total Bytes: %d\n", st.pcount, st.bcount);

  traceLog(TRACE_NORMAL, "ifrate exit\n");

  return EXIT_SUCCESS;
}
