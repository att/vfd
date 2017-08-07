// vi: sw=4 ts=4 noet:
/*
	Mnemonic:	vfd -- VF daemon
	Abstract: 	Daemon which manages the configuration and management of VF interfaces
				on one or more NICs.
				Original name was sriov daemon, so some references to that (sriov.h) remain.

	Date:		February 2016
	Authors:	Alex Zelezniak (original code)
				E. Scott Daniels (extensions)

	Mods:		25 Mar 2016 - Corrected bug preventing vfid 0 from being added.
							Added initial support for getting mtu from config.
				28 Mar 2016 - Allow a single vlan in the list when stripping.
				29 Mar 2016 - Converted parms in main() to use global parms; needed
							to support callback.
				30 Mar 2016 - Added parm to bleat log to cause it to roll at midnight.
				01 Apr 2016 - Add ability to suss individual mtu for each pciid defined in
							the /etc parm file.
				15 Apr 2016 - Added check to ensure that the total number of MACs or the
							total number of VLANs across the PF does not exceed the max.
				19 Apr 2016 - Changed message when vetting the parm list to eal-init.
				20 Apr 2016 - Removed newline after address in the stats output message.
				21 Apr 2016 - Insert tag option now mirrors the setting for strip tag.
				24 Apr 2016 - Redid signal handling to trap anything that has a default action
							that isn't ignore; we must stop gracefully at all costs.
				29 Apr 2016 - Removed redundant code in restore_vf_setings(); now calls
							update_nic() function.
				06 May 2016 - Added some messages to dump output. Now forces the drop packet if
							no descriptor available on both port (all queues) and VFs.
				13 May 2016 - Deletes config files unless keep option in the master parm file is on.
				26 May 2016 - Added validation for vlan ids in range and valid mac strings.
							Added support to drive virsh attach/detach commands at start to
							force a VM to reset their driver.
				02 Jun 2016 - Added log purging set up in bleat.
				13 Jun 2016 - Version bump to indicate inclusion of better type checking used in lib.
							Change VLAN ID range bounds to <= 0. Correct error message when rejecting
							because of excessive number of mac addresses.
				19 Jul 2016 - Correct problem which was causing huge status responses to be
							chopped.
				20 Jul 2016 - Correct use of config struct after free.
				09 Aug 2016 - Block VF0 from being used.
				07 Sep 2016 - Drop use of TAILQ as odd things were happening realted to removing
							items from the list.
				14 Oct 2016 - Changes to work with dpdk-1611 differences.
				26 Oct 2016 - Removed invalid option listed in usage message. Added long version string support.
				01 Nov 2016 - renamed var to port2config_map to avoid looking like dpdk var/function (lead rte)
                03 Jan 2017 - Add new string compare function to ignore case-sensitive strings,
                            fix to link_status vfconfig param
				31 Jan 2017 - ensure that the *cast settings are restored after vlan and vlanmac callbacks.
				11 Feb 2017 - Set drop enable bit on VF's queues only between reset and queue ready. Setting
							on constantly causes packet loss on the NIC.  Change the hardware CRC strip
							setting to true. Add ability to turn off hardware crc stripping via the main
							parm file.
				23 Feb 2017 - Allow multiple VLAN IDs with strip == true.
				24 Feb 2017 - Corrected bug causing signal triggered termination when running attached
							to the tty and the window is resized.
				16 May 2017 - To switch -F flag meaning: now means enable flow control rather than disable.
							Flow control can also be enabled in the parm file.
				22 May 2017 - Now set the first MAC in the list as the 'default' and the rest as members of
							the white list.  The same checks for the total number of MACs still apply.
				23 May 2017 - Made log messages during config update consistant.
				24 May 2017 - If the guest pushes a MAC, that will be saved and used as the default rather
							than the first in the list.
				26 May 2017 - Allow promisc mode on PF to be optionally disabled via main config file.
				08 Jun 2017 - Add support to disable huge pages.
				23 Jun 2017 - Ensure socket mem isn't asked for if no-huge is given.
*/


#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>


#include "sriov.h"		// main header file
#include <vfdlib.h>		// if vfdlib.h needs an include it must be included there, can't be include prior
#include "vfd_rif.h"	// request interface stuff
#include "vfd_dcb.h"	// dcb related stuff

//#include "ixgbe_ethdev.h"
//#include "ixgbe_ethdev.h"


#define DEBUG
#define MAX_ARGV_LEN	64		// number of parms (max) passed on eal_init call

// ---------------------globals: bad form, but unavoidable -------------------------------------------------------
static parms_t *g_parms = NULL;											// dpdk callback does not allow data pointer so we must have a global. all other functions should accept a pointer!


// -- global initialisation ----

const char *version = VFD_VERSION "    build: " __DATE__ " " __TIME__;
const char *vnum = "v2";

// --- misc support ----------------------------------------------------------------------------------------------

/*
	stricmp will compair two strins in strcmp() fashion, but will
	ignore the character case.
*/
extern int stricmp(const char *s1, const char *s2)
{
    char c1, c2;

    if ( s1==s2 )
        return 0;

    if ( s1==0 )
        return -1;

    if ( s2==0 )
        return 1;

    do {
        c1 = tolower(*s1);
        c2 = tolower(*s2);
        s1++;
        s2++;
    } while ( (c1 != 0) && (c1 == c2) );

    return (int)(c1 - c2);
}

/*
	Validate the string passed in contains a plausable MAC address of the form:
		hh:hh:hh:hh:hh:hh

	Returns -1 if invalid, 0 if ok.
*/
int is_valid_mac_str( char* mac ) {
	char*	dmac;				// dup so we can bugger it
	char*	tok;				// pointer at token
	char*	strtp = NULL;		// strtok_r reference
	int		ccount = 0;
	

	if( strlen( mac ) < 17 ) {
		return -1;
	}

	for( tok = mac; *tok; tok++ ) {
		if( ! isxdigit( *tok ) ) {
			if( *tok != ':' ) {				// invalid character
				return -1;
			} else {
				ccount++;					// count colons to ensure right number of tokens
			}
		}
	}

	if( ccount != 5 ) {				// bad number of colons
		return -1;
	}
	
	if( (dmac = strdup( mac )) == NULL ) {
		return -1;							// shouldn't happen, but be parinoid
	}

	tok = strtok_r( dmac, ":", &strtp );
	while( tok ) {
		if( atoi( tok ) > 255 ) {			// can't be negative or sign would pop earlier check
			free( dmac );
			return -1;
		}
		tok = strtok_r( NULL, ":", &strtp );
	}
	free( dmac );

	return 0;
}

/*
	Run start and stop user commands.  These are commands defined by
	either the start_cb or stop_cb tags in the VF's config file. The
	commands are run under the user id which owns the config file
	when it was presented to VFd for addition. The commands are generally
	to allow the 'user' to hot-plug, or similar, a device on the VM when
	VFd is cycled.  This might be necessary as some drivers do not seem
	to reset completely when VFd reinitialises on start up.

	State of the command is _not_ captured; it seems that the dpdk lib
	fiddles with underlying system() calls and the status returns -1 regardless
	of what the command returns.

	Output from these user defined commands goes to standard output or
	standard error and won't be capture in our log files.
*/
static void run_start_cbs( sriov_conf_t* conf ) {
	int i;
	int j;
	struct sriov_port_s* port;
	struct vf_s *vf;

	for (i = 0; i < conf->num_ports; ++i){							// run each port we know about
		port = &conf->ports[i];

	    for( j = 0; j < port->num_vfs; ++j ) { 			// traverse each VF and if we have a command, then drive it
			vf = &port->vfs[j];				   			// convenience

			if( vf->num >= 0  &&  vf->start_cb != NULL ) {
				user_cmd( vf->owner, vf->start_cb );		
				bleat_printf( 1, "start_cb for pf=%d vf=%d executed: %s", i, j, vf->start_cb  );
			}
		}
	}
}

