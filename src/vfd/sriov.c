/*
**
** az
**
*/

#include "sriov.h"


#define RTE_PMD_PARAM_UNSET -1


struct rte_port *ports;


static inline 
uint64_t RDTSC(void)
{
  unsigned int hi, lo;
  __asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
  return ((uint64_t)hi << 32) | lo;
}
	  

int 
xdigit(char c) 
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


void 
ether_aton_r(const char *asc, struct ether_addr *addr)
{
  int i, val0, val1;
  
  for (i = 0; i < ETHER_ADDR_LEN; ++i){
    val0 = xdigit(*asc);
    asc++;
    
    if (val0 < 0)
      return;

    val1 = xdigit(*asc);
    asc++;
    if (val1 < 0)
      return;

    addr->addr_bytes[i] = (u_int8_t)((val0 << 4) + val1);

    if (i < ETHER_ADDR_LEN - 1){
      if (*asc != ':')
        return;
      asc++;
    }
  }
  if (*asc != '\0')
    return;    
}


int
port_id_is_invalid(portid_t port_id, enum print_warning warning)
{
  
  traceLog(TRACE_INFO,"Port %d\n", port_id);

	if (port_id == (portid_t)RTE_PORT_ALL)
		return 0;

	if (port_id < RTE_MAX_ETHPORTS && ports[port_id].enabled)
		return 0;

	if (warning == ENABLED_WARN)
		traceLog(TRACE_ERROR,"Invalid port %d\n", port_id);

	return 1;
}


int
set_queue_rate_limit(portid_t port_id, uint16_t queue_idx, uint16_t rate)
{
	int diag;
	struct rte_eth_link link;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return 1;
	rte_eth_link_get_nowait(port_id, &link);
	if (rate > link.link_speed) {
		traceLog(TRACE_ERROR,"Invalid rate value:%u bigger than link speed: %u\n",
			rate, link.link_speed);
		return 1;
	}
	diag = rte_eth_set_queue_rate_limit(port_id, queue_idx, rate);
	if (diag == 0)
		return diag;
	traceLog(TRACE_ERROR,"rte_eth_set_queue_rate_limit for port_id=%d failed diag=%d\n",
		port_id, diag);
	return diag;
}



int
set_vf_rate_limit(portid_t port_id, uint16_t vf, uint16_t rate, uint64_t q_msk)
{
	int diag;
	struct rte_eth_link link;

	if (q_msk == 0)
		return 0;

	if (port_id_is_invalid(port_id, ENABLED_WARN))
		return 1;
	rte_eth_link_get_nowait(port_id, &link);
	if (rate > link.link_speed) {
		traceLog(TRACE_ERROR,"Invalid rate value:%u bigger than link speed: %u\n",
			rate, link.link_speed);
		return 1;
	}
	diag = rte_eth_set_vf_rate_limit(port_id, vf, rate, q_msk);
	if (diag == 0)
		return diag;
	traceLog(TRACE_ERROR, "rte_eth_set_vf_rate_limit for port_id=%d failed diag=%d\n",
		port_id, diag);
	return diag;
}



int
port_reg_off_is_invalid(portid_t port_id, uint32_t reg_off)
{
	uint64_t pci_len;

	if (reg_off & 0x3) {
		traceLog(TRACE_DEBUG, "Port register offset 0x%X not aligned on a 4-byte "
		       "boundary\n",
		       (unsigned)reg_off);
		return 1;
	}
	pci_len = ports[port_id].dev_info.pci_dev->mem_resource[0].len;
	if (reg_off >= pci_len) {
		traceLog(TRACE_DEBUG, "Port %d: register offset %u (0x%X) out of port PCI "
		       "resource (length=%"PRIu64")\n",
		       port_id, (unsigned)reg_off, (unsigned)reg_off,  pci_len);
		return 1;
	}
	return 0;
}


void
rx_vlan_strip_set_on_queue(portid_t port_id, uint16_t queue_id, int on)
{
	int diag;

	diag = rte_eth_dev_set_vlan_strip_on_queue(port_id, queue_id, on);
	if (diag < 0)
		traceLog(TRACE_DEBUG, "rx_vlan_strip_set_on_queue(port_pi=%d, queue_id=%d, on=%d) failed "
	       "diag=%d\n", port_id, queue_id, on, diag);
}



