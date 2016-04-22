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
*/


#include <strings.h>
#include "sriov.h"
#include <vfdlib.h>

#define DEBUG

// -------------------------------------------------------------------------------------------------------------

// TODO - these need to move to header file
#define ADDED	1				// updated states
#define DELETED (-1)
#define UNCHANGED 0

#define RT_NOP	0				// request types
#define RT_ADD	1
#define RT_DEL	2
#define RT_SHOW 3
#define RT_PING 4
#define RT_VERBOSE 5
#define RT_DUMP 6

// --- local structs --------------------------------------------------------------------------------------------

typedef struct request {
	int		rtype;				// type: RT_ const
	char*	resource;			// parm file name, show target, etc.
	char*	resp_fifo;			// name of the return pipe
	int		log_level;			// for verbose
} req_t;

// --- local protos when needed ---------------------------------------------------------------------------------

static int vfd_update_nic( parms_t* parms, struct sriov_conf_c* conf );
static char* gen_stats( struct sriov_conf_c* conf );

// ---------------------globals: bad form, but unavoidable -------------------------------------------------------
static const char* version = "v1.0/64216";
static parms_t *g_parms = NULL;						// most functions should accept a pointer, however we have to have a global for the callback function support

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
	Test function to vet vfd_init_eal()
*/
static int dummy_rte_eal_init( int argc, char** argv ) {
	int i;

	bleat_printf( 2,  "eal_init parm list: %d parms", argc );
	for( i = 0; i < argc; i++ ) {
		bleat_printf( 2, "[%d] = (%s)", i, argv[i] );
	}

	if( argv[argc] != NULL ) {
		bleat_printf( 2, "ERROR:  the last element of argc wasn't nil" );
	}

	return 0;
}

