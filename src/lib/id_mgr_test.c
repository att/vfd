

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

	printf( "---- starting id mgr test\n\n" );
	id = mk_idm( 311 );
	for( i = 0; i < 313; i++ ) {
		if( idm_alloc( id ) < 0 ) {
			if( i < 311 ) {
				printf( "[FAIL] got %d before we ran out\n", i );
				return 0;
			}
			break;
		}
	}
	printf( "we got %d before we ran out\n", i );

	idm_return( id, 250 );
	idm_return( id, 111 );
	idm_return( id, 17 );
	idm_return( id, 29 );	
	idm_return( id, 29 );	
	idm_return( id, 29 );	
	idm_return( id, 29 );	

	printf( "trying to get more\n" );
	for( i = 0; i < 256; i++ ) {		// should only get the four back
		int v;

		if( (v=idm_alloc( id )) < 0 ) {
			if( i != 4 ) {
				printf( "FAIL: got %d before we ran out\n", i );
			} else {
				printf( "OK: got %d before we ran out\n", i );
			}
			return 0;
		}

	}

	printf( "at end got %d before we ran out\n", i );
}

