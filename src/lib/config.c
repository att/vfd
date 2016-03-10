
/*
	Mnemonic:	config.c
	Abstract:	Functions to read and parse the various config files.
	Author:		E. Scott Daniels
	Date:		26 Feb 2016
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "jwrapper.h"
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

	If we cannot stat the file, we assume it's empty or missing and return
	an empty buffer, as opposed to a NULL, so the caller can generate defaults
	or error if an empty/missing file isn't tolerated.
*/
static char* file_into_buf( char* fname ) {
	struct stat	stats;
	off_t		fsize = 8192;	// size of the file
	off_t		nread;			// number of bytes read
	int			fd;
	char*		buf;			// input buffer
	
	if( (fd = open( fname, O_RDONLY )) >= 0 ) {
		if( fstat( fd, &stats ) >= 0 ) {
			if( stats.st_size <= 0 ) {					// empty file
				close( fd );
				fd = -1;
			} else {
				fsize = stats.st_size;						// stat ok, save the file size
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

	close( fd );
	return buf;
}

/*
	Open the file, and read the json there returning a populated structure from 
	the json bits we expect to find. 
*/
extern parms_t* read_parms( char* fname ) {
	parms_t*	parms = NULL;
	void*		jblob;			// parsed json
	char*		buf;			// buffer read from file (nil terminated)
	char*		stuff;
	int			val;
	int			i;

	if( (buf = file_into_buf( fname )) == NULL ) {
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

		parms->log_level = (int) jw_value( jblob, "log_level" );
		parms->log_keep = jw_missing( jblob, "log_keep" ) ? 30 : (int) jw_value( jblob, "allow_bcast" );

		if(  (stuff = jw_string( jblob, "config_dir" )) ) {
			parms->config_dir = strdup( stuff );
		} else {
			parms->config_dir = strdup( "/var/lib/vfd/config" );
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
			parms->pciids = malloc( sizeof( *parms->pciids ) * parms->npciids );
			if( parms->pciids != NULL ) {
				for( i = 0; i < parms->npciids; i++ ) {
					parms->pciids[i] = (char *) jw_string_ele( jblob, "pciids", i );
				}
			} else {
				parms->npciids = 0;			// memory failure; return zip
			}
		}
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

	if( (buf = file_into_buf( fname )) == NULL ) {
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

		vfc->antispoof_mac = 1;				// these are forced to 1 regardless of what was in json
		vfc->antispoof_vlan = 1;

		vfc->strip_stag = jw_missing( jblob, "strip_stag" ) ? 0 : (int) jw_value( jblob, "strip_stag" );
		vfc->allow_bcast = jw_missing( jblob, "allow_bcast" ) ? 1 : (int) jw_value( jblob, "allow_bcast" );
		vfc->allow_mcast = jw_missing( jblob, "allow_mcast" ) ? 1 : (int) jw_value( jblob, "allow_mcast" );
		vfc->allow_un_ucast = jw_missing( jblob, "allow_un_ucast" ) ? 1 : (int) jw_value( jblob, "allow_un_ucast" );
		vfc->vfid = jw_missing( jblob, "vfid" ) ? -1 : (int) jw_value( jblob, "vfid" );			// there is no real default value, so set to invalid

		if(  (stuff = jw_string( jblob, "name" )) ) {
			vfc->name = strdup( stuff );
		}

		if(  (stuff = jw_string( jblob, "pciid" )) ) {
			vfc->pciid = strdup( stuff );
		}

		if(  (stuff = jw_string( jblob, "link_status" )) ) {
			vfc->link_status = strdup( stuff );
		} else {
			vfc->link_status = strdup( "auto" );
		}
	
		if( (vfc->nvlans = jw_array_len( jblob, "vlans" )) > 0 ) {						// pick up values from the json array
			vfc->vlans = malloc( sizeof( *vfc->vlans ) * vfc->nvlans );
			if( vfc->vlans != NULL ) {
				for( i = 0; i < vfc->nvlans; i++ ) {
					vfc->vlans[i] = (int) jw_value_ele( jblob, "vlans", i );
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
		}
		
		// TODO -- add code which picks up mirror stuff (jwrapper must be enhanced first)
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

	for( i = 0; i < vfc->nmacs; i++ ) {
		SFREE( vfc->macs[i] );
	}

	free( vfc );
}

