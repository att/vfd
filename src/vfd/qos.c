
/*
	Mnemonic:	qos.c
	Abstract:	Functions to support management of qos related controls
				on the NIC. 
	Author:		E. Scott Daniels
	Date:		06 June 2016
*/

#include "sriov.h"
#include <vfdlib.h>		// if vfdlib.h needs an include it must be included there, can't be include prior

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

                        |--- bypass buffer free space mon (must be cleared for sriov or dcb)
                        |                              |--- 0==RR 1=WRR (VMPAC)
                        |                              |
                        |                              ||-- 0==RR 1==WSP (TDPAC)
                        |                              ||
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
	
*/
static void qos_enable_arb( portid_t pf ) {
	uint32_t	offset;
	uint32_t	val;
	uint32_t	cval;		// value currently set (preserve reserved)
	uint32_t	mask;		// reserved and current preservation mask

	offset = 0x04900;			// RTTDCS
	mask = 0xff3fffac;			
	val = 0x400013;
	cval = port_pci_reg_read( pf, offset );						// snag current 
	port_pci_reg_write( pf, offset, (cval & mask) | val );		
	bleat_printf( 1, ">>>> qos: set_enable_rttdcs %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );

	offset = 0x0cd00;			// RTTPCS
	mask = 0x003ffedf;
	val = 0x01000120;
	cval = port_pci_reg_read( pf, offset );						// snag current 
	port_pci_reg_write( pf, offset, (cval & mask) | val );		// add our bits or clear what we don't want
	bleat_printf( 1, ">>>> qos: set_enable_rttpcsarb %08x & %08x | %08x = %08x", cval, mask, val, (cval & mask) | val );

	offset = 0x02430;			// RTRPCS
	val = 0x06;
	cval = port_pci_reg_read( pf, offset );						// snag current 
	port_pci_reg_write( pf, offset, cval | val );				// flip on our bits (no mask needed since we aren't clearing bits)
	bleat_printf( 0, ">>> rtrpcs: %08x/nomask %08x = %08x", cval, val, (cval) | val );
}

/*
	NOTE: these are three separate functions as some day we might be 
	setting different values in each. Else they could be the same 
	function that accept the offset.
*/

/*
	Set dcb transmit descriptor plane t2 config [0-7].
	For now we set equallay.
*/
static void qos_set_tdplane( portid_t pf ) {
	int i;
	uint32_t cval;			// current value
	uint32_t offset;
	uint32_t val = 0x001ff0ff;	// max=1ff (23:12)  group=0 tc-credits=ff (8:0)
	uint32_t mask = 0x3f000000;

	offset = 0x04910;		// RTTDT2C
	for( i = 0; i < 8; i++ ) {
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask) | val );		
		bleat_printf( 1, ">>>> qos: rttdt2c  [%d] %08x & %08x | %08x = %08x", i, cval, mask, val, (cval & mask) | val );

		offset += 4;
	}
}

/*
	Set dcb transmit packet plane t2 config [0-7].
	For now we set equallay.
*/
static void qos_set_txpplane( portid_t pf ) {
	int i;
	uint32_t cval;				// current value
	uint32_t offset;
	uint32_t val = 0x1ff0ff;	// max=1ff group=0 credits=ff
	uint32_t mask = 0x3f000000;

	offset = 0x0cd20;			// RTTPT2C
	for( i = 0; i < 8; i++ ) {
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask) | val );		
		bleat_printf( 1, ">>>> qos: rttpt2c  [%d] %08x & %08x | %08x = %08x", i, cval, mask, val, (cval & mask) | val );

		offset += 4;
	}
}
/*---
	int i;
	uint32_t cval;			// current value
	uint32_t offset;

	offset = 0x0cd20;		// RTTPT2C
	for( i = 0; i < 8; i++ ) {
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & 0x3f000000) );		

		offset += 4;
	}
}
*/

/*
	Set dcb receive packet plane t2 config [0-7].
	For now we set equallay.
*/
static void qos_set_rxpplane( portid_t pf ) {
	int i;
	uint32_t cval;			// current value
	uint32_t offset;
	uint32_t val = 0x1ff0ff;	// max=1ff group=0 credits=ff
	uint32_t mask = 0x3f000000;

	offset = 0x02140;		//RTRPT4C
	for( i = 0; i < 8; i++ ) {
		cval = port_pci_reg_read( pf, offset );
		port_pci_reg_write( pf, offset, (cval & mask) | val );		
		bleat_printf( 1, ">>>> qos: rtrp4tc  [%d] %08x & %08x | %08x = %08x", i, cval, mask, val, (cval & mask) | val );

		offset += 4;
	}
}

