// vi: sw=4 ts=4 noet:

/*
	Mnemonic:	vfd_mac.c
	Abstract:	Functions which support managing MAC addresses.
				
	Author:		E. Scott Daniels
	Date:		28 October 2017  (broken from main.c and added extensions.

	Mods:
*/


#include <vfdlib.h>		// if vfdlib.h needs an include it must be included there, can't be include prior
#include <symtab.h>		// our symbol table things
#include "sriov.h"


/*
	We use a staic symbol table to map MACs. Each VF has a slice of the table
	based on their RTE port number.  We don't worry about tracking random MAC
	addresses, but when a VF is removed from our control we will generate a 
	random address to it so that if the guest restarts on a differnt VF and 
	decides to push the same MAC in as the default there won't be a collision.	
*/
static void*	mac_stab = NULL;

// -----------------------------------------------------------------------------------------------------------

/*
	Generates a psuedo random mac address into the caller's buffer and ensures that it is unicast and that the 
	local assignment bit is on (IEEE802). We aren't generating these for security so the system rand function 
	is fine.
*/
static void gen_rand_mac( unsigned char* mac ) {
	int r;
	int i;

	r = rand();
	for( i = 0; i < 6; i++ ) {
		mac[i] = r & 0xff;
		r >>= 4;
	}
	mac[0] &= 0xfe;				// unicast
	mac[0] |= 0x02;				// local -- should prevent collision with any vendor assigned mac leading 3
}

/*
	Generate a random human readable mac string.
	Caller must free.
*/
static char* gen_rand_hrmac( void ) {
	char wbuf[128];
	unsigned char mbuf[8];

	gen_rand_mac( mbuf );
	snprintf( wbuf, sizeof( wbuf ), "%02x:%02x:%02x:%02x:%02x:%02x", mbuf[0], mbuf[1], mbuf[2], mbuf[3], mbuf[4], mbuf[5] );
	return strdup( wbuf );
}

/*
	Validate the string passed in contains a plausable MAC address of the form:
		hh:hh:hh:hh:hh:hh

	Returns 0 if invalid, 1 if ok.
*/
static int is_valid_mac_str( char* mac ) {
	char*	dmac;				// dup so we can bugger it
	char*	tok;				// pointer at token
	char*	strtp = NULL;		// strtok_r reference
	int		ccount = 0;
	

	if( strlen( mac ) < 17 ) {
		return 0;
	}

	for( tok = mac; *tok; tok++ ) {
		if( ! isxdigit( *tok ) ) {
			if( *tok != ':' ) {				// invalid character
				return 0;
			} else {
				ccount++;					// count colons to ensure right number of tokens
			}
		}
	}

	if( ccount != 5 ) {				// bad number of colons
		return 0;
	}
	
	if( (dmac = strdup( mac )) == NULL ) {
		return 0;							// shouldn't happen, but be parinoid
	}

	tok = strtok_r( dmac, ":", &strtp );
	while( tok ) {
		if( atoi( tok ) > 255 ) {			// can't be negative or sign would pop earlier check
			free( dmac );
			return 0;
		}
		tok = strtok_r( NULL, ":", &strtp );
	}
	free( dmac );

	return 1;
}

// --------------------- public ------------------------------------------------------------------------------

/*
	Do any initialisation that is necessary:
		1) create the symtab
		2) collect the random addresses assigned

	Returns 1 on success. If called a second time, it will return 1.
*/
extern int mac_init( void ) {

	if( mac_stab ) {			// already initialised, don't collect bad MAC addresses
		return 1;
	}

	srand( (int) (getpid() + time( NULL ))  );			// set seed for randomised mac addresses

	if( (mac_stab = sym_alloc( 1023 )) == NULL ) {
		bleat_printf( 0, "CRI: unable to allocate mac symbol table" );
		return 0;
	}

	return 1;
}