static void run_stop_cbs( sriov_conf_t* conf ) {
	int i;
	int j;
	struct sriov_port_s* port;
	struct vf_s *vf;

	for (i = 0; i < conf->num_ports; ++i){							// run each port we know about
		port = &conf->ports[i];

	    for( j = 0; j < port->num_vfs; ++j ) { 			// traverse each VF and if we have a command, then drive it
			vf = &port->vfs[j];				   			// convenience

			if( vf->num >= 0  &&  vf->stop_cb != NULL ) {
				user_cmd( vf->owner, vf->stop_cb );		
				bleat_printf( 1, "stop_cb for pf=%d vf=%d executed: %s", i, j, vf->stop_cb  );
			}
		}
	}
}

// --- callback/mailbox support - depend on global parms ---------------------------------------------------------

/*
	Given a dpdk/hardware port id, find our port struct and return a pointer or
	nil if we cant or it's out of range.

	Depends on global running config so that it may be invoked by the callback
	driver which gets no dynamic information.
*/
struct sriov_port_s *suss_port( int portid ) {
	int		rc_idx; 					// index into our config

	if( portid < 0 || portid > running_config->num_ports ) {
		bleat_printf( 1, "suss_port: port is out of range: %d", portid );
		return NULL;
	}

	if( (rc_idx = port2config_map[portid]) < 0 ) { 		// tanslate port to index, it may not xlate if port is not in our config
		return NULL;
	}

	if( rc_idx >= running_config->num_ports ) {
		bleat_printf( 1, "suss_port: port index for port %d (%d) is out of range", portid, rc_idx );
		return NULL;
	}

	return &running_config->ports[rc_idx];
}


/*
	Given a port and vfid, find the vf block and return a pointer to it.
*/
struct vf_s *suss_vf( int port, int vfid ) {
	struct sriov_port_s *p;
	int		i;

	p = suss_port( port );
	for( i = 0; i < p->num_vfs; i++ ) {
		if( p->vfs[i].num == vfid ) {					// found it
			return &p->vfs[i];
		}
	}

	return NULL;
}

/*
	Given a dpdk port id and vf id, suss out the desired configuration setting.
	What is a VF_VAL_ constant. Only settings which can be represnted by an
	integer can be sussed out.

	Depends on global running config pointer so that callback functions have
	access.
*/
extern int get_vf_setting( int portid, int vf, int what ) {
	struct vf_s *p;
	int		rval = 0;			// return value

	if( (p = suss_vf( portid, vf )) == NULL ) {
		return 0;
	}

	rte_spinlock_lock( &running_config->update_lock );	// ensure it doesn't change while we read
	switch( what ) {
		case VF_VAL_MCAST:
			rval = p->allow_mcast;
			break;

		case VF_VAL_BCAST:
			rval = p->allow_bcast;
			break;

		case VF_VAL_MSPOOF:
			rval = p->mac_anti_spoof;
			break;

		case VF_VAL_VSPOOF:
			rval = p->vlan_anti_spoof;
			break;

		case VF_VAL_STRIPVLAN:
			rval = p->strip_stag;
			break;

		case VF_VAL_UNTAGGED:
			rval = p->allow_untagged;
			break;

		case VF_VAL_UNUCAST:
			rval = p->allow_un_ucast;
			break;
	}

	rte_spinlock_unlock( &running_config->update_lock );
	return rval;
}


/*
	Return true if the vlan is permitted for the port/vfid pair.
*/
int valid_vlan( int port, int vfid, int vlan ) {
	struct vf_s *vf;
	int i;

	if( (vf = suss_vf( port, vfid )) == NULL ) {
		bleat_printf( 2, "valid_vlan: cannot find port/vf pair: %d/%d", port, vfid );
		return 0;
	}

	
	for( i = 0; i < vf->num_vlans; i++ ) {
		if( vf->vlans[i] == vlan ) {				// this is in the list; allowed
			bleat_printf( 2, "valid_vlan: vlan OK for port/vfid %d/%d: %d", port, vfid, vlan );
			return 1;
		}
	}

	bleat_printf( 1, "valid_vlan: vlan not valid for port/vfid %d/%d: %d", port, vfid, vlan );
	return 0;
}

/*
	Looks up the loopback flag for the indicated port and returns 1 if it is set; 0 
	otherwise.
*/
int suss_loopback( int port ) {
	struct sriov_port_s *p;

	if( (p = suss_port( port )) != NULL ) {
		return !!(p->flags & PF_LOOPBACK);
	}

	return 0;
}

/*
	Return true if the mtu value is valid for the port given.
*/
int valid_mtu( int port, int mtu ) {
	struct sriov_port_s *p;

	if( (p = suss_port( port )) == NULL ) {				// find our struct
		bleat_printf( 2, "valid_mtu: port doesn't map: %d", port );
		return 0;
	}

	if( mtu >= 0 &&  mtu <= p->mtu ) {
		bleat_printf( 2, "valid_mtu: mtu OK for port/mtu %d/%d: %d", port, p->mtu, mtu );
		return 1;
	}
	
	bleat_printf( 1, "valid_mtu: mtu is not accptable for port/mtu %d/%d: %d", port, p->mtu, mtu );
	return 0;
}

/*
	Pushes the mac string onto the head of the list for the given port/vf combination. Sets
	the first mac index to be 0 so that it is used if a port/vf reset is triggered.
	Mac is a nil terminated ascii string of the form xx:xx:xx:xx:xx:xx.
*/
extern void push_mac( int port, int vfid, char* mac ) {
	struct vf_s* vf;
	
	if( (vf = suss_vf( port, vfid )) == NULL ) {
		bleat_printf( 2, "push_mac: vf doesn't map: %d/%d", port, vfid );
		return;
	}

	vf->first_mac = 0;
	strcpy( vf->macs[0], mac );			// make our copy

	bleat_printf( 1, "default mac pushed onto head of list: %s", vf->macs[0] );
}

// ---------------------------------------------------------------------------------------------------------------
/*
	Close all open PF ports. We assume this releases memory pool allocation as well.  Called by
	signal handlerers before caling abort() to core dump, and at end of normal processing.
*/
static void close_ports( void ) {
	int 	i;
	//char	dev_name[1024];

	bleat_printf( 0, "closing ports" );
	for( i = 0; i < n_ports; i++) {
		bleat_printf( 0, "closing port: %d", i );
		rte_eth_dev_stop( i );
		rte_eth_dev_close( i );
		//rte_eth_dev_detach( i, dev_name );
		//bleat_printf( 2, "device closed and detached: %s", dev_name );
	}

	bleat_printf( 0, "close ports finished" );
}

// ---------------------------------------------------------------------------------------------------------------
/*
	Test function to vet vfd_init_eal()
*/
static int dummy_rte_eal_init( int argc, char** argv ) {
	int i;

	bleat_printf( 2,  "eal_init parm list: %d parms", argc );
	for( i = 0; i < argc; i++ ) {
		bleat_printf( 2, "[%d] = (%s)", i, argv[i] );
	}

	if( argv[argc] != NULL ) {
		bleat_printf( 2, "ERR:  the last element of argc wasn't nil" );
	}

	return 0;
}

/*
    Insert a flag/value pair, or just a flag, into the target array, and advance the index
    accordingly.  If value is nil, then just the flag is inserted.  If an attempt to insert
    beyond the max size, then the process is aborted.
*/
static void insert_pair( char** target, int *index, int max, char const* flag, char const* value ) {
	if( *index < max-2 ) {
		target[*index] = strdup( flag );
		if( value != NULL ) {
			bleat_printf( 1, "set flag: %s %s", flag, value );
			target[(*index)+1] = strdup( value );
			*index += 2;
		} else {
			bleat_printf( 1, "set flag: %s", flag );
			*index += 1;
		}
	} else {
		bleat_printf( 0, "abort: unable to squeeze parms into dpdk initialisation target" );
		exit( 1 );
	}
}