/*
	Set queue transmit/receive 'rates'.
	FIXME: Right now this assumes 4 TCs and assigns the same percentage of credits to each.
*/
static void qos_set_rates( portid_t pf, int* rates ) {
	uint32_t	sel_offset = 0x04904;				// offset of the selector register
	uint32_t	reg_offset = 0x04908;				// register offset
	uint32_t	one_cred = 163;						// credits for one percent
	int v;											// VF index
	int t;											// tc index
	uint32_t q;										// q index
	uint32_t	amt;								// amount to assign to each tc in the pool/queue

	q = 0;
	for( v = 0; v < 32; v++ ) {						// set the credits for each of the possible queues
		amt = rates[v] * one_cred;					// figure the amount for this pool

		for( t = 0; t < 4; t++ ) {
			port_pci_reg_write( pf, sel_offset, q );		// select the queue to work on
			q++;
			port_pci_reg_write( pf, reg_offset, amt );		// set the credits 
			bleat_printf( 2, "qos set rate: [%d=%d] rate=%d credits=%d", t, q, rates[v], amt );
		}
	}
}

/*
	Set the flow control config for QoS.
*/
static void qos_set_fcc( portid_t pf ) {
	uint32_t val;
	uint32_t cval = 0;				// current value read from nic
	uint32_t offset;
	uint32_t mask;

	offset = 0x03d00;		// FCCFG.TFCE=10b
	mask = 0xffffffe7;
	cval = port_pci_reg_read( pf, offset );
	val = 1 << 3;
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

	offset = 0x3020;				// RTRUP2TC
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
*/
static void qos_set_vtctl( portid_t pf ) {
	uint32_t	val;
	uint32_t	cval;
	uint32_t	offset;

	val = (1 << 29) + 1;		// drop packets which don't match a pool + enable VT

	offset = 0x051b0;			// PFVTCTL
	cval = port_pci_reg_read( pf, offset );
	port_pci_reg_write( pf, offset, cval | val );				// no mask needed; we're only setting bits
	bleat_printf( 1, ">>> qos pfvtctl turn on: %08x", val );
}

/*
	Set the multiple transmit queues command reg based on 4 or 8 TCs.
	The doc has the wrong section identified for this. The values 
	coded here are based on section 8.2.3.9.15. 
*/
static void qos_set_mtqc( portid_t pf, int tc8_mode ) {
	uint32_t	val = 0x0b;			// default to dcb-ena, vt-ena, TC0-3 & 32 VMs 
	uint32_t	cval;				// current value
	uint32_t	offset = 0x8120;

	if( tc8_mode ) {
		val = 0x0f;					// dcb/vt,TC0-7 & 16 VMs
	}

	cval = port_pci_reg_read( pf, offset );
	port_pci_reg_write( pf, offset, cval | val );				// no mask needed; we're only setting bits
}

/*
	Set the multiple receive queues command register based on 4 or 8 TCs

	MRQC  = 0x0ec80
	1100b == virt & dcb 16 pools (8TCs)	== 0x0c
	1101b == virt & dcb 32 pools (4TCs) == 0x0d

	hashing bits:
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
	Enable qos on the PF passed in, using the percentages given.
	The percentags is assumed to be an array of 32 integers with values
	in the range of -1 through 100 inclusive. A value <0 indicates that
	we should NOT configure the queues associated with the VF. 

	Returns 0 if error (percentags total != 100% etc), 1 
	otherwise.
*/
extern int enable_dcb_qos( portid_t pf, int* pctgs, int tc8_mode ) {
	int i;
	int sum;

	sum = 0;
	for( i = 0; i < 32; i++ ) {
		if( pctgs[i] > 0 ) {
			sum += pctgs[i];
		}	
	}

	if( sum > 100 || sum <= 0 ) {				// we allow total to be less than 100%
		bleat_printf( 2, "qos enable: bad sum for pf %d: %d", pf, sum );
		return -1;
	}

												// from the list on pg 181, step 1
	qos_set_sizes( pf, tc8_mode );				// set packet buffer sizes and threshold
	qos_set_mrqc( pf, tc8_mode );				// set mult rec/tx queue control
	qos_set_mtqc( pf, tc8_mode );
	qos_set_vtctl( pf );
	set_queue_drop( pf, 1 );					// enable queue dropping for all
	for( i = 0; i < 32; i ++ ) {
		set_split_erop( pf, i, 1 );				// set split drop for all VFs
	}
	qos_set_rup2tc( pf, tc8_mode );				// user priority to traffic class mapping
	qos_set_tup2tc( pf, tc8_mode );
	qos_set_maxszreq( pf );

	qos_set_fcc( pf ); 							// from the list -- step 2

												// from the list step 3
	qos_set_rates( pf, pctgs );					// set quantums based on percentages
	qos_set_tdplane( pf );				// tc plane
	qos_set_txpplane( pf );			// tx and rx packet plane
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
