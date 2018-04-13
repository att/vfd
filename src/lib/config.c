// :vi noet tw=4 ts=4:
/*
	Mnemonic:	config.c
	Abstract:	Functions to read and parse the various config files.
	Author:		E. Scott Daniels
	Date:		26 Feb 2016

	Mods:		10 Mar 2016 : Added support for additional parm file values.
				01 Apr 2016 : Support variable mtu for each pciid in the parm file.
				10 May 2016 : Add keep boolien from main config.
				13 Jun 2016 : Changes to allow the more fine graned primative type
					checking in jwrapper to be used.
				16 Jun 2016 : Add option to allow loop-back.
				18 Oct 2016 : Add chenges to support new QoS entries.
				29 Nov 2016 : Added changes to support queue share in vf config.
				11 Feb 2017 : Fix issues with leading spaces rather than tabs (formatting)
				26 May 2017 : Allow promisc to be set (default is true to match original behavour)
				08 Jun 2017 : Allow huge_pages to be set (defult is on)
				10 Jul 2017 : We now support "mac": "addr" rather than an array.
				07 Feb 2018 : Add memory support back.
				14 Feb 2018 : Add default for vf config name.
				13 Apr 2018 : Add cpu alarm threshold to the config.

	TODO:		convert things to the new jw_xapi functions to make for easier to read code.
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "vfdlib.h"

// -------------------------------------------------------------------------------------
// safe free (free shouldn't balk on nil, but don't chance it)
#define SFREE(p) if((p)){free(p);}			

// Ensure low <= v <= high and return v == low if below or v == high if v is over.
#define IBOUND(v,low,high) ((v) < (low) ? (low) : ((v) > (high) ? (high) : (v)))

// --------------------------- utility   --------------------------------------------------------------
/*
	Trim leading spaces.  If the resulting string is completely empty NULL
	is returned, else a pointer to a trimmed string is returned. The original
	is unharmed.
*/
static char* ltrim( char* orig ) {
	char*	ch;			// pointer into buffer

	if( ! orig || !(*orig) ) {
		return NULL;
	}

	for( ch = orig; *ch && isspace( *ch ); ch++ );
	if( *ch ) {
		return strdup( ch );
	}

	return NULL;
}

// ---- vfd configuration (parms) ------------------------------------------------------

/*
	Read the json from the file (e.g. /etc/vfd/vfd.cfg).
	Returns a pointer to a struct  or nil if error.
*/

/*
	Read an entire file into a buffer. We assume for config files
	they will be smallish and so this won't be a problem.
	Returns a pointer to the buffer, or NULL. Caller must free.
	Terminates the buffer with a nil character for string processing.


	If uid is not a nil pointer, then the user number of the owner of the file
	is returned to the caller via this pointer.

	If we cannot stat the file, we assume it's empty or missing and return
	an empty buffer, as opposed to a NULL, so the caller can generate defaults
	or error if an empty/missing file isn't tolerated.
*/
static char* file_into_buf( char* fname, uid_t* uid ) {
	struct stat	stats;
	off_t		fsize = 8192;	// size of the file
	off_t		nread;			// number of bytes read
	int			fd;
	char*		buf;			// input buffer
	
	if( uid != NULL ) {
		*uid = -1;				// invalid to begin with
	}
	
	if( (fd = open( fname, O_RDONLY )) >= 0 ) {
		if( fstat( fd, &stats ) >= 0 ) {
			if( stats.st_size <= 0 ) {					// empty file
				close( fd );
				fd = -1;
			} else {
				fsize = stats.st_size;						// stat ok, save the file size
				if( uid != NULL ) {
					*uid = stats.st_uid;					// pass back the user id
				}
			}
		} else {
			fsize = 8192; 								// stat failed, we'll leave the file open and try to read a default max of 8k
		}
	}

	if( fd < 0 ) {											// didn't open or empty
		if( (buf = (char *) malloc( sizeof( char ) * 128 )) == NULL ) {
			return NULL;
		}

		*buf = 0;
		return buf;
	}

	if( (buf = (char *) malloc( sizeof( char ) * fsize + 2 )) == NULL ) {
		close( fd );
		errno = ENOMEM;
		return NULL;
	}

	nread = read( fd, buf, fsize );
	if( nread < 0 || nread > fsize ) {							// too much or two little
		errno = EFBIG;											// likely too much to handle
		close( fd );
		return NULL;
	}

	buf[nread] = 0;

	close( fd );
	return buf;
}

