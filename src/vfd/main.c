// vi: sw=4 ts=4:
/*
	Mnemonic:	vfd -- VF daemon
	Abstract: 	Daemon which manages the configuration and management of VF interfaces
				on one or more NICs.
				Original name was sriov daemon, so some references to that (sriov.h) remain.

	Date:		February 2016
	Authors:	Alex Zelezniak (original code)
				E. Scott Daniels (extensions)
*/


#include <strings.h>
#include "sriov.h"
#include "vfdlib.h"

#define DEBUG

struct rte_port *ports;

 
// -------------------------------------------------------------------------------------------------------------

#define RT_NOP	0				// request types
#define RT_ADD	1
#define RT_DEL	2
#define RT_SHOW 3
#define RT_PING 4
#define RT_VERBOSE 5

typedef struct request {
	int		rtype;				// type: RT_ const 
	char*	resource;			// parm file name, show target, etc.
	char*	resp_fifo;			// name of the return pipe
	int		log_level;			// for verbose
} req_t;

// ---------------------globals: bad form, but unavoidable -------------------------------------------------------
const char* version = "v1.0/63116";

/*
	Test function to vet vfd_init_eal()
*/
static int dummy_rte_eal_init( int argc, char** argv ) {
	int i;

	fprintf( stderr, "dummy: %d parms\n", argc );
	for( i = 0; i < argc; i++ ) {
		fprintf( stderr, "[%d] = (%s)\n", i, argv[i] );
	}

	if( argv[argc] != NULL ) {
		fprintf( stderr, "ERROR:  the last element of argc wasn't nil\n" );
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

	if( parms->npciids <= 0 ) {
		bleat_printf( 0, "abort: no pciids were defined in the configuration file" );
		exit( 1 );
	}

	argc = argc_idx + (parms->npciids * 2);											// 2 slots for each pcciid;  number to alloc is one larger to allow for ending nil
	if( (argv = (char **) malloc( (argc + 1) * sizeof( char* ) )) == NULL ) {		// n static parms + 2 slots for each pciid + null
		bleat_printf( 0, "abort: unable to alloc memory for eal initialisation" );
		exit( 1 );
	}
	memset( argv, 0, sizeof( char* ) * (argc + 1) );

	argv[0] = strdup(  "vfd" );						// dummy up a command line to pass to rte_eal_init() -- it expects that we got these on our command line (what a hack)

	if( *parms->cpu_mask != '#' ) {
		snprintf( wbuf, sizeof( wbuf ), "#%02x", atoi( parms->cpu_mask ) );				// assume integer as a string given; cvt to hex
		free( parms->cpu_mask );
		parms->cpu_mask = strdup( wbuf );
	}
	
	argv[1] = strdup( "-c" );
	argv[2] = strdup( parms->cpu_mask );

	argv[3] = strdup( "-n" );
	argv[4] = strdup( "4" );
		
	argv[5] = strdup( "–m" );
	argv[6] = strdup( "50" );
	
	argv[7] = strdup( "--file-prefix" );
	argv[8] = strdup( "vfd" ); 				//sprintf(argv[8], "%s", "sriovctl" );
	
	argv[9] = strdup( "--log-level" );
	snprintf( wbuf, sizeof( wbuf ), "%d", parms->dpdk_log_level );
	argv[10] = strdup( wbuf );
	
	argv[11] = strdup( "--no-huge" );
  
  
	for( i = 0; i < parms->npciids && argc_idx < argc; i++ ) {			// add in the -w pciid values to the list
		argv[argc_idx++] = strdup( "-w" );
		argv[argc_idx++] = strdup( parms->pciids[i] );
		bleat_printf( 1, "add pciid to dpdk dummy command line -w %s", parms->pciids[i] );
	}

	//return rte_eal_init( argc, argv ); 			// http://dpdk.org/doc/api/rte__eal_8h.html
	return dummy_rte_eal_init( argc, argv );
}

/*
	Create our fifo and tuck the handle into the parm struct. Returns 0 on 
	success and <0 on failure. 
*/
static int vfd_init_fifo( parms_t* parms ) {
	if( !parms ) {
		return -1;
	}

	parms->rfifo = rfifo_create( parms->fifo_path );
	if( parms->rfifo == NULL ) {
		bleat_printf( 0, "error: unable to create request fifo (%s): %s", parms->fifo_path, strerror( errno ) );
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
	static int called = 0;
	int i;
	int pidx = 0;			// port idx in conf list
	struct sriov_port_s* port;

	if( called ) 
		return;
	called = 1;
	
	for( i = 0; pidx < MAX_PORTS  && i < parms->npciids; i++, pidx++ ) {
		port = &conf->ports[pidx];
		snprintf( port->name, sizeof( port->name ), "port-%d",  i);				// TODO--- support getting a name from the config
		snprintf( port->pciid, sizeof( port->pciid ), "%s", parms->pciids[i] );
		port->mtu = 9000;														// TODO -- support getting mtu from config
		port->num_mirros = 0;
		port->num_vfs = 0;
		
		bleat_printf( 1, "add pciid to in memory config: %s", parms->pciids[i] );
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

	bleat_printf( 2, "config data: name: %s", vfc->name );
	bleat_printf( 2, "config data: pciid: %s", vfc->pciid );
	bleat_printf( 2, "config data: vfid: %d", vfc->vfid );

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
		}
	}

	if( hole >= 0 ) {			// set the index into the vf array based on first hole found, or no holes
		vidx = hole;
	} else {
		vidx = i;
	}

	if( vidx >= MAX_VFS || vfc->vfid < 1 || vfc->vfid > 32) {							// something is out of range
		snprintf( mbuf, sizeof( mbuf ), "max VFs already defined or vfid %d is out of range", vfc->vfid );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}

		free_config( vfc );
		return 0;
	}

	if( vfc->nvlans > MAX_VF_VLANS ) {
		snprintf( mbuf, sizeof( mbuf ), "number of vlans supplied (%d) exceeds the maximum (%d)", vfc->nvlans, MAX_VF_VLANS );
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

	if( vfc->strip_stag  &&  vfc->nvlans > 0 ) {
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
	vf->strip_stag = vfc->strip_stag;
	vf->allow_bcast = vfc->allow_bcast;
	vf->allow_mcast = vfc->allow_mcast;
	vf->allow_un_ucast = vfc->allow_un_ucast;

	vf->allow_untagged = 0;					// for now these cannot be set by the config file data
	vf->vlan_anti_spoof = 1;
	vf->mac_anti_spoof = 1;
	vf->rate = 0.0;							// best effort :)
	
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
			bleat_printf( 1, "link_status not recognised in config: %s", vfc->link_status );
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
		bleat_printf( 1, "no vf configuration files (*.json) found in %s", parms->config_dir );
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
		bleat_printf( 0, "warn: write of response to pipe failed: %s: state=%d msg=%s", rpipe, state, msg ? msg : "" );
	}

	bleat_printf( 2, "response written to pipe" );
	bleat_pop_lvl();			// we assume it was pushed when the request received; we pop it once we respond
	close( fd );
}

/*
	Cleanup a request.
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
	Read a request from the fifo, and format it into a request block
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
		bleat_printf( 0, "error: failed to create a json paring object for: %s\n", rbuf );
		free( rbuf );
		return NULL;
	}

	if( (stuff = jw_string( jblob, "action" )) == NULL ) {
		bleat_printf( 0, "error: request received without action: %s", rbuf );
		free( rbuf );
		jw_nuke( jblob );
		return NULL;
	}

	
	if( (req = (req_t *) malloc( sizeof( *req ) )) == NULL ) {
		bleat_printf( 0, "error: memory allocation error tying to alloc request for: %s", rbuf );
		free( rbuf );
		jw_nuke( jblob );
		return NULL;
	}
	memset( req, 0, sizeof( *req ) );

	bleat_printf( 1, "raw message: (%s)", rbuf );

	switch( *stuff ) {
		case 'a':
		case 'A':					// assume add until something else starts with a
			req->rtype = RT_ADD;
			break;

		case 'd':
		case 'D':					// assume delete until something else with d comes along
			req->rtype = RT_DEL;
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
			bleat_printf( 0, "error: unrecognised action in request: %s", rbuf );
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
	Testing loop for now. This is a black hole -- we never come out.
*/
static void vfd_dummy_loop( parms_t *parms, struct sriov_conf_c* conf ) {
	req_t*	req;
	char	mbuf[2048];			// message and work buffer
	int		rc = 0;
	char*	reason;

	*mbuf = 0;
	while( 1 ) {
		if( (req = vfd_read_request( parms )) != NULL ) {
			bleat_printf( 1, "got request\n" );
			bleat_printf( 2, "chatty to test temp log bump up" );

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
					if( vfd_add_vf( conf, req->resource, &reason ) ) {
						bleat_printf( 1, "vf added: %s", mbuf );
						snprintf( mbuf, sizeof( mbuf ), "VF added successfully: %s", req->resource );
						vfd_response( req->resp_fifo, 0, mbuf );
					} else {
						snprintf( mbuf, sizeof( mbuf ), "unable to add vf: %s: %s", req->resource, reason );
						vfd_response( req->resp_fifo, 1, mbuf );
						free( reason );
					}
					break;

				case RT_DEL:
					vfd_response( req->resp_fifo, 0, "dummy request handler: got your DELETE request and promptly ignored it." );
					break;

				case RT_SHOW:
					vfd_response( req->resp_fifo, 0, "dummy request handler: got your SHOW request and promptly ignored it." );
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

			
			bleat_printf( 2, "chatty shouldn't show as tmp log bump pops in response gen (unless verbose increased to >= 2" );
			vfd_free_request( req );
		}
		
		sleep( 1 );
	}
}

// -------------------------------------------------------------------------------------------------------------

static inline uint64_t RDTSC(void)
{
  unsigned int hi, lo;
  __asm__ volatile("rdtsc" : "=a" (lo), "=d" (hi));
  return ((uint64_t)hi << 32) | lo;
}

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
  
  int res = readConfigFile(fname);
  if (res != 0) {
    traceLog(TRACE_ERROR, "Can not read config file: %s\n", fname);
  }
  
  res = update_ports_config();
  if (res != 0) {
    traceLog(TRACE_ERROR, "Error updating ports configuration: %s\n", res);
  }
  
  traceLog(TRACE_NORMAL, "Received HUP signal\n");
}



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

	traceLog(TRACE_DEBUG, "Restoring Settings, Port: %d, VF: %d", p_reset->port, p_reset->vf);
	restore_vf_setings(p_reset->port, p_reset->vf);

	free(param);
}


