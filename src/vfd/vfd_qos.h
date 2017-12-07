/*
	Mnemonic:	qos.h
	Abstract:	Headers specifically for the qos (hard coded) implementation.
				As these (if these?) ever make it to DPDK, then this file should
				go the way of the dodo.
	Author:		E. Scott Daniels
	Date:		26 Augusg 2016
*/

#ifndef _VFD_QOS_H
#define _VFD_QOS_H
											// traffic class related flags
#define QOS_TCFL_GSP	0x01				// group strict priority	(the actual value of each of these is VERY SIGNIFICANT!)
#define QOS_TCFL_LSP	0x02				// link strict priority

/*
	Manages the various things that can be configured for a traffic class.
	This struct is passed from the main application into the qos functions.
*/
typedef struct {
	uint32_t	bw_group;				// bandwidth group the TC is assigned to
	uint32_t	max_credits;			// max credits the TC is allowed to cary
	uint32_t	credit_refil;			// the number of credits refilled on each cycle
	uint8_t		flags;					// QOS_FL constants for link and group strict priority
} qos_tc_t;



// ------------ prototypes -------------------------------------------------------------

extern int enable_dcb_qos( sriov_port_t *port, int* pctgs, int tc8_mode, int option );


#endif