/*
	Open the file, and read the json there returning a populated structure from
	the json bits we expect to find.

	Primative type checking is done, and if the expected value for a field isn't the expected
	type, then the default value is generally used.  For example, if the field 'stuff' should 
	be a boolean, but "stuff": goo  is given in the json, the result is a bad type for 'stuff' 
	and the default value is used. This keeps our code a bit more simple and puts the 
	responsibility of getting the json correct on the 'user'. In some cases the default value 
	is a bad value which might trigger an error in the code which is making use of this library.

	Primative types are those types returned by the json parser as value, boolean or NULL.
*/
extern parms_t* read_parms( char* fname ) {
	parms_t*	parms = NULL;
	void*		jblob;			// parsed json
	void*		pobj;			// parsed sub object
	char*		buf;			// buffer read from file (nil terminated)
	char*		stuff;
	char		sm_wrk[128];	// small work buffer
	int			i, j, k;
	int			def_mtu;		// default mtu (pulled and used to set pciid struct, but not kept in parms
	tc_class_t* tc_class_ptr;	// ponter to heap location
	int		 priority;	   // hold priority read from tclasses object
	void*	   tcobj;
	void*	   bwgrpobj;

	if( (buf = file_into_buf( fname, NULL )) == NULL ) {
		return NULL;
	}

	if( *buf == 0 ) {											// empty/missing file
		free( buf );
		buf = strdup( "{ \"empty\": true }" );					// dummy json to parse which will cause all defaults to be set
	}

	if( (jblob = jw_new( buf )) != NULL ) {						// json successfully parsed
		if( (parms = (parms_t *) malloc( sizeof( *parms ) )) == NULL ) {
			errno = ENOMEM;
			return NULL;
		}
		memset( parms, 0, sizeof( *parms ) );					// probably not needed, but we don't do this frequently enough to worry

		parms->dpdk_log_level = !jw_is_value( jblob, "dpdk_log_level" ) ? 0 : (int) jw_value( jblob, "dpdk_log_level" );
		parms->dpdk_init_log_level = !jw_is_value( jblob, "dpdk_init_log_level" ) ? 0 : (int) jw_value( jblob, "dpdk_init_log_level" );
		parms->log_level = !jw_is_value( jblob, "log_level" ) ? 0 : (int) jw_value( jblob, "log_level" );
		parms->init_log_level = !jw_is_value( jblob, "init_log_level" ) ? 1 : (int) jw_value( jblob, "init_log_level" );
		parms->log_keep = !jw_is_value( jblob, "log_keep" ) ? 30 : (int) jw_value( jblob, "log_keep" );
		parms->delete_keep = !jw_is_bool( jblob, "delete_keep" ) ? 0 : (int) jw_value( jblob, "delete_keep" );

		parms->cpu_alrm_thresh = 0.10;										// default to 10%
		if( jw_is_value( jblob, "cpu_alarm" ) ) {							// we allow real float value e.g. 1.05 == 105%, or string
			parms->cpu_alrm_thresh = (double) jw_value( jblob, "cpu_alarm" );
		} else {
			if( (stuff = jw_string( jblob, "cpu_alarm" )) != NULL ) {		// assume something like "30%" or just "30"
				k = atoi( stuff );
				parms->cpu_alrm_thresh = (double) k / 100.0;
			}
		}
		if( parms->cpu_alrm_thresh < 0.05 ) {
			parms->cpu_alrm_thresh = .05;				// enforce some level of sanity
		}

		if( (stuff = jw_string( jblob, "cpu_alarm_type" )) != NULL ) {		// default to "WRN:" but allow them to change to CRI or something else
			parms->cpu_alrm_type = strdup( stuff );
		} else {
			parms->cpu_alrm_type = strdup( "WRN:" );
		}
			
		
		if( jw_is_bool( jblob, "enable_qos" ) ) {
			if( jw_value( jblob, "enable_qos" ) ) {
				parms->rflags |= RF_ENABLE_QOS;
			}
		}

		if( jw_is_bool( jblob, "huge_pages" ) ) {				// we enable huge pages by default, flag in parms is to disable (no_huge) set when this is false
			if( ! jw_value( jblob, "huge_pages" ) ) {
				parms->rflags |= RF_NO_HUGE;
			}
		}

		if( jw_is_bool( jblob, "enable_flowcontrol" ) ) {
			if( jw_value( jblob, "enable_flowcontrol" ) ) {
				parms->rflags |= RF_ENABLE_FC;
			}
		}

		if( jw_missing( jblob, "default_mtu" ) ) {			// could be an old install using deprecated mtu, so look for that and default if neither is there
			def_mtu = jw_missing( jblob, "mtu" ) ? 9420 : (int) jw_value( jblob, "mtu" );
		} else {
 		 	def_mtu = (int) jw_value( jblob, "default_mtu" );
		}

		if(  (stuff = jw_string( jblob, "config_dir" )) ) {
			parms->config_dir = ltrim( stuff );
		} else {
			parms->config_dir = strdup( "/var/lib/vfd/config" );
		}

		if(  (stuff = jw_string( jblob, "pid_fname" )) ) {
			parms->pid_fname = ltrim( stuff );
		} else {
			parms->pid_fname = strdup( "/var/run/vfd.pid" );
		}

		if(  (stuff = jw_string( jblob, "stats_path" )) ) {
			parms->stats_path = ltrim( stuff );
		} else {
			parms->stats_path = strdup( "/var/lib/vfd/stats" );
		}

		if(  (stuff = jw_string( jblob, "fifo" )) ) {
			parms->fifo_path = ltrim( stuff );
		} else {
			parms->fifo_path = strdup( "/var/lib/vfd/request" );
		}

		if(  (stuff = jw_string( jblob, "log_dir" )) ) {
			parms->log_dir = ltrim( stuff );
		} else {
			parms->log_dir = strdup( "/var/log/vfd" );
		}

		if( (stuff = jw_string( jblob, "cpu_mask" )) ) {
			parms->cpu_mask = ltrim( stuff );
		}
		
		parms->numa_mem = jwx_get_value_as_str( jblob, "numa_mem", "64,64", JWFMT_INT );

		if( (parms->npciids = jw_array_len( jblob, "pciids" )) > 0 ) {			// pick up the list of pciids
			if( (parms->pciids = (pfdef_t *) malloc( sizeof( *parms->pciids ) * parms->npciids )) == NULL ) {
				errno = ENOMEM;
				jw_nuke( jblob );
				free_parms( parms );
				return NULL;
			}
			memset( parms->pciids, 0, sizeof( *parms->pciids ) * parms->npciids );

			if( parms->pciids != NULL ) {
				for( i = 0; i < parms->npciids; i++ ) {
					stuff = (char *) jw_string_ele( jblob, "pciids", i );
					if( stuff != NULL ) {										// string, use default mtu
						parms->pciids[i].id = ltrim( stuff );
						parms->pciids[i].mtu = def_mtu;
						parms->pciids[i].flags &= ~PFF_LOOP_BACK;
						parms->pciids[i].flags |= PFF_PROMISC;					// this defaults to on to be consistent with original version
					} else {
						if( (pobj = jw_obj_ele( jblob, "pciids", i )) != NULL ) {		// full pciid object -- take values from it
							int jntcs;				// number of tc objects in the json

							if( (stuff = jw_string( pobj, "id" )) == NULL ) {
								snprintf( sm_wrk, sizeof( sm_wrk ),  "missing-id" );
								stuff = sm_wrk;
							}
							parms->pciids[i].id = ltrim( stuff );

							parms->pciids[i].mtu = !jw_is_value( pobj, "mtu" ) ? def_mtu : (int) jw_value( pobj, "mtu" );
							parms->pciids[i].hw_strip_crc = jwx_get_bool( pobj, "hw_strip_crc", 1 );		// strip on by default
							if( jwx_get_bool( pobj, "promiscuous", 0 ) ) {									// set promisc; default is off
								parms->pciids[i].flags |= PFF_PROMISC;
							} else {
								parms->pciids[i].flags &= ~PFF_PROMISC;
							}

							if( !jw_is_bool( pobj, "enable_loopback" ) ? 0 : (int) jw_value( pobj, "enable_loopback" ) ) {		
								parms->pciids[i].flags |= PFF_LOOP_BACK; 			// default to false if not there
							} else {
								parms->pciids[i].flags &= ~PFF_LOOP_BACK;			// disable if set to false
							}
							if( !jw_is_bool( pobj, "vf_oversubscription" ) ? 0 : (int) jw_value( pobj,  "vf_oversubscription" ) ) {	
								parms->pciids[i].flags |= PFF_VF_OVERSUB; 			// default to false if not there
							} else {
								parms->pciids[i].flags &= ~PFF_VF_OVERSUB;		  // disable if set to false
							}
							
							parms->pciids[i].ntcs = 4;							// default to 4 and we will up to 8 if we see pri > 3
							if( (jntcs = jw_array_len( pobj, "tclasses" )) > 0 ) {					  				// number of tcs supplied in the json
								//fprintf( stderr, "parsing tclasses = %d\n", parms->pciids[i].ntcs);  // **Debugging purpose only
								if( (tc_class_ptr = (tc_class_t*) malloc (sizeof(tc_class_t) * MAX_TCS)) == NULL ) {	// allways allocate a full set
									errno = ENOMEM;
									jw_nuke( jblob );
									free_parms( parms );
									return NULL;
								}
								memset( tc_class_ptr, 0, sizeof(tc_class_t) * MAX_TCS );
								parms->pciids[i].tcs[0] = tc_class_ptr;			// dont chance that pri == 0 is always there; this ensures us a ptr to free

								for( j = 0; j < jntcs; j++ ) {
									if( (tcobj = jw_obj_ele( pobj, "tclasses", j )) != NULL ) {					// pull out the next element
										priority = (int) jw_value( tcobj, "pri" );
										if( priority < 0 || priority >= MAX_TCS ) {								// don't allow priority out of range
											continue;
										}

										if( priority > 3 ) {
											parms->pciids[i].ntcs = 8;
										}

										parms->pciids[i].tcs[priority] = tc_class_ptr + priority;			// use priority as index into the block allocated

										if( (stuff = jw_string( tcobj, "name" )) == NULL ) {
											snprintf( sm_wrk, sizeof( sm_wrk ), "TC-%d", priority );
											stuff = sm_wrk;
										} 
										parms->pciids[i].tcs[priority]->hr_name = ltrim( stuff );

										if( !jw_is_bool( tcobj, "llatency" ) ? 0 : (int) jw_value( tcobj, "llatency" ) ) {
											parms->pciids[i].tcs[priority]->flags |= TCF_LOW_LATENCY;
										} else {
											parms->pciids[i].tcs[priority]->flags &= ~TCF_LOW_LATENCY;
										}
										if( !jw_is_bool( tcobj, "lsp" ) ? 0 : (int) jw_value( tcobj, "lsp" ) ) {
											parms->pciids[i].tcs[priority]->flags |= TCF_LNK_STRICTP;
										} else {
											parms->pciids[i].tcs[priority]->flags &= ~TCF_LNK_STRICTP;
										}
										if( !jw_is_bool( tcobj, "bsp" ) ? 0 : (int) jw_value( tcobj, "bsp" ) ) {
											parms->pciids[i].tcs[priority]->flags |= TCF_BW_STRICTP;
										} else {
											parms->pciids[i].tcs[priority]->flags &= ~TCF_BW_STRICTP;
										}
										parms->pciids[i].tcs[priority]->max_bw = !jw_is_value( tcobj, "max_bw" ) ? 100 : IBOUND( (int)jw_value( tcobj, "max_bw" ), 1, 100 );
										parms->pciids[i].tcs[priority]->min_bw = !jw_is_value( tcobj, "min_bw" ) ? 1 : IBOUND( (int)jw_value( tcobj, "min_bw" ), 1, 100 );
									} else {
										fprintf( stderr, "internal mishap parsing tclasses from config file j=%d expected=%d\n", j, parms->pciids[i].ntcs );
										jw_nuke( jblob );
										free_parms( parms );
										return NULL;
									}
								}

							}
							if (( bwgrpobj = jw_blob( pobj, "bw_grps" )) != NULL) {
								for ( j = 0; j < sizeof(parms->pciids[i].bw_grps)/sizeof(bw_grp_t); j++ ) {
									sprintf(stuff, "bwg%d", j);
									if( jw_exists(bwgrpobj, stuff) && (parms->pciids[i].bw_grps[j].ntcs  = jw_array_len( bwgrpobj, stuff )) > 0 ) {
										for( k = 0; k < parms->pciids[i].bw_grps[j].ntcs; k++ ) {
											parms->pciids[i].bw_grps[j].tcs[k] = (int) jw_value_ele( bwgrpobj, stuff, k );
										}
									}
								}
							}
						}
					}

					if(  parms->pciids[i].mtu > 9420 ) {
						 parms->pciids[i].mtu = 9420;		// niantic has issues with packets > 9.5K when loopback is enabled, so cap here
					}
				}
			} else {
				parms->npciids = 0;			// memory failure; return zip
			}
		} else {
			parms->npciids = 0;				// could be set to neg value as a return length; ensure 0 if missing or none
		}

		jw_nuke( jblob );
	} else {
		fprintf( stderr, "internal mishap parsing json blob\n" );
	}

	free( buf );
	return parms;
}