void
restore_vf_setings(uint8_t port_id, int vf_id)
{
  dump_sriov_config(running_config);
  int i;
  int on = 1;
 
  for (i = 0; i < running_config.num_ports; ++i){
    struct sriov_port_s *port = &running_config.ports[i];
    
    if (port_id == port->rte_port_number){
      traceLog(TRACE_DEBUG, "------------------ PORT ID: %d --------------------\n", port->rte_port_number);
      traceLog(TRACE_DEBUG, "------------------ PORT PCIID: %s --------------------\n", port->pciid);
      
      int y;
      for(y = 0; y < port->num_vfs; ++y){
        
        struct vf_s *vf = &port->vfs[y];   
        
        traceLog(TRACE_DEBUG, "------------------ CHECKING VF ID: %d --------------------\n", vf->num);
        
        if(vf_id == vf->num){
           
          uint32_t vf_mask = VFN2MASK(vf->num); 

          traceLog(TRACE_DEBUG, "------------------ DELETING VLANS, VF: %d --------------------\n", vf->num);
          
          int v;
          for(v = 0; v < vf->num_vlans; ++v) {
            int vlan = vf->vlans[v];
            traceLog(TRACE_DEBUG, "------------------ DELETING VLAN: %d, VF: %d --------------------\n", vlan, vf->num );
            set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, 0);
          }
        
          // add new vlans from config fil
          traceLog(TRACE_DEBUG, "------------------ ADDING VLANS, VF: %d --------------------\n", vf->num);
          
          for(v = 0; v < vf->num_vlans; ++v) {
            int vlan = vf->vlans[v];
            traceLog(TRACE_DEBUG, "------------------ ADDIND VLAN: %d, VF: %d --------------------\n", vlan, vf->num );
            set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, on);
          }
                                   
          traceLog(TRACE_DEBUG, "------------------ DELETING MACs, VF: %d --------------------\n", vf->num);
          
          int m;
          for(m = 0; m < vf->num_macs; ++m) {
            char *mac = vf->macs[m];
						traceLog(TRACE_DEBUG, "------------------ DELETING MAC: %s, VF: %d --------------------\n", mac, vf->num );
            set_vf_rx_mac(port->rte_port_number, mac, vf->num, 0);
          }

          traceLog(TRACE_DEBUG, "------------------ ADDING MACs, VF: %d --------------------\n", vf->num);

          // iterate through all macs
          for(m = 0; m < vf->num_macs; ++m) {
            //__attribute__((__unused__)) char *mac = vf->macs[m];
						char *mac = vf->macs[m];
						traceLog(TRACE_DEBUG, "------------------ ADDING MAC: %s, VF: %d --------------------\n", mac, vf->num );
            set_vf_rx_mac(port->rte_port_number, mac, vf->num, 1);
          }
					
					
					// set VLAN anti spoofing when VLAN filter is used
					
					traceLog(TRACE_DEBUG, "------------------ SETTING VLAN ANTI SPOOFING: %d, VF: %d --------------------\n", vf->vlan_anti_spoof, vf->num);
          set_vf_vlan_anti_spoofing(port->rte_port_number, vf->num, vf->vlan_anti_spoof);   

					traceLog(TRACE_DEBUG, "------------------ SETTING MAC ANTISPOOFING: %d, VF: %d --------------------\n", vf->mac_anti_spoof, vf->num);					
          set_vf_mac_anti_spoofing(port->rte_port_number, vf->num, vf->mac_anti_spoof);

          traceLog(TRACE_DEBUG, "------------------ STRIP TAG: %d, VF: %d -------------------\n", vf->strip_stag, vf->num);	    
          rx_vlan_strip_set_on_vf(port->rte_port_number, vf->num, vf->strip_stag); 

					traceLog(TRACE_DEBUG, "------------------ INSERT TAG: %d, VF: %d --------------------\n", vf->insert_stag, vf->num);					
          rx_vlan_insert_set_on_vf(port->rte_port_number, vf->num, vf->insert_stag);
 
                 
					traceLog(TRACE_DEBUG, "------------------ SET PROMISCUOUS: %d, VF: %d --------------------\n", port->rte_port_number, vf->num);			 
          set_vf_allow_bcast(port->rte_port_number, vf->num, vf->allow_bcast);
          set_vf_allow_mcast(port->rte_port_number, vf->num, vf->allow_mcast);
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
 
			 }
      }
    }      
  }   
}


