
/*
	Mnemonic:	qos.c
	Abstract:	Functions which allow the overall configuration of QoS via DCB with
				traffic class queue shares for Tx. The functions are as described in
				the 82599 datasheet and are very specific to that NIC. The enable_dcb_qos
				function implements the whole setup as described in the data sheet and
				was used for initial testing and proof of concept before DPDK version 16.11
				which implemented the ability to enable both DCB and SR-IOV concurrently.

				Of the initial functions which enable_dcb_qos() invokes, only two are
				necessary for VFd to support 'dynamic' QoS (configurable qshares (credits
				ultimately) which can be reconfigured on the fly). These are: qos_enable_arb()
				and qos_set_credits(). 
		
				The packet and descriptor planes must also be configured with functions
				included below.  The DPDK library has functions which do this work, but
				currently they don't accept outside percentages (or I've yet to figure out
				to supply this data).

				The next 'step' is to move them into DPDK; at that point this whole module
				will be deprecated.

				Currently, the vfd_dcb module does the port initialisation and then invokes
				the two mentioned functions to complete the setup.  The main function will
				invoke the qos_set_credits() function any time a configuration change is
				made and the queue shares are recomputed.


				CAUTION:  These are 'hard coded' direct NIC tweaking funcitons used
					as an initial proof of concept for QoS validation.  Some of the
					functionality implemented here exists in the 16.11 version of
					the ixgbe driver/dcb code within DPDK, some does not.  What
					does will be deprecated such that those functions are used
					directly, and what does not will be merged into the DPDK
					library with the intent of upstreaming them into a future
					release.

	Author:		E. Scott Daniels
	Date:		06 June 2016
*/

#include "sriov.h"
#include "vfd_qos.h"
#include <vfdlib.h>		// if vfdlib.h needs an include it must be included there, can't be include prior

static int option1 = 1;

/*
	Set security buffer minimum ifg.
*/
static void qos_set_minifg( portid_t pf ) {
	uint32_t	offset;
	uint32_t	val;		// our changes
	uint32_t	cval;		// current value

	offset = 0x08810;									//  SECTXMINIFG
	val = 0x1f00;										// pfc is enabled; set 0x1f per data sheet (all bits in field, so no need to clear)
	cval = port_pci_reg_read( pf, offset );						// snag current
	bleat_printf( 1, ">>> minifg: %08x %08x = %08x", cval, val, cval | val );
	port_pci_reg_write( pf, offset, cval | val );				// flip our bits on and write
}

