// vi: sw=4 ts=4:
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

*/


#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "sriov.h"
#include <vfdlib.h>		// if vfdlib.h needs an include it must be included there, can't be include prior

#define DEBUG

// -------------------------------------------------------------------------------------------------------------

#define ADDED	1				// updated states
#define DELETED (-1)
#define UNCHANGED 0
#define RESET	2

#define RT_NOP	0				// request types
#define RT_ADD	1
#define RT_DEL	2
#define RT_SHOW 3
#define RT_PING 4
#define RT_VERBOSE 5
#define RT_DUMP 6

#define BUF_1K	1024			// simple buffer size constants
#define BUF_10K BUF_1K * 10

#define QOS_4TC_MODE 0			// 4 TCs mode flag
#define QOS_8TC_MODE 1			// 8 TCs mode flag

// --- local structs --------------------------------------------------------------------------------------------

typedef struct request {
	int		rtype;				// type: RT_ const
	char*	resource;			// parm file name, show target, etc.
	char*	resp_fifo;			// name of the return pipe
	int		log_level;			// for verbose
} req_t;

// --- local protos when needed ---------------------------------------------------------------------------------

static int vfd_update_nic( parms_t* parms, struct sriov_conf_c* conf );
static char* gen_stats( struct sriov_conf_c* conf, int pf_only );

// ---------------------globals: bad form, but unavoidable -------------------------------------------------------
static const char* version = "v1.3/1a186";
static parms_t *g_parms = NULL;						// most functions should accept a pointer, however we have to have a global for the callback function support

// --- misc support ----------------------------------------------------------------------------------------------

/*
	Validate the string passed in contains a plausable MAC address of the form:
		hh:hh:hh:hh:hh:hh

	Returns -1 if invalid, 0 if ok.
*/

