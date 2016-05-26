
/*
	Mneminic:	hot_plug_test.c
	Abstract: 	Unit test for the hot-plug functions.
	Date:		26 May 2016
	Author:		E. Scott Daniels
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <string.h>

#include "vfdlib.h"

int main( int argc, char** argv ) {
	int	rc;

	if( argc < 3 ) {
		fprintf( stderr, "usage: %s user-id command string\n", argv[0] );
		exit( 1 );
	}

	rc = user_cmd( atoi( argv[1] ), argv[2] );
	fprintf( stderr, "rc from detach: %d\n", rc );

	exit( rc );		// bad exit if we failed a test
}