/*
	Enable arbitration.
		r=reserved c=current 1=set 0=clear
	
    - Tx Descriptor plane Control and Status (RTTDCS), bits:
        TDPAC=1b, VMPAC=1b, TDRM=1b, BDPM=1b, BPBFSM=0b

		CONFLICT:  there is a conflict in the data sheet with regard to the BDPM setting
					section 4 says the bit should be set, while the description of this
					register in section 8 states that the bit should not be set in dcb mode.

                        +--- bypass buffer free space mon (must be cleared for sriov or dcb)
                        |+--- bypass data pipe monitor ?????? value uncertain -- conflict in doc
                        ||
                        ||                             +--- 0==RR 1=WRR (VMPAC) VM
                        ||                             |
                        ||                             |+-- 0==RR 1==WSP (TDPAC) TC
                        ||                             ||
            crrr rrrr   01rr cccr   rrrr rrrr   r0r1 rr11
            xxxx xxxx   xxxx xxxx   xxxx xxxx   xxxx xxxx
	mask:   ff          3f          ff          ac
	val     00          40          00          13


    - Tx Packet Plane Control and Status (RTTPCS):
		TPPAC=1b, TPRM=1b, ARBD=0x004

                  | -- per data sheet set to 0x004 for DCB mode
                  |                               |--- 0==RR 1=Strict pri
            /-----^------\                        |
            0000 0001   00rr rrrr   rrrr rrr1   rr1r rrrr
            xxxx xxxx   xxxx xxxx   xxxx xxxx   xxxx xxxx
	mask:   00          3f          fe          df
	val     01          00          01          20


    - Rx Packet Plane Control and Status (RTRPCS):
		RAC=1b, RRM=1b
                                                      |-- 0==RR 1=WSB
                                                      ||- must be 1 for DCB
            rrrr rrrr   rrrr rrrr   rrrr rrrr   rrrr r11r
            xxxx xxxx   xxxx xxxx   xxxx xxxx   xxxx xxxx
	mask:   ff          ff          ff          f9				(unused since we don't clear anything)
	val     00          00          00          06
	
	This should be covered by existing DPDK when DCB is set.   Though we may need a
	way to selectively disable/enable the arbitration while leaving all other
	bits set.
*/
extern void qos_enable_arb( portid_t pf ) {
	uint32_t	offset;
	uint32_t	val;
	uint32_t	cval;		// value currently set (preserve reserved)
	uint32_t	mask;		// reserved and current preservation mask

	offset = 0x04900;			// RTTDCS
	mask = 0xff3fffac;			
	if( option1 ) {
		val = 0x00400013;
	} else {
		val = 0x00400010;
	}
	cval = port_pci_reg_read( pf, offset );						// snag current
	port_pci_reg_write( pf, offset, (cval & mask) | val );		
	bleat_printf( 3, ">>>> qos: set_enable_rttdcs (%08x & %08x) | %08x = %08x", cval, mask, val, (cval & mask) | val );

	offset = 0x0cd00;			// RTTPCS
	mask = 0x003ffedf;
	val = 0x01000120;
	cval = port_pci_reg_read( pf, offset );						// snag current
	port_pci_reg_write( pf, offset, (cval & mask) | val );		// add our bits or clear what we don't want
	bleat_printf( 3, ">>>> qos: set_enable_rttpcsarb (%08x & %08x) | %08x = %08x", cval, mask, val, (cval & mask) | val );

	offset = 0x02430;			// RTRPCS
	val = 0x00000006;
	cval = port_pci_reg_read( pf, offset );						// snag current
	port_pci_reg_write( pf, offset, cval | val );				// flip on our bits (no mask needed since we aren't clearing bits)
	bleat_printf( 3, ">>> qos: rtrpcs: %08x/nomask %08x = %08x", cval, val, (cval) | val );
}


/*
	Given an array of percentages for traffic classes, compute the percentage to
	credit factor. The factor can then be multiplied against percentages in the
	TC to convert to their credit value.
*/
static double compute_credit_factor( uint8_t *pctgs, int npctgs, int mtu ) {
	double		base_cred;		// base credits derived from the mtu
	int			i;
	int			smallp = 100;	// smallest percentage	

	base_cred = (double)mtu/64;

	for( i = 0; i < npctgs; i++ ) {
		if( pctgs[i] > 0  &&  pctgs[i] < smallp ) {
			smallp = pctgs[i];
		}
	}

	return base_cred / smallp;
}

/*
	NOTE: the next two funcitons are separate functions as some day
	we might be setting different values in each. Else they could be
	the same function that accepts the offset.
*/
/*
	Set dcb transmit descriptor plane t2 config [0-7].
	For now we set equally.

	FUTURE:  if setting gsp or lsp changes will be needed

    |- 1==lsp                   |-- bwg index
    |                           |
    ||- 1==gsp     max cl       |    CR quant
    ||         /------^------\ /-\/---^------\
    xxrr rrrr  xxxx xxxx  xxxx xxxr  xxxx xxxx
    00         0001 1111  1111 ???0  1111 1111
       0    0     1    f     f  ?       f    f
    0011 1111  0000 0000  0000 0001  0000 0000 mask
       3    f     0    0    0     0     0    0

	This should be covered by the current DPDK DCB mode setup. (but it's not)

	pctgs is an array of percentages for each TC 0..ntcs-1
	bwgs is an array which indicates the bandwidth group the TC belongs to
	In our environment, we do no support variable MTUs across traffic classes (all
	TCs are expected to support the PF's MTU) and thus only one MTU value is needed.
	We also do not support variable bursts defined in the config, and thus compute a
	constant max value based on an assumed burst max of 5x.
*/
#define BURST_FACTOR	5
extern void qos_set_tdplane( portid_t pf, uint8_t* pctgs, uint8_t *bwgs, int ntcs, int mtu ) {
	int i;
	uint32_t cval;				// current value
	uint32_t offset;
	uint32_t mask = 0x3f000000;	// mask which preserves reserved bits in current value
	double	factor;				// factor necessary to convert percentage to credit value
	uint32_t group;				// values which are banged together and written to the register
	uint32_t credits;
	uint32_t max;				// max credits for a tc

	factor = compute_credit_factor( pctgs, ntcs, mtu );
	offset = 0x04910;			// RTTDT2C
	for( i = 0; i < ntcs; i++ ) {
		group = bwgs[i] << 9;
		credits = (int) (pctgs[i] * factor);
		max = (credits * BURST_FACTOR) << 12;
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask) | max | credits | group );		
		bleat_printf( 1, "qos: set tdplane:  tc=%d cur=0x%08x max=%d creds=%d grp=%d write: [%04x] -> 0x%02x",
			i, (int) cval, (int) credits * BURST_FACTOR, (int) credits, (int) bwgs[i], (int) offset, (int) (cval & mask) | max | credits | group );

		offset += 4;
	}
}