int
update_ports_config(void)
{
  int i;
  int on = 1;
 
  for (i = 0; i < sriov_config.num_ports; ++i){
    
    int ret;
  
    struct sriov_port_s *port = &sriov_config.ports[i];
    
    // running config
    struct sriov_port_s *r_port = &running_config.ports[i];
    
    // if running config older then new config update settings
    if (r_port->last_updated < port->last_updated) {
      traceLog(TRACE_DEBUG, "------------------ UPDADING PORT: %d, r_port time: %d, c_port time: %d, --------------------\n",
              i, r_port->last_updated, port->last_updated);
      
      rte_eth_promiscuous_enable(port->rte_port_number);
      rte_eth_allmulticast_enable(port->rte_port_number);
      
      ret = rte_eth_dev_uc_all_hash_table_set(port->rte_port_number, on);
      if (ret < 0)
        traceLog(TRACE_ERROR, "bad unicast hash table parameter, return code = %d \n", ret);
      
      r_port->rte_port_number = port->rte_port_number;
      r_port->last_updated = port->last_updated;
      strcpy(r_port->name, port->name);
      strcpy(r_port->pciid, port->pciid);
      r_port->last_updated = port->last_updated;
      r_port->mtu = port->mtu;
      r_port->num_mirros= port->num_mirros;
      r_port->num_vfs = port->num_vfs;
    }
     
    
    /* go through all VF's and set VLAN's */
    uint32_t vf_mask;
    
    int y;
    for(y = 0; y < port->num_vfs; ++y){
      
      struct vf_s *vf = &port->vfs[y];   
      
      // running VF's
      struct vf_s *r_vf = &r_port->vfs[y];
      vf_mask = VFN2MASK(vf->num);    

      if(r_vf->last_updated == 0)
        r_vf->num = vf->num;
      
      traceLog(TRACE_DEBUG, "HERE WE ARE = %d, vf->num %d, r_vf->num: %d, vf->last_updated: %d, r_vf->last_updated: %d\n", 
      y, vf->num, r_vf->num, vf->last_updated, r_vf->last_updated);      
      
      int v;
      // delete running vlans
      if (vf->num == r_vf->num && vf->last_updated > r_vf->last_updated) {
        traceLog(TRACE_DEBUG, "------------------ DELETING VLANS, VF: %d --------------------\n", vf->num);
        
        for(v = 0; v < r_vf->num_vlans; ++v) {
          int vlan = r_vf->vlans[v];
					traceLog(TRACE_DEBUG, "------------------ DELETING VLAN: %d, VF: %d --------------------\n", vlan, vf->num );
          set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, 0);
        }
      
        // add new vlans from config file

        traceLog(TRACE_DEBUG, "------------------ ADDING VLANS, VF: %d --------------------\n", vf->num);
        int v;
        for(v = 0; v < vf->num_vlans; ++v) {
          int vlan = vf->vlans[v];
					traceLog(TRACE_DEBUG, "------------------ ADDIND VLAN: %d, VF: %d --------------------\n", vlan, vf->num );
          set_vf_rx_vlan(port->rte_port_number, vlan, vf_mask, on);
          
          // update running config
          r_vf->vlans[v] = vlan;
        }
        r_vf->num_vlans = vf->num_vlans;
        

           
        traceLog(TRACE_DEBUG, "------------------ DELETING MACs, VF: %d --------------------\n", vf->num);
        
        int m;
        for(m = 0; m < r_vf->num_macs; ++m) {
          char *mac = vf->macs[m];
					traceLog(TRACE_DEBUG, "------------------ DELETING MAC: %s, VF: %d --------------------\n", mac, vf->num );
          set_vf_rx_mac(port->rte_port_number, mac, vf->num, 0);
        }

        traceLog(TRACE_DEBUG, "------------------ ADDING MACs, VF: %d --------------------\n", vf->num);

        // iterate through all macs
        for(m = 0; m < vf->num_macs; ++m) {
          char *mac = vf->macs[m];
					traceLog(TRACE_DEBUG, "------------------ ADDING MAC: %s, VF: %d --------------------\n", mac, vf->num );
          set_vf_rx_mac(port->rte_port_number, mac, vf->num, 1);

					strcpy(r_vf->macs[m], mac);
        }
				r_vf->num_macs = vf->num_macs;
        

        // set VLAN anti spoofing when VLAN filter is used
				
				traceLog(TRACE_DEBUG, "------------------ SETTING VLAN ANTI SPOOFING: %d, VF: %d --------------------\n", vf->vlan_anti_spoof, vf->num);
        set_vf_vlan_anti_spoofing(port->rte_port_number, vf->num, vf->vlan_anti_spoof);  

				traceLog(TRACE_DEBUG, "------------------ SETTING MAC ANTISPOOFING: %d, VF: %d --------------------\n", vf->mac_anti_spoof, vf->num);					
        set_vf_mac_anti_spoofing(port->rte_port_number, vf->num, vf->mac_anti_spoof);

        traceLog(TRACE_DEBUG, "------------------ STRIP TAG: %d, VF: %d -------------------\n", vf->strip_stag, vf->num);	
        rx_vlan_strip_set_on_vf(port->rte_port_number, vf->num, vf->strip_stag);   

				traceLog(TRACE_DEBUG, "------------------ INSERT TAG: %d, VF: %d --------------------\n", vf->insert_stag, vf->num);				
        rx_vlan_insert_set_on_vf(port->rte_port_number, vf->num, vf->insert_stag);
        
        set_vf_allow_bcast(port->rte_port_number, vf->num, vf->allow_bcast);
        set_vf_allow_mcast(port->rte_port_number, vf->num, vf->allow_mcast);
        set_vf_allow_un_ucast(port->rte_port_number, vf->num, vf->allow_un_ucast); 
        
        r_vf->vlan_anti_spoof = vf->vlan_anti_spoof;
        r_vf->mac_anti_spoof  = vf->mac_anti_spoof;
        r_vf->strip_stag      = vf->strip_stag;
        r_vf->insert_stag     = vf->insert_stag;
        r_vf->allow_bcast     = vf->allow_bcast;
        r_vf->allow_mcast     = vf->allow_mcast;
        r_vf->allow_un_ucast  = vf->allow_un_ucast;
        
        
        r_vf->last_updated = vf->last_updated;
      }
      
			traceLog(TRACE_DEBUG, "------------------ SET PROMISCUOUS: %d, VF: %d --------------------\n", port->rte_port_number, vf->num);
      uint16_t rx_mode = 0;

      // figure this out if we have to update it every time we change VLANS/MACS 
      // or once when update ports config
      rte_eth_promiscuous_enable(port->rte_port_number);
      rte_eth_allmulticast_enable(port->rte_port_number);  
      ret = rte_eth_dev_uc_all_hash_table_set(port->rte_port_number, on);
      

      // don't accept untagged frames
      rx_mode |= ETH_VMDQ_ACCEPT_UNTAG; 
      ret = rte_eth_dev_set_vf_rxmode(port->rte_port_number, vf->num, rx_mode, !on);
	
      if (ret < 0)
        traceLog(TRACE_DEBUG, "set_vf_allow_untagged(): bad VF receive mode parameter, return code = %d \n", ret);    
  
    }     
  }
  
  running_config.num_ports = sriov_config.num_ports;
  
  return 0;
}




