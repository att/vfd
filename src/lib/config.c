
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
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "vfdlib.h"

// -------------------------------------------------------------------------------------
#define SFREE(p) if((p)){free(p);}			// safe free (free shouldn't balk on nil, but don't chance it)


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
	int			i;
	int			def_mtu;		// default mtu (pulled and used to set pciid struct, but not kept in parms

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

		if( jw_missing( jblob, "default_mtu" ) ) {			// could be an old install using deprecated mtu, so look for that and default if neither is there
			def_mtu = jw_missing( jblob, "mtu" ) ? 9000 : (int) jw_value( jblob, "mtu" );
		} else {
 		 	def_mtu = (int) jw_value( jblob, "default_mtu" );
		}

		if(  (stuff = jw_string( jblob, "config_dir" )) ) {
			parms->config_dir = strdup( stuff );
		} else {
			parms->config_dir = strdup( "/var/lib/vfd/config" );
		}

		if(  (stuff = jw_string( jblob, "pid_fname" )) ) {
			parms->pid_fname = strdup( stuff );
		} else {
			parms->pid_fname = strdup( "/var/run/vfd.pid" );
		}

		if(  (stuff = jw_string( jblob, "stats_path" )) ) {
			parms->stats_path = strdup( stuff );
		} else {
			parms->stats_path = strdup( "/var/lib/vfd/stats" );
		}

		if(  (stuff = jw_string( jblob, "fifo" )) ) {
			parms->fifo_path = strdup( stuff );
		} else {
			parms->fifo_path = strdup( "/var/lib/vfd/request" );
		}

		if(  (stuff = jw_string( jblob, "log_dir" )) ) {
			parms->log_dir = strdup( stuff );
		} else {
			parms->log_dir = strdup( "/var/log/vfd" );
		}

		if( (stuff = jw_string( jblob, "cpu_mask" )) ) {
			parms->cpu_mask = strdup( stuff );
		}

		if( (parms->npciids = jw_array_len( jblob, "pciids" )) > 0 ) {			// pick up the list of pciids
			parms->pciids = (pfdef_t *) malloc( sizeof( *parms->pciids ) * parms->npciids );
			memset( parms->pciids, 0, sizeof( *parms->pciids ) * parms->npciids );

			if( parms->pciids != NULL ) {
				for( i = 0; i < parms->npciids; i++ ) {
					stuff = (char *) jw_string_ele( jblob, "pciids", i );
					if( stuff != NULL ) {										// string, use default mtu
						parms->pciids[i].id = strdup( stuff );
						parms->pciids[i].mtu = def_mtu;
						parms->pciids[i].flags |= PFF_LOOP_BACK;
					} else {
						if( (pobj = jw_obj_ele( jblob, "pciids", i )) != NULL ) {		// full pciid object -- take values from it
							if( (stuff = jw_string( pobj, "id" )) == NULL ) {
								stuff = strdup( "missing-id" );
							}
							parms->pciids[i].id = strdup( stuff );
							parms->pciids[i].mtu = !jw_is_value( pobj, "mtu" ) ? def_mtu : (int) jw_value( pobj, "mtu" );
							if( !jw_is_bool( pobj, "enable_loopback" ) ? 1 : (int) jw_value( pobj, "enable_loopback" ) ) {		// default to true if not there
								parms->pciids[i].flags |= PFF_LOOP_BACK;
							} else {
								parms->pciids[i].flags &= ~PFF_LOOP_BACK;			// disable if set to false
							}
						}
					}
				}
			} else {
				parms->npciids = 0;			// memory failure; return zip
			}
		}

		jw_nuke( jblob );
	}

	free( buf );
	return parms;
}

/*
	Cleanup a parm block and free the data.
*/
extern void free_parms( parms_t* parms ) {
	if( ! parms ) {
		return;
	}

	SFREE( parms->log_dir );
	SFREE( parms->fifo_path );
	SFREE( parms->config_dir );
	SFREE( parms->pciids );
	SFREE( parms->pid_fname );
	SFREE( parms->stats_path );

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
	char*		buf;			// buffer read from file (nil terminated)
	char*		stuff;
	int			val;
	int			i;
	uid_t		uid;

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
		vfc->antispoof_mac = 1;				// these are forced to 1 regardless of what was in json
		vfc->antispoof_vlan = 1;

		//vfc->antispoof_mac = jw_missing( jblob, "antispoof_mac" ) ? 0 : (int) jw_value( jblob, "antispoof_mac" );
		//vfc->antispoof_vlan = jw_missing( jblob, "antispoof_vlan" ) ? 0 : (int) jw_value( jblob, "antispoof_vlan" );

		vfc->allow_untagged = !jw_is_bool( jblob, "allow_untagged" ) ? 0 : (int) jw_value( jblob, "allow_untagged" );

		vfc->strip_stag = !jw_is_bool( jblob, "strip_stag" ) ? 0 : (int) jw_value( jblob, "strip_stag" );
		vfc->allow_bcast = !jw_is_bool( jblob, "allow_bcast" ) ? 1 : (int) jw_value( jblob, "allow_bcast" );
		vfc->allow_mcast = !jw_is_bool( jblob, "allow_mcast" ) ? 1 : (int) jw_value( jblob, "allow_mcast" );
		vfc->allow_un_ucast = !jw_is_bool( jblob, "allow_un_ucast" ) ? 1 : (int) jw_value( jblob, "allow_un_ucast" );
		vfc->vfid = !jw_is_value( jblob, "vfid" ) ? -1 : (int) jw_value( jblob, "vfid" );			// there is no real default value, so set to invalid

		vfc->rate = jw_missing( jblob, "rate" ) ? 0 : (float) jw_value( jblob, "rate" );

		if(  (stuff = jw_string( jblob, "name" )) ) {
			vfc->name = strdup( stuff );
		}

		if(  (stuff = jw_string( jblob, "pciid" )) ) {
			vfc->pciid = strdup( stuff );
		}

		if(  (stuff = jw_string( jblob, "stop_cb" )) ) {					// command that is executed on owner's behalf as we shutdown
			vfc->stop_cb = strdup( stuff );
		}
		if(  (stuff = jw_string( jblob, "start_cb" )) ) {					// command that is executed on owner's behalf as we start (last part of init)
			vfc->start_cb = strdup( stuff );
		}

		if(  (stuff = jw_string( jblob, "link_status" )) ) {
			vfc->link_status = strdup( stuff );
		} else {
			vfc->link_status = strdup( "auto" );
		}

		if(  (stuff = jw_string( jblob, "vm_mac" )) ) {
			vfc->vm_mac = strdup( stuff );
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
						vfc->macs[i] = strdup( stuff );
					} else {
						vfc->macs[i] = NULL;
					}
				}
			} else {
				// TODO -- how to handle error? free and return nil?
			}
		} else {
			vfc->nmacs = 0;		// if not set len() might return -1
			vfc->macs = NULL;	// take no chances
		}
		
		// TODO -- add code which picks up mirror stuff (jwrapper must be enhanced first)

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

	for( i = 0; i < vfc->nmacs; i++ ) {
		SFREE( vfc->macs[i] );
	}

	free( vfc );
}