/*
	Set dcb transmit packet plane t2 config [0-7].
	For now we set equally.

	See qos_set_tdplane flower box for values.

	Corresponding ixgbe function:
		s32 ixgbe_dcb_config_tx_data_arbiter_82599(struct ixgbe_hw *hw, u16 *refill, u16 *max, u8 *bwg_id, u8 *tsa, u8 *map)

	This should be covered by the current DPDK DCB mode setup. (alas it does not)
*/
extern void qos_set_txpplane( portid_t pf, uint8_t* pctgs, uint8_t *bwgs, int ntcs, int mtu ) {
	int i;
	uint32_t cval;				// current value
	uint32_t offset;
	uint32_t mask = 0x3f000000;
	double	factor;
	uint32_t group;				// values which are banged together and written to the register
	uint32_t credits;
	uint32_t max;				// max credits for a tc

	factor = compute_credit_factor( pctgs, ntcs, mtu );

	offset = 0x0cd20;			// RTTPT2C
	for( i = 0; i < ntcs; i++ ) {
		group = bwgs[i] << 9;
		credits = (int) (pctgs[i] * factor);
		max = (credits * BURST_FACTOR) << 12;
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask) | max | credits | group );		
		bleat_printf( 1, "qos: set txpplane:  tc=%d cur=0x%08x max=%d creds=%d grp=%d write: [%04x] -> 0x%02x",
			i, (int) cval, (int) credits * BURST_FACTOR, (int) credits, (int) bwgs[i], (int) offset, (int) (cval & mask) | max | credits | group );

		offset += 4;
	}
}

/*
	Set dcb receive packet plane t2 config [0-7].
	For now we set equallay.

	See qos_set_tdplane flowerbox for details

	This should be covere by the current DPDK DCB mode setup.
*/
static void qos_set_rxpplane( portid_t pf ) {
	int i;
	uint32_t cval;				// current value
	uint32_t offset;
	uint32_t val = 0x1ff0ff;	// max=1ff group=0 credits=ff
	uint32_t mask = 0x3f000000;
	uint32_t group = 0x00;		// we'll just use a sequential group and assign each tc to its own for now

	offset = 0x02140;			//RTRPT4C
	for( i = 0; i < 8; i++ ) {
		group = i << 9;
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask) | val | group );		
		bleat_printf( 1, ">>>> qos: rtrp4tc  [%d] %08x & %08x | %08x = %08x", i, cval, mask, val, (cval & mask) | val| group  );

		offset += 4;
	}
}

