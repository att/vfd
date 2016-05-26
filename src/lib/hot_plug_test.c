
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
		fprintf( stderr, "usage: %s detach-parms attach-parms\n", argv[0] );
		exit( 1 );
	}

	rc = virsh_detach( argv[1] );
	fprintf( stderr, "rc from detach: %d\n", rc );

	rc = virsh_attach( argv[2] );
	fprintf( stderr, "rc from attach: %d\n", rc );

	exit( rc );		// bad exit if we failed a test
}
