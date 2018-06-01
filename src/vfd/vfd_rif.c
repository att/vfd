// vi: sw=4 ts=4 noet:

/*
	Mnemonic:	vfd_rif.c
	Abstract:	These functions provide the request interface between VFd and
				iplex.
	Author:		E. Scott Daniels
	Date:		11 October 2016 (broken out of main.c)

	Mods:		29 Nov 2016 : Add queue share support from vf config.
				06 Jan 2017 : Incorporate DJ's fix for link mode.
				30 Jan 2017 : Fix vfid check to detect pars error.
				14 Feb 2017 : Correct bug in del range check on vf number.
				21 Feb 2017 : Prevent empty vlan id list from being accepted.
				23 Mar 2017 : Allow multiple VLAN IDs when strip == true.
				22 Sep 2017 : Prevent hanging lock in add_ports if already called.
				25 Sep 2017 : Fix validation of mirror target bug.
				10 Oct 2017 : Add support for mirror update and show mirror commands.
				30 Jan 2017 : correct bug in mirror target range check (issue #242)
				09 Feb 2018 : Fix potential memory leak if no json files exist in directory.
								Correct loop initialisation bug; $259
				14 Feb 2018 : Add support to keep config file name field and compare at delete. (#262)
				19 Feb 2018 : Add support for 'live' config directory (#263)
				04 Apr 2018 : Change mv to copy with src-unlink to allow for 'move' to a directory
								on a different file system (possible container requirement).
				17 Apr 2018 : Correct bug related to issue 291.
				18 Apr 2018 : Correct placment for first_mac initialisation.
				24 Apr 2018 : Correct double free bug if pciid wasn't right in a config file.
*/


#include <vfdlib.h>		// if vfdlib.h needs an include it must be included there, can't be include prior
#include "sriov.h"
#include "vfd_rif.h"

//--------------------------------------------------------------------------------------------------------------

/*
	Create our fifo and tuck the handle into the parm struct. Returns 0 on
	success and <0 on failure.
*/
extern int vfd_init_fifo( parms_t* parms ) {
	if( !parms ) {
		return -1;
	}

	umask( 0 );
	parms->rfifo = rfifo_open( parms->fifo_path, 0666 );		//TODO -- set mode more sanely, but this runs as root, so regular users need to write to this thus open wide for now
	if( parms->rfifo == NULL ) {
		bleat_printf( 0, "ERR: unable to create request fifo (%s): %s", parms->fifo_path, strerror( errno ) );
		return -1;
	} else {
		bleat_printf( 0, "listening for requests via pipe: %s", parms->fifo_path );
	}

	return 0;
}

/*
	Move a 'used' configuration file. If suffix is nil, then we move the file to the 'live'
	directory and do not change the filename.  If a suffix is provided, we just rename the 
	file adding the suffix (assuming foo.json will be renamed foo.json.error in the spot
	where the virtualisation manager left the bad meat.
*/
static void relocate_vf_config( parms_t* parms, char* filename, const_str suffix ) {
	char	wbuf[2048];
	unsigned int len;
	const_str base;								// basename portion of filename

	memset( wbuf, 0, sizeof( wbuf ) );			// keeps valgrind happy

	if( suffix == NULL ) {						// move to the live directory
		len = snprintf( wbuf, sizeof( wbuf ), "%s_live/", parms->config_dir );
	} else {
		if( (base = strrchr( filename, '/' )) != NULL ) {		// point 1 past the last slant
			if( ++base == 0 ) {
				bleat_printf( 0, "internal mishap: relocate vf config with suffix cannot be directory: %s", filename );
				return;
			}
		} else {
			base = filename;
		}
		len = snprintf( wbuf, sizeof( wbuf ), "%s/%s%s", parms->config_dir, base, suffix );
	}

	if( len >= sizeof( wbuf ) ) {			// truncated, config directory path just too bloody long
		bleat_printf( 0, "cannot construct a target file or path to relocate %%s", filename );
		return;
	}

	if( ! cp_file( filename, wbuf, 1 ) ) {		// copy and unlink src
		bleat_printf( 0, "config file relocation from %s to %s failed: %s", filename, wbuf, strerror( errno ) );
	} else {
		bleat_printf( 2, "config file relocated from %s to %s", filename, wbuf );
	}
}

/*
	Removes a vf configuration file from the live directory. If the directory name
	passed is not nil, then the file is moved there and given a trailing dash (-), 
	otherwise it is just unlinked.
*/
static void delete_vf_config( const_str fname, const_str target_dir ) {
	char wbuf[2048];
	const_str	tok;

	if( ! target_dir ) {
		if( unlink( fname ) < 0 ) {
			bleat_printf( 0, "del_vf_conf: unable to unlink config file: %s: %s", fname, strerror( errno ) );
			return;
		}

		bleat_printf( 2, "vdel_vf_conf: f config file deleted: %s", fname );
		return;
	}

	if( (tok = strrchr( fname, '/' )) == NULL ) {		// point at basename if a full path (which it should be)
		tok = fname;
	} else {
		tok++;
	}

	if( snprintf( wbuf, sizeof( wbuf ), "%s/%s-", target_dir, tok ) >= (int) sizeof( wbuf ) ) {
		bleat_printf( 0, "del_vf_conf: unable to create target pathname, just unlinked: %s", fname );
		unlink( fname );
		return;	
	}
	
	if( ! mv_file( fname, wbuf ) ) {
		bleat_printf( 0, "del_vf_conf: unable to move %s to %s; just unlinked", fname, wbuf );
		return;
	}

	bleat_printf( 2, "del_vf_conf: moved %s to %s", fname, wbuf );
	
}


// ---------------------- validation --------------------------------------------------------------------------

/*
	Looks at the currently configured PF and determines whether or not the requested
	traffic class percentages can be added without 'busting' the limits if we are in
	strict (no overscription) mode for the PF.  If we are in relaxed mode (oversub
	is allowed) then this function should not be called.

	Port is the PF number mapped from the pciid in the parm file.
	req_tcs is an array of the reqested tc percentages ordered traffic class 0-7.

	Return code of 0 indicates success; non-zero is failure.
	
*/
extern int check_qs_oversub( struct sriov_port_s* port, uint8_t* qshares ) {

	int	totals[MAX_TCS];			// current pct totals
	int	i;
	int j;
	int	rc = 0;						// return code; assume good

	memset( totals, 0, sizeof( totals ) );

	for( i = 0; i < port->num_vfs; i++ ) {			// sum the pctgs for each TC across all VFs
		if( port->vfs[i].num >= 0 ) {				// active VF
			for( j = 0; j < MAX_TCS; j++ ) {
				totals[j] += port->vfs[i].qshares[j];	// add in this total
			}
		}
	}

	for( i = 0; i < MAX_TCS; i++ ) {
		if( totals[i] + qshares[i] > 100 ) {
			rc = 1;
			bleat_printf( 1, "requested traffic class percentage causes limit to be exceeded: tc=%d current=%d requested=%d", i, totals[i], qshares[i] );
		}
	}

	return rc;
}