void
rx_vlan_insert_set_on_vf(portid_t port_id, uint16_t vf_id, int vlan_id)
{

  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(port_id, &dev_info);

  uint32_t reg_off = 0x08000;
  
  reg_off += 4 * vf_id;

  traceLog(TRACE_DEBUG, "BEFORE RX_vlan_insert_set_on_queue(bar=0x%08X, vf_id=%x, on=%x)\n", reg_off, vf_id, 0);
  
  uint32_t ctrl = port_pci_reg_read(port_id, reg_off);

  traceLog(TRACE_DEBUG, "RX_vlan_insert_set_on_queue(bar=0x%08X, vf_id=%x, ctrl=%x)\n", reg_off, vf_id, ctrl);
 
  
  if (vlan_id){
    ctrl = vlan_id;
    ctrl |= 0x40000000;
  }
  else
    ctrl = 0;
      
  port_pci_reg_write(port_id, reg_off, ctrl);    
  
 traceLog(TRACE_DEBUG, "rx_insert_set_on_queue(bar=0x%08X, vfid_id=%d, ctrl=0x%08X)\n", reg_off, vf_id, ctrl);
}


void
rx_vlan_strip_set_on_vf(portid_t port_id, uint16_t vf_id, int on)
{

  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(port_id, &dev_info);
 
  uint32_t queues_per_pool = dev_info.vmdq_queue_num / dev_info.max_vmdq_pools; 
  
  uint32_t reg_off = 0x01028;
  
  reg_off += (0x40 * vf_id * queues_per_pool);
 
  uint32_t q;
  for(q = 0; q < queues_per_pool; ++q){
    
    reg_off += 0x40 * q;

    traceLog(TRACE_DEBUG, "BEFORE RX_vlan_strip_set_on_queue(bar=0x%08X, vf_id=%x, on=%x)\n", reg_off, vf_id, 0);
    
    uint32_t ctrl = port_pci_reg_read(port_id, reg_off);

    traceLog(TRACE_DEBUG, "RX_vlan_strip_set_on_queue(bar=0x%08X, vf_id=%x, ctrl=%x)\n", reg_off, vf_id, ctrl);
   
    
    if (on)
      ctrl |= IXGBE_RXDCTL_VME;
    else 
      ctrl &= ~IXGBE_RXDCTL_VME;
        
    port_pci_reg_write(port_id, reg_off, ctrl);    
    
    traceLog(TRACE_DEBUG, "rx_vlan_strip_set_on_queue(bar=0x%08X, vfid_id=%d, ctrl=0x%08X)\n", reg_off, vf_id, ctrl);
  }
}



void
rx_vlan_strip_set(portid_t port_id, int on)
{
	int diag;
	int vlan_offload;


	vlan_offload = rte_eth_dev_get_vlan_offload(port_id);

	if (on)
		vlan_offload |= ETH_VLAN_STRIP_OFFLOAD;
	else
		vlan_offload &= ~ETH_VLAN_STRIP_OFFLOAD;

	diag = rte_eth_dev_set_vlan_offload(port_id, vlan_offload);
	if (diag < 0)
		traceLog(TRACE_INFO, "rx_vlan_strip_set(port_pi=%d, on=%d) failed, diag=%d\n", port_id, on, diag);
}



void 
set_vf_allow_bcast(portid_t port_id, uint16_t vf_id, int on)
{
  int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_BROADCAST,(uint8_t) on);
	
	if (ret < 0)
    traceLog(TRACE_INFO, "set_vf_allow_bcast(): bad VF receive mode parameter, return code = %d \n", ret);
}


void 
set_vf_allow_mcast(portid_t port_id, uint16_t vf_id, int on)
{
	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_MULTICAST,(uint8_t) on);
	
	if (ret < 0)
    traceLog(TRACE_INFO, "set_vf_allow_mcast(): bad VF receive mode parameter, return code = %d \n", ret);  
}


void 
set_vf_allow_un_ucast(portid_t port_id, uint16_t vf_id, int on)
{
	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, ETH_VMDQ_ACCEPT_HASH_UC,(uint8_t) on);
	
	if (ret < 0)
    traceLog(TRACE_INFO, "set_vf_allow_un_ucast(): bad VF receive mode parameter, return code = %d \n", ret);  
}