/*
	Cleanup a parm block and free the data.
*/
extern void free_parms( parms_t* parms ) {
	int i;

	if( ! parms ) {
		return;
	}

	for( i = 0; i < parms->npciids; i++ ) {
		SFREE( parms->pciids[i].tcs[0] );			// all of the blocks are allocated in one hunk
	}

	SFREE( parms->log_dir );
	SFREE( parms->fifo_path );
	SFREE( parms->config_dir );
	SFREE( parms->pciids );
	SFREE( parms->pid_fname );
	SFREE( parms->stats_path );
	SFREE( parms->numa_mem );

	free( parms );
}


// --------------------------- vf config --------------------------------------------------------------
/*
	Open and read a VF config file returning a struct with the information populated
	and defaults in places where the information was omitted.
*/
extern vf_config_t*	read_config( char* fname ) {
	vf_config_t*	vfc = NULL;
	void*		jblob;			// parsed json
	void*		mirror;			// mirror blob in the tree
	char*		buf;			// buffer read from file (nil terminated)
	char*		stuff;
	int			val;
	int			i;
	uid_t		uid;
	int			jnqueues;		// number of queue definitions in the json
	void*		qobj;			// pointer to the queue object in the json

	if( (buf = file_into_buf( fname, &uid )) == NULL ) {
		return NULL;
	}

	if( *buf == 0 ) {											// empty/missing file, an error in this situation because not everything has a default
		free( buf );
		return NULL;
	}

	if( (jblob = jw_new( buf )) != NULL ) {						// json successfully parsed
		if( (vfc = (vf_config_t *) malloc( sizeof( *vfc ) )) == NULL ) {
			errno = ENOMEM;
			return NULL;
		}

		memset( vfc, 0, sizeof( *vfc ) );						// pointers default to nil


		vfc->owner = uid;
		vfc->antispoof_mac = 1;				
		vfc->antispoof_vlan = 1;	// these are forced to 1 regardless of what was in json

		vfc->antispoof_mac = jw_missing( jblob, "mac_anti_spoof" ) ? 0 : (int) jw_value( jblob, "mac_anti_spoof" );
		vfc->antispoof_vlan = jw_missing( jblob, "vlan_anti_spoof" ) ? 0 : (int) jw_value( jblob, "vlan_anti_spoof" );

		vfc->allow_untagged = !jw_is_bool( jblob, "allow_untagged" ) ? 0 : (int) jw_value( jblob, "allow_untagged" );

		vfc->strip_stag = !jw_is_bool( jblob, "strip_stag" ) ? 0 : (int) jw_value( jblob, "strip_stag" );
		vfc->strip_ctag = !jw_is_bool( jblob, "strip_ctag" ) ? 0 : (int) jw_value( jblob, "strip_ctag" );
		vfc->allow_bcast = !jw_is_bool( jblob, "allow_bcast" ) ? 1 : (int) jw_value( jblob, "allow_bcast" );
		vfc->allow_mcast = !jw_is_bool( jblob, "allow_mcast" ) ? 1 : (int) jw_value( jblob, "allow_mcast" );
		vfc->allow_un_ucast = !jw_is_bool( jblob, "allow_un_ucast" ) ? 0 : (int) jw_value( jblob, "allow_un_ucast" );
		vfc->vfid = !jw_is_value( jblob, "vfid" ) ? -1 : (int) jw_value( jblob, "vfid" );			// there is no real default value, so set to invalid

		vfc->rate = jw_missing( jblob, "rate" ) ? 0 : (float) jw_value( jblob, "rate" );
		vfc->min_rate = jw_missing( jblob, "min_rate" ) ? 0 : (float) jw_value( jblob, "min_rate" );

		if(  (stuff = jw_string( jblob, "name" )) ) {
			vfc->name = strdup( stuff );
		} else {
			vfc->name = strdup( "unnamed" );
		}

		if(  (stuff = jw_string( jblob, "pciid" )) ) {
			vfc->pciid = ltrim( stuff );
		}

		if(  (stuff = jw_string( jblob, "stop_cb" )) ) {					// command that is executed on owner's behalf as we shutdown
			vfc->stop_cb = ltrim( stuff );
		}
		if(  (stuff = jw_string( jblob, "start_cb" )) ) {					// command that is executed on owner's behalf as we start (last part of init)
			vfc->start_cb = ltrim( stuff );
		}

		if(  (stuff = jw_string( jblob, "link_status" )) ) {
			vfc->link_status = ltrim( stuff );
		} else {
			vfc->link_status = strdup( "auto" );
		}

		if(  (stuff = jw_string( jblob, "vm_mac" )) ) {
			vfc->vm_mac = ltrim( stuff );
		}
	
		if( (vfc->nvlans = jw_array_len( jblob, "vlans" )) > 0 ) {						// pick up values from the json array
			vfc->vlans = malloc( sizeof( *vfc->vlans ) * vfc->nvlans );
			if( vfc->vlans != NULL ) {
				for( i = 0; i < vfc->nvlans; i++ ) {
					if( jw_is_value_ele( jblob, "vlans", i ) ) {
						vfc->vlans[i] = (int) jw_value_ele( jblob, "vlans", i );
					} else {
						vfc->vlans[i] = -1;												// vfd should toss this out
					}
				}
			} else {
				// TODO -- how to handle error? free and return nil?
			}
		} else {
			vfc->nvlans = 0;		// if not set len() might return -1
		}

		if( (vfc->nmacs = jw_array_len( jblob, "macs" )) > 0 ) {						// pick up values from the json array
			vfc->macs = malloc( sizeof( *vfc->macs ) * vfc->nmacs );
			if( vfc->macs != NULL ) {
				for( i = 0; i < vfc->nmacs; i++ ) {
					if( (stuff = jw_string_ele( jblob, "macs", i )) != NULL ) {
						vfc->macs[i] = ltrim( stuff );
					} else {
						vfc->macs[i] = NULL;
					}
				}
			} else {
				// TODO -- how to handle error? free and return nil?
			}
		} else {
			if(  (stuff = jw_string( jblob, "mac" )) ) {							// new, going forward, just one MAC
				vfc->nmacs = 1;
				vfc->macs = malloc( sizeof( *vfc->macs ) * 1 );						// VFd should always support an array 
				vfc->macs[0] = ltrim( stuff );
			} else {
				vfc->nmacs = 0;		// if not set len() might return -1
				vfc->macs = NULL;	// take no chances
			}
		}

		// ---- pick up the qos parameters --------------------------
		for( i = 0; i < MAX_TCS; i++ ) {
			vfc->qshare[i] = 3;														// small default allowing 32 vfs to share evenly
		}
		if( (jnqueues = jw_array_len( jblob, "queues" )) > 0 ) {					// number of tcs supplied in the json
			int pri;			// values converted from json block
			int share;
			char*	val;

			for( i = 0; i < jnqueues; i++ ) {
				if( (qobj = jw_obj_ele( jblob, "queues", i )) != NULL ) {			// pull out the next element as an object
					pri = jw_missing( qobj, "priority" ) ? -1 : (int) jw_value( qobj, "priority" );
					val = jw_string( qobj, "share" );
					if( val ) {
						share = atoi( val );
						if( pri >= 0 && pri < 8 && share > 0 ) {
							vfc->qshare[pri] = share;
						}
					}
				}
			}
		}
		
		// ----- pick up mirror info --------------------------------
		vfc->mirror_dir = MIRROR_OFF;
		vfc->mirror_target = -1;

		if( (mirror = jw_blob( jblob, "mirror" )) != NULL ) {
			char *direction;

			if( (vfc->mirror_target = jw_missing( mirror, "target" ) ? -1 : (int) jw_value( mirror, "target" )) >= 0 ) {
				vfc->mirror_dir = MIRROR_ALL;			// if target given, default is all

				direction = jw_missing( mirror, "direction" ) ? "all" : jw_string( mirror, "direction" );

				switch( *direction ) {
					case 'b':					// both or all
					case 'a':
						vfc->mirror_dir = MIRROR_ALL;
						break;
						
					case 'o':
						if( strcmp( direction, "out" ) == 0 ) {
							vfc->mirror_dir = MIRROR_OUT;
						} else {
							vfc->mirror_dir = MIRROR_OFF;
						}
						break;
						
					case 'i':
						vfc->mirror_dir = MIRROR_IN;
						break;
				}
			}
		}

		jw_nuke( jblob );
	} else {
		errno = EINVAL;	
	}

	free( buf );
	return vfc;
}

/*
	One stop shopping to release a config struct and what ever it points to.
*/
extern void free_config( vf_config_t *vfc ) {
	int i;

	if( ! vfc ) {
		return;
	}

	SFREE( vfc->name );
	SFREE( vfc->pciid );
	SFREE( vfc->link_status);
	SFREE( vfc->vlans );
	SFREE( vfc->start_cb );
	SFREE( vfc->stop_cb );

	for( i = 0; i < vfc->nmacs; i++ ) {		// drop each referenced string
		SFREE( vfc->macs[i] );
	}
	SFREE( vfc->macs );						// finally drop the buffer itself

	free( vfc );
}