/*
	Queue shares on a traffic class for some NICs cannot exceed a 10x limit between 
	min and max.  This function will check the queue shares and return non-zero if
	the difference between min and max is greater than 10x. Qshares is a pointer to
	the values which are being added to the port and will be taken into consideration
	with the current port settings.

	Return of 0 indicates that the qshares can safely be added; non-zero indicates one 
	or more of the shares busts the limit.
*/
extern int check_qs_spread( struct sriov_port_s* port, uint8_t* qshares ) {
	int	min[MAX_TCS];			// min and max for each TC
	int	max[MAX_TCS];
	int	i;
	int j;
	int	rc = 0;						// return code; assume good

	for( i = 0; i < MAX_TCS; i++ ) {				// seed with the values we wish to insert
		min[i] = max[i] = qshares[i];
	}

	for( i = 0; i < port->num_vfs; i++ ) {			// sum the pctgs for each TC across all VFs
		if( port->vfs[i].num >= 0 ) {				// active VF
			for( j = 0; j < MAX_TCS; j++ ) {
				if( port->vfs[i].qshares[j] > 0  &&  min[j] > port->vfs[i].qshares[j] ) {		// zeros are ignored
					min[j] = port->vfs[i].qshares[j];
				}
				if( max[j] < port->vfs[i].qshares[j] ) {
					max[j] = port->vfs[i].qshares[j];
				}
			}
		}
	}

	for( i = 0; i < MAX_TCS; i++ ) {
		if( min[i] != 0 && max[i] / min[i]  > 10 ) {
			rc = 1;
			bleat_printf( 1, "requested traffic class percentage takes spread to more than 10x for tc %d min=%d max=%d", i, min[i], max[i] );
		}
	}

	return rc;
}

// -------------- queue share related things ------------------------------------------------------------------------
/*
	Generate the array of queue share percentages adjusting for under/over subscription such that the percentages
	across each TC total exactly 100%.  The output array is grouped by VF (illustrated below) and attached to the
	port's struct.

	if 4 TCs
		VF0-TC0 | VF0-TC1 | VF0-TC2 | VF0-TC3 | VF1-TC0 | VF1-TC1 | VF1-TC2 | VF1-TC3 | VF2-TC0 | VF2-TC1 | VF2-TC2 | VF2-TC3 | ...
	if 8 TCs
		VF0-TC0 | VF0-TC1 | VF0-TC2 | VF0-TC3 | VF0-TC4 | VF0-TC5 | VF0-TC6 | VF0-TC7 | VF1-TC0 | VF1-TC1 | VF1-TC2 | VF1-TC3 | ...

	Over subscription policy is enforced when the VF's config file is parsed and added to the
	running config (rejected if over subscription is not allowed and the requested percentages
	would take the values out of range). The sum for each TC is normalised here such that values
	are increased proportionally if the TC is undersubscribed, and reduced proportionally if the
	TC is over subscribed.

	This should be called after every VF add/delete to recompute the queue shares across all.
*/
void gen_port_qshares( sriov_port_t *port ) {
	int* 	norm_pctgs;				// normalised percentages (to be returned)
	int 	i;
	int		j;
	int		sums[MAX_TCS];					// TC percentage sums
	int		ntcs;							// number of TCs
	double	v;								// computed value
	int		vfid;							// the vf number we are looking at (vf # might not correspond to index in table)
	double	factor;							// normalisation factor

	norm_pctgs = (int *) malloc( sizeof( *norm_pctgs ) * MAX_QUEUES );
	if( norm_pctgs == NULL ) {
		bleat_printf( 0, "error: unable to allocate %d bytes for max-pctg array", sizeof( *norm_pctgs ) * MAX_QUEUES  );
		return;
	}
	memset( norm_pctgs, 0, sizeof( *norm_pctgs ) * MAX_QUEUES );

	ntcs = port->ntcs;
	for( i = 0; i < ntcs; i++ ) {			// for each tc, compute the overall sum based on configured
		sums[i] = 0;

		for( j = 0; j < port->num_vfs; j++ ) {
			if( port->vfs[j].num >= 0 ) {					// only for active VFs
				//bleat_printf( 1, ">>> add to sum tc=%d vf=%d sum=%d share=%d", i, port->vfs[j].num, sums[i], port->vfs[j].qshares[i] );
				sums[i] += port->vfs[j].qshares[i];
			}
		}
	}

	for( i = 0; i < ntcs; i++ ) {
		if( sums[i] != 100 ) {									// over/under subscribed; must normalise
			factor = 100.0 / (double) sums[i];
			bleat_printf( 3, "normalise qshare: tc=%d factor=%.2f sum=%d", i, factor, sums[i] );
			sums[i] = 0;

			for( j = 0; j < port->num_vfs; j++ ) {
				if( (vfid = port->vfs[j].num) >= 0 ) {			// only deal with active VFs
					v = port->vfs[j].qshares[i] * factor;		// adjust the configured value
					norm_pctgs[(vfid * ntcs)+i] = (uint8_t) v;	// stash it, dropping fractional part

					sums[i] += (int) v;
				}
			}	

			if( sums[i] < 100 ) {									// rounding will likely leave us short and DPDK demands an exact 100% total
				for( j = 0; j < port->num_vfs && sums[i] < 100; j++ ) {
					if( (vfid = port->vfs[j].num) >= 0 ) {
						norm_pctgs[(vfid * ntcs)+i]++;		// fudge up each until we top off at 100; not fair, but did we promise to be?
					}
				}
			}
		} else {
			bleat_printf( 3, "no qshare normalisation needed: tc=%d sum=%d", i,  sums[i] );
			for( j = 0; j < port->num_vfs; j++ ) {
				if( (vfid = port->vfs[j].num) >= 0 ){								// active VF
					norm_pctgs[(vfid * ntcs)+i] =  port->vfs[j].qshares[i];			// sum is 100, stash unchanged
				}
			}
		}
	}

	if( bleat_will_it( 2 ) ) {
		for( i = 0; i < MAX_QUEUES; i += 16 ) {
			bleat_printf( 2, "port %s qshares %d - %d:", port->name, i, i + 15  );
				bleat_printf( 2, "\t %3d %3d %3d %3d  %3d %3d %3d %3d   %3d %3d %3d %3d  %3d %3d %3d %3d",
					norm_pctgs[i], norm_pctgs[i+1], norm_pctgs[i+2], norm_pctgs[i+3], norm_pctgs[i+4], norm_pctgs[i+5], norm_pctgs[i+6], norm_pctgs[i+7],
					norm_pctgs[i+8], norm_pctgs[i+9], norm_pctgs[i+10], norm_pctgs[i+11], norm_pctgs[i+12], norm_pctgs[i+13], norm_pctgs[i+14], norm_pctgs[i+15] );
		}
	}

	if( port->vftc_qshares != NULL ) {
		free( port->vftc_qshares );
	}

	port->vftc_qshares = norm_pctgs;
}

//  --------------------- global config management ------------------------------------------------------------