void 
set_vf_allow_untagged(portid_t port_id, uint16_t vf_id, int on)
{
  uint16_t rx_mode = 0;
  rx_mode |= ETH_VMDQ_ACCEPT_UNTAG;
  
  traceLog(TRACE_INFO, "set_vf_allow_untagged(): rx_mode = %d, on = %d \n", rx_mode, on);
   
	int ret = rte_eth_dev_set_vf_rxmode(port_id, vf_id, rx_mode, (uint8_t) on);
	
	if (ret < 0)
    traceLog(TRACE_INFO, "set_vf_allow_untagged(): bad VF receive mode parameter, return code = %d \n", ret);
}



void
set_vf_rx_mac(portid_t port_id, const char* mac, uint32_t vf,  __attribute__((__unused__)) uint8_t on)
{
	int diag;
  struct ether_addr mac_addr;
  ether_aton_r(mac, &mac_addr);

	diag = rte_eth_dev_mac_addr_add(port_id, &mac_addr, vf);
	if (diag == 0)
		return;
  
	traceLog(TRACE_ERROR, "rte_eth_dev_set_vf_vlan_filter for port_id=%d failed "
	       "diag=%d\n", port_id, diag);
         
}

  

void
set_vf_rx_vlan(portid_t port_id, uint16_t vlan_id, uint64_t vf_mask, uint8_t on)
{
	int diag;

	diag = rte_eth_dev_set_vf_vlan_filter(port_id, vlan_id, vf_mask, on);
	if (diag == 0)
		return;
  
	traceLog(TRACE_ERROR, "rte_eth_dev_set_vf_vlan_filter for port_id=%d failed "
	       "diag=%d\n", port_id, diag);
         
}



void
set_vf_vlan_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on)
{
	int diag;

	diag = rte_eth_dev_set_vf_vlan_anti_spoof(port_id, vf, on);
	if (diag == 0)
		return;
	traceLog(TRACE_ERROR, "rte_eth_dev_set_vf_vlan_anti_spoof for port_id=%d failed "
	       "diag=%d vf=%d\n", port_id, diag, vf);
             
}


void
set_vf_mac_anti_spoofing(portid_t port_id, uint32_t vf, uint8_t on)
{
	int diag;

	diag = rte_eth_dev_set_vf_mac_anti_spoof(port_id, vf, on);
	if (diag == 0)
		return;
	traceLog(TRACE_ERROR, "rte_eth_dev_set_vf_mac_anti_spoof for port_id=%d failed "
	       "diag=%d vf=%d\n", port_id, diag, vf);
             
}

void
nic_stats_clear(portid_t port_id)
{

	rte_eth_stats_reset(port_id);
	traceLog(TRACE_DEBUG, "\n  NIC statistics for port %d cleared\n", port_id);
}


void
nic_stats_display(uint8_t port_id)
{
	struct rte_eth_stats stats;
//	struct rte_port *port = &ports[port_id];
//	uint8_t i;

	static const char *nic_stats_border = "########################";


	rte_eth_stats_get(port_id, &stats);
	traceLog(TRACE_DEBUG, "\n  %s NIC statistics for port %-2d %s\n",
	       nic_stats_border, port_id, nic_stats_border);


		traceLog(TRACE_DEBUG, "  RX-packets:              %10"PRIu64"    RX-errors: %10"PRIu64
		       "    RX-bytes: %10"PRIu64"\n",
		       stats.ipackets, stats.ierrors, stats.ibytes);
		traceLog(TRACE_DEBUG, "  RX-errors:  %10"PRIu64"\n", stats.ierrors);
		traceLog(TRACE_DEBUG, "  RX-nombuf:               %10"PRIu64"\n",
		       stats.rx_nombuf);
		traceLog(TRACE_DEBUG, "  TX-packets:              %10"PRIu64"    TX-errors: %10"PRIu64
		       "    TX-bytes: %10"PRIu64"\n",
		       stats.opackets, stats.oerrors, stats.obytes);

	traceLog(TRACE_DEBUG, "  %s############################%s\n",
	       nic_stats_border, nic_stats_border);
}