extern void mlx5_set_vf_tcqos( sriov_port_t *port, uint32_t link_speed ) {
	int *shares = port->vftc_qshares;
	uint32_t rate;
	int vfid;
	int i, j;

	if (port->ntcs != 8) {
		bleat_printf( 1, "mlx5 set vf tc qos: Cannot set configuration if less then 8 tcs");
		return;
	}

	for(i = 0; i < port->num_vfs; i++) {
		vfid = port->vfs[i].num;
		if( vfid >= 0 ) {
			for(j = 0; j < MAX_TCS; j++) {
				rate = (uint32_t)((float)(link_speed * shares[(vfid * MAX_TCS) + j]) / 100);
				vfd_mlx5_set_vf_tcqos( port->rte_port_number, vfid, j, rate );
				bleat_printf( 2, "mlx5 set vf tc qos: port=%d vf=%d tc=%d rate_share=%d%% rate=%dMbps",
						port->rte_port_number, vfid, j, shares[(vfid * MAX_TCS) + j], rate );
			}
		}
	}
}
/*
	Accepts an array of percentages (rates), where each element defines a percentage of the related
	TC that the queue is to be given.  These percentages are converted into refil credits
	which are then written to the NIC for the corresponding queue. The assumption is that the queues
	are assigned to each VF in order such at if there are 4 traffic classes, queues 0 through 3 are
	assigned to VF0, TCs  through 3; queues 4 through 7 are assigned to VF1 TCs 0 through 3 etc. Thus
	the rates array is laid out in this maner.

	Credits are based on the upper limit of the MTU for the PF such that the
	MTU/64 is used as the base credit value for the queue assigned the smallest percentage
	in each traffic class. The credit values assigned to the other queues in the traffic class
	are determined using the ratio of the queue's percentage and the minimal percentage as a
	multiplier.
	As an example:
			MTU = 9000
			base = 9000/64 = 141
			qpctgs for TC0 = 10 13 20 57  (assuming 4 VFs)
			q0 would receive 141 credits (the base)
			q1 receives (13/10) * 141 = 184 credits
			q2 receives (20/10) * 141 = 282 credits
			q3 receives (57/10) * 141 = 804 credits

	MTU cannot be less than 1536 bytes (1.5 * 1024) and thus the minmum number of credits is 24.
*/
extern void qos_set_credits( portid_t pf, int mtu, int* rates, int tc8_mode ) {
	uint32_t	sel_offset = 0x04904;				// offset of the selector register
	uint32_t	reg_offset = 0x04908;				// register offset
	double		cred_factor[MAX_TCS];				// multiplier which converts a percentage into credit value
	uint32_t	cval;
	uint32_t	q;									// q index
	uint32_t	amt;								// amount to assign to each tc in the pool/queue
	uint32_t	mask;
	int			tc;
	int			i;
	int			j;

	int 	num_tcs = 4;

	if( tc8_mode ) {
		num_tcs = 8;
	}

	if( mtu < 1536 ) {			// see flowerbox
		mtu = 1536;
	}

	for( i = 0; i < num_tcs; i++ ) {				// find base for each TC
		cred_factor[i] = 100;						// start with a max pct value

		for( j = i; j < MAX_QUEUES; j += num_tcs ) {				// first find min pctg for this tc
			if( rates[j] > 0 && rates[j] < cred_factor[i] ) {
				cred_factor[i] = rates[j];
			}
		}

		bleat_printf( 3, "qos_set_credits: pf=%d tc=%d lowrate=%.2f", (int) pf, i, cred_factor[i] );
		cred_factor[i] = ((mtu/64.0) / cred_factor[i]);				// cred_factor is now a multiplier to convert pct into creds for the TC
		bleat_printf( 3, "qos_set_credits: pf=%d tc=%d factor=%.2f", (int) pf, i, cred_factor[i] );
	}
	
	mask = 0xffffc000;								// we set bits 0:13; we'll mask those off the current value first to preserve what might be set
	for( q = 0; q < MAX_QUEUES; q++ ) {				// set the credits for each of the possible queues
		tc = q % num_tcs;
		amt = ceil( (double)rates[q] * cred_factor[tc] );					// figure the amount for this pool

		// --- this seems dodgy if another process/thread can select before we make our second write ----
		port_pci_reg_write( pf, sel_offset, q );						// select the queue to work on
		cval = port_pci_reg_read( pf, reg_offset );						// read to preserve reserved bits
		port_pci_reg_write( pf, reg_offset, (cval & mask) | amt );		// set the credits
		if( amt > 0 ) {
			bleat_printf( 2, "qos set rate: q=%d mtu=%d rate=%d%% credits=%d cval&mask|amt=%08x", q, mtu, rates[q], amt, (cval & mask) | amt );
		}
	}
}