/*
	Pull the list of pciids from the parms and set into the in memory configuration that
	is maintained. If this is called more than once, it will refuse to do anything.
*/
extern void vfd_add_ports( parms_t* parms, sriov_conf_t* conf ) {
	static int called = 0;		// doesn't makes sense to do this more than once
	int i;
	int j;
	int k;
	int pidx = 0;				// port idx in conf list
	struct sriov_port_s* port;
	pfdef_t*	pfc;			// pointer to the config info for a port (pciid)

	rte_spinlock_lock( &conf->update_lock );
	if( called ) {
		rte_spinlock_unlock( &conf->update_lock );
		return;
	}
	called = 1;
	
	for( i = 0; pidx < MAX_PORTS  && i < parms->npciids; i++, pidx++ ) {
		pfc = &parms->pciids[i];					// point at the pf's configuration info

		port = &conf->ports[pidx];
		port->flags = 0;
		port->last_updated = ADDED;						 						// flag newly added so the nic is configured next go round
		snprintf( port->name, sizeof( port->name ), "port_%d",  i);				// TODO--- support getting a name from the config
		snprintf( port->pciid, sizeof( port->pciid ), "%s", pfc->id );
		port->mtu = pfc->mtu;

		if( pfc->flags & PFF_PROMISC ) {
			port->flags |= PF_PROMISC;											// set promisc mode on the PF
		}
		if( pfc->flags & PFF_LOOP_BACK ) {
			port->flags |= PF_LOOPBACK;											// enable VM->VM traffic without leaving nic
		}
		if( pfc->flags & PFF_VF_OVERSUB ) {
			port->flags |= PF_OVERSUB;											// enable VM->VM traffic without leaving nic
		}

		port->num_mirrors = 0;
		port->num_vfs = 0;
		port->ntcs = pfc->ntcs;					// number of traffic classes to maintain
		
		for( j = 0; j < MAX_TCS; j++ ) {
			port->tc_config[j] = pfc->tcs[j];	// point at the config struct
			pfc->tcs[j] = NULL;					// unmark it so it won't free	
		}

		memset( port->tc2bwg, 0, sizeof( port->tc2bwg ) );		// by default a tc is in group 0
		for( j = 0; j < NUM_BWGS; j++ ) {					// set the map which defines the bandwidth group each TC belongs to
			bw_grp_t*	bwg;
			
			bwg = &pfc->bw_grps[j];
			for( k = 0; k < bwg->ntcs; k++ ) {
				port->tc2bwg[bwg->tcs[k]] = j;	// map the TC to this bw group
			}
		}

		if( bleat_will_it( 2 ) ) {
			bleat_printf( 2, "pf %d configured: %s %s mtu=%d flags-0x02x ntcs==%d", i, port->name, port->pciid, port->mtu, port->flags, port->ntcs );
			for( j = 0; j < MAX_TCS; j++ ) {
				if( port->tc_config[j] != NULL ) {
					bleat_printf( 2, "pf %d tc[%d]: flags=0x%02x min=%d bwg=%d", i, j, port->tc_config[j]->flags, port->tc_config[j]->min_bw,  port->tc2bwg[j] );
				}
			}
		}
	}

	conf->num_ports = pidx;
	rte_spinlock_unlock( &conf->update_lock );
}

/*
	Trapse through the mirror stuff and generate a buffer with statistics.
	Caller must free buffer returned.
*/
static char* gen_mirror_stats( struct sriov_conf_c* conf, int limit ) {
	char* buf;
	char wbuf[128];
	int blen = 0;
	const_str	dir;
	int p;
	int v;
	struct mirror_s* mirror;

	if( (buf = (char*) malloc( sizeof( char ) * 4096 )) == NULL ) {
		return NULL;
	}

	strcpy( buf, "\n" );			// seed with leading newline
	for( p = 0; p < conf->num_ports; p++ ) {
		if( limit >= 0 && conf->ports[p].rte_port_number != limit ) {		// if just displaying one, skip if not match
			continue;
		}

		blen += snprintf( wbuf, sizeof( wbuf ), "port %d has %d mirrors:\n", conf->ports[p].rte_port_number, conf->ports[p].num_mirrors );
		if( blen >= 4080 ) {
			strcat( buf, "<truncated>\n" );
			return buf;				// out of room
		}
		strcat( buf, wbuf );

		for( v = 0; v < MAX_VFS; v++ ) {
			if( (mirror = suss_mirror( conf->ports[p].rte_port_number, v )) != NULL ) {
				if( mirror->target < MAX_VFS ) {		// mirror defined
					switch( mirror->dir ) {
						case MIRROR_IN: dir = "in"; break;
						case MIRROR_OUT: dir = "out"; break;
						case MIRROR_ALL: dir = "all"; break;
						default: dir = "off";
					}

					blen += snprintf( wbuf, sizeof( wbuf ), "  vf %d (%s) ==> vf %d\n", v, dir, mirror->target );
					if( blen >= 4080 ) {
						strcat( buf, "<truncated>\n" );
						return buf;				// out of room
					}
					strcat( buf, wbuf );
				}
			}
		}
	}

	return buf;
}


/*
	Read a mirror request from iplex and update the pf/vf config if it passes 
	vetting.  The request (req) is a string assumed to be of the form:
		<pf> <vf> <state>
	where pf and vf are the respective numbers and state is one of:
		in, out, all, off.
*/
static int vfd_update_mirror( sriov_conf_t* conf, const_str req, char** reason ) {
	struct vf_s*  vf;					// vf block for confirmation that pf/vf is managed
	struct sriov_port_s* pf;			// pf info block (where mirror list is)
	struct mirror_s* mirror;	// mirroring info for the vf
	const_str	msg = NULL;
	char*	raw;				// raw request we can mangle
	char*	tok;
	char*	tok_base = NULL;	// strtok_r() base pointer
	int		vfid = -1;			// the vf id as known to the DPDK environment
	int		pfid = -1;
	int		state = 0;			// return state; 0 == fail
	int		req_dir = MIRROR_OFF;
	int		target;				// target vf for mirrored traffic

	if( conf == NULL ) {
		if( reason != NULL ) {
			*reason = strdup( "no configuration" );
		}
		return 0;
	}

	if( req == NULL ) {
		if( reason != NULL ) {
			*reason = strdup( "no request string" );
		}
		return 0;
	}

	msg = "invalid request string";
	raw = strdup( req );
	if( (tok = strtok_r( raw, " ", &tok_base )) != NULL ) {
		pfid = atoi( tok );

		if( (tok = strtok_r( NULL, " ", &tok_base )) != NULL ) {
			vfid = atoi( tok );

			if( (tok = strtok_r( NULL, " ", &tok_base )) != NULL ) {
				if( (vf = suss_vf( pfid, vfid )) != NULL ) {
					mirror = suss_mirror( pfid, vfid );					// find the mirror block
					pf = suss_port( pfid );

					switch( *tok ) {				// set the direction and fetch pointer at target token
						case 'i':
							req_dir = MIRROR_IN;
							tok = strtok_r( NULL, " ", &tok_base );		// at target
							break;

						case 'o':
							if( strcmp( tok, "out" ) == 0 ) {		// off is the default; only set if out found
								req_dir = MIRROR_OUT;
								tok = strtok_r( NULL, " ", &tok_base );		// at target
							} else {
								if( strcmp( tok, "on" ) == 0 ) {				// not in spec, but we'll treat as all
									req_dir = MIRROR_ALL;
									tok = strtok_r( NULL, " ", &tok_base );		// at target
								}
							}
							break;

						case 'a':
							req_dir = MIRROR_ALL;
							tok = strtok_r( NULL, " ", &tok_base );		// at target
							break;

						default: 
							break;			// for now silently turn off anything that is not valid
					}

					if( req_dir != MIRROR_OFF ) {
						if( tok != NULL ) {										// target supplied (if missing it goes unchagned)
							target = atoi( tok );
							if( target >= 0 && target < MAX_VFS ) {				// must be in range
								mirror->target = target;
								if( mirror->dir == MIRROR_OFF ) {					// if mirror was previously off
									pf->num_mirrors++;
									mirror->id = idm_alloc( conf->mir_id_mgr );		// alloc an unused id value
								}
	
								mirror->dir = req_dir;								// safe to set the direction now
								state = 1;
							} else {
								msg = "target VF number is out of range";
							}
						} else {
							msg = "target VF number not supplied";
						}

					} else {
						state = 1;
						if( mirror->dir != MIRROR_OFF ) {			// mirror for this vf existed
							if( pf->num_mirrors > 0 ) {
								pf->num_mirrors--;
							}
						}											// no harm to turn off if off, so no extra logic

						mirror->dir = req_dir;
					}

					if( state ) {									// all vetted successfully
						bleat_printf( 1, "update mirror:  setting: pf/vf=%d/%d dir=%d target=%d",  pf->rte_port_number, vf->num, req_dir, mirror->target );
						if( set_mirror( pf->rte_port_number, vf->num, mirror->id, mirror->target, mirror->dir ) < 0 ) {		// actually do it
							msg = "unable to update nic with mirror request";
							state = 0;
						} else {
							msg = NULL;
						}
					}

					if( mirror->dir == MIRROR_OFF ) {				// cannot reset target until after call to set_mirror()
						mirror->target = MAX_VFS + 1;				// no target when turning off (target is unsigned, make high)
					}

				} else {
					msg = "vf/pf combination not currently managed";
				}
			}
		}
	}

	free( raw );
	if( msg && reason != NULL ) {
		*reason = strdup( msg );
	}

	return state;
}


