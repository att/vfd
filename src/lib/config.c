
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

		parms->log_level = (int) jw_value( jblob, "log_level" );
		if( (parms->log_keep = (int) jw_value( jblob, "log_keep" )) == 0 ) {
			parms->log_keep = 30;
		}

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
			parms->log_dir = strdup( "/var/log/vfd/" );
		}
	}

	free( buf );
	return parms;
}