int
readConfigFile(char *fname)
{
 
  //printf("Fname: %s\n", fname);
  
  int num_ports, num_vfs;
  config_t cfg;
  config_setting_t *p_setting, *v_settings, *vl_settings; 

  config_init(&cfg);
 
  if(! config_read_file(&cfg, fname)) {
    fprintf(stderr, "file: %s, %s:%d - %s\n", fname, config_error_file(&cfg),
            config_error_line(&cfg), config_error_text(&cfg));
            
    config_destroy(&cfg);
    exit(EXIT_FAILURE);
  } 

  
  // iterate through all ports
  p_setting = config_lookup(&cfg, "ports");
  if(p_setting != NULL) {
    num_ports = config_setting_length(p_setting);
    
    if(num_ports > MAX_PORTS){
      traceLog(TRACE_ERROR, "too many ports: %d\n", num_ports);
      exit(EXIT_FAILURE);
    }
    
    sriov_config.num_ports = num_ports;
    
    int i;

    for(i = 0; i < num_ports; i++) {
      config_setting_t *port = config_setting_get_elem(p_setting, i);

      // Only output the record if all of the expected fields are present.
      const char *name, *pciid;
      int last_updated, mtu;
    

      if(!(config_setting_lookup_string(port, "name", &name)
           && config_setting_lookup_string(port, "pciid", &pciid)
           && config_setting_lookup_int(port, "last_updated", &last_updated)
           && config_setting_lookup_int(port, "mtu", &mtu)))

        continue;

      
      stpcpy(sriov_config.ports[i].name, name);
      stpcpy(sriov_config.ports[i].pciid, pciid);
      sriov_config.ports[i].last_updated = last_updated;
      sriov_config.ports[i].mtu = mtu;
      

      // not sure how to use this stuff ;(
      char sstr[15];
      sprintf(sstr, "ports.[%d].VFs", i);
      v_settings = config_lookup(&cfg, sstr);
      
      if(v_settings != NULL) {

        num_vfs = config_setting_length(v_settings);
        
        sriov_config.ports[i].num_vfs = num_vfs;
        
        if (num_vfs > MAX_VFS) {
          traceLog(TRACE_ERROR, "too many VF's: %d\n", num_vfs);
          exit(EXIT_FAILURE);
        }
        
        int y;

        
        for(y = 0; y < num_vfs; y++) {
          config_setting_t *vf = config_setting_get_elem(v_settings, y);
            
          /* Only output the record if all of the expected fields are present. */
          int vfn, strip_stag, insert_stag, vlan_anti_spoof, mac_anti_spoof;
          int allow_bcast, allow_mcast, allow_un_ucast, last_updated, link;
          double rate;
       
                
          if(!(config_setting_lookup_int(vf, "vf", &vfn)
               && config_setting_lookup_int(vf, "last_updated", &last_updated)
               && config_setting_lookup_int(vf, "strip_stag", &strip_stag)
               && config_setting_lookup_int(vf, "insert_stag", &insert_stag)
               && config_setting_lookup_int(vf, "vlan_anti_spoof", &vlan_anti_spoof)
               && config_setting_lookup_int(vf, "mac_anti_spoof", &mac_anti_spoof)
               && config_setting_lookup_float(vf, "rate", &rate)
               && config_setting_lookup_int(vf, "link", &link)
               && config_setting_lookup_int(vf, "allow_bcast", &allow_bcast)
               && config_setting_lookup_int(vf, "allow_mcast", &allow_mcast)
               && config_setting_lookup_int(vf, "allow_un_ucast", &allow_un_ucast)))


               
             continue;

          sriov_config.ports[i].vfs[y].num = vfn;
          sriov_config.ports[i].vfs[y].last_updated = last_updated;
          sriov_config.ports[i].vfs[y].strip_stag = strip_stag;
          sriov_config.ports[i].vfs[y].insert_stag = insert_stag;
          sriov_config.ports[i].vfs[y].vlan_anti_spoof = vlan_anti_spoof;
          sriov_config.ports[i].vfs[y].mac_anti_spoof = mac_anti_spoof;
          sriov_config.ports[i].vfs[y].allow_bcast = allow_bcast;
          sriov_config.ports[i].vfs[y].allow_mcast = allow_mcast;
          sriov_config.ports[i].vfs[y].allow_un_ucast = allow_un_ucast;
          sriov_config.ports[i].vfs[y].link = link;
          
          sriov_config.ports[i].vfs[y].rate = rate;
   
          /*
          printf("%-5d  %-36s  %-10d  %-11d  %-8d  %-7d  %-11d  %-11d  %-14d  %2.2f\n", 
                  sriov_config.ports[i].vfs[y].num,
                  sriov_config.ports[i].vfs[y].last_updated,
                  sriov_config.ports[i].vfs[y].strip_stag,
                  sriov_config.ports[i].vfs[y].insert_stag,
                  sriov_config.ports[i].vfs[y].vlan_anti_spoof,
                  sriov_config.ports[i].vfs[y].mac_anti_spoof,
                  sriov_config.ports[i].vfs[y].allow_bcast,
                  sriov_config.ports[i].vfs[y].allow_mcast,
                  sriov_config.ports[i].vfs[y].allow_un_ucast,
                  sriov_config.ports[i].vfs[y].rate = rate);
          */        
 
                  
          char vstr[30];
          sprintf(vstr, "ports.[%d].VFs.[%d].VLANs", i, y); 
          vl_settings = config_lookup(&cfg, vstr);
          
          if(vl_settings != NULL) {
            
            int count = config_setting_length(vl_settings);   
            
            if(count > MAX_VF_VLANS) {
              traceLog(TRACE_ERROR, "too many VLANs: %d\n", count);
              exit(EXIT_FAILURE);
            }
            
            sriov_config.ports[i].vfs[y].num_vlans = count;

            int x;
           // printf("%-5s\n", "VLAN ID");
           
            for(x = 0; x < count; x++) {
              int vlan_id = config_setting_get_int_elem(vl_settings, x);             
              sriov_config.ports[i].vfs[y].vlans[x] = vlan_id;

              //printf("%-5d\n", sriov_config.ports[i].vfs[y].vlans[x]);
            }
          }

          
          sprintf(vstr, "ports.[%d].VFs.[%d].MACs", i, y); 
          vl_settings = config_lookup(&cfg, vstr);
          
          if(vl_settings != NULL) {
            
            int count = config_setting_length(vl_settings);
            int x;
            //printf("%-5s\n", "MAC");

            if(count > MAX_VF_MACS) {
              traceLog(TRACE_ERROR, "too many MACs: %d\n", count);
              exit(EXIT_FAILURE);
            }

            sriov_config.ports[i].vfs[y].num_macs = count;
            
            for(x = 0; x < count; x++) {
              const char *mac = config_setting_get_string_elem(vl_settings, x);           
              strcpy(sriov_config.ports[i].vfs[y].macs[x], mac); 
              //printf("%-5s\n", sriov_config.ports[i].vfs[y].macs[x]);
            }
          }
        }
      }      
    }
  }

  if (debug)
    dump_sriov_config(sriov_config);
  
  return 0;
}