/*
	Checks to see if the MAC (human readable) is valid from the perspective of:

		1) the addition of a MAC does not cause the PF limit to be exceeded 
		2) the VF has room for another MAC.  
		3) the MAC is not duplicated on another VF on the same PF

	O is returned if any of these does not hold;

	0 is also returned if we are not able to map the pf/vf combination. 

	If valid; 1 is returned.
*/
extern int can_add_mac( int port, int vfid, char* mac ) {
	struct vf_s* vf = NULL;				// references to our pf/vf structs
	struct sriov_port_s* p = NULL;
	int total = 0;						// number of MACs defined for the PF
	int i;
	void* sresult;						// result from the symtab look up
	

	if( ! is_valid_mac_str( mac ) ) {
		bleat_printf( 1, "can_add_mac: mac is not valid: %s", mac );
		return 0;
	}

	if( (p = suss_port( port )) == NULL ) {
		bleat_printf( 1, "can_add_mac: port doesn't map: %d", port );
		return 0;
	}

	if( (vf = suss_vf( port, vfid )) == NULL ) {
		bleat_printf( 1, "can_add_mac: vf doesn't map: pf/vf=%d/%d", port, vfid );
		return 0;
	}

	if( (sresult = sym_get( mac_stab, mac, port )) ) {			// see if defined for any VF on the PF
		bleat_printf( 1, "can_add_mac: mac is already assigned to on port %d: %s", port, mac );
		return 0;
	}

	for( i = 0; i < p->num_vfs; i++ ) {			// we do NOT depend on a counter inc'd and dec'd we count every time to be sure
		total += p->vfs[i].num_macs;
	}

	if( total+1 > MAX_PF_MACS ) {
		bleat_printf( 1, "can_add_mac: adding mac would exceed PF limit: pf/vf=%d/%d current_pf=%d mac=%s", port, vfid, total, mac );
		return 0;
	}

	if( vf->num_macs +1 > MAX_VF_MACS ) {
		bleat_printf( 1, "can_add_mac: adding mac would exceed VF limit: pf/vf=%d/%d current_vf=%d mac=%s", port, vfid, vf->num_macs, mac );
		return 0;
	}

	return 1;
}

/*
	Accepts a mac string and adds it to the list of MACs for the pf/vf provided that it is valid
	(see can_add_mac() function).

	If the MAC is already listed for the PF/VF given, then we do nothing and silently
	ignore the call returning 1 (success).

	The parm mac is expected to be an ASCII-z string in human readable xx:xx... form.

	This function does NOT push anything out to the NIC; it only sets the MAC addresses
	up in the VF struct which is then used by the functions that acutually update the 
	NIC at the appropriate time(s).
*/
extern int add_mac( int port, int vfid, char* mac ) {
	struct vf_s* vf = NULL;				// references to our pf/vf structs
	struct sriov_port_s* p = NULL;
	int total = 0;						// number of MACs defined for the PF
	int i;
	
	if( (p = suss_port( port )) == NULL ) {
		bleat_printf( 1, "add_mac: port doesn't map: %d", port );
		return 0;
	}

	if( (vf = suss_vf( port, vfid )) == NULL ) {
		bleat_printf( 1, "add_mac: vf doesn't map: pf/vf=%d/%d", port, vfid );
		return 0;
	}

																// this check must be BEFORE can_add_mac() call
	for( i = vf->first_mac; i <= vf->num_macs; i++ ) {			// if duplicate of what defined for this VF, then its OK
		if( strcmp( vf->macs[i], mac ) == 0 ) {
			bleat_printf( 1, "add_mac: no action needed: mac already in list for: pf/vf=%d/%d mac=%s", port, vfid, mac );
			return 1;
		}
	}

	if( ! can_add_mac( port, vfid, mac ) ) {
		return 0;										// reason logged in call
	}

	//  --- all vetting must be before this, at this point we're good to add, so update things ----------------
	bleat_printf( 2, "add_mac: allowed: pf/vf=%d/%d counts=%d/%d %s", port, vfid, total+1, vf->num_macs+1, mac );

	sym_map( mac_stab, mac, port, (void*) 1 );			// assign this to the PF for dup checking
	vf->num_macs++;
	strncpy( vf->macs[vf->num_macs], mac, 17 );			// will add final 0 if a:b:c style resulting in short string
	vf->macs[vf->num_macs][17] = 0;						// if long string passed in; ensure 0 terminated

	return 1;
}