/*
	Initialise the EAL.  We must dummy up what looks like a command line and pass it to the dpdk funciton.
	This builds the base command, and then adds a -w option for each pciid/vf combination that we know
	about.

	We strdup all of the argument strings that are eventually passed to dpdk as the man page indicates that
	they might be altered, and that we should not fiddle with them after calling the init function. Thus we
	give them their own copy, and suffer a small leak.
	
	This function causes a process abort if any of the following are true:
		- unable to alloc memory
		- no vciids were listed in the config file
		- dpdk eal initialisation fails
*/
static int vfd_eal_init( parms_t* parms ) {
	int		argc = 0;					// argc/v parms we dummy up
	char** argv;
	int		i;
	char	wbuf[128];				// scratch buffer
	int		count;

	if( parms->npciids <= 0 ) {
		bleat_printf( 0, "CRI: abort: no pciids were defined in the configuration file" );
		exit( 1 );
	}

	if( (argv = (char **) malloc( MAX_ARGV_LEN * sizeof( char* ) )) == NULL ) {		// get enough for a max set of pointers
		bleat_printf( 0, "CRI: abort: unable to alloc argv array for eal initialisation" );
		exit( 1 );
	}
	memset( argv, 0, sizeof( char* ) * MAX_ARGV_LEN );

	argv[argc++] = strdup( "vfd" );							// dummy up a command line to pass to rte_eal_init() -- it expects that we got these on our command line (what a hack)


	if( parms->cpu_mask != NULL ) {
		i = (int) strtol( parms->cpu_mask, NULL, 0 );			// enforce sanity (only allow one bit else we hog multiple cpus)
		if( i <= 0 ) {
			free( parms->cpu_mask );						 	// free and use default below
			parms->cpu_mask = NULL;
		} else {
			count = 0;
			while( i )  {
				if( i & 0x01 ) {
					count++;
				}
				i >>= 1;
			}

			if( count > 1 ) {							// invalid number of bits
				bleat_printf( 0, "WRN: cpu_mask value in parms (%s) is not acceptable (too many bits); setting to 0x04", parms->cpu_mask );
				free( parms->cpu_mask );
				parms->cpu_mask = NULL;
			}
		}
	}
	if( parms->cpu_mask == NULL ) {
		parms->cpu_mask = strdup( "0x04" );
	} else {
		if( *(parms->cpu_mask+1) != 'x' ) {														// not something like 0xff
			snprintf( wbuf, sizeof( wbuf ), "0x%02x", atoi( parms->cpu_mask ) );				// assume integer as a string given; cvt to hex
			free( parms->cpu_mask );
			parms->cpu_mask = strdup( wbuf );
		}
	}
	
	insert_pair( argv, &argc, MAX_ARGV_LEN, "-c", parms->cpu_mask );
	insert_pair( argv, &argc, MAX_ARGV_LEN, "-n", "4" );
	insert_pair( argv, &argc, MAX_ARGV_LEN, "--file-prefix", "vfd" );
	
	snprintf( wbuf, sizeof( wbuf ), "%d", parms->dpdk_init_log_level );
	insert_pair( argv, &argc, MAX_ARGV_LEN, "--log-level", wbuf );
	
	if( parms->rflags & RF_NO_HUGE ) {
		insert_pair( argv, &argc, MAX_ARGV_LEN, "--no-huge", NULL );
		insert_pair( argv, &argc, MAX_ARGV_LEN, "-m", "128" );
	} else {
		insert_pair( argv, &argc, MAX_ARGV_LEN, "--socket-mem", "64,64" );				// can't specify if huge pages are off
	}


	for( i = 0; i < parms->npciids; i++ ) {												// add in the -w pciid values to the list
		insert_pair( argv, &argc, MAX_ARGV_LEN, "-w", parms->pciids[i].id );
	}

	dummy_rte_eal_init( argc, argv );													// print out parms, vet, etc.
	if( parms->forreal ) {
		bleat_printf( 1, "invoking real rte initialisation argc=%d", argc );
		i = rte_eal_init( argc, argv ); 												// http://dpdk.org/doc/api/rte__eal_8h.html
		bleat_printf( 1, "initialisation returned %d", i );
		rte_eal_devargs_dump(stdout);
	} else {
		bleat_printf( 1, "rte initialisation skipped (no harm mode)" );
		i = 1;
	}

	return i;
}

// ----------------- actual nic management ------------------------------------------------------------------------------------

/*
	Generate a set of stats to a single buffer. Return buffer to caller (caller must free).
	If pf_only is true, then the VF stats are skipped. If pf >= 0, then only that pf, and
	its VFs are printed.
*/
char*  gen_stats( sriov_conf_t* conf, int pf_only, int pf ) {
	char*	rbuf;			// buffer to return
	int		rblen = 0;		// lenght
	int		rbidx = 0;
	char	buf[BUF_SIZE];
	int		l;
	int		i;
	struct rte_eth_dev_info dev_info;

	rblen = BUF_SIZE;
	rbuf = (char *) malloc( sizeof( char ) * rblen );
	if( !rbuf ) {
		return NULL;
	}

	rbidx = snprintf( rbuf, BUF_SIZE, "%s %6s  %6s %6s %15s %15s %15s %15s %15s %15s %15s %15s\n",
			"\nPF/VF  ID           PCIID",
			 "Link",
			 "Speed",
			 "Duplex",
			 "RX pkts",
			 "RX bytes",
			 "RX errors",
			 "RX dropped",
			 "TX pkts",
			 "TX bytes",
			 "TX errors",
			 "Spoofed"
		);
	
	for( i = 0; i < conf->num_ports; ++i ) {
		if( pf > 0 && i != pf ) {					// if specific pf requested, do only that one
			continue;
		}

		rte_eth_dev_info_get( conf->ports[i].rte_port_number, &dev_info );				// must use port number that we mapped during initialisation

		l = snprintf( buf, sizeof( buf ), "%s   %4d    %04X:%02X:%02X.%01X",
					"pf",
					conf->ports[i].rte_port_number,
					dev_info.pci_dev->addr.domain,
					dev_info.pci_dev->addr.bus,
					dev_info.pci_dev->addr.devid,
					dev_info.pci_dev->addr.function);
							
		if( l + rbidx > rblen ) {
			rblen += BUF_SIZE;
			rbuf = (char *) realloc( rbuf, sizeof( char ) * rblen );
			if( !rbuf ) {
				return NULL;
			}
		}

		strcat( rbuf+rbidx,  buf );
		rbidx += l;		
   				
		l = nic_stats_display( conf->ports[i].rte_port_number, buf, sizeof( buf ) );

		if( l + rbidx > rblen ) {
			rblen += BUF_SIZE + l;
			rbuf = (char *) realloc( rbuf, sizeof( char ) * rblen );
			if( !rbuf ) {
				return NULL;
			}
		}
		strcat( rbuf+rbidx,  buf );
		rbidx += l;
		
		if( ! pf_only ) {
			// pack PCI ARI into 32bit to be used to get VF's ARI later
			uint32_t pf_ari = dev_info.pci_dev->addr.bus << 8 | dev_info.pci_dev->addr.devid << 3 | dev_info.pci_dev->addr.function;
			
			//iterate over active (configured) VF's only
			int * vf_arr = malloc(sizeof(int) * conf->ports[i].num_vfs);
			int v;
			for (v = 0; v < conf->ports[i].num_vfs; v++)
				vf_arr[v] = conf->ports[i].vfs[v].num;

			// sort vf numbers
			qsort(vf_arr, conf->ports[i].num_vfs, sizeof(int), cmp_vfs);
			
			for (v = 0; v < conf->ports[i].num_vfs; v++) {
				if( (l = vf_stats_display(conf->ports[i].rte_port_number, pf_ari, vf_arr[v], buf, sizeof( buf ))) > 0 ) {  // < 0 out of range, not in use
					if( l + rbidx > rblen ) {
						rblen += BUF_SIZE + l;
						rbuf = (char *) realloc( rbuf, sizeof( char ) * rblen );
						if( !rbuf ) {
							bleat_printf( 0, "ERR: gen_stats: realloc failed");
							return NULL;
						}
					}
					strcat( rbuf+rbidx,  buf );
					rbidx += l;
				}
			}		
			free(vf_arr);
		}

		if( !pf_only ) {
			strcat( rbuf+rbidx, "\n" );		// extra blank line for easy reading
			rbidx++;
		}
	}

	bleat_printf( 2, "status buffer size: %d", rbidx );
	return rbuf;
}

