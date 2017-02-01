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
	parms->rfifo = rfifo_create( parms->fifo_path, 0666 );		//TODO -- set mode more sanely, but this runs as root, so regular users need to write to this thus open wide for now
	if( parms->rfifo == NULL ) {
		bleat_printf( 0, "ERR: unable to create request fifo (%s): %s", parms->fifo_path, strerror( errno ) );
		return -1;
	} else {
		bleat_printf( 0, "listening for requests via pipe: %s", parms->fifo_path );
	}

	return 0;
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
		if( max[i] / min[i]  > 10 ) {
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
			for( j = i; j < port->num_vfs; j++ ) {
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
	if( called )
		return;
	called = 1;
	
	for( i = 0; pidx < MAX_PORTS  && i < parms->npciids; i++, pidx++ ) {
		pfc = &parms->pciids[i];					// point at the pf's configuration info

		port = &conf->ports[pidx];
		port->flags = 0;
		port->last_updated = ADDED;												// flag newly added so the nic is configured next go round
		snprintf( port->name, sizeof( port->name ), "port-%d",  i);				// TODO--- support getting a name from the config
		snprintf( port->pciid, sizeof( port->pciid ), "%s", pfc->id );
		port->mtu = pfc->mtu;

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

	if( ! (port->flags & PF_OVERSUB) ) {						// if in strict mode, ensure TC amounts can be added to current settings without busting 100% cap
		if( check_qs_oversub( port, vfc->qshare ) != 0 ) {
			snprintf( mbuf, sizeof( mbuf ), "TC percentages cause one or more total allocation to exceed 100%%" );
			bleat_printf( 1, "vf not added: %s", mbuf );
			if( reason ) {
				*reason = strdup( mbuf );
			}
			return 0;
		}
	}

	if( check_qs_spread( port, vfc->qshare ) != 0 ) {				// ensure that the min-max spread on any TC won't be taken out of bounds
		snprintf( mbuf, sizeof( mbuf ), "min-max spread for one or more TCs would exceed 10x" );
		bleat_printf( 1, "vf not added: %s", mbuf );
		if( reason ) {
			*reason = strdup( mbuf );
		}
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

	// -------------------------------------------------------------------------------------------------------------
	// CAUTION: if we fail because of a parm error it MUST happen before here!

	// All validation was successful, safe to update the config data
	if( vidx == port->num_vfs ) {		// inserting at end, bump the num we have used
		port->num_vfs++;
	}
	
	rte_spinlock_lock( &conf->update_lock );

	vf = &port->vfs[vidx];						// copy from config data doing any translation needed
	memset( vf, 0, sizeof( *vf ) );				// assume zeroing everything is good
	vf->owner = vfc->owner;
	vf->num = vfc->vfid;
	port->vfs[vidx].last_updated = ADDED;		// signal main code to configure the buggger
	vf->strip_stag = vfc->strip_stag;
	vf->insert_stag = vfc->strip_stag;			// both are pulled from same config parm
	vf->allow_bcast = vfc->allow_bcast;
	vf->allow_mcast = vfc->allow_mcast;
	vf->allow_un_ucast = vfc->allow_un_ucast;

	vf->allow_untagged = 0;					// for now these cannot be set by the config file data
	vf->vlan_anti_spoof = 1;
	vf->mac_anti_spoof = 1;

	vf->rate = vfc->rate;
	
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

	for( i = 0; i < vfc->nmacs; i++ ) {
		strcpy( vf->macs[i], vfc->macs[i] );		// we vet for length earlier, so this is safe.
	}
	vf->num_macs = vfc->nmacs;

	for( i = 0; i < MAX_TCS; i++ ) {				// copy in the VF's share of each traffic class (percentage)
		vf->qshares[i] = vfc->qshare[i];
	}

	rte_spinlock_unlock( &conf->update_lock );		// updates finished, safe to release now

	if( reason ) {
		*reason = NULL;								// no reason passed back when successful
	}

	bleat_printf( 2, "VF was added: %s %s id=%d", vfc->name, vfc->pciid, vfc->vfid );
	free_config( vfc );
	return 1;
}

/*
	Get a list of all config files and add each one to the current config.
	If one fails, we will generate an error and ignore it.
*/
extern void vfd_add_all_vfs(  parms_t* parms, sriov_conf_t* conf ) {
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
extern int vfd_del_vf( parms_t* parms, sriov_conf_t* conf, char* fname, char** reason ) {
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
extern int vfd_write( int fd, const char* buf, int len ) {
	int	tries = 5;				// if we have this number of times where there is no progress we give up
	int	nsent;					// number of bytes actually sent
	int n2send;					// number of bytes left to send


	if( (n2send = len) <= 0 ) {
		bleat_printf( 0, "WARN: response send length invalid: %d", len );
		return 0;
	}

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
extern void vfd_response( char* rpipe, int state, const char* msg ) {
	int 	fd;
	char	buf[BUF_1K];

	if( rpipe == NULL ) {
		return;
	}

	if( (fd = open( rpipe, O_WRONLY | O_NONBLOCK, 0 )) < 0 ) {
	 	bleat_printf( 0, "unable to deliver response: open failed: %s: %s", rpipe, strerror( errno ) );
		return;
	}

	if( bleat_will_it( 4 ) ) {
		bleat_printf( 4, "sending response: %s(%d) [%d] %s", rpipe, fd, state, msg );
	} else {
		bleat_printf( 2, "sending response: %s(%d) [%d] %d bytes", rpipe, fd, state, strlen( msg ) );
	}

	snprintf( buf, sizeof( buf ), "{ \"state\": \"%s\", \"msg\": \"", state ? "ERROR" : "OK" );
	if ( vfd_write( fd, buf, strlen( buf ) ) > 0 ) {
		if ( msg != NULL ) {
			vfd_write( fd, msg, strlen( msg ) );				// ignore state; we need to close the json regardless
		}

		snprintf( buf, sizeof( buf ), "\" }\n" );				// terminate the json
		vfd_write( fd, buf, strlen( buf ) );
		bleat_printf( 2, "response written to pipe" );			// only if all of message written
	}

	bleat_pop_lvl();			// we assume it was pushed when the request received; we pop it once we respond
	close( fd );
}

/*
	Cleanup a request and free the memory.
*/
extern void vfd_free_request( req_t* req ) {
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
extern req_t* vfd_read_request( parms_t* parms ) {
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
  						dump_sriov_config( conf );
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
  						dump_sriov_config( conf );
					}
					break;

				case RT_DUMP:									// spew everything to the log
					dump_dev_info( conf->num_ports);			// general info about each port
  					dump_sriov_config( conf );					// pf/vf specific info
					vfd_response( req->resp_fifo, 0, "dump captured in the log" );

					char*	stats_buf;
					if( (stats_buf = (char *) malloc( sizeof( char ) * 10 * 1024 )) != NULL ) {
						if( port_xstats_display( 0, stats_buf, sizeof( char ) * 1024 * 10 ) > 0 ) {
							bleat_printf( 0, "%s", stats_buf );
						}
					}
					break;

				case RT_SHOW:
					if( parms->forreal ) {
						if( req->resource == NULL ) {
							vfd_response( req->resp_fifo, 1, "unable to generate stats: internal mishap: null resource" );
						} else {
							switch( *req->resource ) {
								case 'a':
									if( strcmp( req->resource, "all" ) == 0 ) {				// dump just the VF information
										if( (buf = gen_stats( conf, !PFS_ONLY, ALL_PFS )) != NULL )  {
											vfd_response( req->resp_fifo, 0, buf );
											free( buf );
										} else {
											vfd_response( req->resp_fifo, 1, "unable to generate stats" );
										}
									}
									break;

								case 'e':
									if( strncmp( req->resource, "ex", 2 ) == 0 ) {							// show extended stats
										buf = gen_exstats( conf );						// create a buffer with stats for all ports
										if( buf != NULL ) {
											vfd_response( req->resp_fifo, 0, buf );
											free( buf );
										} else {
											vfd_response( req->resp_fifo, 1, "unable to generate extended stats" );
										}
									}
									break;

								case 'p':
									if( strcmp( req->resource, "pfs" ) == 0 ) {								// dump just the PF information (skip vf)
										if( (buf = gen_stats( conf, PFS_ONLY, ALL_PFS )) != NULL )  {
											vfd_response( req->resp_fifo, 0, buf );
											free( buf );
										} else {
											vfd_response( req->resp_fifo, 1, "unable to generate pf stats" );
										}
									}
										break;
								
								default:
									if( isdigit( *req->resource ) ) {						// dump just for the indicated pf
										if( (buf = gen_stats( conf, !PFS_ONLY, atoi( req->resource ) )) != NULL )  {
											vfd_response( req->resp_fifo, 0, buf );
											free( buf );
										} else {
											vfd_response( req->resp_fifo, 1, "unable to generate pf stats" );
										}
									} else {												// assume we dump for all
										if( req->resource ) {
											bleat_printf( 2, "show: unknown target supplied: %s", req->resource );
										}
										vfd_response( req->resp_fifo, 1, "unable to generate stats: unnown target supplied (not one of all, pfs, extended or pf-number)" );
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