void 
dump_sriov_config(struct sriov_conf_c sriov_config)
{
  traceLog(TRACE_DEBUG, "Number of ports: %d\n", sriov_config.num_ports);
  int i;
  
  for (i = 0; i < sriov_config.num_ports; i++){
    traceLog(TRACE_DEBUG, "Port #: %d, name: %s, pciid %s, last_updated %d, mtu: %d, num_mirrors: %d, num_vfs: %d\n",
          i, sriov_config.ports[i].name, 
          sriov_config.ports[i].pciid, 
          sriov_config.ports[i].last_updated,
          sriov_config.ports[i].mtu,
          sriov_config.ports[i].num_mirros,
          sriov_config.ports[i].num_vfs );
    
    int y;
    for (y = 0; y < sriov_config.ports[i].num_vfs; y++){
      traceLog(TRACE_DEBUG, "VF num: %d, last_updated: %d\nstrip_stag %d\ninsert_stag %d\nvlan_aspoof: %d\nmac_aspoof: %d\nallow_bcast: %d\n\
allow_ucast: %d\nallow_mcast: %d\nallow_untagged: %d\nrate: %f\nlink: %d\num_vlans: %d\nnum_macs: %d\n", 
            sriov_config.ports[i].vfs[y].num, 
            sriov_config.ports[i].vfs[y].last_updated, 
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
      traceLog(TRACE_DEBUG, "VLANs [ ");
      for (x = 0; x < sriov_config.ports[i].vfs[y].num_vlans; x++) {
        traceLog(TRACE_DEBUG, "%d ", sriov_config.ports[i].vfs[y].vlans[x]);
      }   
      traceLog(TRACE_DEBUG, "]\n");
      
      int z;
      traceLog(TRACE_DEBUG, "MACs [ ");
      for (z = 0; z < sriov_config.ports[i].vfs[y].num_macs; z++) {
        traceLog(TRACE_DEBUG, "%s ", sriov_config.ports[i].vfs[y].macs[z]);
      }   
      traceLog(TRACE_DEBUG, "]\n");
      traceLog(TRACE_DEBUG, "------------------------------------------------------------------------------\n");
    }
  }
}

int 
main(int argc, char **argv)
{
	char*	parm_file = NULL;							// default in /etc, -p overrieds
	parms_t*	parms = NULL;							// info read from the parm file
	char	log_file[1024];				// buffer to build full log file in
	char	run_asynch = 1;				// -f sets off to keep attached to tty
	
  int  opt;
  //int	opterr = 0;

  //"  sriovctl [options] -f <file_name>\n"
  const char * main_help =
	"vfd\n"
	"Usage:\n"
	"  Options:\n"
  "\t -c <mask> Processor affinity mask\n"
  "\t -f 		keep in 'foreground'\n"
  "\t -p <file> parmm file (/etc/vfd/vfd.cfg)\n"
  "\t -v <num>  Verbose (if num > 3 foreground) num - verbose level\n"
  "\t -s <num>  syslog facility 0-11 (log_kern - log_ftp) 16-23 (local0-local7) see /usr/include/sys/syslog.h\n"
	"\t -h|?  Display this help screen\n";


  
  //int devnum = 0;
 
 	struct rte_mempool *mbuf_pool;
	//unsigned n_ports;
	
  prog_name = strdup(argv[0]);
  useSyslog = 1; 

	int i;

//	for( i = 0; i < argc; i++)
//		printf("ARGV[%d] = %s\n", i, argv[i]);

	parm_file = strdup( "/etc/vfd/vfd.cfg" );				// set default before command line parsing as -p overrides 
  fname = NULL;
  
  // Parse command line options
  while ( (opt = getopt(argc, argv, "fhv:p:s:")) != -1)			// f,c  dropped
  {
    switch (opt)
    {

	/*		now from the parm file
    case 'c':
      cpu_mask = atoi(optarg);
      break;
	*/

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
      
	/* -- we read from the config directory now, not a single file
    case 'f':
      fname = strdup(optarg);
      break; 
	*/

	case 'p':
		parm_file = strdup( optarg );
		break;

    case 's':
      logFacility = (atoi(optarg) << 3);
      break;
    


    case 'h':
    case '?':
      printf("%s\n", main_help);
      exit(EXIT_FAILURE);
      break;

	default:
		fprintf( stderr, "unknown commandline flag: %c\n", opt );
		fprintf( stderr, "%s\n", main_help );
		exit( 1 );
    }
  }


	/* --- we read from config directory now
	if(fname == NULL) {
		printf("%s\n", main_help);
		exit(EXIT_FAILURE);
	}
	*/
  
	if( (parms = read_parms( parm_file )) == NULL ) { 						// get overall configuration (includes list of pciids we manage)
		fprintf( stderr, "unable to read configuration from %s: %s\n", parm_file, strerror( errno ) );
		exit( 1 );
	} 
  
	snprintf( log_file, sizeof( log_file ), "%s/vfd.log", parms->log_dir );
	if(  run_asynch ) {
		bleat_set_log( log_file, BLEAT_ADD_DATE );									// open bleat log with date suffix
	}
	bleat_set_lvl( parms->log_level );												// set default level
	bleat_printf( 0, "VFD initialising" );
	bleat_printf( 0, "config dir set to: %s", parms->config_dir );

	vfd_add_ports( parms, &running_config );										// add the pciid info from parms to the ports list
	vfd_add_all_vfs( parms, &running_config );										// read all config files and add the VFs to the config

	if( vfd_init_fifo( parms ) < 0 ) {
		bleat_printf( 0, "abort: unable to initialise request fifo" );
		exit( 1 );
	}

	if( vfd_eal_init( parms ) < 0 ) {												// dpdk function returns -1 on error
		bleat_printf( 0, "abort: unable to initialise dpdk eal environment" );
		exit( 1 );
	}

	vfd_dummy_loop( parms, &running_config );
bleat_printf( 0, "testing exit being taken" );
exit( 0 ); // TESTING ---- 




	/*
  int res = readConfigFile(fname);
  
  if (res < 0)
    rte_exit(EXIT_FAILURE, "Can not parse config file %s\n", fname);
	*/

    
  argc -= optind;
  argv += optind;
  optind = 0;


  //argc = 11;
	argc = 12;

  
  
  int argc_port = argc + sriov_config.num_ports * 2;
  
	//char **cli_argv = (char**)malloc(argc * sizeof(char*));
  char **cli_argv = (char**)malloc(argc_port * sizeof(char*));

  
  // add # num of ports * 2 to args, so we can do -w pciid stuff
  for(i = 0; i < argc_port; i ++) {
    cli_argv[i] = (char*)malloc(20 * sizeof(char));
  }

  //sprintf(cli_argv[0], "sriovctl");
  sprintf(cli_argv[0], "vfd");						// dummy up a command line to pass to rte_eal_init() -- it expects that we got these on our command line (what a hack)

  sprintf(cli_argv[1], "-c");
  sprintf(cli_argv[2], "%#02x", cpu_mask);

  sprintf(cli_argv[3], "-n");
  sprintf(cli_argv[4], "4");

  sprintf(cli_argv[5], "–m");
  sprintf(cli_argv[6], "50");

  sprintf(cli_argv[7], "--file-prefix");
  //sprintf(cli_argv[8], "%s", "sriovctl");
  sprintf(cli_argv[8], "%s", "vfd");

  sprintf(cli_argv[9], "--log-level");
  sprintf(cli_argv[10], "%d", 8);

  sprintf(cli_argv[11], "%s", "--no-huge");
  
  
  int y = 0;										// to that add n -w <pciid> options (these need to be defined in the parm file)
  for(i = argc; i < argc_port; i+=2) {
    sprintf(cli_argv[i], "-w");
    sprintf(cli_argv[i + 1], "%s", sriov_config.ports[y].pciid);
      
    traceLog(TRACE_INFO, "PCI num: %d, PCIID: %s\n", y, sriov_config.ports[y].pciid);
    y++;
  }
  
  
	//TESTING -- dont detach if(!debug) daemonize();
    
			
	// http://dpdk.org/doc/api/rte__eal_8h.html
	// init EAL 
	int ret = rte_eal_init(argc_port, cli_argv);

		
	if (ret < 0) {
		bleat_printf( 0, "abort: unable to initalise EAL" );			// is there an error we can log?
		rte_exit(EXIT_FAILURE, "Error with EAL initialization\n");
	}
	
	rte_set_log_type(RTE_LOGTYPE_PMD && RTE_LOGTYPE_PORT, 0);
	
	traceLog(TRACE_INFO, "LOG LEVEL = %d, LOG TYPE = %d\n", rte_get_log_level(), rte_log_cur_msg_logtype());

    
	rte_set_log_level(8);
	

	n_ports = rte_eth_dev_count();


  if(n_ports != sriov_config.num_ports) {
    traceLog(TRACE_ERROR, "ports found (%d) != ports requested (%d)\n", n_ports, sriov_config.num_ports);  
  }

  traceLog(TRACE_NORMAL, "n_ports = %d\n", n_ports);

  

 /*
  const struct rte_memzone *mz;

  mz = rte_memzone_reserve(IF_PORT_INFO, sizeof(struct ifrate_s), rte_socket_id(), 0);
	if (mz == NULL)
		rte_exit(EXIT_FAILURE, "Cannot reserve memory zone for port information\n");
  memset(mz->addr, 0, sizeof(struct ifrate_s));
  

  ifrate_stats = mz->addr; 
  
  printf("%p\t\n", (void *)ifrate_stats);

  */

	// Creates a new mempool in memory to hold the mbufs.
	mbuf_pool = rte_pktmbuf_pool_create("sriovctl", NUM_MBUFS * n_ports,
                      MBUF_CACHE_SIZE,
                      0, 
                      RTE_MBUF_DEFAULT_BUF_SIZE,
                      rte_socket_id());

	if (mbuf_pool == NULL)
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");

	/* Initialize all ports. */
  u_int16_t portid;
	for (portid = 0; portid < n_ports; portid++)
		if (port_init(portid, mbuf_pool) != 0)
			rte_exit(EXIT_FAILURE, "Cannot init port %"PRIu8 "\n", portid);
   

   
  int port; 
  
  for(port = 0; port < n_ports; ++port){
    
    struct rte_eth_dev_info dev_info;
    rte_eth_dev_info_get(port, &dev_info);
      
    //struct ether_addr addr;
    rte_eth_macaddr_get(port, &addr);
    traceLog(TRACE_INFO, "Port: %u, MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8
           ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ", ",
        (unsigned)port,
        addr.addr_bytes[0], addr.addr_bytes[1],
        addr.addr_bytes[2], addr.addr_bytes[3],
        addr.addr_bytes[4], addr.addr_bytes[5]);


    traceLog(TRACE_INFO, "Driver Name: %s, Index %d, Pkts rx: %lu, ", 
            dev_info.driver_name, dev_info.if_index, st.pcount);
    
    traceLog(TRACE_INFO, "PCI: %04X:%02X:%02X.%01X, Max VF's: %d, Numa: %d\n\n", dev_info.pci_dev->addr.domain, 
            dev_info.pci_dev->addr.bus , dev_info.pci_dev->addr.devid , dev_info.pci_dev->addr.function, 
            dev_info.max_vfs, dev_info.pci_dev->numa_node);

            
    /*
     * rte could inumerate ports different then in config file
     * rte_config_portmap array will hold index to config
     */
    int i;    
    for(i = 0; i < sriov_config.num_ports; ++i) {
      char pciid[16];
      sprintf(pciid, "%04X:%02X:%02X.%01X", 
            dev_info.pci_dev->addr.domain, 
            dev_info.pci_dev->addr.bus, 
            dev_info.pci_dev->addr.devid, 
            dev_info.pci_dev->addr.function);
      
      if (strcmp(pciid, sriov_config.ports[i].pciid) == 0) {;
        rte_config_portmap[port] = i;
        // point config port back to rte port
        sriov_config.ports[i].rte_port_number = port;
      }
    }
  }
  

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

  traceLog(TRACE_NORMAL, "starting sriovctl loop\n");
  

  update_ports_config();

  char buff[1024];
  if(mkfifo(STATS_FILE, 0666) != 0)
    traceLog(TRACE_ERROR, "can't create pipe: %s, %d\n", STATS_FILE, errno);

  int fd;
  /*
	FILE * dump = fopen("/tmp/pci_dump.txt", "w");
	rte_eal_pci_dump(dump);
	fclose (dump);
	*/
	
  while(!terminated)
	{
		usleep(20000);
   
    fd = open(STATS_FILE, O_WRONLY);
    sprintf(buff, "%s %18s %10s %10s %10s %10s %10s %10s %10s %10s %10s\n", "Iface", "Link", "Speed", "Duplex", "RX pkts", "RX bytes", 
      "RX errors", "RX dropped", "TX pkts", "TX bytes", "TX errors");   
    
    __attribute__((__unused__)) int ret;
    ret = write(fd, buff, strlen(buff));
    
    for (i = 0; i < n_ports; ++i)
    {
			struct rte_eth_dev_info dev_info;
			rte_eth_dev_info_get(i, &dev_info);			

			sprintf(buff, "%04X:%02X:%02X.%01X", 
			dev_info.pci_dev->addr.domain, 
			dev_info.pci_dev->addr.bus, 
			dev_info.pci_dev->addr.devid, 
			dev_info.pci_dev->addr.function);
						
      ret = write(fd, buff, strlen(buff));  
      
      nic_stats_display(i, buff);
      ret = write(fd, buff, strlen(buff));       
    }
    
    close(fd);
   // if (debug)
        //nic_stats_display(i);
	}
 
  if(unlink(STATS_FILE) != 0)
    traceLog(TRACE_ERROR, "can't delete pipe: %s\n", STATS_FILE);
  
  gettimeofday(&st.endTime, NULL);
  traceLog(TRACE_NORMAL, "Duration %.f sec\n", timeDelta(&st.endTime, &st.startTime));

  traceLog(TRACE_NORMAL, "sriovctl exit\n");

  return EXIT_SUCCESS;
}