int
cmp_vfs (const void * a, const void * b)
{
   return ( *(const int*)a - *(const int*)b );
}

/*
	2017/03/23 - We now allow strip/insert when there are multiple VLAN IDs:
		If strip == true and one ID is supplied, that ID will stripped on Rx and 
		inserted on Tx.

		If strip == true and more than one ID supplied, the outer (s) tag will be
		stripped on Rx, and the ID from the packet descriptor will be inserted on Tx.
	
		IF strip == false, no stripping will take place, and insertion will be based
		on the packet descriptor if it exists (this is default behavour when insert
		setting is clear).

	Returns 0 on failure; 1 on success.
*/
static int vfd_set_ins_strip( struct sriov_port_s *port, struct vf_s *vf ) {
	if( port == NULL || vf == NULL ) {
		bleat_printf( 1, "cannot set strip/insert: port or vf pointers were nill" );
		return 0;
	}

	if( vf->num_vlans == 1 ) {
		bleat_printf( 2, "pf: %s vf: %d set strip vlan tag %d", port->name, vf->num, vf->strip_stag );
		rx_vlan_strip_set_on_vf(port->rte_port_number, vf->num, vf->strip_stag );			// if just one in the list, push through user strip option

		if( vf->insert_stag ) {																// when stripping, we must also insert
			bleat_printf( 2, "%s vf: %d set insert vlan tag with id %d", port->name, vf->num, vf->vlans[0] );
			tx_vlan_insert_set_on_vf(port->rte_port_number, vf->num, vf->vlans[0] );
		} else {
			bleat_printf( 2, "%s vf: %d set insert vlan tag with id 0", port->name, vf->num );
			tx_vlan_insert_set_on_vf( port->rte_port_number, vf->num, 0 );					// no strip, so no insert
		}
	} else {
		bleat_printf( 2, "%s vf: %d vlan list contains %d entries; strip set to %d; insert turned off", port->name, vf->num, vf->num_vlans, vf->strip_stag );
		rx_vlan_strip_set_on_vf( port->rte_port_number, vf->num, vf->strip_stag );		// strip is variable
		tx_vlan_insert_set_on_vf( port->rte_port_number, vf->num, 0 );					// but insert must always be clear so that packet descriptor is used
	}

	return 1;
}
	