/*
	Set the flow control config for QoS.

	This should be set up in DPDK dcb stuff.
*/
static void qos_set_fcc( portid_t pf ) {
	uint32_t val;
	uint32_t cval = 0;				// current value read from nic
	uint32_t offset;
	uint32_t mask;

	offset = 0x03d00;		// FCCFG.TFCE=10b
	mask = 0xffffffe7;
	cval = port_pci_reg_read( pf, offset );
	val = 2 << 3;												// 10b (priority) when in dcb mode
	port_pci_reg_write( pf, offset, (cval & mask) | val );		// flip on our bits, and set
	bleat_printf( 1, ">>>> qos: tfce %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );

	offset = 0x04294;    	// MFLCN.RPFCE=1b RFCE=0b
	val = 0x1 << 2;
	mask =0xfffffff0;
	cval = port_pci_reg_read( pf, offset );
	port_pci_reg_write( pf, offset, (cval & mask) | val );		// flip on our bits, and set
	bleat_printf( 1, ">>>> qos: rpfce %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );
}

/*
	Set the max number of outstanding byte requests. (DTXMXSZRQ)
*/
static void qos_set_maxszreq( portid_t pf ) {
	uint32_t val = 0x10;			// 256 * 16 =  4KB

	port_pci_reg_write( pf, 0x08100, val );
}


/*
	Set the mapping for user priority 802.1p to TC. If tc8_mode is not
	set (4 TCs) then we map value paris (0 & 1, 2 & 3, etc) to TCs 0-3. If
	8 TCs are used, then it's a 1:1 mapping:


		Mapping such that higher tc number is higher priority
             --TC3-- --TC2-- --TC1-- --TC0--
		  4: 011 011 010 010 001 001 000 000 == 6d2240
		      P7  P6  P5  P4  P3  P2  P1  P0

			 TC7 TC6 TC5 TC4 TC3 TC2 TC1 TC0
		  8: 111 110 101 100 011 010 001 000 == fac688
			 TC7 TC6 TC5 TC4 TC3 TC2 TC1 TC0


		Not preferred, but mapping tc0 as higest priorty:
             --TC0-- --TC1-- --TC2-- --TC3--
		  4: 000 000 001 001 010 010 011 011 == 00949b
		      P7  P6  P5  P4  P3  P2  P1  P0

			 TC0 TC1 TC2 TC3 TC4 TC5 TC6 TC7
		  8: 000 001 010 011 100 101 110 111 == 053077
			 TC7 TC6 TC5 TC4 TC3 TC2 TC1 TC0

	
*/
static void qos_set_rup2tc( portid_t pf, int tc8_mode ) {
	uint32_t	val = 0x6d2240;		// tc4 default: see flower box
	uint32_t	cval;
	uint32_t	mask;
	uint32_t 	offset;

	if( tc8_mode ) {
		val = 0xfac688;
	}

	offset = 0x03020;				// RTRUP2TC
	mask = 0xff000000;
	cval = port_pci_reg_read( pf, offset );
	port_pci_reg_write( pf, offset, (cval & mask) | val );
	bleat_printf( 1, ">>>> qos: set_rup2tc %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );
}

/*
	Set transmit 802.1p priority to TC mapping. See rup2tc funciton
	header for more details
*/
static void qos_set_tup2tc( portid_t pf, int tc8_mode ) {
	uint32_t	val = 0x00949b;		// tc4 default: see flower box for set_rup2tc

	if( tc8_mode ) {
		val = 0x053077;
	}

	port_pci_reg_write( pf, 0x0c800, val );
}

/*
	Set the virt control registers.
	bit 0 should match bit 1 of mtqc (per data sheet)
*/
static void qos_set_vtctl( portid_t pf ) {
	uint32_t	val;
	uint32_t	cval;
	uint32_t	offset;
	uint32_t	mask;

	//xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx
	//rXXr rrrr rrrr rrrr rrrp pppp prrr rrr1
	//1001 1111 1111 1111 1110 0000 0111 1110

	mask = 0x9fffe07e;
	val = (1 << 29) + 1;		// drop packets which don't match a pool + enable VT
	//val = 1;					// enable VT

	offset = 0x051b0;			// PFVTCTL
	cval = port_pci_reg_read( pf, offset );
	port_pci_reg_write( pf, offset, (cval & mask) | val );				// no mask needed; we're only setting bits
	bleat_printf( 1, ">>> qos pfvtctl turn on: %08x", (cval & mask) | val );
}

/*
	Set the multiple transmit queues command reg based on 4 or 8 TCs.
	The doc has the wrong section identified for this. The values
	coded here are based on section 8.2.3.9.15.
*/
static void qos_set_mtqc( portid_t pf, int tc8_mode ) {
	uint32_t	val = 0x0b;			// default to dcb-ena, vt-ena, TC0-3 & 32 VMs (1011)
	uint32_t	mask = 0xffffff00;
	uint32_t	cval;				// current value
	uint32_t	offset = 0x8120;

	if( tc8_mode ) {
		val = 0x0f;					// dcb/vt,TC0-7 & 16 VMs
	}

	cval = port_pci_reg_read( pf, offset );
	port_pci_reg_write( pf, offset, (cval & mask) | val );				// no mask needed; we're only setting bits
	bleat_printf( 1, ">>> qos mtqc: offset=%8x %08x", offset, (cval&mask) | val );
}

/*
	Set the multiple receive queues command register based on 4 or 8 TCs

	xxxx xxxx xxxx xxxx xxxx xxxx xxxx xxxx
       0    0    0    0 rrrr rrrr rrrr 1101	== 4tcs
       0    0    0    0 rrrr rrrr rrrr 1100	== 8tcs

	00 00 ff f0	mask

	MRQC  = 0x0ec80
	1100b == virt & dcb 16 pools (8TCs)	== 0x0c
	1101b == virt & dcb 32 pools (4TCs) == 0x0d

	hashing bits:		not using rss, should be off
	.... .... .... ....
    rrrr rrrr 1111 rr11
	ff0c mask
    00f3 set
*/
static void qos_set_mrqc( portid_t pf, int tc8_mode ) {
	uint32_t	val = 0x0d;						// default for 4 TCs
	uint32_t	hash_bits = 0xf3 << 16; 		// TODO:  what hashing bits really need to be set
	uint32_t	cval;
	uint32_t	mask;
	uint32_t	offset = 0xec80;

	if( tc8_mode ) {
		val = 0x0c;
	}

	val |= hash_bits;
	mask = 0xff0c0ff0;
	//mask = 0x0000fff0;

	cval = port_pci_reg_read( pf, offset );
	bleat_printf( 1, ">>>> qos: mrqc  -low %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );
	port_pci_reg_write( pf, offset, (cval & mask) | val );
}

/*
	Set the tx and rx buffer sizes for 4 or 8 TC mode. In 4 TC mode, Values
	0-3 are set and 4-7 are cleared.  The tx threshold value is related, so
	it is also set in this function.
*/
static void qos_set_sizes( portid_t pf, int tc8_mode ) {
	int 		i;
	uint32_t	offset;				// offset of the register
	uint32_t	tx_low = 0x28;		// default values for 4 TC mode
	uint32_t	tx_high = 0x00;		// indexes 4-7
	uint32_t	rx_low = 0x80;		// indexes 0-3
	uint32_t	rx_high = 0x00;
	uint32_t	val;
	uint32_t	cval;
	uint32_t	mask;

	if( tc8_mode ) {
		tx_high = tx_low = 0x14;		// reset values if setting 8 TCs
		rx_high = rx_low = 0x40;
	}

	mask = 0xfff003ff;					// mask off bits we set from the current value
	val = tx_low << 10;					// values are inserted at bytes 10-19

	offset = 0x0cc00; 					// TDWBAH
	for( i = 0; i < 4; i++ ) {								// set TX 0-3
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask ) | val );
		bleat_printf( 1, ">>>> qos: tdwbah  -low %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );
		offset += 4;
	}

	val = tx_high << 10;
	for( i = 0; i < 4; i++ ) {								// clears TX 4-7 in 4 tc mode
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask ) | val );
		bleat_printf( 1, ">>>> qos: tdwbah  -high %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );
		offset += 4;
	}

	val = rx_low << 10;										// values are inserted at bytes 10-19
	offset = 0x3c00;
	for( i = 0; i < 4; i++ ) {								// set RX 0-3
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask ) | val );
		bleat_printf( 1, ">>>> qos: rdwbah  -low %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );
		offset += 4;
	}

	val = rx_high << 10;
	for( i = 0; i < 4; i++ ) {								// clears RX 4-7 in 4 tc mode
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask ) | val );
		bleat_printf( 1, ">>>> qos: rdwbah  -high %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );
		offset += 4;
	}

	mask = 0xfffffc00;
	offset = 0x04950;										// TXPBTHRESH
	val = tx_low;
	for( i = 0; i < 4; i++ ) {								// these values must match the tx_low and tx_high values set above
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask ) | tx_low );
		bleat_printf( 1, ">>>> qos: txpbthresh -low %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );
		offset += 4;
	}

	val = tx_high;
	for( i = 0; i < 4; i++ ) {								// clears if in 4 TC mode
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask ) | tx_high );
		bleat_printf( 1, ">>>> qos: txpbthresh -high %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );
		offset += 4;
	}
}

