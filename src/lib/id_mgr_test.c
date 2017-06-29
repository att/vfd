

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <dirent.h>
#include <time.h>

#include "vfdlib.h"

int main( ) {
	void* id;
	int i;

	id = mk_idm( 311 );
	for( i = 0; i < 313; i++ ) {
		if( idm_alloc( id ) < 0 ) {
			if( i != 311 ) {
				printf( "[FAIL] error allocating at %d\n", i );
				return 1;
			}
			break;
		}
	}
	printf( "[OK]   we got %d before we ran out which is what we tried to create\n", i );

	idm_return( id, 250 );
	idm_return( id, 111 );
	idm_return( id, 17 );
	idm_return( id, 11 );
	idm_return( id, 29 );	
	idm_return( id, 29 );		// these should have no effect
	idm_return( id, 29 );	
	idm_return( id, 29 );	

	i = idm_use( id, 11 );		// attempt to assign directly
	if( i != 1 ) {
		printf( "[FAIL] Attempted to directly assign id 11 expected 1 as return, got %d\n", i );
		return 1;
	} else {
		printf( "[OK]   Attempted to directly assign id 11 and was successful\n" );
	}

	i = idm_is_used( id, 11 );		// should return that this is used
	if( i != 1 ) {
		printf( "[FAIL] Attempted to query state of id 11; expected 1 as return, got %d\n", i );
		return 1;
	} else {
		printf( "[OK]   Attempt to query state of used ID was good.\n" );
	}

	i = idm_is_used( id, 29 );		// should return that this is not used
	if( i != 0 ) {
		printf( "[FAIL] Attempted to query state of id 29; expected 0 as return, got %d\n", i );
		return 1;
	} else {
		printf( "[OK]   Attempt to query state of unused ID was good.\n" );
	}

	printf( "[INFO] trying to get more\n" );
	for( i = 0; i < 256; i++ ) {		// should only get the four back
		int v;

		if( (v=idm_alloc( id )) < 0 ) {
			if( i != 4 ) {
				printf( "[FAIL] got %d before we ran out; expected to get 4\n", i );
			} else {
				printf( "[OK]   expected to allocate 4 more and got %d before we ran out\n", i );
			}

			break;
		}
	}

	return 0;
}