static int is_valid_mac_str( char* mac ) {
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
static void run_start_cbs( struct sriov_conf_c* conf ) {
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

static void run_stop_cbs( struct sriov_conf_c* conf ) {
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
*/
static struct sriov_port_s *suss_port( int portid ) {
	int		rc_idx; 					// index into our config

	if( portid < 0 || portid > running_config.num_ports ) {
		bleat_printf( 1, "suss_port: port is out of range: %d", portid );
		return NULL;
	}

	rc_idx = rte_config_portmap[portid];				// tanslate port to index
	if( rc_idx >= running_config.num_ports ) {
		bleat_printf( 1, "suss_port: port index for port %d (%d) is out of range", portid, rc_idx );
		return NULL;
	}

	return &running_config.ports[rc_idx];
}

/*
	Given a port and vfid, find the vf block and return a pointer to it.
*/
static struct vf_s *suss_vf( int port, int vfid ) {
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
	int		argc;					// argc/v parms we dummy up
	char** argv;
	int		argc_idx = 12;			// insertion index into argc (initial value depends on static parms below)
	int		i;
	char	wbuf[128];				// scratch buffer
	int		count;

	if( parms->npciids <= 0 ) {
		bleat_printf( 0, "CRI: abort: no pciids were defined in the configuration file" );
		exit( 1 );
	}

	argc = argc_idx + (parms->npciids * 2);											// 2 slots for each pcciid;  number to alloc is one larger to allow for ending nil
	if( (argv = (char **) malloc( (argc + 1) * sizeof( char* ) )) == NULL ) {		// n static parms + 2 slots for each pciid + null
		bleat_printf( 0, "CRI: abort: unable to alloc memory for eal initialisation" );
		exit( 1 );
	}
	memset( argv, 0, sizeof( char* ) * (argc + 1) );

	argv[0] = strdup(  "vfd" );						// dummy up a command line to pass to rte_eal_init() -- it expects that we got these on our command line (what a hack)


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
	
	argv[1] = strdup( "-c" );
	argv[2] = strdup( parms->cpu_mask );

	argv[3] = strdup( "-n" );
	argv[4] = strdup( "4" );
		
	argv[5] = strdup( "-m" );
	argv[6] = strdup( "50" );					// MiB of memory
	
	argv[7] = strdup( "--file-prefix" );
	argv[8] = strdup( "vfd" );					// dpdk creates some kind of lock file, this is used for that
	
	argv[9] = strdup( "--log-level" );
	snprintf( wbuf, sizeof( wbuf ), "%d", parms->dpdk_init_log_level );
	argv[10] = strdup( wbuf );
	
	argv[11] = strdup( "--no-huge" );

	for( i = 0; i < parms->npciids && argc_idx < argc - 1; i++ ) {			// add in the -w pciid values to the list
		argv[argc_idx++] = strdup( "-w" );
		argv[argc_idx++] = strdup( parms->pciids[i].id );
		bleat_printf( 1, "add pciid to dpdk dummy command line -w %s", parms->pciids[i].id );
	}

	dummy_rte_eal_init( argc, argv );			// print out parms, vet, etc.
	if( parms->forreal ) {
		bleat_printf( 1, "invoking real rte initialisation argc=%d", argc );
		i = rte_eal_init( argc, argv ); 			// http://dpdk.org/doc/api/rte__eal_8h.html
		bleat_printf( 1, "initialisation returned %d", i );
	} else {
		bleat_printf( 1, "rte initialisation skipped (no harm mode)" );
		i = 1;
	}

	return i;
}

/*
	Create our fifo and tuck the handle into the parm struct. Returns 0 on
	success and <0 on failure.
*/
static int vfd_init_fifo( parms_t* parms ) {
	if( !parms ) {
		return -1;
	}

	umask( 0 );
	parms->rfifo = rfifo_create( parms->fifo_path, 0666 );		//TODO -- set mode more sainly, but this runs as root, so regular users need to write to this thus open wide for now
	if( parms->rfifo == NULL ) {
		bleat_printf( 0, "ERR: unable to create request fifo (%s): %s", parms->fifo_path, strerror( errno ) );
		return -1;
	} else {
		bleat_printf( 0, "listening for requests via pipe: %s", parms->fifo_path );
	}

	return 0;
}

//  --------------------- global config management ------------------------------------------------------------

/*
	Pull the list of pciids from the parms and set into the in memory configuration that
	is maintained. If this is called more than once, it will refuse to do anything.
*/
static void vfd_add_ports( parms_t* parms, struct sriov_conf_c* conf ) {
	static int called = 0;		// doesn't makes sense to do this more than once
	int i;
	int pidx = 0;				// port idx in conf list
	struct sriov_port_s* port;

	if( called )
		return;
	called = 1;
	
	for( i = 0; pidx < MAX_PORTS  && i < parms->npciids; i++, pidx++ ) {
		port = &conf->ports[pidx];
		port->last_updated = ADDED;												// flag newly added so the nic is configured next go round
		snprintf( port->name, sizeof( port->name ), "port-%d",  i);				// TODO--- support getting a name from the config
		snprintf( port->pciid, sizeof( port->pciid ), "%s", parms->pciids[i].id );
		port->mtu = parms->pciids[i].mtu;
		port->enable_loopback = !!( parms->pciids[i].flags & PFF_LOOP_BACK );
		port->num_mirros = 0;
		port->num_vfs = 0;
		
		bleat_printf( 1, "add pciid to in memory config: %s mtu=%d", parms->pciids[i].id, parms->pciids[i].mtu );
	}

	conf->num_ports = pidx;
}

/*
	Add one of the nova generated configuration files to a global config struct passed in.
	A small amount of error checking (vf id dup, etc) is done, so the return is either
	1 for success or 0 for failure. Errno is set only if we can't open the file.
	If reason is not NULL we'll create a message buffer and drop the address there
	(caller must free).

	Future:
	It would make more sense for the config reader in lib to actually populate the
	actual vf struct rather than having to copy it, but because the port struct
	doesn't have dynamic VF structs (has a hard array), we need to read it into
	a separate location and copy it anyway, so the manual copy, rathter than a
	memcpy() is a minor annoyance.  Ultimately, the port should reference an
	array of pointers, and config should pull directly into a vf_s and if the
	parms are valid, then the pointer added to the list.
*/
static int vfd_add_vf( struct sriov_conf_c* conf, char* fname, char** reason ) {
	vf_config_t* vfc;					// raw vf config file contents	
	int	i;
	int j;
	int vidx;							// index into the vf array
	int	hole = -1;						// first hole in the list;
	struct sriov_port_s* port = NULL;	// reference to a single port in the config
	struct vf_s*	vf;					// point at the vf we need to fill in
	char mbuf[BUF_1K];					// message buffer if we fail
	int tot_vlans = 0;					// must count vlans and macs to ensure limit not busted
	int tot_macs = 0;
	

	if( conf == NULL || fname == NULL ) {
		bleat_printf( 0, "vfd_add_vf called with nil config or filename pointer" );
		if( reason ) {
			snprintf( mbuf, sizeof( mbuf), "internal mishap: config ptr was nil" );
			*reason = strdup( mbuf );
		}
		return 0;
	}

	if( (vfc = read_config( fname )) == NULL ) {
		snprintf( mbuf, sizeof( mbuf ), "unable to read config file: %s: %s", fname, errno > 0 ? strerror( errno ) : "unknown sub-reason" );
		bleat_printf( 1, "vfd_add_vf failed: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		return 0;
	}

	bleat_printf( 2, "add: config data: name: %s", vfc->name );
	bleat_printf( 2, "add: config data: pciid: %s", vfc->pciid );
	bleat_printf( 2, "add: config data: vfid: %d", vfc->vfid );

	if( vfc->pciid == NULL || vfc->vfid < 1 ) {
		snprintf( mbuf, sizeof( mbuf ), "unable to read or parse config file: %s", fname );
		bleat_printf( 1, "vfd_add_vf failed: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

	for( i = 0; i < conf->num_ports; i++ ) {						// find the port that this vf is attached to
		if( strcmp( conf->ports[i].pciid, vfc->pciid ) == 0 ) {	// match
			port = &conf->ports[i];
			break;
		}
	}

	if( port == NULL ) {
		snprintf( mbuf, sizeof( mbuf ), "%s: could not find port %s in the config", vfc->name, vfc->pciid );
		bleat_printf( 1, "vf not added: %s", mbuf );
		free_config( vfc );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		return 0;
	}

	for( i = 0; i < port->num_vfs; i++ ) {				// ensure ID is not already defined
		if( port->vfs[i].num < 0 ) {					// this is a hole
			if( hole < 0 ) {
				hole = i;								// we'll insert here
			}
		} else {
			if( port->vfs[i].num == vfc->vfid ) {			// dup, fail
				snprintf( mbuf, sizeof( mbuf ), "vfid %d already exists on port %s", vfc->vfid, vfc->pciid );
				bleat_printf( 1, "vf not added: %s", mbuf );
				if( reason ) {
					*reason = strdup( mbuf );
				}
				free_config( vfc );
				return 0;
			}

			tot_vlans += port->vfs[i].num_vlans;
			tot_macs += port->vfs[i].num_macs;
		}
	}

	if( hole >= 0 ) {			// set the index into the vf array based on first hole found, or no holes
		vidx = hole;
	} else {
		vidx = i;
	}

	if( vidx >= MAX_VFS || vfc->vfid < 1 || vfc->vfid > 31) {							// something is out of range
		snprintf( mbuf, sizeof( mbuf ), "max VFs already defined or vfid %d is out of range", vfc->vfid );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}

		free_config( vfc );
		return 0;
	}

	if( vfc->vfid >= port->nvfs_config ) {		// greater than the number configured
		snprintf( mbuf, sizeof( mbuf ), "vf %d is out of range; only %d VFs are configured on port %s", vfc->vfid, port->nvfs_config, port->pciid );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}

		free_config( vfc );
		return 0;
	}

	if( vfc->nvlans > MAX_VF_VLANS ) {				// more than allowed for a single VF
		snprintf( mbuf, sizeof( mbuf ), "number of vlans supplied (%d) exceeds the maximum (%d)", vfc->nvlans, MAX_VF_VLANS );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

	if( vfc->nvlans + tot_vlans > MAX_PF_VLANS ) { 			// would bust the total across the whole PF
		snprintf( mbuf, sizeof( mbuf ), "number of vlans supplied (%d) cauess total for PF to exceed the maximum (%d)", vfc->nvlans, MAX_PF_VLANS );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

	if( vfc->nmacs + tot_macs > MAX_PF_MACS ) { 			// would bust the total across the whole PF
		snprintf( mbuf, sizeof( mbuf ), "number of macs supplied (%d) cauess total for PF to exceed the maximum (%d)", vfc->nmacs, MAX_PF_MACS );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}


	if( vfc->nmacs > MAX_VF_MACS ) {
		snprintf( mbuf, sizeof( mbuf ), "number of macs supplied (%d) exceeds the maximum (%d)", vfc->nmacs, MAX_VF_MACS );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

	if( vfc->strip_stag  &&  vfc->nvlans > 1 ) {		// one vlan is allowed when stripping
		snprintf( mbuf, sizeof( mbuf ), "conflicting options: strip_stag may not be supplied with a list of vlan ids" );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

														// check vlan and mac arrays for duplicate values and bad things
	if( vfc->nvlans == 1 ) {							// no need for a dup check, just a range check
		if( vfc->vlans[0] < 1 || vfc->vlans[0] > 4095 ) {
			snprintf( mbuf, sizeof( mbuf ), "invalid vlan id: %d", vfc->vlans[0] );
			bleat_printf( 1, "vf not added: %s", mbuf );
			if( reason ) {
				*reason = strdup( mbuf );
			}
			free_config( vfc );
			return 0;
		}
	} else {
		for( i = 0; i < vfc->nvlans-1; i++ ) {
			if( vfc->vlans[i] < 1 || vfc->vlans[i] > 4095 ) {			// range check
				snprintf( mbuf, sizeof( mbuf ), "invalid vlan id: %d", vfc->vlans[i] );
				bleat_printf( 1, "vf not added: %s", mbuf );
				if( reason ) {
					*reason = strdup( mbuf );
				}
				free_config( vfc );
				return 0;
			}
	
			for( j = i+1; j < vfc->nvlans; j++ ) {
				if( vfc->vlans[i] == vfc->vlans[j] ) {					// dup check
					snprintf( mbuf, sizeof( mbuf ), "dupliate vlan in list: %d", vfc->vlans[i] );
					bleat_printf( 1, "vf not added: %s", mbuf );
					if( reason ) {
						*reason = strdup( mbuf );
					}
					free_config( vfc );
					return 0;
				}
			}
		}
	}

	if( vfc->nmacs == 1 ) {											// only need range check if one
		if( is_valid_mac_str( vfc->macs[0] ) < 0 ) {
			snprintf( mbuf, sizeof( mbuf ), "invalid mac in list: %s", vfc->macs[0] );
			bleat_printf( 1, "vf not added: %s", mbuf );
			if( reason ) {
				*reason = strdup( mbuf );
			}
			free_config( vfc );
			return 0;
		}
	} else {
		for( i = 0; i < vfc->nmacs-1; i++ ) {
			if( is_valid_mac_str( vfc->macs[i] ) < 0 ) {					// range check
				snprintf( mbuf, sizeof( mbuf ), "invalid mac in list: %s", vfc->macs[i] );
				bleat_printf( 1, "vf not added: %s", mbuf );
				if( reason ) {
					*reason = strdup( mbuf );
				}
				free_config( vfc );
				return 0;
			}

			for( j = i+1; j < vfc->nmacs; j++ ) {
				if( strcmp( vfc->macs[i], vfc->macs[j] ) == 0 ) {			// dup check
					snprintf( mbuf, sizeof( mbuf ), "dupliate mac in list: %s", vfc->macs[i] );
					bleat_printf( 1, "vf not added: %s", mbuf );
					if( reason ) {
						*reason = strdup( mbuf );
					}
					free_config( vfc );
					return 0;
				}
			}
		}
	}

	if( vfc->start_cb != NULL && strchr( vfc->start_cb, ';' ) != NULL ) {
		snprintf( mbuf, sizeof( mbuf ), "start_cb command contains invalid character: ;" );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}
	if( vfc->stop_cb != NULL && strchr( vfc->stop_cb, ';' ) != NULL ) {
		snprintf( mbuf, sizeof( mbuf ), "stop_cb command contains invalid character: ;" );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

	// CAUTION: if we fail because of a parm error it MUST happen before here!
	if( vidx == port->num_vfs ) {		// inserting at end, bump the num we have used
		port->num_vfs++;
	}
	
	vf = &port->vfs[vidx];						// copy from config data doing any translation needed
	memset( vf, 0, sizeof( *vf ) );				// assume zeroing everything is good
	vf->owner = vfc->owner;
	vf->num = vfc->vfid;
	port->vfs[vidx].last_updated = ADDED;		// signal main code to configure the buggger
	vf->strip_stag = vfc->strip_stag;
	vf->insert_stag = vfc->strip_stag;		// for now they are based on the same flag
	vf->allow_bcast = vfc->allow_bcast;
	vf->allow_mcast = vfc->allow_mcast;
	vf->allow_un_ucast = vfc->allow_un_ucast;

	vf->allow_untagged = 0;					// for now these cannot be set by the config file data
	vf->vlan_anti_spoof = 1;
	vf->mac_anti_spoof = 1;

	vf->rate = 0.0;							// best effort :)
	vf->rate = vfc->rate;
	
	if( vfc->start_cb != NULL ) {
		vf->start_cb = strdup( vfc->start_cb );
	}
	if( vfc->stop_cb != NULL ) {
		vf->stop_cb = strdup( vfc->stop_cb );
	}

	vf->link = 0;							// default if parm missing or mis-set (not fatal)
	switch( *vfc->link_status ) {			// down, up or auto are allowed in config file
		case 'a':
		case 'A':
			vf->link = 0;					// auto is really: use what is configured in the PF	
			break;
		case 'd':
		case 'D':
			vf->link = -1;
			break;
		case 'u':
		case 'U':
			vf->link = 1;
			break;

		
		default:
			bleat_printf( 1, "link_status not recognised in config: %s; defaulting to auto", vfc->link_status );
			vf->link = 0;
			break;
	}
	
	for( i = 0; i < vfc->nvlans; i++ ) {
		vf->vlans[i] = vfc->vlans[i];
	}
	vf->num_vlans = vfc->nvlans;

	for( i = 0; i < vfc->nmacs; i++ ) {
		strcpy( vf->macs[i], vfc->macs[i] );		// we vet for length earlier, so this is safe.
	}
	vf->num_macs = vfc->nmacs;

	if( reason ) {
		*reason = NULL;
	}

	bleat_printf( 2, "VF was added: %s %s id=%d", vfc->name, vfc->pciid, vfc->vfid );
	free_config( vfc );
	return 1;
}

/*
	Get a list of all config files and add each one to the current config.
	If one fails, we will generate an error and ignore it.
*/
static void vfd_add_all_vfs(  parms_t* parms, struct sriov_conf_c* conf ) {
	char** flist; 					// list of files to pull in
	int		llen;					// list length
	int		i;

	if( parms == NULL || conf == NULL ) {
		bleat_printf( 0, "internal mishap: NULL conf or parms pointer passed to add_all_vfs" );
		return;
	}

	flist = list_files( parms->config_dir, "json", 1, &llen );
	if( flist == NULL || llen <= 0 ) {
		bleat_printf( 1, "zero vf configuration files (*.json) found in %s; nothing restored", parms->config_dir );
		return;
	}

	bleat_printf( 1, "adding %d existing vf configuration files to the mix", llen );

	
	for( i = 0; i < llen; i++ ) {
		bleat_printf( 2, "parsing %s", flist[i] );
		if( ! vfd_add_vf( conf, flist[i], NULL ) ) {
			bleat_printf( 0, "add_all_vfs: could not add %s", flist[i] );
		}
	}
	
	free_list( flist, llen );
}

/*
	Delete a VF from a port.  We expect the name of a file which we can read the
	parms from and suss out the pciid and the vfid.  Those are used to find the
	info in the global config and render it useless. The first thing we attempt
	to do is to remove or rename the config file.  If we can't do that we
	don't do anything else because we'd give the false sense that it was deleted
	but on restart we'd recreate it, or worse have a conflict with something that
	was added.
*/
static int vfd_del_vf( parms_t* parms, struct sriov_conf_c* conf, char* fname, char** reason ) {
	vf_config_t* vfc;					// raw vf config file contents	
	int	i;
	int vidx;							// index into the vf array
	struct sriov_port_s* port = NULL;	// reference to a single port in the config
	char mbuf[BUF_1K];					// message buffer if we fail
	
	if( conf == NULL || fname == NULL ) {
		bleat_printf( 0, "vfd_del_vf called with nil config or filename pointer" );
		if( reason ) {
			snprintf( mbuf, sizeof( mbuf), "internal mishap: config ptr was nil" );
			*reason = strdup( mbuf );
		}
		return 0;
	}

	if( (vfc = read_config( fname )) == NULL ) {
		snprintf( mbuf, sizeof( mbuf ), "unable to read config file: %s: %s", fname, errno > 0 ? strerror( errno ) : "unknown sub-reason" );
		bleat_printf( 1, "vfd_del_vf failed: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		return 0;
	}

	if( parms->delete_keep ) {											// need to keep the old by renaming it with a trailing -
		snprintf( mbuf, sizeof( mbuf ), "%s-", fname );
		if( rename( fname, mbuf ) < 0 ) {
			snprintf( mbuf, sizeof( mbuf ), "unable to rename config file: %s: %s", fname, strerror( errno ) );
			bleat_printf( 1, "vfd_del_vf failed: %s", mbuf );
			if( reason ) {
				*reason = strdup( mbuf );
			}
			free_config( vfc );
			return 0;
		}
	} else {
		if( unlink( fname ) < 0 ) {
			snprintf( mbuf, sizeof( mbuf ), "unable to delete config file: %s: %s", fname, strerror( errno ) );
			bleat_printf( 1, "vfd_del_vf failed: %s", mbuf );
			if( reason ) {
				*reason = strdup( mbuf );
			}
			free_config( vfc );
			return 0;
		}
	}

	bleat_printf( 2, "del: config data: name: %s", vfc->name );
	bleat_printf( 2, "del: config data: pciid: %s", vfc->pciid );
	bleat_printf( 2, "del: config data: vfid: %d", vfc->vfid );

	if( vfc->pciid == NULL || vfc->vfid < 1 ) {
		snprintf( mbuf, sizeof( mbuf ), "unable to read config file: %s", fname );
		bleat_printf( 1, "vfd_del_vf failed: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

	for( i = 0; i < conf->num_ports; i++ ) {						// find the port that this vf is attached to
		if( strcmp( conf->ports[i].pciid, vfc->pciid ) == 0 ) {	// match
			port = &conf->ports[i];
			break;
		}
	}

	if( port == NULL ) {
		snprintf( mbuf, sizeof( mbuf ), "%s: could not find port %s in the config", vfc->name, vfc->pciid );
		bleat_printf( 1, "vf not added: %s", mbuf );
		free_config( vfc );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		return 0;
	}

	vidx = -1;
	for( i = 0; i < port->num_vfs; i++ ) {				// suss out the id that is listed
		if( port->vfs[i].num == vfc->vfid ) {			// this is it.
			vidx = i;
			break;
		}
	}

	if( vidx >= 0 ) {									//  it's there -- take down in the config
		port->vfs[vidx].last_updated = DELETED;			// signal main code to nuke the puppy (vfid stays set so we don't see it as a hole until it's gone)
	} else {
		bleat_printf( 1, "warning: del didn't find the pciid/vf combination in the active config: %s/%d", vfc->pciid, vfc->vfid );
	}
	
	if( reason ) {
		*reason = NULL;
	}
	bleat_printf( 2, "VF was deleted: %s %s id=%d", vfc->name, vfc->pciid, vfc->vfid );
	return 1;
}

// ---- request/response functions -----------------------------------------------------------------------------

/*
	Write to an open file des with a simple retry mechanism.  We cannot afford to block forever,
	so we'll try only a few times if we make absolutely no progress.
*/
static int vfd_write( int fd, const char* buf, int len ) {
	int	tries = 5;				// if we have this number of times where there is no progress we give up
	int	nsent;					// number of bytes actually sent
	int n2send;					// number of bytes left to send

	n2send = len;
	while( n2send > 0 && tries > 0 ) {
		nsent = write( fd, buf, n2send );			// hard error; quit immediately
		if( nsent < 0 ) {
			bleat_printf( 0, "WRN: write error attempting %d, wrote only %d bytes: %s", len, len - n2send, strerror( errno ) );
			return -1;
		}
			
		if( nsent == n2send ) {
			return len;
		}

		if( nsent > 0 ) { 		// something sent, so we assume iplex is actively reading
			n2send -= nsent;
			buf += nsent;
		} else {
			tries--;
			usleep(50000);			// .5s
		}
	}

	bleat_printf( 0, "WRN: write timed out attempting %d, but wrote only %d bytes", len, len - n2send );
	return -1;
}

/*
	Construct json to write onto the response pipe.  The response pipe is opened in non-block mode
	so that it will fail immiediately if there isn't a reader or the pipe doesn't exist. We assume
	that the requestor opens the pipe before sending the request so that if it is delayed after
	sending the request it does not prevent us from writing to the pipe.  If we don't open in 	
	blocked mode we could hang foever if the requestor dies/aborts.
*/
static void vfd_response( char* rpipe, int state, const char* msg ) {
	int 	fd;
	char	buf[BUF_1K];

	if( rpipe == NULL ) {
		return;
	}

	if( (fd = open( rpipe, O_WRONLY | O_NONBLOCK, 0 )) < 0 ) {
	 	bleat_printf( 0, "unable to deliver response: open failed: %s: %s", rpipe, strerror( errno ) );
		return;
	}

	if( bleat_will_it( 2 ) ) {
		bleat_printf( 2, "sending response: %s(%d) [%d] %d bytes", rpipe, fd, state, strlen( msg ) );
	} else {
		bleat_printf( 3, "sending response: %s(%d) [%d] %s", rpipe, fd, state, msg );
	}

	snprintf( buf, sizeof( buf ), "{ \"state\": \"%s\", \"msg\": \"", state ? "ERROR" : "OK" );
	if ( vfd_write( fd, buf, strlen( buf ) ) > 0 ) {
		if ( msg == NULL || vfd_write( fd, msg, strlen( msg ) ) > 0 ) {
			snprintf( buf, sizeof( buf ), "\" }\n" );				// terminate the json
			vfd_write( fd, buf, strlen( buf ) );
			bleat_printf( 2, "response written to pipe" );			// only if all of message written 
		}
	}

	bleat_pop_lvl();			// we assume it was pushed when the request received; we pop it once we respond
	close( fd );
}

/*
	Cleanup a request and free the memory.
*/
static void vfd_free_request( req_t* req ) {
	if( req->resource != NULL ) {
		free( req->resource );
	}
	if( req->resp_fifo != NULL ) {
		free( req->resp_fifo );
	}

	free( req );
}

/*
	Read an iplx request from the fifo, and format it into a request block.
	A pointer to the struct is returned; the caller must use vfd_free_request() to
	properly free it.
*/
static req_t* vfd_read_request( parms_t* parms ) {
	void*	jblob;				// json parsing stuff
	char*	rbuf;				// raw request buffer from the pipe
	char*	stuff;				// stuff teased out of the json blob
	req_t*	req = NULL;
	int		lvl;				// log level supplied

	rbuf = rfifo_read( parms->rfifo );
	if( ! *rbuf ) {				// empty, nothing to do
		free( rbuf );
		return NULL;
	}

	if( (jblob = jw_new( rbuf )) == NULL ) {
		bleat_printf( 0, "ERR: failed to create a json parsing object for: %s", rbuf );
		free( rbuf );
		return NULL;
	}

	if( (stuff = jw_string( jblob, "action" )) == NULL ) {
		bleat_printf( 0, "ERR: request received without action: %s", rbuf );
		free( rbuf );
		jw_nuke( jblob );
		return NULL;
	}

	
	if( (req = (req_t *) malloc( sizeof( *req ) )) == NULL ) {
		bleat_printf( 0, "ERR: memory allocation error tying to alloc request for: %s", rbuf );
		free( rbuf );
		jw_nuke( jblob );
		return NULL;
	}
	memset( req, 0, sizeof( *req ) );

	bleat_printf( 2, "raw message: (%s)", rbuf );

	switch( *stuff ) {				// we assume compiler builds a jump table which makes it faster than a bunch of nested string compares
		case 'a':
		case 'A':					// assume add until something else starts with a
			req->rtype = RT_ADD;
			break;

		case 'd':
		case 'D':
			if( strcmp( stuff, "dump" ) == 0 ) {
				req->rtype = RT_DUMP;
			} else {
				req->rtype = RT_DEL;
			}
			break;

		case 'p':					// ping
			req->rtype = RT_PING;
			break;

		case 's':
		case 'S':					// assume show
			req->rtype = RT_SHOW;
			break;

		case 'v':
			req->rtype = RT_VERBOSE;
			break;	

		default:
			bleat_printf( 0, "ERR: unrecognised action in request: %s", rbuf );
			jw_nuke( jblob );
			return NULL;
			break;
	}

	if( (stuff = jw_string( jblob, "params.filename")) != NULL ) {
		req->resource = strdup( stuff );
	} else {
		if( (stuff = jw_string( jblob, "params.resource")) != NULL ) {
			req->resource = strdup( stuff );
		}
	}
	if( (stuff = jw_string( jblob, "params.r_fifo")) != NULL ) {
		req->resp_fifo = strdup( stuff );
	}
	
	req->log_level = lvl = jw_missing( jblob, "params.loglevel" ) ? 0 : (int) jw_value( jblob, "params.loglevel" );
	bleat_push_glvl( lvl );					// push the level if greater, else push current so pop won't fail

	free( rbuf );
	jw_nuke( jblob );
	return req;
}

/*
	Request interface. Checks the request pipe and handles a reqest. If
	forever is set then this is a black hole (never returns).
	Returns true if it handled a request, false otherwise.
*/
static int vfd_req_if( parms_t *parms, struct sriov_conf_c* conf, int forever ) {
	req_t*	req;
	char	mbuf[2048];			// message and work buffer
	char*	buf;				// buffer gnerated by something else
	int		rc = 0;
	char*	reason;
	int		req_handled = 0;

	if( forever ) {
		bleat_printf( 1, "req_if: forever loop entered" );
	}

	*mbuf = 0;
	do {
		if( (req = vfd_read_request( parms )) != NULL ) {
			bleat_printf( 3, "got request" );
			req_handled = 1;

			switch( req->rtype ) {
				case RT_PING:
					snprintf( mbuf, sizeof( mbuf ), "pong: %s", version );
					vfd_response( req->resp_fifo, 0, mbuf );
					break;

				case RT_ADD:
					if( strchr( req->resource, '/' ) != NULL ) {									// assume fully qualified if it has a slant
						strcpy( mbuf, req->resource );
					} else {
						snprintf( mbuf, sizeof( mbuf ), "%s/%s", parms->config_dir, req->resource );
					}

					bleat_printf( 2, "adding vf from file: %s", mbuf );
					if( vfd_add_vf( conf, req->resource, &reason ) ) {		// read the config file and add to in mem config if ok
						if( vfd_update_nic( parms, conf ) == 0 ) {			// added to config was good, drive the nic update
							snprintf( mbuf, sizeof( mbuf ), "vf added successfully: %s", req->resource );
							vfd_response( req->resp_fifo, 0, mbuf );
							bleat_printf( 1, "vf added: %s", mbuf );
						} else {
							// TODO -- must turn the vf off so that another add can be sent without forcing a delete
							// 		update_nic always returns good now, so this waits until it catches errors and returns bad
							snprintf( mbuf, sizeof( mbuf ), "vf add failed: unable to configure the vf for: %s", req->resource );
							vfd_response( req->resp_fifo, 0, mbuf );
							bleat_printf( 1, "vf add failed nic update error" );
						}
					} else {
						snprintf( mbuf, sizeof( mbuf ), "unable to add vf: %s: %s", req->resource, reason );
						vfd_response( req->resp_fifo, 1, mbuf );
						free( reason );
					}
					if( bleat_will_it( 3 ) ) {					// TODO:  remove after testing
  						dump_sriov_config( *conf );
					}
					break;

				case RT_DEL:
					if( strchr( req->resource, '/' ) != NULL ) {									// assume fully qualified if it has a slant
						strcpy( mbuf, req->resource );
					} else {
						snprintf( mbuf, sizeof( mbuf ), "%s/%s", parms->config_dir, req->resource );
					}

					bleat_printf( 1, "deleting vf from file: %s", mbuf );
					if( vfd_del_vf( parms, conf, req->resource, &reason ) ) {		// successfully updated internal struct
						if( vfd_update_nic( parms, conf ) == 0 ) {			// nic update was good too
							snprintf( mbuf, sizeof( mbuf ), "vf deleted successfully: %s", req->resource );
							vfd_response( req->resp_fifo, 0, mbuf );
							bleat_printf( 1, "vf deleted: %s", mbuf );
						} // TODO need else -- see above
					} else {
						snprintf( mbuf, sizeof( mbuf ), "unable to delete vf: %s: %s", req->resource, reason );
						vfd_response( req->resp_fifo, 1, mbuf );
						free( reason );
					}
					if( bleat_will_it( 3 ) ) {					// TODO:  remove after testing
  						dump_sriov_config( *conf );
					}
					break;

				case RT_DUMP:									// spew everything to the log
					dump_dev_info( conf->num_ports);			// general info about each port
  					dump_sriov_config( *conf );					// pf/vf specific info
					vfd_response( req->resp_fifo, 0, "dump captured in the log" );
					break;

				case RT_SHOW:
					if( parms->forreal ) {
						if( req->resource != NULL  &&  strcmp( req->resource, "pfs" ) == 0 ) {				// dump just the VF information
							if( (buf = gen_stats( conf, 1 )) != NULL )  {		// todo need to replace 1 with actual number of ports
								vfd_response( req->resp_fifo, 0, buf );
								free( buf );
							} else {
								vfd_response( req->resp_fifo, 1, "unable to generate pf stats" );
							}
						} else {
							if( req->resource != NULL  &&  isdigit( *req->resource ) ) {						// dump just for the indicated pf (future)
								vfd_response( req->resp_fifo, 1, "show of specific PF is not supported in this release; use 'all' or 'pfs'." );
							} else {												// assume we dump for all
								if( (buf = gen_stats( conf, 0 )) != NULL )  {		// todo need to replace 1 with actual number of ports
									vfd_response( req->resp_fifo, 0, buf );
									free( buf );
								} else {
									vfd_response( req->resp_fifo, 1, "unable to generate stats" );
								}
							}
						}
					} else {
							vfd_response( req->resp_fifo, 1, "VFD running in 'no harm' (-n) mode; no stats available." );
					}
					break;

				case RT_VERBOSE:
					if( req->log_level >= 0 ) {
						bleat_set_lvl( req->log_level );
						bleat_push_lvl( req->log_level );			// save it so when we pop later it doesn't revert

						bleat_printf( 0, "verbose level changed to %d", req->log_level );
						snprintf( mbuf, sizeof( mbuf ), "verbose level changed to: %d", req->log_level );
					} else {
						rc = 1;
						snprintf( mbuf, sizeof( mbuf ), "loglevel out of range: %d", req->log_level );
					}

					vfd_response( req->resp_fifo, rc, mbuf );
					break;
					

				default:
					vfd_response( req->resp_fifo, 1, "dummy request handler: urrecognised request." );
					break;
			}

			vfd_free_request( req );
		}
		
		if( forever )
			sleep( 1 );
	} while( forever );

	return req_handled;			// true if we did something -- more frequent recall if we did
}

// ----------------- actual nic management ------------------------------------------------------------------------------------

/*
	Generate a set of stats to a single buffer. Return buffer to caller (caller must free).
	If pf_only is true, then the VF stats are skipped.
*/
static char*  gen_stats( struct sriov_conf_c* conf, int pf_only ) {
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

	rbidx = snprintf( rbuf, BUF_SIZE, "%s %14s %10s %10s %10s %10s %10s %10s %10s %10s %10s %10s\n",
			"\nPF/VF  ID    PCIID", "Link", "Speed", "Duplex", "RX pkts", "RX bytes", "RX errors", "RX dropped", "TX pkts", "TX bytes", "TX errors", "Spoofed");
	
	for( i = 0; i < conf->num_ports; ++i ) {
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
	Set up the insert and strip charastics on the NIC. The interface should ensure that
	the right parameter combinations are set and reject an add request if not, but 
	we are a bit parinoid and will help to enforce things here too.  If one VLAN is in
	the list, then we allow strip_stag to control what we do. If multiple VLANs are in 
	the list, then we don't strip nor insert.

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

		if( vf->strip_stag ) {																// when stripping, we must also insert
			bleat_printf( 2, "%s vf: %d set insert vlan tag with id %d", port->name, vf->num, vf->vlans[0] );
			tx_vlan_insert_set_on_vf(port->rte_port_number, vf->num, vf->vlans[0] );
		} else {
			bleat_printf( 2, "%s vf: %d set insert vlan tag with id 0", port->name, vf->num );
			tx_vlan_insert_set_on_vf( port->rte_port_number, vf->num, 0 );					// no strip, so no insert
		}
	} else {
		bleat_printf( 2, "%s vf: %d vlan list contains %d entries; strip/insert turned off", port->name, vf->num, vf->num_vlans );
		rx_vlan_strip_set_on_vf(port->rte_port_number, vf->num, 0 );					// if more than one vlan in the list force strip to be off
		tx_vlan_insert_set_on_vf( port->rte_port_number, vf->num, 0 );					// and set insert to id 0
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
static int vfd_update_nic( parms_t* parms, struct sriov_conf_c* conf ) {
	int i;
	int on = 1;
    uint32_t vf_mask;
    int y;

	if( parms->initialised == 0 ) {
		bleat_printf( 2, "update_nic: not initialised, nic settings not updated" );
		return 0;
	}

	for (i = 0; i < conf->num_ports; ++i){							// run each port we know about
		int ret;
		struct sriov_port_s* port;

		port = &conf->ports[i];

		if( parms->forreal ) {
			if( port->enable_loopback ) {
				tx_set_loopback( i, 1 ); 				// ensure it is set
			} else {
				tx_set_loopback(i, 0); 					// disable NIC loopback meaning all VM-VM traffic must go to TOR before coming back
			}

			set_queue_drop( i, 1 );						// enable packet dropping if no descriptor matches
		}

		if( port->last_updated == ADDED ) {								// updated since last call, reconfigure
			if( parms->forreal ) {
				bleat_printf( 1, "port updated: %s/%s",  port->name, port->pciid );
				rte_eth_promiscuous_enable(port->rte_port_number);
				rte_eth_allmulticast_enable(port->rte_port_number);
	
				ret = rte_eth_dev_uc_all_hash_table_set(port->rte_port_number, on);
				if (ret < 0)
					bleat_printf( 0, "ERR: bad unicast hash table parameter, return code = %d", ret);
	
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
			struct vf_s *vf = &port->vfs[y];   			// at the VF to work on

			vf_mask = VFN2MASK(vf->num);

			if( vf->last_updated != UNCHANGED ) {					// this vf was changed (add/del/reset), reconfigure it
				const char* reason;

				switch( vf->last_updated ) {
					case ADDED:		reason = "add"; break;
					case DELETED:	reason = "delete"; break;
					case RESET:		reason = "reset"; break;
					default:		reason = "unknown reason"; break;
				}
				bleat_printf( 1, "reconfigure vf for %s: %s vf=%d", reason, port->pciid, vf->num );

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

					for(v = 0; v < vf->num_vlans; ++v) {
						int vlan = vf->vlans[v];
						bleat_printf( 2, "delete vlan: %s vf=%d vlan=%d", port->pciid, vf->num, vlan );
						if( parms->forreal )
							set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, 0);		// remove the vlan id from the list
					}
				} else {
					int v;
					for(v = 0; v < vf->num_vlans; ++v) {
						int vlan = vf->vlans[v];
						bleat_printf( 2, "add vlan: %s vf=%d vlan=%d", port->pciid, vf->num, vlan );
						if( parms->forreal )
							set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, on );		// add the vlan id to the list
					}
				}

				if( vf->last_updated == DELETED ) {				// delete the macs
					for(m = 0; m < vf->num_macs; ++m) {
						mac = vf->macs[m];
						bleat_printf( 2, "delete mac: %s vf=%d mac=%s", port->pciid, vf->num, mac );
		
						if( parms->forreal )
							set_vf_rx_mac(port->rte_port_number, mac, vf->num, 0);
					}
				} else {
					for(m = 0; m < vf->num_macs; ++m) {
						mac = vf->macs[m];
						bleat_printf( 2, "adding mac: %s vf=%d mac=%s", port->pciid, vf->num, mac );

						if( parms->forreal )
							set_vf_rx_mac(port->rte_port_number, mac, vf->num, 1);
					}
				}

				if( vf->rate > 0 ) {
					bleat_printf( 1, "setting rate: %d", (int)  ( 10000 * vf->rate ) );
					set_vf_rate_limit( port->rte_port_number, vf->num, (uint16_t)( 10000 * vf->rate ), 0x01 );
				}

				if( vf->last_updated == DELETED ) {				// do this last!
					vf->num = -1;								// must reset this so an add request with the now deleted number will succeed
					// TODO -- is there anything else that we need to clean up in the struct?
				}

				if( vf->num >= 0 ) {
					if( parms->forreal ) {
						set_split_erop( i, y, 1 );				// allow drop of packets when there is no matching descriptor

						bleat_printf( 2, "%s vf: %d set anti-spoof %d", port->name, vf->num, vf->vlan_anti_spoof );
						set_vf_vlan_anti_spoofing(port->rte_port_number, vf->num, vf->vlan_anti_spoof);
	
						bleat_printf( 2, "%s vf: %d set mac-anti-spoof %d", port->name, vf->num, vf->mac_anti_spoof );
						set_vf_mac_anti_spoofing(port->rte_port_number, vf->num, vf->mac_anti_spoof);
	
						vfd_set_ins_strip( port, vf );				// set insert/strip options

						bleat_printf( 2, "%s vf: %d set allow broadcast %d", port->name, vf->num, vf->allow_bcast );
						set_vf_allow_bcast(port->rte_port_number, vf->num, vf->allow_bcast);

						bleat_printf( 2, "%s vf: %d set allow multicast %d", port->name, vf->num, vf->allow_mcast );
						set_vf_allow_mcast(port->rte_port_number, vf->num, vf->allow_mcast);

						bleat_printf( 2, "%s vf: %d set allow un-ucast %d", port->name, vf->num, vf->allow_un_ucast );
						set_vf_allow_un_ucast(port->rte_port_number, vf->num, vf->allow_un_ucast);
					} else {
						bleat_printf( 1, "update vf skipping setup for spoofing, bcast, mcast, etc; forreal is off: %s vf=%d", port->pciid, vf->num );
					}
				}

				vf->last_updated = UNCHANGED;				// mark processed
			}

			if( vf->num >= 0 ) {
				if( parms->forreal ) {
					bleat_printf( 3, "set promiscuous: port: %d, vf: %d ", port->rte_port_number, vf->num);
					uint16_t rx_mode = 0;
			
			
					// az says: figure out if we have to update it every time we change VLANS/MACS
					// 			or once when update ports config
					rte_eth_promiscuous_enable(port->rte_port_number);
					rte_eth_allmulticast_enable(port->rte_port_number);
					ret = rte_eth_dev_uc_all_hash_table_set(port->rte_port_number, on);
			
			
					// don't accept untagged frames
					rx_mode |= ETH_VMDQ_ACCEPT_UNTAG;
					ret = rte_eth_dev_set_vf_rxmode(port->rte_port_number, vf->num, rx_mode, !on);
			
					if (ret < 0)
						bleat_printf( 3, "set_vf_allow_untagged(): bad VF receive mode parameter, return code = %d", ret);
				} else {
					bleat_printf( 1, "skipped end round updates to port: %s", port->pciid );
				}
			}
		}				// end for each vf on this port
    }     // end for each port

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
	terminated = 1;

	switch( sig ) {
		case SIGABRT:
		case SIGFPE:
		case SIGSEGV:
				bleat_printf( 0, "signal caught (aborting): %d", sig );
				close_ports();				// must attempt to do this else we potentially crash the machine
				abort( );
				break;

		default:
				bleat_printf( 0, "signal caught (terminating): %d", sig );
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
					SIGTRAP, SIGURG, SIGVTALRM, SIGXCPU, SIGXFSZ, SIGIO, SIGWINCH };

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
*/
void
restore_vf_setings(uint8_t port_id, int vf_id) {
	int i;
	int matched = 0;		// number matched for log

	if( bleat_will_it( 2 ) ) {
		dump_sriov_config(running_config);
	}

	bleat_printf( 3, "restore settings begins" );
	for (i = 0; i < running_config.num_ports; ++i){
		struct sriov_port_s *port = &running_config.ports[i];

		if (port_id == port->rte_port_number){

			int y;
			for(y = 0; y < port->num_vfs; ++y){
				struct vf_s *vf = &port->vfs[y];

				if(vf_id == vf->num){
					//uint32_t vf_mask = VFN2MASK(vf->num);

					matched++;															// for bleat message at end
					vf->last_updated = RESET;											// flag for update_nic()
					if( vfd_update_nic( g_parms, &running_config ) != 0 ) {				// now that dpdk is initialised run the list and 'activate' everything
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
dump_sriov_config(struct sriov_conf_c sriov_config)
{
	int i;
	int y;
	int split_ctl;			// split receive control reg setting


	bleat_printf( 0, "dump: config has %d port(s)", sriov_config.num_ports );

	for (i = 0; i < sriov_config.num_ports; i++){
		bleat_printf( 0, "dump: port: %d, pciid: %s, pciid %s, updated %d, mtu: %d, num_mirrors: %d, num_vfs: %d",
          i, sriov_config.ports[i].name,
          sriov_config.ports[i].pciid,
          sriov_config.ports[i].last_updated,
          sriov_config.ports[i].mtu,
          sriov_config.ports[i].num_mirros,
          sriov_config.ports[i].num_vfs );

		for (y = 0; y < sriov_config.ports[i].num_vfs; y++){
			if( sriov_config.ports[i].vfs[y].num >= 0 ) {
				split_ctl = get_split_ctlreg( i, sriov_config.ports[i].vfs[y].num );
				bleat_printf( 1, "dump: vf: %d, updated: %d  strip: %d  insert: %d  vlan_aspoof: %d  mac_aspoof: %d  allow_bcast: %d  allow_ucast: %d  allow_mcast: %d  allow_untagged: %d  rate: %f  link: %d  num_vlans: %d  num_macs: %d  splitctl=0x%08x",
					sriov_config.ports[i].vfs[y].num, sriov_config.ports[i].vfs[y].last_updated,
					sriov_config.ports[i].vfs[y].strip_stag,
					sriov_config.ports[i].vfs[y].insert_stag,
					sriov_config.ports[i].vfs[y].vlan_anti_spoof,
					sriov_config.ports[i].vfs[y].mac_anti_spoof,
					sriov_config.ports[i].vfs[y].allow_bcast,
					sriov_config.ports[i].vfs[y].allow_un_ucast,
					sriov_config.ports[i].vfs[y].allow_mcast,
					sriov_config.ports[i].vfs[y].allow_untagged,
					sriov_config.ports[i].vfs[y].rate,
					sriov_config.ports[i].vfs[y].link,
					sriov_config.ports[i].vfs[y].num_vlans,
					sriov_config.ports[i].vfs[y].num_macs,
					split_ctl );
	
				int x;
				for (x = 0; x < sriov_config.ports[i].vfs[y].num_vlans; x++) {
					bleat_printf( 2, "dump: vlan[%d] %d ", x, sriov_config.ports[i].vfs[y].vlans[x]);
				}
	
				int z;
				for (z = 0; z < sriov_config.ports[i].vfs[y].num_macs; z++) {
					bleat_printf( 2, "dump: mac[%d] %s ", z, sriov_config.ports[i].vfs[y].macs[z]);
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


  const char * main_help =
		"\n"
		"Usage: vfd [-f] [-n] [-p parm-file] [-v level] [-q]\n"
		"Usage: vfd -?\n"
		"  Options:\n"
		"\t -f        keep in 'foreground'\n"
		"\t -n        no-nic actions executed\n"
		"\t -p <file> parmm file (/etc/vfd/vfd.cfg)\n"
		"\t -q        enable dcb qos (tmp until parm file enabled)\n"
		"\t -h|?  Display this help screen\n"
		"\n";

  		//"\t -s <num>  syslog facility 0-11 (log_kern - log_ftp) 16-23 (local0-local7) see /usr/include/sys/syslog.h\n"

	struct rte_mempool *mbuf_pool = NULL;
	prog_name = strdup(argv[0]);
 	useSyslog = 1;

	parm_file = strdup( "/etc/vfd/vfd.cfg" );				// set default before command line parsing as -p overrides
	log_file = (char *) malloc( sizeof( char ) * BUF_1K );

  // Parse command line options
  while ( (opt = getopt(argc, argv, "?fhnqv:p:s:")) != -1)
  {
    switch (opt)
    {
		case 'f':
			run_asynch = 0;
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

		case 'h':
		case '?':
			printf( "\nvfd %s\n", version );
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
	free( parm_file );

	g_parms->forreal = forreal;												// fill in command line captured things that are passed in parms

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
	bleat_printf( 0, "VFD %s initialising", version );
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
	vfd_add_ports( g_parms, &running_config );			// add the pciid info from parms to the ports list (must do before dpdk init, config file adds wait til after)

	if( g_parms->forreal ) {										// begin dpdk setup and device discovery
		int port;
		int ret;					// returned value from some call
		u_int16_t portid;
		uint32_t pci_control_r;  

		bleat_printf( 1, "starting rte initialisation" );
		rte_set_log_type(RTE_LOGTYPE_PMD && RTE_LOGTYPE_PORT, 0);
		
		bleat_printf( 2, "log level = %d, log type = %d", rte_get_log_level(), rte_log_cur_msg_logtype());
		rte_set_log_level( g_parms->dpdk_init_log_level );

		n_ports = rte_eth_dev_count();
		bleat_printf( 1, "hardware reports %d ports", n_ports );


		if(n_ports < running_config.num_ports) {
			bleat_printf( 1, "WRN: port count mismatch: config lists %d device has %d", running_config.num_ports, n_ports );
		} else {
	  		if (n_ports > running_config.num_ports ) {
				bleat_printf( 1, "CRI: abort: config file reports more devices than dpdk reports: cfg=%d ndev=%d", running_config.num_ports, n_ports );
			}
		}

		static pthread_t tid;
		rq_list = NULL;						// nothing on the reset list
		
		ret = pthread_create(&tid, NULL, (void *)process_refresh_queue, NULL);	
		if (ret != 0) {
			bleat_printf( 0, "CRI: abort: cannot crate refresh_queue thread" );
			rte_exit(EXIT_FAILURE, "Cannot create refresh_queue thread\n");
		}
		bleat_printf( 1, "refresh queue management thread created" );
	
		bleat_printf( 1, "creating memory pool" );
		// Creates a new mempool in memory to hold the mbufs.
		mbuf_pool = rte_pktmbuf_pool_create("sriovctl", NUM_MBUFS * n_ports,
						  MBUF_CACHE_SIZE,
						  0,
						  RTE_MBUF_DEFAULT_BUF_SIZE,
						  rte_socket_id());

		if (mbuf_pool == NULL) {
			bleat_printf( 0, "CRI: abort: mbfuf pool creation failed" );
			rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
		}

		bleat_printf( 1, "initialising all (%d) ports", n_ports );
		for (portid = 0; portid < n_ports; portid++) { 									/* Initialize all ports. */
			if (port_init(portid, mbuf_pool) != 0) {
				bleat_printf( 0, "CRI: abort: port initialisation failed: %d", (int) portid );
				rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);
			} else {
				bleat_printf( 2, "port initialisation successful for port %d", portid );
			}
		}
		bleat_printf( 2, "port initialisation complete" );
	
	
		bleat_printf( 1, "looping over %d ports to map indexes", n_ports );
		for(port = 0; port < n_ports; ++port){					// for each port reported by driver
			int i;
			char pciid[25];
			struct rte_eth_dev_info dev_info;

			rte_eth_dev_info_get(port, &dev_info);
		
			rte_eth_macaddr_get(port, &addr);
			bleat_printf( 1,  "mapping port: %u, MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ", ",
					(unsigned)port,
					addr.addr_bytes[0], addr.addr_bytes[1],
					addr.addr_bytes[2], addr.addr_bytes[3],
					addr.addr_bytes[4], addr.addr_bytes[5]);

			bleat_printf( 1, "driver: %s, index %d, pkts rx: %lu", dev_info.driver_name, dev_info.if_index, st.pcount);
			bleat_printf( 1, "pci: %04X:%02X:%02X.%01X, max VF's: %d", dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus,
				dev_info.pci_dev->addr.devid , dev_info.pci_dev->addr.function, dev_info.max_vfs );
				
			/*
			* rte could enumerate ports differently than in config files
			* rte_config_portmap array will hold index to config
			*/
			snprintf(pciid, sizeof( pciid ), "%04X:%02X:%02X.%01X",
				dev_info.pci_dev->addr.domain,
				dev_info.pci_dev->addr.bus,
				dev_info.pci_dev->addr.devid,
				dev_info.pci_dev->addr.function);
		
			for(i = 0; i < running_config.num_ports; ++i) {							// suss out the device in our config and map the two indexes
				if (strcmp(pciid, running_config.ports[i].pciid) == 0) {
					bleat_printf( 2, "physical port %i maps to config %d", port, i );
					rte_config_portmap[port] = i;
					running_config.ports[i].nvfs_config = dev_info.max_vfs;			// number of configured VFs (could be less than max)
					running_config.ports[i].rte_port_number = port; 				// point config port back to rte port
				}
			}
	  	}

		// read PCI config to get VM offset and stride 
		struct rte_eth_dev *pf_dev = &rte_eth_devices[0];
		rte_eal_pci_read_config(pf_dev->pci_dev, &pci_control_r, 32, 0x174);
		vf_offfset = pci_control_r & 0x0ffff;
		vf_stride = pci_control_r >> 16;
		bleat_printf( 2, "indexes were mapped" );
	
		set_signals();												// register signal handlers 

		gettimeofday(&st.startTime, NULL);

		bleat_printf( 1, "dpdk setup complete" );
	} else {
		bleat_printf( 1, "no action mode: skipped dpdk setup, signal initialisation, and device discovery" );
	}

	if( g_parms->forreal ) {
		g_parms->initialised = 1;										// safe to update nic now, but only if in forreal mode
	}

	vfd_add_all_vfs( g_parms, &running_config );						// read all existing config files and add the VFs to the config
	if( vfd_update_nic( g_parms, &running_config ) != 0 ) {				// now that dpdk is initialised run the list and 'activate' everything
		bleat_printf( 0, "CRI: abort: unable to initialise nic with base config:" );
		if( forreal ) {
			rte_exit( EXIT_FAILURE, "initialisation failure, see log(s) in: %s\n", g_parms->log_dir );
		} else {
			exit( 1 );
		}
	}

	
	run_start_cbs( &running_config );				// run any user startup callback commands defined in VF configs

	bleat_printf( 1, "initialisation complete, setting bleat level to %d; starting to looop", g_parms->log_level );
	bleat_set_lvl( g_parms->log_level );					// initialisation finished, set log level to running level
	if( forreal ) {
		rte_set_log_level( g_parms->dpdk_log_level );
	}

	while(!terminated)
	{
		usleep(50000);			// .5s

		while( vfd_req_if( g_parms, &running_config, 0 ) ); 				// process _all_ pending requests before going on

	}		// end !terminated while

	bleat_printf( 0, "terminating" );
	run_stop_cbs( &running_config );				// run any user stop callback commands that were given in VF conf files

	if( fd >= 0 ) {
		close(fd);
	}

	close_ports();				// clean up the PFs

  gettimeofday(&st.endTime, NULL);
  bleat_printf( 1, "duration %.f sec\n", timeDelta(&st.endTime, &st.startTime));

  return EXIT_SUCCESS;
}