/*
	This was the main prototyping configuration function and is now DEPRECATED!

	Enable qos on the PF passed in, using the percentages given.
	Percentags is assumed to be an array of MAX_QUEUE integers with values
	in the range of 0 through 100 inclusive. These are grouped by VF with
	each VF consisting of ether 4 or 8 consecutive values; one vaue for
	each possible TC.

	With the extension of the tx packet and descriptor plane functions in this
	module, this function no longer sets up the TC percentages.

	Returns 0 if error (percentags total != 100% etc), 1
	otherwise.
*/

extern int enable_dcb_qos( sriov_port_t *port, int* pctgs, int tc8_mode, int option ) {
	portid_t pf;			// the port number for nic writes

	option1 = option;		// TESTING to set arbitor selector bit

	pf = port-> rte_port_number;

	qos_set_sizes( pf, tc8_mode );				// set packet buffer sizes and threshold
	qos_set_mrqc( pf, tc8_mode );				// set mult rec/tx queue control
	qos_set_mtqc( pf, tc8_mode );
	qos_set_vtctl( pf );

	// do NOT invoke set_queue_drop(); drop enable bit is set by callback when needed
	
	disable_default_pool( pf );					// prevent unknown packets from being placed on default queue

	/*
	DEPRECATED == this is handled by set_queue_drop
	for( i = 0; i < 32; i ++ ) {
		set_split_erop( pf, i, 1 );				// set split drop for all VFs
	}
	*/
	qos_set_rup2tc( pf, tc8_mode );				// user priority to traffic class mapping
	qos_set_tup2tc( pf, tc8_mode );
	qos_set_maxszreq( pf );

	qos_set_fcc( pf ); 							// from the list -- step 2

												// from the list step 3
	qos_set_credits( pf, port->mtu, pctgs, tc8_mode );		// set quantums based on percentages
	//qos_set_tdplane( pf );				// tc plane
	//qos_set_txpplane( pf );				// tx and rx packet plane
	qos_set_rxpplane( pf );

	qos_enable_arb( pf );				// part 4 from the list

	qos_set_minifg( pf );						// part 5 from the list

	return 1;
}