/*
	Add one of the virtualisation manager generated configuration files to a global
	config struct passed in.  A small amount of error checking (vf id dup, etc) is
	done, so the return is either 1 for success or 0 for failure. Errno is set only
	if we can't open the file.  If reason is not NULL we'll create a message buffer
	and drop the address there (caller must free).

	Future:
	It would make more sense for the config reader in lib to actually populate the
	actual vf struct rather than having to copy it, but because the port struct
	doesn't have dynamic VF structs (has a hard array), we need to read it into
	a separate location and copy it anyway, so the manual copy, rathter than a
	memcpy() is a minor annoyance.  Ultimately, the port should reference an
	array of pointers, and config should pull directly into a vf_s and if the
	parms are valid, then the pointer added to the list. This would be beneficial
	as the lock would be held for less time.
*/
extern int vfd_add_vf( sriov_conf_t* conf, char* fname, char** reason ) {
	vf_config_t* vfc;					// raw vf config file contents	
	int	i;
	int j;
	int vidx;							// index into the vf array
	int	hole = -1;						// first hole in the list;
	struct sriov_port_s* port = NULL;	// reference to a single port in the config
	struct vf_s*	vf;					// point at the vf we need to fill in
	char mbuf[BUF_1K];					// message buffer if we fail
	int tot_vlans = 0;					// must count vlans and macs to ensure limit not busted
	//int tot_macs = 0;
	float tot_min_rate = 0;
	

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

	if( vfc->pciid == NULL || vfc->vfid < 0 ) {			// this is a parse check so <0 is right; proper range check is later with appropriate msg
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
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
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
			//tot_macs += port->vfs[i].num_macs;
			tot_min_rate += port->vfs[i].min_rate;
		}
	}

	if( hole >= 0 ) {			// set the index into the vf array based on first hole found, or no holes
		vidx = hole;
	} else {
		vidx = i;
	}

	if( vidx >= MAX_VFS || vfc->vfid < 0 || vfc->vfid > 31 ) {				// something is out of range TODO: replace 31 with actual number of VFs?
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

	if( vfc->min_rate + tot_min_rate > 1 ) {	// Rate oversubscription
		snprintf( mbuf, sizeof( mbuf ), "total guaranteed rate exceeds link speed" );
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

	if( vfc->nvlans <= 0 ) {							// must have at least one VLAN defined or bad things happen on the NIC
		snprintf( mbuf, sizeof( mbuf ), "vlan id list is empty; it must contain at least one id" );
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
					snprintf( mbuf, sizeof( mbuf ), "duplicate vlan in list: %d", vfc->vlans[i] );
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

	if( vfc->nmacs > MAX_VF_MACS ) {				// too many mac addresses specified for this (can_add cannot check this until VF/PF is actually added to config)
		snprintf( mbuf, sizeof( mbuf ), "too many mac addresses given: %d > limit of %d", vfc->nmacs, MAX_VF_MACS );
		bleat_printf( 0, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

	for( i = 0; i < vfc->nmacs; i++ ) {				// if a mac is duplicated it will be weeded out when we add
		if( ! can_add_mac( port->rte_port_number, -1, vfc->macs[i] ) ) {			// must pass -1 for vfid as it's not in the config yet
			snprintf( mbuf, sizeof( mbuf ), "mac cannot be added to this port (invalid, inuse, or max exceeded for VF): mac=(%s)", vfc->macs[i] ? vfc->macs[i] : "" );
			bleat_printf( 0, "vf not added: %s", mbuf );
			if( reason ) {
				*reason = strdup( mbuf );
			}
			free_config( vfc );
			return 0;
		}
		bleat_printf( 2, "mac address can be added to config: [%d] (%s)",  i, vfc->macs[i] );
	}

	if( ! (port->flags & PF_OVERSUB) ) {						// if in strict mode, ensure TC amounts can be added to current settings without busting 100% cap
		if( check_qs_oversub( port, vfc->qshare ) != 0 ) {
			snprintf( mbuf, sizeof( mbuf ), "TC percentages cause one or more total allocation to exceed 100%%" );
			bleat_printf( 1, "vf not added: %s", mbuf );
			if( reason ) {
				*reason = strdup( mbuf );
			}
			free_config( vfc );
			return 0;
		}
	}

	if( check_qs_spread( port, vfc->qshare ) != 0 ) {				// ensure that the min-max spread on any TC won't be taken out of bounds
		snprintf( mbuf, sizeof( mbuf ), "min-max spread for one or more TCs would exceed 10x" );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
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

	if( vfc->mirror_dir != MIRROR_OFF ) {
		if( vfc->mirror_target == vfc->vfid ||  vfc->mirror_target < 0 || vfc->mirror_target > port->nvfs_config ) {
			snprintf( mbuf, sizeof( mbuf ), "mirror target is out of range or is the same as this VF (%d): target=%d range=0-%d", (int) vfc->vfid, vfc->mirror_target, port->nvfs_config );
			if( reason ) {
				*reason = strdup( mbuf );
			}
			bleat_printf( 1, "vf not added: %s", mbuf );
			free_config( vfc );
			return 0;
		}

	}

	if( vfc->name == NULL ) {
		vfc->name = strdup( "missing" );
	}

	// -------------------------------------------------------------------------------------------------------------
	// CAUTION: if we fail because of a parm error it MUST happen before here!

	bleat_printf( 2, "vf configuration vet complete for %s", vfc->name );

	// All validation was successful, safe to update the config data
	if( vidx == port->num_vfs ) {		// inserting at end, bump the num we have used
		port->num_vfs++;
	}
	
	rte_spinlock_lock( &conf->update_lock );

	vf = &port->vfs[vidx];						// copy from config data doing any translation needed
	memset( vf, 0, sizeof( *vf ) );				// assume zeroing everything is good
	vf->config_name = strdup( vfc->name );		// hold name for delete
	vf->owner = vfc->owner;
	vf->num = vfc->vfid;
	port->vfs[vidx].last_updated = ADDED;		// signal main code to configure the buggger
	vf->strip_stag = vfc->strip_stag;
	vf->strip_ctag = vfc->strip_ctag;
	vf->insert_stag = vfc->strip_stag;			// both are pulled from same config parm
	vf->insert_ctag = vfc->strip_ctag;			// both are pulled from same config parm
	vf->allow_bcast = vfc->allow_bcast;
	vf->allow_mcast = vfc->allow_mcast;
	vf->allow_un_ucast = vfc->allow_un_ucast;

	port->mirrors[vidx].dir = vfc->mirror_dir;						// mirrors are added to the port list
	if( vfc->mirror_dir != MIRROR_OFF ) {
		port->mirrors[vidx].target = vfc->mirror_target;
		port->mirrors[vidx].id = idm_alloc( conf->mir_id_mgr );		// alloc an unused id value
	} else {
		port->mirrors[vidx].target = MAX_VFS + 1;					// target is unsigned -- make high
	}

	vf->allow_untagged = 0;					// for now these cannot be set by the config file data
	vf->vlan_anti_spoof = 1;
	vf->mac_anti_spoof = get_mac_antispoof( port->rte_port_number );		// value depends on the nic in some cases
	vf->default_mac_set = 0;

	vf->rate = vfc->rate;
	vf->min_rate = vfc->min_rate;
	
	if( vfc->start_cb != NULL ) {
		vf->start_cb = strdup( vfc->start_cb );
	}
	if( vfc->stop_cb != NULL ) {
		vf->stop_cb = strdup( vfc->stop_cb );
	}

	vf->link = 0;							// default if parm missing or mis-set (not fatal)
											// on, off or auto are allowed in config file, default to auto if unrecognised
    if (!stricmp(vfc->link_status, "on")) {
        vf->link = 1;
    } else if (!stricmp(vfc->link_status, "off")) {
        vf->link = -1;
    } else if (!stricmp(vfc->link_status, "auto")) {
        vf->link = 0;
    } else {
        bleat_printf( 1, "link_status not recognised in config: %s; defaulting to auto", vfc->link_status );
    }
	
	for( i = 0; i < vfc->nvlans; i++ ) {
		vf->vlans[i] = vfc->vlans[i];
	}
	vf->num_vlans = vfc->nvlans;

	vf->first_mac = 1;													// if guests pushes a mac, we'll add it to [0] and reset the index
	for( i = 1; i <= vfc->nmacs; i++ ) {								// src is 0 based but vf list is 1 based to allow for easy push if guests sets a default mac
		//strcpy( vf->macs[i], vfc->macs[i-1] );						// length vetted earlier, so this is safe
		add_mac( port->rte_port_number, vf->num, vfc->macs[i-1] );		// this should not fail as we vetted it before
	}

	vf->num_macs = vfc->nmacs;

	for( i = 0; i < MAX_TCS; i++ ) {				// copy in the VF's share of each traffic class (percentage)
		vf->qshares[i] = vfc->qshare[i];
	}

	rte_spinlock_unlock( &conf->update_lock );		// updates finished, safe to release now

	if( reason ) {
		*reason = NULL;								// no reason passed back when successful
	}

	bleat_printf( 2, "VF was added to internal config: %s %s id=%d nm=%d", vfc->name, vfc->pciid, vfc->vfid, vf->num_macs );
	free_config( vfc );
	return 1;
}

/*
	Get a list of all config files and add each one to the current config.
	If one fails, we will generate an error and ignore it. We take the config dir name
	supplied in the main VFd configuration and add "_live" to build the directory 
	of live vf configuration files.  This prevents the virtualisation manager from 
	dropping a few files while we're down which have conflicts/duplications that
	would cause a non-deterministic start state.
*/
extern void vfd_add_all_vfs(  parms_t* parms, sriov_conf_t* conf ) {
	char** flist; 					// list of files to pull in
	int		llen;					// list length
	int		i;
	char	wbuf[2048];				// we'll bang on our 'live' designation to the config dir string in this

	if( parms == NULL || conf == NULL ) {
		bleat_printf( 0, "internal mishap: NULL conf or parms pointer passed to add_all_vfs" );
		return;
	}

	if( snprintf( wbuf, sizeof( wbuf ), "%s_live", parms->config_dir ) >= (int) sizeof( wbuf ) ) {		// create something like /var/lib/vfd/config_live
		bleat_printf( 0, "WRN: cannot construct live directory; work buffer not large enough (%d)", (int) sizeof( wbuf ) );
		return;
	}

	flist = list_files( wbuf, "json", 1, &llen );
	if( flist == NULL || llen <= 0 ) {
		bleat_printf( 1, "zero vf configuration files (*.json) found in %s_live; nothing restored", parms->config_dir );
		free_list( flist, 0 );											// still must free core structure
		return;
	}

	bleat_printf( 1, "adding %d existing vf configuration files to the mix", llen );
	
	for( i = 0; i < llen; i++ ) {
		bleat_printf( 2, "parsing %s", flist[i] );
		if( ! vfd_add_vf( conf, flist[i], NULL ) ) {
			bleat_printf( 0, "add_all_vfs: could not add %s (moved off to %s)", flist[i], parms->config_dir );
			delete_vf_config( flist[i], parms->config_dir );
		}
	}
	
	free_list( flist, llen );
}

/*
	Delete a VF from a port.  We expect the name of a file which we can read the
	parms from and suss out the pciid and the vfid.  Those are used to find the
	info in the global config and render it useless. The delete will be rejected
	if the name in the parm file doesn't match the name we saved when adding the
	configuration.  This is a false sense of security, but was a user request
	so it was added.  The first thing we attempt to do is to remove or rename the 
	config file.  If we can't do that we don't do anything else because we'd give 
	the false sense that it was deleted but on restart we'd recreate it, or worse 
	have a conflict with something that was added. 

	Regardless of the outcome after reading the configuration file, if we can open 
	the file we will delete it.
*/
extern int vfd_del_vf( parms_t* parms, sriov_conf_t* conf, char* fname, char** reason ) {
	vf_config_t* vfc;					// raw vf config file contents	
	int	i;
	int vidx;							// index into the vf array
	struct sriov_port_s* port = NULL;	// reference to a single port in the config
	char mbuf[BUF_1K];					// message buffer if we fail
	unsigned int mblen = BUF_1K - 1;	// length of usable spaece in work buffer
	char*	target_dir = NULL;			// target directory if keep is set

	mbuf[mblen-1] = 0;					// avoid needing a check for each snprintf	

	
	if( parms->delete_keep ) {											// need to keep the old by renaming it with a trailing -
		target_dir = parms->config_dir;									// we'll move files back to this dir
	}
	
	if( conf == NULL || fname == NULL ) {
		bleat_printf( 0, "vfd_del_vf called with nil config or filename pointer" );
		if( reason ) {
			snprintf( mbuf, mblen, "internal mishap: config ptr was nil" );
			*reason = strdup( mbuf );
		}
		return 0;
	}

	if( (vfc = read_config( fname )) == NULL ) {
		snprintf( mbuf, mblen, "unable to read config file: %s: %s", fname, errno > 0 ? strerror( errno ) : "unknown sub-reason" );
		bleat_printf( 1, "vfd_del_vf failed: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		return 0;
	}

	if( vfc->pciid == NULL || vfc->vfid < 0 ) {						// file opened and parsed, but information we need was missing
		snprintf( mbuf, mblen, "invalid configuration contents in file: %s", fname );
		bleat_printf( 1, "no config change related to del request: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		delete_vf_config( fname, target_dir );
		return 0;
	}

	for( i = 0; i < conf->num_ports; i++ ) {						// find the port that this vf is attached to
		if( strcmp( conf->ports[i].pciid, vfc->pciid ) == 0 ) {	// match
			port = &conf->ports[i];
			break;
		}
	}

	if( port == NULL ) {
		snprintf( mbuf, mblen, "%s: could not find port %s in the config", vfc->name, vfc->pciid );
		bleat_printf( 1, "no config change related to del request: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		delete_vf_config( fname, target_dir );
		return 0;
	}

	vidx = -1;
	for( i = 0; i < port->num_vfs; i++ ) {				// suss out the id that is listed
		if( port->vfs[i].num == vfc->vfid ) {			// this is it.
			vidx = i;
			break;
		}
	}

	if( vidx < 0 ) {									//  vf not configured on this port
		snprintf( mbuf, mblen, "%s: vf %d not configured on port %s", vfc->name, vfc->vfid, vfc->pciid );
		bleat_printf( 1, "no config change related to del request: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		delete_vf_config( fname, target_dir );
		return 0;
	}

	if( strcmp( vfc->name, port->vfs[vidx].config_name ) != 0 ) { 				// confirm name in current config matches what we had when we added it
		snprintf( mbuf, mblen, "%s: name in config did not match name given when VF was added: expected %s, found %s", vfc->name,  port->vfs[vidx].config_name, vfc->name );
		bleat_printf( 1, "no config change related to del request: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
		free_config( vfc );
		return 0;
	}

	delete_vf_config( fname, target_dir );

	bleat_printf( 2, "del: config data: name: %s", vfc->name );
	bleat_printf( 2, "del: config data: pciid: %s", vfc->pciid );
	bleat_printf( 2, "del: config data: vfid: %d", vfc->vfid );

	port->vfs[vidx].last_updated = DELETED;			// signal main code to nuke the puppy (vfid stays set so we don't see it as a hole until it's gone)
	
	if( reason ) {
		*reason = NULL;
	}
	bleat_printf( 2, "VF internal config was deleted: %s %s id=%d", vfc->name, vfc->pciid, vfc->vfid );
	free_config( vfc );
	return 1;
}

// ---- request/response functions -----------------------------------------------------------------------------

/*
	Write to an open file des with a simple retry mechanism.  We cannot afford to block forever,
	so we'll try only a few times if we make absolutely no progress.
*/
extern int vfd_write( int fd, const_str buf, int len ) {
	int	tries = 10;				// we'll try for about 2.5 seconds and then we give up
	int	nsent = 0;				// number of bytes actually sent
	int n2send = 0;				// number of bytes left to send


	if( (n2send = len) <= 0 ) {
		bleat_printf( 0, "WARN: response send length invalid: %d", len );
		return 0;
	}

	bleat_printf( 3, "response write starts for %d bytes", len );
	while( n2send > 0 && tries > 0 ) {
		nsent = write( fd, buf, n2send );
		if( nsent < 0 ) {
			if( errno != EAGAIN ) { 					// hard error; quit immediately
				bleat_printf( 0, "WRN: write error attempting %d, wrote only %d bytes: %s", len, len - n2send, strerror( errno ) );
				return -1;
			}

			bleat_printf( 2, "response write would block (will retry ) attempting=%d, prev-written=%d bytes total-desired=%d: %s", n2send, len - n2send, len, strerror( errno ) );
			nsent = 0;									// will soft fail and try up to max
		} 
			
		if( nsent == n2send ) {
			return len;
		}

		if( nsent > 0 ) { 		// something sent, so we assume iplex is actively reading
			bleat_printf( 2, "response write partial:  nsent=%d n2send=%d", nsent, n2send - nsent );
			n2send -= nsent;
			buf += nsent;
		} else {
			tries--;
			usleep(250000);			// .25s
			
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
	non-blocked mode we could hang foever if the requestor dies/aborts.

	To work with remote requests (tokay and containers) the response must contain an action which
	is 'response', and the vfd_rid which was passed in.  This allows a single response pipe to be
	used, and allows for future expansion of other information sent via the pipe, not just responses.
*/
extern void vfd_response( char* rpipe, int state, const_str vfd_rid, const_str msg ) {
	int 	fd;
	char	buf[BUF_1K * 4];
	char*	dmsg;			// duplicate message that we can mutilate
	char*	dptr;			// pointer into dmsg for strtok
	char*	tok;
	const_str	sep = "\n";	// message seperators in the array; lead newline helps with visual alignment which can be important

	if( rpipe == NULL ) {
		bleat_printf( 1, "response: unable to respond, response pipe name is nil" );
		return;
	}

	if( vfd_rid == NULL ) {
		bleat_printf( 1, "response: did not have a vfd_rid to send back" );
		vfd_rid = "not-supplied";
	}

	bleat_printf( 3, "response: opening response pipe: %s", rpipe );
	if( (fd = open( rpipe, O_WRONLY | O_NONBLOCK, 0 )) < 0 ) {
	 	bleat_printf( 0, "unable to deliver response: open failed: %s: %s", rpipe, strerror( errno ) );
		return;
	}

	if( bleat_will_it( 4 ) ) {
		bleat_printf( 4, "sending response: %s(%d) [%d] %s", rpipe, fd, state, msg );
	} else {
		bleat_printf( 2, "sending response: %s(%d) [%d] %d bytes", rpipe, fd, state, strlen( msg ) );
	}

	snprintf( buf, sizeof( buf ), "{ \"action\": \"response\", \"vfd_rid\": \"%s\", \"state\": \"%s\", \"msg\": [", vfd_rid, state ? "ERROR" : "OK" );
	bleat_printf( 3, "response: header: %s", buf );

	if( vfd_write( fd, buf, strlen( buf ) ) > 0 ) {
		if( msg != NULL  && (dmsg = strdup( msg )) != NULL ) {
			dptr = dmsg;
			while( (tok = strtok_r( NULL, "\n", &dptr )) != NULL ) {	//  bloody json doesn't accept strings with newlines, so we build an array; grrr
				snprintf( buf, sizeof( buf ), "%s\"%s\"", sep, tok );
				vfd_write( fd, buf, strlen( buf ) );					// ignore state; we need to close the json regardless
				sep = ",\n";											// after the first we need commas before the next
			}

			free( dmsg );
		}
	}
	
	snprintf( buf, sizeof( buf ), " ] }\n@eom@\n" );				// terminate the the message array, then the json
	vfd_write( fd, buf, strlen( buf ) );
	bleat_printf( 2, "response written to pipe" );			// only if all of message written


	bleat_pop_lvl();			// we assume it was pushed when the request received; we pop it once we respond
	close( fd );
}

/*
	Cleanup a request and free the memory.
*/
extern void vfd_free_request( req_t* req ) {
	if( req->vfd_rid != NULL ) {
		free( req->vfd_rid );
	}

	if( req->resource != NULL ) {
		free( req->resource );
	}
	if( req->resp_fifo != NULL ) {
		free( req->resp_fifo );
	}

	free( req );
}

/*
	Read a request from the fifo, and format it into a request block.
	A pointer to the struct is returned; the caller must use vfd_free_request() to
	properly free it.
*/
extern req_t* vfd_read_request( parms_t* parms ) {
	void*	jblob;				// json parsing stuff
	char*	rbuf;				// raw request buffer from the pipe
	char*	stuff;				// stuff teased out of the json blob
	char*	rid;				// request id we must track for caller
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

	if( (stuff = jw_string( jblob, "action" )) == NULL ) {					// CAUTION: switch below expects stuff to have action
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

	if( (rid = jw_string( jblob, "params.vfd_rid" )) != NULL ) {					// if request id supplied
		req->vfd_rid = strdup( rid );
	}

	switch( *stuff ) {				// we assume compiler builds a jump table which makes it faster than a bunch of nested string compares
		case 'a':
		case 'A':					// assume add until something else starts with a
			req->rtype = RT_ADD;
			break;

		case 'c':					// assume "cpu_alrm_thresh"
			req->rtype = RT_CPU_ALARM;
			break;

		case 'd':
		case 'D':
			if( strcmp( stuff, "dump" ) == 0 ) {
				req->rtype = RT_DUMP;
			} else {
				req->rtype = RT_DEL;
			}
			break;

		case 'm':
			req->rtype = RT_MIRROR;
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
	} else {
		bleat_printf( 1, "no response fifo given in request" );
	}
	
	req->log_level = lvl = jw_missing( jblob, "params.loglevel" ) ? 0 : (int) jw_value( jblob, "params.loglevel" );
	bleat_push_glvl( lvl );					// push the level if greater, else push current so pop won't fail

	free( rbuf );
	jw_nuke( jblob );
	return req;
}

/*
	Fill a buffer with the extended stats for all ports. Caller must free the buffer.
	If memory becomes an issue, this returns NULL to indicate error.
*/
static char* gen_exstats( sriov_conf_t* conf ) {
	char*	xbuf = NULL;							// extended stats from one port
	char*	rbuf = NULL;							// response buffer with all output
	int		rbsize = sizeof( char ) * 1024 * 10;	// amount allocated in rbuf
	int		rbused = 0;								// amount used in the response buffer
	int		xbsize = sizeof( char ) * 1024 * 5;
	int		i;
	int		len;

	if( (rbuf = (char *) malloc( rbsize )) != NULL ) {
		*rbuf = 0;
		if( (xbuf = (char *) malloc( xbsize )) != NULL ) {
			for( i = 0; i < conf->num_ports; i++ ) {
				len = sprintf( xbuf, "\nport %d:\n", i );
				len += port_xstats_display( conf->ports[i].rte_port_number, xbuf + len, xbsize - len );
				if( len + rbused > rbsize ) {
					while( rbused + len < rbsize ) {
						rbsize += rbsize/2;
					}
					if( (rbuf = (char *) realloc( rbuf, rbsize )) == NULL ) {
						bleat_printf( 0, "WARN: unable to get enough memory to display extended stats" );
						return NULL;
					}
				}

				strcat( rbuf, xbuf );
				rbused += len;
			}

			free( xbuf );
		}
	}

	return rbuf;
}

												
/*
	Request interface. Checks the request pipe and handles a reqest. If
	forever is set then this is a black hole (never returns).
	Returns true if it handled a request, false otherwise.
*/
extern int vfd_req_if( parms_t *parms, sriov_conf_t* conf, int forever ) {
	req_t*	req;
	char	mbuf[2048];			// message and work buffer
	char*	buf;				// buffer gnerated by something else
	int		rc = 0;
	char*	reason;
	int		req_handled = 0;

	if( forever ) {
		bleat_printf( 1, "req_if: forever loop entered" );
	}

	memset( mbuf, 0, sizeof( mbuf ) );								// avoid valgrind's kinckers twisting because it's not intiialised
	*mbuf = 0;
	do {
		if( (req = vfd_read_request( parms )) != NULL ) {
			bleat_printf( 3, "got request" );
			req_handled = 1;

			switch( req->rtype ) {
				case RT_PING:
					bleat_printf( 3, "responding to ping" );
					snprintf( mbuf, sizeof( mbuf ), "pong: %s", version );
					vfd_response( req->resp_fifo, RESP_OK, req->vfd_rid, mbuf );
					break;

				case RT_ADD:
					if( strchr( req->resource, '/' ) != NULL ) {									// assume fully qualified if it has a slant
						strcpy( mbuf, req->resource );
					} else {
						snprintf( mbuf, sizeof( mbuf ), "%s/%s", parms->config_dir, req->resource );
					}

					bleat_printf( 2, "adding vf from file: %s", mbuf );
					if( vfd_add_vf( conf, mbuf, &reason ) ) {				// read the config file and add to in mem config if ok
						relocate_vf_config( parms, mbuf, NULL );			// move the config to the live directory on success (nil suffix indicates live dir)
						if( vfd_update_nic( parms, conf ) == 0 ) {			// added to config was good, drive the nic update
							snprintf( mbuf, sizeof( mbuf ), "vf added successfully: %s", req->resource );
							vfd_response( req->resp_fifo, RESP_OK, req->vfd_rid, mbuf );
							bleat_printf( 1, "vf added: %s", mbuf );
						} else {
							// TODO -- must turn the vf off so that another add can be sent without forcing a delete
							// 		update_nic always returns good now, so this waits until it catches errors and returns bad
							snprintf( mbuf, sizeof( mbuf ), "vf add failed: unable to configure the vf for: %s", req->resource );
							vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, mbuf );
							bleat_printf( 1, "vf add failed nic update error" );
						}
					} else {
						relocate_vf_config( parms, mbuf, ".error" );		// move the config file to *.error for debugging, but keep in same directory
						snprintf( mbuf, sizeof( mbuf ), "unable to add vf: %s: %s", req->resource, reason );
						vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, mbuf );
						free( reason );
					}
					if( bleat_will_it( 4 ) ) {					// TODO:  remove after testing
  						dump_sriov_config( conf );
					}
					break;

				case RT_DEL:
					if( strchr( req->resource, '/' ) != NULL ) {									// assume fully qualified if it has a slant
						strcpy( mbuf, req->resource );
					} else {
						snprintf( mbuf, sizeof( mbuf ), "%s_live/%s", parms->config_dir, req->resource );		// if unqualified, assume it's in the live for deletion
					}

					bleat_printf( 1, "deleting vf from file: %s", mbuf );
					if( vfd_del_vf( parms, conf, mbuf, &reason ) ) {		// successfully updated internal struct
						if( vfd_update_nic( parms, conf ) == 0 ) {			// nic update was good too
							snprintf( mbuf, sizeof( mbuf ), "vf deleted successfully: %s", req->resource );
							vfd_response( req->resp_fifo, RESP_OK, req->vfd_rid, mbuf );
							bleat_printf( 1, "vf deleted: %s", mbuf );
						} // TODO need else -- see above
					} else {
						snprintf( mbuf, sizeof( mbuf ), "unable to delete internal config for vf: %s: %s", req->resource, reason );
						vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, mbuf );
						free( reason );
					}
					if( bleat_will_it( 4 ) ) {					// TODO:  remove after testing
  						dump_sriov_config( conf );
					}
					break;

				case RT_DUMP:									// spew everything to the log
					dump_dev_info( conf->num_ports);			// general info about each port
  					dump_sriov_config( conf );					// pf/vf specific info
					vfd_response( req->resp_fifo, RESP_OK, req->vfd_rid, "dump captured in the log" );

					char*	stats_buf;
					if( (stats_buf = (char *) malloc( sizeof( char ) * 10 * 1024 )) != NULL ) {
						if( port_xstats_display( 0, stats_buf, sizeof( char ) * 1024 * 10 ) > 0 ) {
							bleat_printf( 0, "%s", stats_buf );
						}

						free( stats_buf );
					}
					break;

				case RT_MIRROR:
					if( parms->forreal ) {
						if( vfd_update_mirror( conf, req->resource, &reason ) ) {
							snprintf( mbuf, sizeof( mbuf ), "mirror update successful: %s", req->resource );
							vfd_response( req->resp_fifo, RESP_OK, req->vfd_rid, mbuf );
						} else {
							snprintf( mbuf, sizeof( mbuf ), "mirror update failed: %s: %s", req->resource, reason ? reason : "" );
							vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, mbuf );
						}
						bleat_printf( 1, "%s", mbuf );

					} else {
						bleat_printf( 1, "mirror request received, but ignored (forreal is off): %s", req->resource == NULL ? "" : req->resource );
					}
					break;

				case RT_SHOW:
					if( parms->forreal ) {
						if( req->resource == NULL ) {
							vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "unable to generate stats: internal mishap: null resource" );
						} else {
							switch( *req->resource ) {
								case 'a':
									if( strcmp( req->resource, "all" ) == 0 ) {				// dump just the VF information
										if( (buf = gen_stats( conf, !PFS_ONLY, ALL_PFS )) != NULL )  {
											vfd_response( req->resp_fifo, RESP_OK, req->vfd_rid, buf );
											free( buf );
										} else {
											vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "unable to generate stats" );
										}
									} else {
										vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "unrecognised show suboption" );
									}
									break;

								case 'e':
									if( strncmp( req->resource, "ex", 2 ) == 0 ) {							// show extended stats
										buf = gen_exstats( conf );						// create a buffer with stats for all ports
										if( buf != NULL ) {
											vfd_response( req->resp_fifo, RESP_OK, req->vfd_rid, buf );
											free( buf );
										} else {
											vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "unable to generate extended stats" );
										}
									} else {
										vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "unrecognised show suboption" );
									}
									break;

								case 'm':			// show mirrors for a pf
									if( strncmp( req->resource, "mirror", 6 ) == 0 ) {
										if( (buf = gen_mirror_stats( conf, -1 )) != NULL ) {
											vfd_response( req->resp_fifo, RESP_OK, req->vfd_rid, buf );
											free( buf );
										} else {
											vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "unable to generate mirror stats" );
										}
									} else {
										vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "unrecognised show suboption" );
									}
									break;

								case 'p':
									if( strcmp( req->resource, "pfs" ) == 0 ) {								// dump just the PF information (skip vf)
										if( (buf = gen_stats( conf, PFS_ONLY, ALL_PFS )) != NULL )  {
											vfd_response( req->resp_fifo, RESP_OK, req->vfd_rid, buf );
											free( buf );
										} else {
											vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "unable to generate pf stats" );
										}
									} else {
										vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "unrecognised show suboption" );
									}
									break;
								
								default:
									if( isdigit( *req->resource ) ) {						// dump just for the indicated pf
										if( (buf = gen_stats( conf, !PFS_ONLY, atoi( req->resource ) )) != NULL )  {
											vfd_response( req->resp_fifo, RESP_OK, req->vfd_rid, buf );
											free( buf );
										} else {
											vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "unable to generate pf stats" );
										}
									} else {												// assume we dump for all
										if( req->resource ) {
											bleat_printf( 2, "show: unknown target supplied: %s", req->resource );
										}
										vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, 
												"unable to generate stats: unnown target supplied (not one of all, pfs, extended or pf-number)" );
									}
							}
						}
					} else {
						vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "VFD running in 'no harm' (-n) mode; no stats available." );
					}
					break;

				case RT_CPU_ALARM:
						if( req->resource != NULL ) {
							if( strchr( req->resource, '%' ) ) {				// allow 30% or .30
								parms->cpu_alrm_thresh = (double) atoi( req->resource ) / 100.0;
							} else {
								parms->cpu_alrm_thresh = strtod( req->resource, NULL );
							}
							if( parms->cpu_alrm_thresh < 0.05 ) {
								parms->cpu_alrm_thresh = 0.05;			// enforce sanity (no upper limit enforced allowing it to be set off with high value)
							}

							bleat_printf( 1, "cpu alarm threshold changed to %d%%", (int) (parms->cpu_alrm_thresh  * 100) );
							snprintf( mbuf, sizeof( mbuf ), "cpu alarm threshold changed to: %d%%", (int) (parms->cpu_alrm_thresh * 100) );
						} else {
							rc = 1;
							snprintf( mbuf, sizeof( mbuf ), "cpu alarm threshold not changed to: bad or missing value" );
						}

						vfd_response( req->resp_fifo, rc, req->vfd_rid, mbuf );
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

					vfd_response( req->resp_fifo, rc, req->vfd_rid, mbuf );
					break;
					

				default:
					vfd_response( req->resp_fifo, RESP_ERROR, req->vfd_rid, "dummy request handler: urrecognised request." );
					break;
			}

			vfd_free_request( req );
		}
		
		if( forever )
			sleep( 1 );
	} while( forever );

	return req_handled;			// true if we did something -- more frequent recall if we did
}
