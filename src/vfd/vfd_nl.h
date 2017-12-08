// vi: sw=4 ts=4 noet:
/*
	Mnemonic:	vfd_nl.h
	Abstract: 	Main header file for netlonk related stuff.

	Date:		October 2017
	Authors:	Alex Zelezniak (original code)
				E. Scott Daniels


*/

#ifndef _VFD_NL_H_
#define _VFD_NL_H_


#define _GNU_SOURCE


#include <asm/types.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>

#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <getopt.h>

#include <linux/connector.h>

#include "sriov.h"
#include "../vfd-net/vfd-net.h"


// --------------------------------------------------------------------------------------
#define NETLINK_CONNECTOR 	11

#define CN_VFD_IDX		CN_NETLINK_USERS + 3
#define CN_VFD_VAL		0x456



int get_port_by_pci(const char * pciaddr);
int get_vf_stats(int pf, int vf, struct rte_eth_stats *stats);

void netlink_init(void);
void netlink_connect(void);
int netlink_send(int s, struct cn_msg *msg);
int send_message(struct cn_msg *msg);

void get_all_devices(void);
void device_message(int p, int v, int req, int resp);


#endif /* _VFD_NL_H_ */