/*
	Runs through the configuration and makes adjustments.  This is
	a tweak of the original code (update_ports_config) inasmuch as the dynamic
	changes to the configuration based on nova add/del requests are made to the
	"running config" -- there is no longer a new/old config to compare with.  This
	function will update a port/vf based on the last_updated flag in any port/VF
	in the config:
		-1 delete (remove macs and vlans)
		0  no change, no action
		1  add (add macs  and vlans)

	Bleat messages have been added so that dynamically adjusted verbosity is
	available.

	Conf is the configuration to check. If parms->forreal is set, then we actually
	make the dpdk calls to do the work.


	TODO:  the original, and thus this, function always return 0 (good); we need to
		figure out how to handle errors back from the rte_ calls.
*/
extern int vfd_update_nic( parms_t* parms, sriov_conf_t* conf ) {
	int i;
	int on = 1;
    uint32_t vf_mask;
    int y;

	if( (parms->rflags & RF_INITIALISED) == 0 ) {
		bleat_printf( 2, "update_nic: not initialised, nic settings not updated" );
		return 0;
	}

	rte_spinlock_lock( &running_config->update_lock );
	
	for (i = 0; i < conf->num_ports; ++i){							// run each port we know about
		int ret;
		struct sriov_port_s* port;

		port = &conf->ports[i];

		if( parms->forreal ) {
			tx_set_loopback( port->rte_port_number, !!(port->flags & PF_LOOPBACK) );		// enable loopback if set (disabled: all vm-vm traffic must go to TOR and back

			// do NOT call set_queue_drop() as it causes packetloss; drop enable handled by callback process now

			disable_default_pool( port->rte_port_number );
		}

		if( port->last_updated == ADDED ) {								// updated since last call, reconfigure
			if( parms->forreal ) {
				bleat_printf( 1, "port updated: %s/%s",  port->name, port->pciid );

				if( port->flags & PF_PROMISC ) {
					bleat_printf( 1, "enabling promiscuous mode for port %d", port->rte_port_number );
					rte_eth_promiscuous_enable(port->rte_port_number);
				}
				else {
					bleat_printf( 1, "disabling promiscuous mode for port %d", port->rte_port_number );
					rte_eth_promiscuous_disable(port->rte_port_number);
				}
				
				if (get_nic_type(port->rte_port_number) == VFD_BNXT)
					rte_eth_allmulticast_disable(port->rte_port_number);
				else
					rte_eth_allmulticast_enable(port->rte_port_number);
			
				if (get_nic_type(port->rte_port_number) == VFD_NIANTIC) {
					ret = rte_eth_dev_uc_all_hash_table_set(port->rte_port_number, on);
					
					if (ret < 0)
						bleat_printf( 0, "ERR: bad unicast hash table parameter, return code = %d", ret);
				}	

	
			} else {
				bleat_printf( 1, "port update commands not sent (forreal is off): %s/%s",  port->name, port->pciid );
			}

			port->last_updated = UNCHANGED;								// mark that we did this for next go round
		} else {
			bleat_printf( 2, "update configs: skipped port, not changed: %s/%s", port->name, port->pciid );
		}

	    for(y = 0; y < port->num_vfs; ++y){ 							/* go through all VF's and (un)set VLAN's/macs for any vf that has changed */
			int v;
			int m;
			char *mac;
			int	change2port;							// set true if one or more VFs changed; need to redo qos allotment if so
			struct vf_s *vf = &port->vfs[y];   			// at the VF to work on

			vf_mask = VFN2MASK(vf->num);

			change2port = 0;
			if( vf->last_updated != UNCHANGED ) {					// this vf was changed (add/del/reset), reconfigure it
				const char* reason;

				change2port = 1;

				switch( vf->last_updated ) {
					case ADDED:		reason = "add"; break;
					case DELETED:	reason = "delete"; break;
					case RESET:		reason = "reset"; break;
					default:		reason = "unknown reason"; break;
				}
				bleat_printf( 1, "reconfigure vf for %s port: %d vf=%d", reason, port->rte_port_number, vf->num );

				// TODO: order from original kept; probably can group into to blocks based on updated flag
				if( vf->last_updated == DELETED ) { 							// delete vlans, free any buffers
					if( vf->start_cb ) {
						free( vf->start_cb );
						vf->start_cb = NULL;
					}
					if( vf->stop_cb ) {
						free( vf->stop_cb );
						vf->stop_cb = NULL;
					}

					if( port->mirrors[y].dir != MIRROR_OFF ) {													// stop the mirror on delete
						set_mirror( port->rte_port_number, vf->num, port->mirrors[y].id, port->mirrors[y].target, MIRROR_OFF );		// turn off, no target needed (-1)
						port->mirrors[y].dir = MIRROR_OFF;
						idm_return( conf->mir_id_mgr, port->mirrors[y].id );									// mark the id as unused in allocator
					}

					//AZif (get_nic_type(port->rte_port_number) == VFD_NIANTIC)
					//	set_vf_rx_vlan(port->rte_port_number, 0, vf_mask, 0);		// remove vlan id 0 do we need it here for i40e?
					
					for(v = 0; v < vf->num_vlans; ++v) {
						int vlan = vf->vlans[v];
						bleat_printf( 2, "delete vlan: port: %d vf: %d vlan: %d", port->rte_port_number, vf->num, vlan );
						if( parms->forreal )
							set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, SET_OFF );		// remove the vlan id from the list
					}

					for( m = vf->first_mac; m <= vf->num_macs; ++m ) {				// delete MAC addresses 
						mac = vf->macs[m];
						bleat_printf( 2, "delete mac: port: %d vf: %d mac: %s", port->rte_port_number, vf->num, mac );
		
						if( parms->forreal )
							set_vf_rx_mac(port->rte_port_number, mac, vf->num, SET_OFF );
					}

					vf->num = -1;				//DO THIS LAST --  must reset this so an add request with the now deleted number will succeed
				} else {
					int v;

					if( port->mirrors[y].dir != MIRROR_OFF ) {						// setup the mirror
						set_mirror( port->rte_port_number, vf->num, port->mirrors[y].id, port->mirrors[y].target, port->mirrors[y].dir );		// set target and type (in/out/both)
					}

					for(v = 0; v < vf->num_vlans; ++v) {
						int vlan = vf->vlans[v];
						bleat_printf( 2, "add vlan: port: %d vf=%d vlan=%d", port->rte_port_number, vf->num, vlan );
						if( parms->forreal )
							set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, on );		// add the vlan id to the list
					}
				}

				if( vf->last_updated == DELETED ) {				// delete the macs
					for( m = vf->first_mac; m <= vf->num_macs; ++m ) {
						mac = vf->macs[m];
						bleat_printf( 2, "delete mac: port: %d vf: %d mac: %s", port->rte_port_number, vf->num, mac );
		
						if( parms->forreal )
							set_vf_rx_mac(port->rte_port_number, mac, vf->num, SET_OFF );
					}
				} else {
					bleat_printf( 2, "configuring %d mac addresses: port: %d vf: %d firstmac=%d", vf->num_macs, port->rte_port_number, vf->num, vf->first_mac );
					for( m = vf->num_macs; m >= vf->first_mac; m-- ) {				// must run in reverse order because of FV oddness
						mac = vf->macs[m];
						bleat_printf( 2, "adding mac [%d]: port: %d vf: %d mac: %s", m, port->rte_port_number, vf->num, mac );

						if( parms->forreal ) {
							if( m > vf->first_mac ) {
								set_vf_rx_mac( port->rte_port_number, mac, vf->num, SET_ON );	// set in whitelist
							} else {
								set_vf_default_mac( port->rte_port_number, mac, vf->num );		// first is set as default
							}
						}
					}
				}

				if( vf->rate > 0 ) {
					bleat_printf( 1, "setting rate: %d", (int)  ( 10000 * vf->rate ) );
					set_vf_rate_limit( port->rte_port_number, vf->num, (uint16_t)( 10000 * vf->rate ), 0x01 );
				}


				if( vf->num >= 0 ) {
					if( parms->forreal ) {
						bleat_printf( 2, "port: %d vf: %d set anti-spoof to %d", port->rte_port_number, vf->num, vf->vlan_anti_spoof );
						set_vf_vlan_anti_spoofing(port->rte_port_number, vf->num, vf->vlan_anti_spoof);
	
						bleat_printf( 2, "port: %d vf: %d set mac-anti-spoof to %d", port->rte_port_number, vf->num, vf->mac_anti_spoof );
						set_vf_mac_anti_spoofing(port->rte_port_number, vf->num, vf->mac_anti_spoof);
	
						vfd_set_ins_strip( port, vf );				// set insert/strip options

						bleat_printf( 2, "port: %d vf: %d set allow broadcast to %d", port->rte_port_number, vf->num, vf->allow_bcast );
						set_vf_allow_bcast(port->rte_port_number, vf->num, vf->allow_bcast);

						bleat_printf( 2, "port: %d vf: %d set allow multicast to %d", port->rte_port_number, vf->num, vf->allow_mcast );
						set_vf_allow_mcast(port->rte_port_number, vf->num, vf->allow_mcast);

						bleat_printf( 2, "port: %d vf: %d set allow un-ucast to %d", port->rte_port_number, vf->num, vf->allow_un_ucast );
						set_vf_allow_un_ucast(port->rte_port_number, vf->num, vf->allow_un_ucast);
					} else {
						bleat_printf( 1, "update vf skipping setup for spoofing, bcast, mcast, etc; forreal is off: %d vf=%d", port->rte_port_number, vf->num );
					}
				}

				vf->last_updated = UNCHANGED;				// mark processed
			}

			if( change2port && (g_parms->rflags & RF_ENABLE_QOS) ) {		// changes, we must recompute queue shares and push to nic
				//uint8_t* pp;
				gen_port_qshares( port );									// compute and save in the port struct
				qos_set_credits( port->rte_port_number, port->mtu, port->vftc_qshares, TC_4PERQ_MODE );	// push out to nic
				//qos_set_credits( port->rte_port_number, port->mtu, pp, TC_4PERQ_MODE );	// push out to nic
			}

			if( change2port && vf->num >= 0 ) {
				if( parms->forreal ) {
					bleat_printf( 3, "set promiscuous: port: %d, vf: %d ", port->rte_port_number, vf->num);
			
					// az says: figure out if we have to update it every time we change VLANS/MACS
					// 			or once when update ports config
					if( port->flags & PF_PROMISC ) {
						bleat_printf( 1, "enabling promiscuous mode for port %d", port->rte_port_number );
						rte_eth_promiscuous_enable(port->rte_port_number);
					}
					else {
						bleat_printf( 1, "disabling promiscuous mode for port %d", port->rte_port_number );
						rte_eth_promiscuous_disable(port->rte_port_number);
					}
					
					if (get_nic_type(port->rte_port_number) == VFD_BNXT)
						rte_eth_allmulticast_disable(port->rte_port_number);
					else
						rte_eth_allmulticast_enable(port->rte_port_number);
					
					if (get_nic_type(port->rte_port_number) == VFD_NIANTIC) {
						ret = rte_eth_dev_uc_all_hash_table_set(port->rte_port_number, on);
						
						if (ret < 0)
							bleat_printf( 0, "ERR: bad unicast hash table parameter, return code = %d", ret);
					}					
						
					
					// don't accept untagged frames
					set_vf_allow_untagged(port->rte_port_number, vf->num, !on);

				} else {
					bleat_printf( 1, "skipped end round updates to port: %d", port->rte_port_number );
				}
			}
		}				// end for each vf on this port
    }     // end for each port

	rte_spinlock_unlock( &running_config->update_lock );
	return 0;
}


// -------------------------------------------------------------------------------------------------------------

static inline uint64_t RDTSC(void)
{
  unsigned int hi, lo;
  __asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
  return ((uint64_t)hi << 32) | lo;
}



// ---- signal managment (setup and handlers) ------------------------------------------------------------------