/*
	Clears all of the MAC addresses that have been assigned to this PF/VF combination.
	The setting of random determines how the default MAC address is handled. If 
	random == true, then the default MAC address is replaced with a randomly generated
	address in the same manner that one was generated by the PF when VFd initialised.
	If random == false, then the default MAC is left UNCHANGED, with the net effect
	of this function to clear only the 'white list' MAC addresses. 

	Returns 0 on failure; 1 on success.
*/
extern int clear_macs( int port, int vfid, int assign_random ) {
	struct vf_s* vf = NULL;				// references to our pf/vf information
	struct sriov_port_s* pf = NULL;
	char*	mac;
	char*	rmac;						// random mac
	int m;
	
	if( (pf = suss_port( port )) == NULL ) {
		bleat_printf( 1, "clear_macs: port doesn't map: %d", port );
		return 0;
	}

	if( (vf = suss_vf( port, vfid )) == NULL ) {		// not really an 'error' as the config might be deleted, but the driver may still call back for it
		bleat_printf( 2, "clear_macs: vf doesn't map: pf/vf=%d/%d", port, vfid );
		return 0;
	}

	for( m =  vf->first_mac + 1; m <= vf->num_macs; ++m ) {				// for all but the default
		mac = vf->macs[m];
		bleat_printf( 2, "clear macs:  [%d] pf/vf=%d/%d %s", m, pf->rte_port_number, vf->num, mac );
		
		sym_del( mac_stab, vf->macs[m], port );							// nix from the symtab
		set_vf_rx_mac( port, mac, vfid, SET_OFF );						// clear from 'white list'
	}

	if( assign_random ) {										// if replacing the default, do so with a random address
		sym_del( mac_stab, vf->macs[vf->first_mac], port );		// ensure old one is not in the symtab

		rmac = gen_rand_hrmac();								// random mac to push into the nic
		set_vf_default_mac( port, rmac, vfid );

		bleat_printf( 2, "clear macs: replacing default %s with random: %s", vf->macs[vf->first_mac], rmac );
		free( rmac );

		vf->num_macs = 0;		// at this point we are not shoving any addresses to the NIC for this VF
	} else {
		bleat_printf( 2, "clear macs: leaving default [%d] %s", vf->first_mac, vf->macs[vf->first_mac] );
		vf->num_macs = 1;		// we are leaving the default in place so adjust
	}

	return 1;
}

/*
	Pushes the mac string onto the head of the list for the given port/vf combination. Sets
	the first mac index to be 0 so that it is used if a port/vf reset is triggered.
	Mac is a nil terminated ascii string of the form xx:xx:xx:xx:xx:xx.

	Returns 1 if mac can and was pushed on the list, 0 on error.
*/
extern int push_mac( int port, int vfid, char* mac ) {
	struct vf_s* vf;
	
	if( (vf = suss_vf( port, vfid )) == NULL ) {
		bleat_printf( 2, "push_mac: vf doesn't map: pf/vf=%d/%d", port, vfid );
		return 0;
	}

	
	if( vf->num_macs > 0  && strcmp( mac, vf->macs[vf->first_mac] ) == 0 ) {		// we already have this as the default
		bleat_printf( 2, "push_mac: mac is already default for pf/vf=%d/%d [%d]: %s", port, vfid, vf->first_mac, mac );
		return 1;
	}

	if( ! can_add_mac( port, vfid, mac ) ) {
		return 0;								// reason is logged by can_add function, so no msg here
	}

	vf->first_mac = 0;
	strcpy( vf->macs[0], mac );			// make our copy

	bleat_printf( 1, "push_mac: default mac pushed onto head of list: pf/vf=%d/%d %s", port, vfid, vf->macs[0] );
	return 1;
}

/*
	Run the list of MAC addresses whe have associated with the VF and push them out to the NIC.
	We run the list in _reverse_ order because on some NICs the last one pushed is assumed to be
	the default MAC (there is no specific set default api).

	Returns 0 on failure; 1 on success.
*/
extern int set_macs( int port, int vfid ) {
	struct vf_s* vf = NULL;				// references to our pf/vf information
	struct sriov_port_s* pf = NULL;
	char*	mac;
	int m;
	
	if( (pf = suss_port( port )) == NULL ) {
		bleat_printf( 1, "setl_macs: port doesn't map: %d", port );
		return 0;
	}

	if( (vf = suss_vf( port, vfid )) == NULL ) {
		bleat_printf( 1, "clear_macs: vf doesn't map: pf/vf=%d/%d", port, vfid );
		return 0;
	}

	bleat_printf( 1, "configuring %d mac addresses on pf/vf=%d/%d firstmac=%d", vf->num_macs, port, vfid, vf->first_mac );
	for( m = vf->num_macs; m >= vf->first_mac; m-- ) {
		mac = vf->macs[m];
		bleat_printf( 2, "adding mac [%d]: port: %d vf: %d mac: %s", m, port, vfid, mac );

		if( m > vf->first_mac ) {
			set_vf_rx_mac( port, mac, vfid, SET_ON );	// set in whitelist
		} else {
			set_vf_default_mac( port, mac, vfid );		// first is set as default
			bleat_printf( 2, "Setting default mac was succesfull");
		}
	}

	return 1;
}