/*

TXPBSIZE[i] = 0x0CC00 + ((i) * 4))		31:20(reserved) 19:10(size) 9:0(reserved)
RXPBSIZE[i] = 0x03C00 + ((i) * 4))		31:20(reserved) 19:10(size) 9:0(reserved)
MRQC 0x0ec80	31:16(hash field selection) 15(res) 14:4(res) 3:0(mrq enable bits)
				1100b == virt & dcb 16 pools 8TCs
				1101b == virt & dcb 32 pools 4TCs

RTTDT1C	0x04908
RTTST1S 0x0490c

Setting up for qos:
 the list --  from datasheet (approximately) on page 181

1. Configure packet buffers, queues, and traffic mapping:
    - 8 TCs mode - Packet buffer size and threshold, typically
        RXPBSIZE[0-7].SIZE=0x40
        TXPBSIZE[0-7].SIZE=0x14
        but non-symmetrical sizing is also allowed (see Section 8.2.3.9.13 for rules)
        TXPBTHRESH.THRESH[0-7]=TXPBSIZE[0-7].SIZE - Maximum expected Tx
        packet length in this TC

    - 4 TCs mode - Packet buffer size and threshold, typically
        RXPBSIZE[0-3].SIZE=0x80,
        RXPBSIZE[[4-7].SIZE=0x0

        TXPBSIZE[0-3].SIZE=0x28,
        TXPBSIZE[4-7].SIZE=0x0

        but non-symmetrical sizing among TCs[0-3] is also allowed (see
        Section 8.2.3.9.13 for rules)

        TXPBTHRESH.THRESH[0-3]=TXPBSIZE[0-3].SIZE - Maximum expected Tx
        packet length in this TC

        TXPBTHRESH.THRESH[4-7]=0x0

    - Multiple Receive and Transmit Queue Control (MRQC and MTQC)
        Set MRQC.MRQE to 1xxxb, with the three least significant bits set according
        to the number of VFs, TCs, and RSS mode as described in Section 8.2.3.7.12.

            8.2.3.7.12 Multiple Receive Queues Command Register- MRQC (0x0EC80 / 0x05818; RW)
            MRQE bits 3:0
            1101b = Virtualization and DCB — 32 pools, each allocated 4 TCs.
            14:4 reserved
            15 reserved
            31:16 various hash function enabling bits (see page 589)


        Set both RT_Ena and VT_Ena bits in the MTQC register.

        Set MTQC.NUM_TC_OR_Q according to the number of TCs/VFs enabled as
        described in Section 8.2.3.7.16. <<< typo in datasheet
		REALLY: 8.2.3.9.15

    - Set the PFVTCTL.VT_Ena (as the MTQC.VT_Ena)

    - Queue Drop Enable (PFQDE) - In SR-IO the QDE bit should be set to 1b in the
        PFQDE register for all queues. In VMDq mode, the QDE bit should be set to 0b for
        all queues.

    - Split receive control (SRRCTL[0-127]): Drop_En=1 - drop policy for all the
        queues, in order to avoid crosstalk between VMs

    - Rx User Priority (UP) to TC (RTRUP2TC)

    - Tx UP to TC (RTTUP2TC)

    - DMA TX TCP Maximum Allowed Size Requests (DTXMXSZRQ) - set
        Max_byte_num_req = 0x010 = 4 KB

2. Enable PFC and disable legacy flow control:
    - Enable transmit PFC via: FCCFG.TFCE=10b
    - Enable receive PFC via: MFLCN.RPFCE=1b
    - Disable receive legacy flow control via: MFLCN.RFCE=0b
    - Refer to Section 8.2.3.3 for other registers related to flow control

3. Configure arbiters, per TC[0-1]:
    - Tx descriptor plane T1 Config (RTTDT1C) per queue, via setting RTTDQSEL first.
        Note that the RTTDT1C for queue zero must always be initialized.
    - Tx descriptor plane T2 Config (RTTDT2C[0-7])
    - Tx packet plane T2 Config (RTTPT2C[0-7])
    - Rx packet plane T4 Config (RTRPT4C[0-7])

4. Enable TC and VM arbitration layers:
    - Tx Descriptor plane Control and Status (RTTDCS), bits:
        TDPAC=1b, VMPAC=1b, TDRM=1b, BDPM=1b, BPBFSM=0b
    - Tx Packet Plane Control and Status (RTTPCS): TPPAC=1b, TPRM=1b,
        ARBD=0x004
    - Rx Packet Plane Control and Status (RTRPCS): RAC=1b, RRM=1b


5. Set the SECTXMINIFG.SECTXDCB field to 0x1F.

*/