/*
	Called for any signal that has a default terminate action so that we
	force a cleanup before stopping. We'll call abort() for a few so that we
	might get a usable core dump when needed. If we call abort(), rather than
	just setting the terminated flag, we _must_ close the PFs gracefully or
	risk a machine crash.
*/
static void sig_int( int sig ) {
	if( terminated ) {					// ignore concurrent signals
		return;
	}

	switch( sig ) {
		case SIGABRT:
		case SIGFPE:
		case SIGSEGV:
				terminated = 1;				// prevent loop
				bleat_printf( 0, "signal caught (aborting): %d", sig );
				close_ports();				// must attempt to do this else we potentially crash the machine
				abort( );					// to get core; not safe to just set term flag and end normally
				break;

		case SIGUSR1:						// for these we just ignore and go on
		case SIGUSR2:
		case SIGALRM:
				bleat_printf( 0, "signal caught (ignored): %d", sig );
				break;

		default:
				terminated = 1;
				bleat_printf( 0, "signal caught (terminating): %d", sig );
				break;
	}

	return;
}

/*
	Signals we choose to ignore drive this.
*/
static void
sig_ign( int sig ) {
	bleat_printf( 1, "signal ignored: %d", sig );
}

/*	
	Setup all of the signal handling. Because a VFd exit without gracefully closing ports
	seems to crash (all? most?) physical hosts, we must catch everything that has a default
	action which is not ignore.  While mentioned on the man page, SIGEMT and SIGLOST seem
	unsupported in linux.
*/
static void set_signals( void ) {
	struct sigaction sa;
	int	sig_list[] = { SIGQUIT, SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGPIPE,				// list of signals we trap
       				SIGALRM, SIGTERM, SIGUSR1 , SIGUSR2, SIGBUS, SIGPROF, SIGSYS,
					SIGTRAP, SIGURG, SIGVTALRM, SIGXCPU, SIGXFSZ, SIGIO };

	int i;
	int nele;		// number of elements in the list
	
	sa.sa_handler = sig_ign;						// we ignore hup, so special function for this
	if( sigaction( SIGHUP, &sa, NULL ) < 0 ) {
		bleat_printf( 0, "WRN: unable to set signal trap for %d: %s", SIGHUP, strerror( errno ) );
	}

	nele = (int) ( sizeof( sig_list )/sizeof( int ) );		// convert raw size to the number of elements
	for( i = 0; i < nele; i ++ ) {
		memset( &sa, 0, sizeof( sa ) );
		sa.sa_handler = sig_int;				// all signals which default to term or core must be caught
		if( sigaction( sig_list[i], &sa, NULL ) < 0 ) {
			bleat_printf( 0, "WRN: unable to set signal trap for %d: %s", sig_list[i], strerror( errno ) );
		}
	}
}

//-----------------------------------------------------------------------------------------------------------------------

// Time difference in millisecond

double
timeDelta(struct timeval * now, struct timeval * before)
{
  time_t delta_seconds;
  time_t delta_microseconds;

  //compute delta in second, 1/10's and 1/1000's second units

  delta_seconds      = now -> tv_sec  - before -> tv_sec;
  delta_microseconds = now -> tv_usec - before -> tv_usec;

  if(delta_microseconds < 0){
    // manually carry a one from the seconds field
    delta_microseconds += 1000000;  // 1e6
    -- delta_seconds;
  }
  return((double)(delta_seconds * 1000) + (double)delta_microseconds/1000);
}



/*
	This should work without change.
	Driven to refresh a single vf on a port. Called by the callback which (we assume)
	is driven by the dpdk environment.

	It does seem to be a duplication of the vfd_update_nic() function.  Would it make
	sense to set the add flag in the matched VF and then just call update?
	It also seems that deleting VLAN and MAC values might not catch anything/everything
	that has been set on the VF since it's only working off of the values that are
	configured here.  Is there a reset all? for these?  If so, that should be worked into
	the update_nic() funciton for an add, and probably for the delete too.

	This function may also be called in an extreme event when all active VFs on the port
	must be refreshed.  If vf_id passed in is < 0, then we reset all of the VFs that 
	we are currently managing.
*/
void
restore_vf_setings(uint8_t port_id, int vf_id) {
	int i;
	int matched = 0;		// number matched for log

	if( bleat_will_it( 5 ) ) {
		dump_sriov_config(running_config);
	}

	//bleat_printf( 2, "drop any untagged packets for all VFs: port %d vf %d", port_id, vf_id );
	//set_vf_allow_untagged(port_id, vf_id, 0);	
	
	bleat_printf( 3, "restore settings begins" );
	for (i = 0; i < running_config->num_ports; ++i){
		struct sriov_port_s *port = &running_config->ports[i];

		if (port_id == port->rte_port_number){

			int y;
			for(y = 0; y < port->num_vfs; ++y){
				struct vf_s *vf = &port->vfs[y];

				if( (vf_id < 0 && vf->num >= 0) || (vf_id == vf->num) ){
					//uint32_t vf_mask = VFN2MASK(vf->num);

					matched++;															// for bleat message at end
					vf->last_updated = RESET;											// flag for update_nic()
					if( vfd_update_nic( g_parms, running_config ) != 0 ) {				// now that dpdk is initialised run the list and 'activate' everything
						bleat_printf( 0, "WRN: reset of port %d vf %d failed", port_id, vf_id );
					}
				}
			}
		}
	}
	
	bleat_printf( 1, "restore for  port=%d vf=%d matched %d vfs in the config", port_id, vf_id, matched );
}


/*
	Runs the current in memory configuration and dumps stuff to the log.
	Only mods were to replace tracelog calls with bleat calls to allow
	for dynamic level changes and file rolling.
*/
void
dump_sriov_config( sriov_conf_t* sriov_config)
{
	int i;
	int y;
	int split_ctl;			// split receive control reg setting


	bleat_printf( 0, "dump: config has %d port(s)", sriov_config->num_ports );

	for (i = 0; i < sriov_config->num_ports; i++){
		bleat_printf( 0, "dump: port: %d, pciid: %s, pciid %s, updated %d, mtu: %d, num_mirrors: %d, num_vfs: %d",
          i, sriov_config->ports[i].name,
          sriov_config->ports[i].pciid,
          sriov_config->ports[i].last_updated,
          sriov_config->ports[i].mtu,
          sriov_config->ports[i].num_mirrors,
          sriov_config->ports[i].num_vfs );

		for (y = 0; y < sriov_config->ports[i].num_vfs; y++){
			if( sriov_config->ports[i].vfs[y].num >= 0 ) {
				split_ctl = get_split_ctlreg( i, sriov_config->ports[i].vfs[y].num );
				bleat_printf( 1, "dump: vf: %d, updated: %d  strip: %d  insert: %d  vlan_aspoof: %d  mac_aspoof: %d  allow_bcast: %d  allow_ucast: %d  allow_mcast: %d  allow_untagged: %d  rate: %f  link: %d  num_vlans: %d  num_macs: %d  splitctl=0x%08x",
					sriov_config->ports[i].vfs[y].num, sriov_config->ports[i].vfs[y].last_updated,
					sriov_config->ports[i].vfs[y].strip_stag,
					sriov_config->ports[i].vfs[y].insert_stag,
					sriov_config->ports[i].vfs[y].vlan_anti_spoof,
					sriov_config->ports[i].vfs[y].mac_anti_spoof,
					sriov_config->ports[i].vfs[y].allow_bcast,
					sriov_config->ports[i].vfs[y].allow_un_ucast,
					sriov_config->ports[i].vfs[y].allow_mcast,
					sriov_config->ports[i].vfs[y].allow_untagged,
					sriov_config->ports[i].vfs[y].rate,
					sriov_config->ports[i].vfs[y].link,
					sriov_config->ports[i].vfs[y].num_vlans,
					sriov_config->ports[i].vfs[y].num_macs,
					split_ctl );
	
				int x;
				for (x = 0; x < sriov_config->ports[i].vfs[y].num_vlans; x++) {
					bleat_printf( 2, "dump: vlan[%d] %d ", x, sriov_config->ports[i].vfs[y].vlans[x]);
				}
	
				int z;
				for (z = 0; z < sriov_config->ports[i].vfs[y].num_macs; z++) {
					bleat_printf( 2, "dump: mac[%d] %s ", z, sriov_config->ports[i].vfs[y].macs[z]);
				}
			} else {
				bleat_printf( 2, "dump: port %d index %d is not configured", i, y );
			}
		}
	}
}