/*
	Initialise the EAL.  We must dummy up what looks like a command line and pass it to the dpdk funciton.
	This builds the base command, and then adds a -w option for each pciid/vf combination that we know
	about.

	We strdup all of the arument strings that are eventually passed to dpdk as the man page indicates that
	they might be altered, and that we should not fiddle with them after calling the init function. We give
	them their own copy, and suffer a small leak.
	
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
		
	argv[5] = strdup( "–m" );
	argv[6] = strdup( "50" );
	
	argv[7] = strdup( "--file-prefix" );
	argv[8] = strdup( "vfd" );
	
	argv[9] = strdup( "--log-level" );
	snprintf( wbuf, sizeof( wbuf ), "%d", parms->dpdk_init_log_level );
	argv[10] = strdup( wbuf );
	
	argv[11] = strdup( "--no-huge" );

	for( i = 0; i < parms->npciids && argc_idx < argc - 1; i++ ) {			// add in the -w pciid values to the list
		argv[argc_idx++] = strdup( "-w" );
		argv[argc_idx++] = strdup( parms->pciids[i].id );
		bleat_printf( 1, "add pciid to dpdk dummy command line -w %s", parms->pciids[i].id );
	}

	dummy_rte_eal_init( argc, argv );			// print out parms
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
	int vidx;							// index into the vf array
	int	hole = -1;						// first hole in the list;
	struct sriov_port_s* port = NULL;	// reference to a single port in the config
	struct vf_s*	vf;		// point at the vf we need to fill in
	char mbuf[1024];					// message buffer if we fail
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

	if( vfc->pciid == NULL || vfc->vfid < 0 ) {
		snprintf( mbuf, sizeof( mbuf ), "unable to read config file: %s", fname );
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

	if( vidx >= MAX_VFS || vfc->vfid < 0 || vfc->vfid > 31) {							// something is out of range
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

	if( vfc->nmacs + tot_vlans > MAX_PF_MACS ) { 			// would bust the total across the whole PF
		snprintf( mbuf, sizeof( mbuf ), "number of macs supplied (%d) cauess total for PF to exceed the maximum (%d)", vfc->nmacs, MAX_PF_MACS );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}


	if( vfc->nmacs > MAX_VF_MACS ) {
		snprintf( mbuf, sizeof( mbuf ), "number of vlans supplied (%d) exceeds the maximum (%d)", vfc->nvlans, MAX_VF_MACS );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

	for( i = 0; i < vfc->nmacs; i++ ) {					// do we need to vet the address is plausable x:x:x form?
		if( strlen( vfc->macs[i] ) > 17 ) {
			snprintf( mbuf, sizeof( mbuf ), "invalid mac address: %s", vfc->macs[i] );
			bleat_printf( 1, "vf not added: %s", mbuf );
			if( reason ) {
				*reason = strdup( mbuf );
			}
			free_config( vfc );
			return 0;
		}
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

	// CAUTION: if we fail because of a parm error it MUST happen before here!
	if( vidx == port->num_vfs ) {		// inserting at end, bump the num we have used
		port->num_vfs++;
	}
	
	vf = &port->vfs[vidx];						// copy from config data doing any translation needed
	memset( vf, 0, sizeof( *vf ) );				// assume zeroing everything is good
	vf->num = vfc->vfid;
	port->vfs[vidx].last_updated = ADDED;		// signal main code to configure the buggger
	vf->strip_stag = vfc->strip_stag;
	vf->allow_bcast = vfc->allow_bcast;
	vf->allow_mcast = vfc->allow_mcast;
	vf->allow_un_ucast = vfc->allow_un_ucast;

	vf->allow_untagged = 0;					// for now these cannot be set by the config file data
	vf->vlan_anti_spoof = 1;
	vf->mac_anti_spoof = 1;
	vf->rate = 0.0;							// best effort :)
	vf->rate = vfc->rate;
	
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
static int vfd_del_vf( struct sriov_conf_c* conf, char* fname, char** reason ) {
	vf_config_t* vfc;					// raw vf config file contents	
	int	i;
	int vidx;							// index into the vf array
	struct sriov_port_s* port = NULL;	// reference to a single port in the config
	char mbuf[1024];					// message buffer if we fail
	
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

	snprintf( mbuf, sizeof( mbuf ), "%s-", fname );						// for now we move it aside; may want to delete it later
	if( rename( fname, mbuf ) < 0 ) {
		snprintf( mbuf, sizeof( mbuf ), "unable to delete config file: %s: %s", fname, strerror( errno ) );
		bleat_printf( 1, "vfd_del_vf failed: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

	bleat_printf( 2, "del: config data: name: %s", vfc->name );
	bleat_printf( 2, "del: config data: pciid: %s", vfc->pciid );
	bleat_printf( 2, "del: config data: vfid: %d", vfc->vfid );

	if( vfc->pciid == NULL || vfc->vfid < 0 ) {
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
	Construct json to write onto the response pipe.  The response pipe is opened in non-block mode
	so that it will fail immiediately if there isn't a reader or the pipe doesn't exist. We assume
	that the requestor opens the pipe before sending the request so that if it is delayed after
	sending the request it does not prevent us from writing to the pipe.  If we don't open in 	
	blocked mode we could hang foever if the requestor dies/aborts.
*/
static void vfd_response( char* rpipe, int state, const char* msg ) {
	int 	fd;
	char	buf[1024];
	unsigned int		len = 0;

	if( rpipe == NULL ) {
		return;
	}

	if( (fd = open( rpipe, O_WRONLY | O_NONBLOCK, 0 )) < 0 ) {
	 	bleat_printf( 0, "unable to deliver response: open failed: %s: %s", rpipe, strerror( errno ) );
		return;
	}
	bleat_printf( 1, "sending response: %s [%d] %s", rpipe, state, msg );

	snprintf( buf, sizeof( buf ), "{ \"state\": \"%s\", \"msg\": \"%s\" }\n", state ? "ERROR" : "OK", msg == NULL ? "" : msg );
	bleat_printf( 2, "response fd: %d", fd );
	bleat_printf( 2, "response json: %s", buf );
	if( (len = write( fd, buf, strlen( buf ) )) != strlen( buf ) ) {
		bleat_printf( 0, "enum=%s", strerror( errno ) );
		bleat_printf( 0, "WRN: write of response to pipe failed: %s: state=%d msg=%s", rpipe, state, msg ? msg : "" );
	}

	bleat_printf( 2, "response written to pipe" );
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
		bleat_printf( 0, "ERR: failed to create a json parsing object for: %s\n", rbuf );
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

	bleat_printf( 1, "raw message: (%s)", rbuf ); 			// TODO -- change to level 2

	switch( *stuff ) {				// we assume compiler builds a jump table which makes it faster than a bunch of nested sring compares
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
			bleat_printf( 1, "got request" );					// TODO -- increase level after testing
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

					bleat_printf( 2, "deleting vf from file: %s", mbuf );
					if( vfd_del_vf( conf, req->resource, &reason ) ) {		// successfully updated internal struct
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

				case RT_DUMP:					// spew everything to the log
  					dump_sriov_config( *conf );
					vfd_response( req->resp_fifo, 0, "dump captured in the log" );
					break;

				case RT_SHOW:			//TODO -- need to check for a specific thing to show; right now just dumps all
					if( parms->forreal ) {
						if( (buf = gen_stats( conf )) != NULL )  {		// todo need to replace 1 with actual number of ports
							vfd_response( req->resp_fifo, 0, buf );
							free( buf );
						} else {
							vfd_response( req->resp_fifo, 1, "unable to generate stats" );
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

	return req_handled;			// true if we did something -- more frequent recall if we did?
}

// ----------------- actual nic management ------------------------------------------------------------------------------------

/*
	Generate a set of stats to a single buffer. Return buffer to caller (caller must free).
*/
static char*  gen_stats( struct sriov_conf_c* conf ) {
	char*	rbuf;			// buffer to return
	int		rblen = 0;		// lenght
	int		rbidx = 0;
	char	buf[8192];
	int		l;
	int		i;
	struct rte_eth_dev_info dev_info;

	rblen = 8192;
	rbuf = (char *) malloc( sizeof( char ) * rblen );
	if( !rbuf ) {
		return NULL;
	}

	rbidx = snprintf( rbuf, 8192, "%s %18s %10s %10s %10s %10s %10s %10s %10s %10s %10s\n",
			"Iface", "Link", "Speed", "Duplex", "RX pkts", "RX bytes", "RX errors", "RX dropped", "TX pkts", "TX bytes", "TX errors");
	
	for( i = 0; i < conf->num_ports; ++i ) {
		rte_eth_dev_info_get( conf->ports[i].rte_port_number, &dev_info );				// must use port number that we mapped during initialisation

		l = snprintf( buf, sizeof( buf ), "%04X:%02X:%02X.%01X",
					dev_info.pci_dev->addr.domain,
					dev_info.pci_dev->addr.bus,
					dev_info.pci_dev->addr.devid,
					dev_info.pci_dev->addr.function);
							
		if( l + rbidx > rblen ) {
			rblen += 8192;
			rbuf = (char *) realloc( rbuf, sizeof( char ) * rblen );
			if( !rbuf ) {
				return NULL;
			}
		}

		strcat( rbuf+rbidx,  buf );
		rbidx += l;
     				
		l = nic_stats_display( conf->ports[i].rte_port_number, buf, sizeof( buf ) );
		if( l + rbidx > rblen ) {
			rblen += 8192 + l;
			rbuf = (char *) realloc( rbuf, sizeof( char ) * rblen );
			if( !rbuf ) {
				return NULL;
			}
		}
		strcat( rbuf+rbidx,  buf );
		rbidx += l;
	}

	return rbuf;
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
		struct sriov_port_s *port = &conf->ports[i];

		if( port->last_updated == ADDED ) {								// updated since last call, reconfigure
			if( parms->forreal ) {
				bleat_printf( 1, "port updated: %s/%s",  port->name, port->pciid );
				rte_eth_promiscuous_enable(port->rte_port_number);
				rte_eth_allmulticast_enable(port->rte_port_number);
	
				ret = rte_eth_dev_uc_all_hash_table_set(port->rte_port_number, on);
				if (ret < 0)
					traceLog(TRACE_ERROR, "bad unicast hash table parameter, return code = %d \n", ret);
	
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

			if( vf->last_updated != UNCHANGED ) {					// this vf was changed (add/del), reconfigure it
				bleat_printf( 1, "reconfigure vf for %s: %s vf=%d", vf->last_updated == ADDED ? "add" : "delete", port->pciid, vf->num );

				// TODO: order from original kept; probably can group into to blocks based on updated flag
				if( vf->last_updated == DELETED ) { 							// delete vlans
					for(v = 0; v < vf->num_vlans; ++v) {
						int vlan = vf->vlans[v];
						bleat_printf( 2, "delete vlan: %s vf=%d vlan=%d", port->pciid, vf->num, vlan );
						if( parms->forreal )
							set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, 0);
					}
				} else {
					//traceLog(TRACE_DEBUG, "ADDING VLANS, VF: %d ", vf->num);
					int v;
					for(v = 0; v < vf->num_vlans; ++v) {
						int vlan = vf->vlans[v];
						bleat_printf( 2, "add vlan: %s vf=%d vlan=%d", port->pciid, vf->num, vlan );
						if( parms->forreal )
							set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, on);
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

				// set VLAN anti spoofing when VLAN filter is used
						
				if( vf->num >= 0 ) {
					if( parms->forreal ) {
						bleat_printf( 2, "%s vf: %d set anti-spoof %d", port->name, vf->num, vf->vlan_anti_spoof );
						set_vf_vlan_anti_spoofing(port->rte_port_number, vf->num, vf->vlan_anti_spoof);
	
						bleat_printf( 2, "%s vf: %d set mac-anti-spoof %d", port->name, vf->num, vf->mac_anti_spoof );
						set_vf_mac_anti_spoofing(port->rte_port_number, vf->num, vf->mac_anti_spoof);
	
						bleat_printf( 2, "%s vf: %d set strip vlan tag %d", port->name, vf->num, vf->strip_stag );
						rx_vlan_strip_set_on_vf(port->rte_port_number, vf->num, vf->strip_stag);
	
						// CAUTION: per meeting on 3/2/2016 the insert stag config option was removed and this setting mirrors the strip setting.
						//			strip on receipt seems not a flag, so we must either hard set 0 (strip) or just what was in the list if
						// 			the list has len==1. If list is bigger, we don't do anything.
						/*
						bleat_printf( 2, "%s vf: %d set insert vlan tag %d", port->name, vf->num, vf->strip_stag );
						if( vf->strip_stag ) {
							rx_vlan_insert_set_on_vf(port->rte_port_number, vf->num, 0 );			// insert no tag (vlan id == 0)
						} else {
							if( vf->num_vlans == 1 ) {
								rx_vlan_insert_set_on_vf(port->rte_port_number, vf->num, vf->vlans[0] );	// insert what should already be there (no strip)
							}
						}
						*/

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

			if( parms->forreal ) {
				traceLog(TRACE_DEBUG, "SET PROMISCUOUS: %d, VF: %d ", port->rte_port_number, vf->num);
				uint16_t rx_mode = 0;
		
		
				// az says: figure this out if we have to update it every time we change VLANS/MACS
				// 			or once when update ports config
				rte_eth_promiscuous_enable(port->rte_port_number);
				rte_eth_allmulticast_enable(port->rte_port_number);
				ret = rte_eth_dev_uc_all_hash_table_set(port->rte_port_number, on);
		
		
				// don't accept untagged frames
				rx_mode |= ETH_VMDQ_ACCEPT_UNTAG;
				ret = rte_eth_dev_set_vf_rxmode(port->rte_port_number, vf->num, rx_mode, !on);
		
				if (ret < 0)
					traceLog(TRACE_DEBUG, "set_vf_allow_untagged(): bad VF receive mode parameter, return code = %d \n", ret);
			} else {
				bleat_printf( 1, "skipped end round updates to port: %s", port->pciid );
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


static void
sig_int(int sig)
{
  terminated = 1;
  restart = 0;

  int portid;
  if (sig == SIGINT)  {
		for (portid = 0; portid < n_ports; portid++) {
			rte_eth_dev_close(portid);
		}
	}

  static int called = 0;

  if(sig == 1) called = sig;

  if(called)
    return;
  else called = 1;

  traceLog(TRACE_NORMAL, "Received Interrupt signal\n");
}



static void
sig_usr(int sig)
{
  terminated = 1;
  restart = 1;

  static int called = 0;

  if(sig == 1) called = sig;

  if(called)
    return;
  else called = 1;

  traceLog(TRACE_NORMAL, "Restarting vfd");
}


static void
sig_hup(int __attribute__((__unused__)) sig)
{
  restart = 1;

	/*
  int res = readConfigFile(fname);
  if (res != 0) {
    traceLog(TRACE_ERROR, "Can not read config file: %s\n", fname);
  }

  res = update_ports_config();
  if (res != 0) {
    traceLog(TRACE_ERROR, "Error updating ports configuration: %s\n", res);
  }
	*/

  traceLog(TRACE_NORMAL, "Ignored HUP signal\n");
}

/*	
	Setup all of the signal handling.
*/
static void set_signals( void ) {
  struct sigaction sa;

	memset( &sa, 0, sizeof( sa ) );
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


void
restore_vf_setings_cb(void *param){
	struct reset_param_c *p_reset = (struct reset_param_c *) param;

	bleat_printf( 1, "restore settings callback driven: p=%d vf=%d",  p_reset->port, p_reset->vf );
	//traceLog(TRACE_DEBUG, "Restoring Settings, Port: %d, VF: %d", p_reset->port, p_reset->vf);
	restore_vf_setings(p_reset->port, p_reset->vf);

	free(param);
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
	dump_sriov_config(running_config);
	int i;
	int on = 1;
	int matched = 0;		// number matched for log

	for (i = 0; i < running_config.num_ports; ++i){
		struct sriov_port_s *port = &running_config.ports[i];

		if (port_id == port->rte_port_number){

		int y;
		for(y = 0; y < port->num_vfs; ++y){
			struct vf_s *vf = &port->vfs[y];

			if(vf_id == vf->num){
				uint32_t vf_mask = VFN2MASK(vf->num);

				matched++;									// for bleat message at end

				int v;
				for(v = 0; v < vf->num_vlans; ++v) {			// for refresh, we'll turn them off first
					int vlan = vf->vlans[v];

					set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, 0);
				}
	
				for(v = 0; v < vf->num_vlans; ++v) {				// enable vlan ids set from config
					int vlan = vf->vlans[v];

					bleat_printf( 2, "refresh: %s vf: %d set vlan %d", port->name, vf->num, vlan );
					set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, on);
				}
	
				int m;
				for(m = 0; m < vf->num_macs; ++m) {					// for refresh we'll disable them first
					char *mac = vf->macs[m];

					set_vf_rx_mac(port->rte_port_number, mac, vf->num, 0);
				}

				char *mac;
				for(m = 0; m < vf->num_macs; ++m) {					// enable all mac addresses in the list
					mac = vf->macs[m];

					bleat_printf( 2, "refresh: %s vf: %d enable mac %s", port->name, vf->num, mac );
					set_vf_rx_mac(port->rte_port_number, mac, vf->num, 1);
				}


				// set VLAN anti spoofing when VLAN filter is used

				bleat_printf( 2, "refresh: %s vf: %d set anti-spoof %d", port->name, vf->num, vf->vlan_anti_spoof );
				set_vf_vlan_anti_spoofing(port->rte_port_number, vf->num, vf->vlan_anti_spoof);
			
				bleat_printf( 2, "refresh: %s vf: %d set mac-anti-spoof %d", port->name, vf->num, vf->mac_anti_spoof );
				set_vf_mac_anti_spoofing(port->rte_port_number, vf->num, vf->mac_anti_spoof);

				bleat_printf( 2, "refresh: %s vf: %d set strip vlan tag %d", port->name, vf->num, vf->strip_stag );
				rx_vlan_strip_set_on_vf(port->rte_port_number, vf->num, vf->strip_stag);
		
				bleat_printf( 2, "refresh: %s vf: %d set insert vlan tag %d", port->name, vf->num, vf->strip_stag );
				//CAUTION: per 03/02/2016 meeting insert stag option removed from config; this mirrors the strip setting
				rx_vlan_insert_set_on_vf(port->rte_port_number, vf->num, vf->strip_stag);


				bleat_printf( 2, "refresh: %s vf: %d set allow broadcast %d", port->name, vf->num, vf->allow_bcast );
				set_vf_allow_bcast(port->rte_port_number, vf->num, vf->allow_bcast);

				bleat_printf( 2, "refresh: %s vf: %d set allow multicast %d", port->name, vf->num, vf->allow_mcast );
				set_vf_allow_mcast(port->rte_port_number, vf->num, vf->allow_mcast);

				bleat_printf( 2, "refresh: %s vf: %d set allow un-ucast %d", port->name, vf->num, vf->allow_un_ucast );
				set_vf_allow_un_ucast(port->rte_port_number, vf->num, vf->allow_un_ucast);

				rte_eth_promiscuous_enable(port->rte_port_number);
				rte_eth_allmulticast_enable(port->rte_port_number);
				int ret = rte_eth_dev_uc_all_hash_table_set(port->rte_port_number, on);

				// don't accept untagged frames
				uint16_t rx_mode = 0;
				rx_mode |= ETH_VMDQ_ACCEPT_UNTAG;
				ret = rte_eth_dev_set_vf_rxmode(port->rte_port_number, vf->num, rx_mode, !on);
		
				if (ret < 0)
					traceLog(TRACE_DEBUG, "set_vf_allow_untagged(): bad VF receive mode parameter, return code = %d \n", ret);    					

					// TODO -- should be safe to return here -- shouldn't be but one to match
				}
			}
		}
	}

	bleat_printf( 1, "refresh for  port=%d vf=%d matched %d vfs in the config", port_id, vf_id, matched );
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

	bleat_printf( 1, "dump: config has %d port(s)", sriov_config.num_ports );

	for (i = 0; i < sriov_config.num_ports; i++){
		bleat_printf( 2, "dump: Port #: %d, name: %s, pciid %s, last_updated %d, mtu: %d, num_mirrors: %d, num_vfs: %d",
          i, sriov_config.ports[i].name,
          sriov_config.ports[i].pciid,
          sriov_config.ports[i].last_updated,
          sriov_config.ports[i].mtu,
          sriov_config.ports[i].num_mirros,
          sriov_config.ports[i].num_vfs );

		int y;
		for (y = 0; y < sriov_config.ports[i].num_vfs; y++){
			bleat_printf( 2, "dump: VF num: %d, updated: %d  strip_stag %d  insert_stag %d  vlan_aspoof: %d  mac_aspoof: %d  allow_bcast: %d  allow_ucast: %d  allow_mcast: %d  allow_untagged: %d  rate: %f  link: %d  um_vlans: %d  num_macs: %d  ",
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
				sriov_config.ports[i].vfs[y].num_macs);

			int x;
			for (x = 0; x < sriov_config.ports[i].vfs[y].num_vlans; x++) {
				bleat_printf( 2, "dump: vlan[%d] %d ", x, sriov_config.ports[i].vfs[y].vlans[x]);
			}

			int z;
			for (z = 0; z < sriov_config.ports[i].vfs[y].num_macs; z++) {
				bleat_printf( 2, "dump: mac[%d] %s ", z, sriov_config.ports[i].vfs[y].macs[z]);
			}
		}
	}
}

// ===============================================================================================================
int
main(int argc, char **argv)
{
	__attribute__((__unused__))	int ignored;				// ignored return code to keep compiler from whining
	char*	parm_file = NULL;							// default in /etc, -p overrieds
	char	log_file[1024];				// buffer to build full log file in
	char	run_asynch = 1;				// -f sets off to keep attached to tty
	int		forreal = 1;				// -n sets to 0 to keep us from actually fiddling the nic
	char	buff[1024];					// scratch write buffer
	int		have_pipe = 0;				// false if we didn't open the stats pipe
	int		opt;
	int		fd = -1;
	int		l;							// length of something

  const char * main_help =
	"\n"
	"Usage: vfd [-f] [-n] [-p parm-file] [-v level] [-s syslogid]\n"
	"Usage: vfd -?\n"
	"  Options:\n"
  "\t -f        keep in 'foreground'\n"
  "\t -n        no-nic actions executed\n"
  "\t -p <file> parmm file (/etc/vfd/vfd.cfg)\n"
  "\t -v <num>  Verbose (if num > 3 foreground) num - verbose level\n"
  "\t -s <num>  syslog facility 0-11 (log_kern - log_ftp) 16-23 (local0-local7) see /usr/include/sys/syslog.h\n"
	"\t -h|?  Display this help screen\n";



  //int devnum = 0;

 	struct rte_mempool *mbuf_pool = NULL;
	//unsigned n_ports;
	
  prog_name = strdup(argv[0]);
  useSyslog = 1;

	int i;


	parm_file = strdup( "/etc/vfd/vfd.cfg" );				// set default before command line parsing as -p overrides

  // Parse command line options
  while ( (opt = getopt(argc, argv, "fhnv:p:s:")) != -1)			// f,c  dropped
  {
    switch (opt)
    {
		case 'f':
			run_asynch = 0;
			break;
		
		case 'v':
		  traceLevel = atoi(optarg);

		  if(traceLevel > 6) {
		   useSyslog = 0;
		   debug = 1;
		  }
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
			printf( "vfd %s\n", version );
			printf("%s\n", main_help);
			exit( 0 );
			break;

		default:
			fprintf( stderr, "unknown commandline flag: %c\n", opt );
			fprintf( stderr, "%s\n", main_help );
			exit( 1 );
    }
  }


	if( (g_parms = read_parms( parm_file )) == NULL ) {						// get overall configuration (includes list of pciids we manage)
		fprintf( stderr, "unable to read configuration from %s: %s\n", parm_file, strerror( errno ) );
		exit( 1 );
	}

	g_parms->forreal = forreal;												// fill in command line captured things that are passed in parms

	snprintf( log_file, sizeof( log_file ), "%s/vfd.log", g_parms->log_dir );
	if( run_asynch ) {
		bleat_printf( 1, "setting log to: %s", log_file );
		bleat_printf( 3, "detaching from tty (daemonise)" );
		daemonize( g_parms->pid_fname );
		bleat_set_log( log_file, 86400 );									// open bleat log with date suffix _after_ daemonize so it doesn't close our fd
	} else {
		bleat_printf( 2, "-f supplied, staying attached to tty" );
	}
	bleat_set_lvl( g_parms->init_log_level );											// set default level
	bleat_printf( 0, "VFD initialising" );
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
		bleat_printf( 1, "starting rte initialisation" );
		rte_set_log_type(RTE_LOGTYPE_PMD && RTE_LOGTYPE_PORT, 0);
		
		traceLog(TRACE_INFO, "LOG LEVEL = %d, LOG TYPE = %d\n", rte_get_log_level(), rte_log_cur_msg_logtype());

		
		rte_set_log_level( g_parms->dpdk_init_log_level );
		

		n_ports = rte_eth_dev_count();
		bleat_printf( 1, "hardware reports %d ports", n_ports );


	  if(n_ports != running_config.num_ports) {
		bleat_printf( 1, "WRN: port count mismatch: config lists %d device has %d", running_config.num_ports, n_ports );
		traceLog(TRACE_ERROR, "ports found (%d) != ports requested (%d)\n", n_ports, running_config.num_ports);
	  }

	  traceLog(TRACE_NORMAL, "n_ports = %d\n", n_ports);

	 /*
	  === (commented out in original -- left)
	  const struct rte_memzone *mz;

		bleat_printf( 1, "setting memory zones" );
	  mz = rte_memzone_reserve(IF_PORT_INFO, sizeof(struct ifrate_s), rte_socket_id(), 0);
		if (mz == NULL)
			rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for port information\n");
	  memset(mz->addr, 0, sizeof(struct ifrate_s));
	

	  ifrate_stats = mz->addr;
	
	  bleat_printf( 1, "rate stats addr: %p", (void *)ifrate_stats);		// converted from plain printf
	  ==== */

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

		bleat_printf( 1, "initialising all ports" );
		/* Initialize all ports. */
	  u_int16_t portid;
		for (portid = 0; portid < n_ports; portid++) {
			if (port_init(portid, mbuf_pool) != 0) {
				bleat_printf( 0, "CRI: abort: port initialisation failed: %d", (int) portid );
				rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);
			} else {
				bleat_printf( 2, "port initialisation successful for port %d", portid );
			}
		}
		bleat_printf( 2, "port initialisation complete" );
	

	
		bleat_printf( 1, "looping over %d ports to map indexes", n_ports );
	  int port;
	  for(port = 0; port < n_ports; ++port){					// for each port reported by driver
		struct rte_eth_dev_info dev_info;
		rte_eth_dev_info_get(port, &dev_info);
		
		//struct ether_addr addr;
		rte_eth_macaddr_get(port, &addr);
		//traceLog(TRACE_INFO, "Port: %u, MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ", ",
		bleat_printf( 1,  "mapping port: %u, MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ", ",
			(unsigned)port,
			addr.addr_bytes[0], addr.addr_bytes[1],
			addr.addr_bytes[2], addr.addr_bytes[3],
			addr.addr_bytes[4], addr.addr_bytes[5]);



		//traceLog(TRACE_INFO, "Driver Name: %s, Index %d, Pkts rx: %lu, ", dev_info.driver_name, dev_info.if_index, st.pcount);
		bleat_printf( 1, "driver: %s, Index %d, Pkts rx: %lu, ", dev_info.driver_name, dev_info.if_index, st.pcount);
		
		//traceLog(TRACE_INFO, "PCI: %04X:%02X:%02X.%01X, Max VF's: %d, Numa: %d\n\n", dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus, dev_info.pci_dev->addr.devid, dev_info.pci_dev->addr.function, dev_info.max_vfs, dev_info.pci_dev->numa_node);
		bleat_printf( 1, "pci: %04X:%02X:%02X.%01X, max VF's: %d, numa: %d", dev_info.pci_dev->addr.domain, dev_info.pci_dev->addr.bus,
				dev_info.pci_dev->addr.devid , dev_info.pci_dev->addr.function, dev_info.max_vfs, dev_info.pci_dev->numa_node);

				
		/*
		 * rte could enumerate ports differently than in config files
		 * rte_config_portmap array will hold index to config
		 */
		int i;
		char pciid[25];
		snprintf(pciid, sizeof( pciid ), "%04X:%02X:%02X.%01X",
				dev_info.pci_dev->addr.domain,
				dev_info.pci_dev->addr.bus,
				dev_info.pci_dev->addr.devid,
				dev_info.pci_dev->addr.function);
		
		for(i = 0; i < running_config.num_ports; ++i) {						// suss out the device in our config and map the two indexes
		  if (strcmp(pciid, running_config.ports[i].pciid) == 0) {
			bleat_printf( 2, "physical port %i maps to config %d", port, i );
			rte_config_portmap[port] = i;
			running_config.ports[i].nvfs_config = dev_info.max_vfs;			// number of configured VFs (could be less than max)
			running_config.ports[i].rte_port_number = port; 				// point config port back to rte port
		  }
		}
	  }


		bleat_printf( 2, "indexes were mapped" );
	
		set_signals();				// register signal handlers (reload, port reset on shutdown, etc)

	  gettimeofday(&st.startTime, NULL);

	  traceLog(TRACE_NORMAL, "starting sriovctl loop\n");
	

	  //update_ports_config();

		if( g_parms->stats_path != NULL  ) {
			if(  mkfifo( g_parms->stats_path, 0666) != 0) {
				bleat_printf( 2, "creattion of stats pipe failed: %s", g_parms->stats_path, strerror( errno ) );
				//traceLog(TRACE_ERROR, "can't create pipe: %s, %d\n", STATS_FILE, errno);
			} else {
				bleat_printf( 2, "created stats pipe: %s", g_parms->stats_path );
				have_pipe = 1;
			}
		} else {
			bleat_printf( 2, "stats pipe not created, not defined in g_parms" );
		}

	  /*
		FILE * dump = fopen("/tmp/pci_dump.txt", "w");
		rte_eal_pci_dump(dump);
		fclose (dump);
		*/

		bleat_printf( 1, "dpdk setup complete" );
	} else {
		bleat_printf( 1, "no action mode: skipped dpdk setup, signal initialisation, and device discovery" );
	}

	g_parms->initialised = 1;								// safe to update nic now
	vfd_add_all_vfs( g_parms, &running_config );			// read all existing config files and add the VFs to the config
	if( vfd_update_nic( g_parms, &running_config ) != 0 ) {							// now that dpdk is initialised run the list and 'activate' everything
		bleat_printf( 0, "CRI: abort: unable to initialise nic with base config:" );
		if( forreal ) {
			rte_exit( EXIT_FAILURE, "initialisation failure, see log(s) in: %s\n", g_parms->log_dir );
		} else {
			exit( 1 );
		}
	}
	
	if( have_pipe ) {
		fd = open( g_parms->stats_path, O_NONBLOCK | O_RDWR);		// must open non-block or it hangs until there is a reader
		if( fd < 0 ) {
			bleat_printf( 1, "could not open stats pipe: %s: %s", g_parms->stats_path, strerror( errno ) );
		}
	}

	bleat_printf( 1, "initialisation complete, setting bleat level to %d; starting to looop", g_parms->log_level );
	bleat_set_lvl( g_parms->log_level );					// initialisation finished, set log level to running level
	if( forreal ) {
		rte_set_log_level( g_parms->dpdk_log_level );
	}
	while(!terminated)
	{
		usleep(50000);			// .5s

		while( vfd_req_if( g_parms, &running_config, 0 ) ); 				// process _all_ pending requests before going on

		if( bleat_will_it( 3 ) ) {
			if( g_parms->forreal  && have_pipe && fd >= 0 ) {				//TODO -- drop this in favour of show stats?
				l = snprintf(buff, sizeof( buff ), "%s %18s %10s %10s %10s %10s %10s %10s %10s %10s %10s\n", "Iface", "Link", "Speed", "Duplex", "RX pkts", "RX bytes",
						"RX errors", "RX dropped", "TX pkts", "TX bytes", "TX errors");
   	
				ignored = write(fd, buff,  l );
   	
				for (i = 0; i < n_ports; ++i)
				{
					struct rte_eth_dev_info dev_info;
					rte_eth_dev_info_get(i, &dev_info);			
	
					l = snprintf(buff, sizeof( buff ), "%04X:%02X:%02X.%01X",
							dev_info.pci_dev->addr.domain,
							dev_info.pci_dev->addr.bus,
							dev_info.pci_dev->addr.devid,
							dev_info.pci_dev->addr.function);
								
					ignored = write(fd, buff, l );
     					
					l = nic_stats_display(i, buff, sizeof( buff ) );
					ignored = write(fd, buff, l );
				}
			}
		}
	}		// end !terminated while

	if( fd >= 0 ) {
		close(fd);
	}

	if( have_pipe  &&  g_parms->stats_path != NULL  && unlink( g_parms->stats_path ) != 0 ) {
		bleat_printf( 1, "couldn't delete stats pipe" );
		//traceLog(TRACE_ERROR, "can't delete pipe: %s\n", STATS_FILE);
	}

  gettimeofday(&st.endTime, NULL);
  traceLog(TRACE_NORMAL, "Duration %.f sec\n", timeDelta(&st.endTime, &st.startTime));

  traceLog(TRACE_NORMAL, "sriovctl exit\n");

  return EXIT_SUCCESS;
}