int 
port_init(uint8_t port, __attribute__((__unused__)) struct rte_mempool *mbuf_pool)
{
	struct rte_eth_conf port_conf = port_conf_default;
	const uint16_t rx_rings = 1, tx_rings = 1;
	int retval;
	uint16_t q;

	if (port >= rte_eth_dev_count()) {
			traceLog(TRACE_ERROR, "port >= rte_eth_dev_count\n");
   		exit(EXIT_FAILURE);
	}


	// Configure the Ethernet device.
	retval = rte_eth_dev_configure(port, rx_rings, tx_rings, &port_conf);
	if (retval != 0) {
			traceLog(TRACE_ERROR, "Can not configure port %u, retval %d\n", port, retval);
   		exit(EXIT_FAILURE);
	}


  rte_eth_dev_callback_register(port, RTE_ETH_EVENT_INTR_LSC, lsi_event_callback, NULL);
  
  
  rte_eth_dev_callback_register(port, RTE_ETH_EVENT_INTR_VF_RST, vf_msb_event_callback, (void *) &param);    

	// Allocate and set up 1 RX queue per Ethernet port. 
	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port, q, RX_RING_SIZE, rte_eth_dev_socket_id(port), NULL, mbuf_pool);
		if (retval < 0) {
			traceLog(TRACE_ERROR, "Can not setup rx queue, port %u\n", port);
   		exit(EXIT_FAILURE);
		}
	}

	// Allocate and set up 1 TX queue per Ethernet port.
	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port, q, TX_RING_SIZE, rte_eth_dev_socket_id(port), NULL);
		if (retval < 0) {
			traceLog(TRACE_ERROR, "Can not setup tx queue, port %u\n", port);
   		exit(EXIT_FAILURE);
		}
	}


	// Start the Ethernet port. 
	retval = rte_eth_dev_start(port);
	if (retval < 0) {
		traceLog(TRACE_ERROR, "Can not start port %u\n", port);
  	exit(EXIT_FAILURE);
	}
	

	// Display the port MAC address. 
	struct ether_addr addr;
	rte_eth_macaddr_get(port, &addr);
	traceLog(TRACE_DEBUG,  "Port %u MAC: %02" PRIx8 " %02" PRIx8 " %02" PRIx8
			   " %02" PRIx8 " %02" PRIx8 " %02" PRIx8 "\n",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);


	// Enable RX in promiscuous mode for the Ethernet device. 
	rte_eth_promiscuous_enable(port);

	return 0;
}