// ===============================================================================================================
int
main(int argc, char **argv)
{
	__attribute__((__unused__))	int ignored;	// ignored return code to keep compiler from whining
	char*	parm_file = NULL;					// default in /etc, -p overrieds
	char*	log_file;							// buffer to build full log file in
	char	run_asynch = 1;				// -f sets off to keep attached to tty
	int		forreal = 1;				// -n sets to 0 to keep us from actually fiddling the nic
	int		opt;
	int		fd = -1;
	int		enable_qos = 0;				// off by default enable_qos in config should be used to set on
	int		state;
	int 	j;
	int		no_huge = 0;				// -H will turn on and we will flip the appropriate bit in parms

	uint16_t	cfg_offset = 0x100;
	int		enable_fc = 0;				// enable flow control (-F sets)
	u_int16_t portid;


  const char * main_help =
		"\n"
		"Usage: vfd [-f] [-F] [-H] [-n] [-p parm-file] [-v level] [-q]\n"
		"Usage: vfd -?\n"
		"  Options:\n"
		"\t -f        keep in 'foreground'\n"
		"\t -F        enable flow control (might be ignored in qos mode)\n"
		"\t -H        disable use of huge pages\n"
		"\t -n        no-nic actions executed\n"
		"\t -p <file> parmm file (/etc/vfd/vfd.cfg)\n"
		"\t -q        enable dcb qos (use config file parm as general rule)\n"

		"\t -h|?  Display this help screen\n"
		"\n";

  		//"\t -s <num>  syslog facility 0-11 (log_kern - log_ftp) 16-23 (local0-local7) see /usr/include/sys/syslog.h\n"

	struct rte_mempool *mbuf_pool = NULL;
	prog_name = strdup(argv[0]);
 	useSyslog = 1;

	parm_file = strdup( "/etc/vfd/vfd.cfg" );				// set default before command line parsing as -p overrides
	log_file = (char *) malloc( sizeof( char ) * BUF_1K );

  // Parse command line options
  while ( (opt = getopt(argc, argv, "?qfFHhnqv:p:s:")) != -1)
  {
    switch (opt)
    {
		case 'F':
			enable_fc = 1;					// enable flow control (qos might ignore this)
			break;
			
		case 'f':
			run_asynch = 0;
			break;
		
		case 'H':
			no_huge = 1;
			break;
		
		case 'n':
			forreal = 0;						// do NOT actually make calls to change the nic
			break;

		case 'p':
			if( parm_file )
				free( parm_file );
			parm_file = strdup( optarg );
			break;

		case 's':
		  logFacility = (atoi(optarg) << 3);
		  break;

		case 'q':
			enable_qos = 1;
			break;

		case 'h':
		case '?':
			printf( "\nVFd %s %s\n", vnum, version );
			printf( "(17710)\n" );
			printf("%s\n", main_help);
			exit( 0 );
			break;


		default:
			fprintf( stderr, "\nunknown commandline flag: %c\n", opt );
			fprintf( stderr, "%s\n", main_help );
			exit( 1 );
    }
  }


	if( (g_parms = read_parms( parm_file )) == NULL ) {						// get overall configuration (includes list of pciids we manage)
		fprintf( stderr, "CRI: unable to read configuration from %s: %s\n", parm_file, strerror( errno ) );
		exit( 1 );
	}

	if( no_huge ) {							// can be set on cmd line or in config
		g_parms->rflags |= RF_NO_HUGE;
	}

	if( enable_qos ) {							// command line flag overrides the config to force qos on
		g_parms->rflags |= RF_ENABLE_QOS;
	}

	if( enable_fc ) {
		g_parms->rflags |= RF_ENABLE_FC;
	}

	g_parms->forreal = forreal;

	if( (running_config = (sriov_conf_t *) malloc( sizeof( *running_config ) )) == NULL ) {
		bleat_printf( 0, "abort: unable to allocate memory for running config" );
		exit( 1 );
	}
	memset( running_config, 0, sizeof( *running_config ) );
	rte_spinlock_init( &running_config->update_lock );			// initialise and leave unlocked
	running_config->mir_id_mgr = mk_idm( 256 );					// make an id manager with 256 ID 'slots' for allocating mirror IDs

	snprintf( log_file, BUF_1K, "%s/vfd.log", g_parms->log_dir );
	if( run_asynch ) {
		bleat_printf( 1, "setting log to: %s", log_file );
		bleat_printf( 3, "detaching from tty (daemonise)" );
		daemonize( g_parms->pid_fname );
		bleat_set_log( log_file, 86400 );									// open bleat log with date suffix _after_ daemonize so it doesn't close our fd
		if( g_parms->log_keep > 0 ) {										// set days to keep log files
			bleat_set_purge( g_parms->log_dir, "vfd.log.", g_parms->log_keep * 86400 );
		}
	} else {
		bleat_printf( 2, "-f supplied, staying attached to tty" );
	}
	free( log_file );
	bleat_set_lvl( g_parms->init_log_level );											// set default level
	bleat_printf( 0, "VFD %s %s initialising", vnum, version );
	bleat_printf( 0, "config dir set to: %s", g_parms->config_dir );

	if( vfd_init_fifo( g_parms ) < 0 ) {
		bleat_printf( 0, "CRI: abort: unable to initialise request fifo" );
		exit( 1 );
	}

	if( vfd_eal_init( g_parms ) < 0 ) {												// dpdk function returns -1 on error
		bleat_printf( 0, "CRI: abort: unable to initialise dpdk eal environment" );
		exit( 1 );
	}

														// set up config structs. these always succeeed (see notes in README)
	vfd_add_ports( g_parms, running_config );			// add the pciid info from parms to the ports list (must do before dpdk init, config file adds wait til after)

	if( g_parms->forreal ) {										// begin dpdk setup and device discovery
		int ret;					// returned value from some call
		uint32_t pci_control_r;

		bleat_printf( 1, "starting rte initialisation" );
		
		rte_openlog_stream(stderr);
		//rte_log_set_level(~RTE_LOGTYPE_PMD && ~RTE_LOGTYPE_PORT, g_parms->dpdk_init_log_level);
		ret = rte_log_set_level(RTE_LOGTYPE_PMD, g_parms->dpdk_init_log_level);


		bleat_printf( 2, "log level = %d, log type = %d, ret = %d", rte_log_cur_msg_loglevel(), rte_log_cur_msg_logtype(), ret);
		

		n_ports = rte_eth_dev_count();
		if( n_ports > MAX_PORTS ) {
			bleat_printf( 0, "WARN: hardware reports %d ports which exceeds max supported ports (%d); processing only %d ports", n_ports, MAX_PORTS, MAX_PORTS );
			n_ports = MAX_PORTS;
		} else {
			bleat_printf( 1, "hardware reports %d ports", n_ports );
		}

		if(n_ports < running_config->num_ports) {
			bleat_printf( 1, "WRN: port count mismatch: config lists %d device has %d", running_config->num_ports, n_ports );
		} else {
	  		if (n_ports > running_config->num_ports ) {
				bleat_printf( 1, "CRI: abort: config file reports more devices than dpdk reports: cfg=%d ndev=%d", running_config->num_ports, n_ports );
			}
		}

		static pthread_t tid;
		rq_list = NULL;						// nothing initially on the reset list
		
		ret = pthread_create(&tid, NULL, (void *)process_refresh_queue, NULL);	
		if (ret != 0) {
			bleat_printf( 0, "CRI: abort: cannot crate refresh_queue thread" );
			rte_exit(EXIT_FAILURE, "Cannot create refresh_queue thread\n");
		}
		bleat_printf( 1, "refresh queue management thread created" );
	
		bleat_printf( 1, "creating memory pool" ); 									// Creates a new mempool in memory to hold the mbufs.  
		mbuf_pool = rte_pktmbuf_pool_create("sriovctl", NUM_MBUFS, MBUF_CACHE_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
		if (mbuf_pool == NULL) {
			bleat_printf( 0, "CRI: abort: mbuf pool creation failed" );
			rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
		}

		bleat_printf( 1, "initialising all (%d) ports", n_ports );
		for (portid = 0; portid < n_ports; portid++) { 								// initialize ports, but only the ports listed in our config
			int i;
			char pciid[25];
			struct rte_eth_dev_info dev_info;
			int	pfidx;							// port index in our array if we find it; -1 otherwise.

			pfidx = -1;																// default to PF not in our config list
			rte_eth_dev_info_get(portid, &dev_info);
			snprintf(pciid, sizeof( pciid ), "%04x:%02x:%02x.%01x", dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus, dev_info.pci_dev->addr.devid, dev_info.pci_dev->addr.function);
			for(i = 0; i < running_config->num_ports; ++i) {						// must record the 'real' PF number as that likely won't match array order
				if (strcmp(pciid, running_config->ports[i].pciid) == 0) {
					bleat_printf( 2, "physical port %i maps to config %d (%s)", portid, i, pciid );
					pfidx = i;
					port2config_map[portid] = i;									// map real port to our array index
					running_config->ports[i].rte_port_number = portid; 				// record the real pf number
					running_config->ports[i].nvfs_config = dev_info.max_vfs;		// number of configured VFs (could be less than max)
					break;
				}
			}

			if( pfidx >= 0 ) {														// initialise only if in our confilg file list (we may not manage everything)
				for( j = 0; j < 64; j++ ) {
					set_split_erop( portid, j, SET_ON );							// set the split receive drop enable for all VFs
				}

				if( g_parms->rflags & RF_ENABLE_QOS ) {
					state = dcb_port_init( &running_config->ports[pfidx], mbuf_pool );
				} else {
					state = port_init(portid, mbuf_pool, g_parms->pciids[portid].hw_strip_crc, &running_config->ports[pfidx] );
					set_fc_on( portid, !!(g_parms->rflags & RF_ENABLE_FC) );		// if override is set, then force our setting for fc onto nic
				}

				if( state != 0 ) {
					bleat_printf( 0, "CRI: abort: port initialisation failed: %d", (int) portid );
					rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);
				} else {
					bleat_printf( 2, "port initialisation successful for port %d [%d]", portid, pfidx );
				}

				set_pfrx_drop( portid, 1 );			// enable the drop bit for the PF queues on this port
			
				rte_eth_macaddr_get(portid, &addr);
				bleat_printf( 1,  "mapping port: %u, MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ", ",
						(unsigned)portid,
						addr.addr_bytes[0], addr.addr_bytes[1],
						addr.addr_bytes[2], addr.addr_bytes[3],
						addr.addr_bytes[4], addr.addr_bytes[5]);
	
				bleat_printf( 1, "driver: %s, index %d, pkts rx: %lu", dev_info.driver_name, dev_info.if_index, st.pcount);
				bleat_printf( 1, "pci: %04X:%02X:%02X.%01X, max VF's: %d", dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus,
					dev_info.pci_dev->addr.devid , dev_info.pci_dev->addr.function, dev_info.max_vfs );
			} else {
				port2config_map[portid] = -1;					// we must not allow an interrupt to map (we shouldn't get interrupts, but be parinoid)
				bleat_printf( 0, "pf %d (%s) is NOT in vfd config file and was not initialised", portid, pciid );
			}
			
			
			// read PCI config to get VM offset and stride
			struct rte_eth_dev_info pf_dev;
			rte_eth_dev_info_get(portid, &pf_dev);
							
			// Find the SR-IOV extended capability structure
			
			if (get_nic_type(portid) == VFD_BNXT){ 
				do {
					rte_pci_read_config(pf_dev.pci_dev, &pci_control_r, 32, cfg_offset);
					bleat_printf(4, "Header: %08x (%04x)", pci_control_r, cfg_offset);
					if ((pci_control_r & 0xffff) == 0x0010)
						break;
					cfg_offset = (pci_control_r >> 20) & ~3;
					if (cfg_offset == 0)
						break;
				} while(1);

				if (cfg_offset == 0) {
					bleat_printf(0, "Unable to locate SR-IOV configuration");
					rte_exit( EXIT_FAILURE, "initialisation failure, see log(s) in: %s\n", g_parms->log_dir );
					exit ( 1 );
				}
				rte_pci_read_config(pf_dev.pci_dev, &pci_control_r, 32, cfg_offset + 20);
			} else {
				rte_pci_read_config(pf_dev.pci_dev, &pci_control_r, 32, 0x174);					// Intel NIC's
			}
			
			
			struct sriov_port_s *port = &running_config->ports[portid];
			port->vf_offset = pci_control_r & 0x0ffff;
			port->vf_stride = pci_control_r >> 16;
					
			
	  }
		
		bleat_printf( 2, "port initialisation complete" );

		set_signals();												// register signal handlers

		gettimeofday(&st.startTime, NULL);

		bleat_printf( 1, "dpdk setup complete" );
	} else {
		bleat_printf( 1, "no action mode: skipped dpdk setup, signal initialisation, and device discovery" );
	}

	if( g_parms->forreal ) {
		g_parms->rflags |= RF_INITIALISED;								// safe to update nic now (modulo forreal mode setting of course)
	}


	vfd_add_all_vfs( g_parms, running_config );							// read all existing config files and add the VFs to the config

	if( vfd_update_nic( g_parms, running_config ) != 0 ) {				// now that dpdk is initialised run the list and 'activate' everything
		bleat_printf( 0, "CRI: abort: unable to initialise nic with base config:" );
		if( forreal ) {
			rte_exit( EXIT_FAILURE, "initialisation failure, see log(s) in: %s\n", g_parms->log_dir );
		} else {
			exit( 1 );
		}
	}
	
	run_start_cbs( running_config );				// run any user startup callback commands defined in VF configs

	bleat_printf( 1, "version: %s", version );
	bleat_printf( 1, "initialisation complete, setting bleat level to %d; starting to loop", g_parms->log_level );
	bleat_set_lvl( g_parms->log_level );					// initialisation finished, set log level to running level
	if( forreal ) {
		rte_log_set_level(g_parms->dpdk_init_log_level, RTE_LOGTYPE_PMD && RTE_LOGTYPE_PORT);
	}

	free( parm_file );			// now it's safe to free the parm file

#define MAX_PKT_BURST	32
	while(!terminated)
	{
		struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
		usleep(50000);			// .5s

		while( vfd_req_if( g_parms, running_config, 0 ) ); 				// process _all_ pending requests before going on
		// Discard any RX traffic...
		for (portid = 0; portid < n_ports; portid++) {
			uint16_t nb_pkts;
			while ( (nb_pkts = rte_eth_rx_burst(portid, 0, pkts_burst, MAX_PKT_BURST)) > 0 ) {
				uint16_t idx;
				for (idx = 0; idx < nb_pkts; idx++)
					rte_pktmbuf_free(pkts_burst[idx]);
				bleat_printf( 4, "Discarded %hu PF %d frames", nb_pkts, portid);
			}
		}

	}		// end !terminated while

	bleat_printf( 0, "terminating" );
	run_stop_cbs( running_config );				// run any user stop callback commands that were given in VF conf files

	if( fd >= 0 ) {
		close(fd);
	}

	close_ports();				// clean up the PFs

  gettimeofday(&st.endTime, NULL);
  bleat_printf( 1, "duration %.f sec\n", timeDelta(&st.endTime, &st.startTime));

  return EXIT_SUCCESS;
}
