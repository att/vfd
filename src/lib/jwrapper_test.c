/*
	Mnemonic:       jwrapper_test
	Abstract:       This will attempt to put the jwrapper code through its paces.

				It accepts as argv[1] a string of json and causes it to be parsed.
				argv[2] is the name of an element to find in the json.
				argv[3] (optional) is an array element and argv[4] is an index into
				the array of a string element to find.

				Example:
				json='{
					"active_patient": true,
					"last_visit": "2015/02/03",

					"patient_info": {
						"first_name": "Fred",
						"last_name": "Flintsone",
						"dob": "1963/04/03",
						"sex": "M",
						"weight_kilo": 65,
						"drug_alergies": [ "asprin","darvaset" ]
					}

					"Contact_info": {
						"name": "Wilma", "relation": "wife", "phone": "972.612.8918"
					}
				}'

				jwrapper_test "$json" "patient_info.dob" "patient_info.drug_alergies" 1
				jwrapper_test "$json" "last_visit"
				jwrapper_test "$json" "patient_info.weight_kilo"



	Author:	 E. Scott Daniels
	Date:	   24 February 2016

	Mods:	2017 23 Jan - Fix leading tabstops that had been converted to spaces.
*/

#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "vfdlib.h"

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
	struct stat     stats;
	off_t	   fsize = 8192;   // size of the file
	off_t	   nread;		  // number of bytes read
	int		     fd;
	char*	   buf;		    // input buffer

	if( uid != NULL ) {
		*uid = -1;			      // invalid to begin with
	}

	if( (fd = open( fname, O_RDONLY )) >= 0 ) {
		if( fstat( fd, &stats ) >= 0 ) {
			if( stats.st_size <= 0 ) {				      // empty file
				close( fd );
				fd = -1;
			} else {
				fsize = stats.st_size;					  // stat ok, save the file size
				if( uid != NULL ) {
					*uid = stats.st_uid;				    // pass back the user id
				}
			}
		} else {
			fsize = 8192;							   // stat failed, we'll leave the file open and try to read a default max of 8k
		}
	}

	if( fd < 0 ) {										  // didn't open or empty
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
	if( nread < 0 || nread > fsize ) {						      // too much or two little
		errno = EFBIG;										  // likely too much to handle
		close( fd );
		return NULL;
	}

	buf[nread] = 0;

	close( fd );
	return buf;
}


int main( int argc, char **argv ) {
	void    *jblob;					 // parsed json stuff
	char    *stuff;
	float   value;
    char*   buf;

	if( argc <= 2 ) {
		fprintf( stderr, "usage: %s file-name\n", argv[0] );
		exit( 1 );
	}

    if ((buf = file_into_buf(argv[1], NULL)) == NULL) {
		fprintf( stderr, "[FAIL] unable to read a json file into buffer\n" );
		exit( EXIT_FAILURE );
    }

    if (*buf == 0) {
	free(buf);
	buf = strdup("{ \"empty\": true }");
    }

	if( (jblob = jw_new( buf )) == NULL ) {
		fprintf( stderr, "failed to create wrapper\n" );
		exit( 1 );
	}

	if(  (stuff = jw_string( jblob, argv[2] )) ) {
		fprintf( stderr, "found name (as a string): %s = (%s)\n", argv[2], stuff );
	} else {
		if(  (value = jw_value( jblob, argv[2] )) ) {
			fprintf( stderr, "found name (as a value): %s = (%0.2f)\n", argv[2], value );
		} else {
			fprintf( stderr, "failed to find name: %s\n", argv[2] );
		}
	}

	if( argc > 3 ) {				// assume array and index are parms 3, 4
		fprintf( stderr, "array %s has %d elements\n", argv[3], jw_array_len( jblob, argv[3] ) );

		// TODO -- add a value element test though vf_config_test tests this too!
		if(  (stuff = jw_string_ele( jblob, argv[3], atoi(argv[4]) )) ) {
			fprintf( stderr, "found array element: %s[%s] = %s\n", argv[3], argv[4], stuff );
		} else {
			fprintf( stderr, "failed to find string element for: %s[%s]\n", argv[3], argv[4] );
		}
	}

	jw_nuke( jblob );
    free(buf);
}