void
lsi_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param)
{
	struct rte_eth_link link;

	RTE_SET_USED(param);

	traceLog(TRACE_DEBUG, "Event type: %s\n", type == RTE_ETH_EVENT_INTR_LSC ? "LSC interrupt" : "unknown event");
	rte_eth_link_get_nowait(port_id, &link);
	if (link.link_status) {
		traceLog(TRACE_DEBUG, "Port %d Link Up - speed %u Mbps - %s\n\n",
				port_id, (unsigned)link.link_speed,
			(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
				("full-duplex") : ("half-duplex"));
	} else
		traceLog(TRACE_DEBUG, "Port %d Link Down\n\n", port_id);
}

//static uint64_t ff = 777;

void
vf_msb_event_callback(uint8_t port_id, enum rte_eth_event_type type, void *param)
{

  //RTE_SET_USED(param);

  struct rte_msb_param *p = (struct rte_msb_param*) param;
  
  //uint32_t vf =  *(uint32_t*) p->in;
  
  uint32_t vf =  (uint32_t) p->in;
  uint32_t out = 123;
 
  
  //uint32_t vf = (uint32_t) p->in;
  //uint32_t out = (uint32_t) p->out;

  traceLog(TRACE_DEBUG, "--------------------\n Type: %d, Port: %d, VF: %d, OUT: %d\n------------------\n", type, port_id, vf,  p->out);
	traceLog(TRACE_DEBUG, "Event type: %s\n", type == RTE_ETH_EVENT_INTR_VF_RST ? "VF RST interrupt" : "unknown event");
  

  restore_vf_setings(port_id, vf);
  
   p->out = out;
  
  struct rte_eth_dev_info dev_info;
  rte_eth_dev_info_get(port_id, &dev_info);
 

	traceLog(TRACE_DEBUG, "driver_name = %s\n", dev_info.driver_name);
	traceLog(TRACE_DEBUG, "if_index = %d\n", dev_info.if_index);
	traceLog(TRACE_DEBUG, "min_rx_bufsize = %d\n", dev_info.min_rx_bufsize);
	traceLog(TRACE_DEBUG, "max_rx_pktlen = %d\n", dev_info.max_rx_pktlen);
	traceLog(TRACE_DEBUG, "max_rx_queues = %d\n", dev_info.max_rx_queues);
	traceLog(TRACE_DEBUG, "max_tx_queues = %d\n", dev_info.max_tx_queues);
	traceLog(TRACE_DEBUG, "max_mac_addrs = %d\n", dev_info.max_mac_addrs);
	traceLog(TRACE_DEBUG, "max_hash_mac_addrs = %d\n", dev_info.max_hash_mac_addrs);
	// Maximum number of hash MAC addresses for MTA and UTA. 
	traceLog(TRACE_DEBUG, "max_vfs = %d\n", dev_info.max_vfs);
	traceLog(TRACE_DEBUG, "max_vmdq_pools = %d\n", dev_info.max_vmdq_pools);
	traceLog(TRACE_DEBUG, "rx_offload_capa = %d\n", dev_info.rx_offload_capa);
	traceLog(TRACE_DEBUG, "reta_size = %d\n", dev_info.reta_size);
	// Device redirection table size, the total number of entries. 
	traceLog(TRACE_DEBUG, "hash_key_size = %d\n", dev_info.hash_key_size);
	///Bit mask of RSS offloads, the bit offset also means flow type 
	traceLog(TRACE_DEBUG, "flow_type_rss_offloads = %lu\n", dev_info.flow_type_rss_offloads);
	traceLog(TRACE_DEBUG, "vmdq_queue_base = %d\n", dev_info.vmdq_queue_base);
	traceLog(TRACE_DEBUG, "vmdq_queue_num = %d\n", dev_info.vmdq_queue_num);
	traceLog(TRACE_DEBUG, "vmdq_pool_base = %d\n", dev_info.vmdq_pool_base);
  
}



void 
traceLog(int eventTraceLevel, const char * file, int line, const char * format, ...)
{
  va_list va_ap;

  char buf[256], out_buf[256];

  char extra_msg[10];
 
  strcpy(extra_msg, " ");

  if(eventTraceLevel <= traceLevel) {
    va_start (va_ap, format);
    memset(buf, 0, sizeof(buf));
    vsnprintf(buf, sizeof(buf) - 1, format, va_ap);

    if(eventTraceLevel == 3 ) {
      //extra_msg = "ERROR: ";
      strcpy(extra_msg, "ERROR: ");
    }
    else if(eventTraceLevel == 4 ) {
      //extra_msg = "WARNING: ";
      strcpy(extra_msg, "WARNING: ");
    }

    
    while(buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';

    if(traceLevel > 6)
      snprintf(out_buf, sizeof(out_buf), "[%s:%d] %s%s", file, line, extra_msg, buf);
    else
      snprintf(out_buf, sizeof(out_buf), "%s%s", extra_msg, buf);


    if(useSyslog){
	    openlog(prog_name, LOG_PID, logFacility);
      syslog(eventTraceLevel, "%s", out_buf);
    }
    else {
      printf("%s\n", out_buf);
    }
  }

  fflush(stdout);
  va_end(va_ap);
}



void 
detachFromTerminal(void)
{
  setsid();  // detach from the terminal

  fclose(stdin);
  fclose(stdout);

  // clear any inherited file mode creation mask
  umask(0);

  setvbuf(stdout, (char *)NULL, _IOLBF, 0);
}


void
daemonize(void)
{
  int childpid;

  //signal(SIGHUP, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  if((childpid = fork()) < 0)
    traceLog(TRACE_ERROR, "INIT: Can not fork process (errno = %d)", errno);
  else {
#ifdef DEBUG
    traceLog(TRACE_INFO, "DEBUG: after fork() in %s (%d)",
	       childpid ? "parent" : "child", childpid);
#endif
    if(!childpid) {
      // child
      traceLog(TRACE_INFO, "INIT: Starting Tcap daemon");
      detachFromTerminal();
    }
    else {
      // parent
      traceLog(TRACE_INFO, "INIT: Parent process exits");
      exit(EXIT_SUCCESS);
    }
  }
}




