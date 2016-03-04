/*
	Mneminic:	parm_file_test.c
	Abstract: 	Unit test for parm file parsing (part of config.c).
				Tests obvious things, may miss edge cases.
	Date:		03 February 2016
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
	char*	fname = "vfd.cfg";
	parms_t* parms;
	int	rc = 0;

	if( argv[1] != NULL ) {
		fname = argv[1];
	}
	parms = read_parms( fname );
	if( parms == NULL ) {
		fprintf( stderr, "[FAIL] unable to read parm file %s: %s\n", fname, strerror( errno ) );
		rc = 1;
	} else {
		fprintf( stderr, "[OK]   Able to open, read and parse json in parm file\n" );
		fprintf( stderr, "  parms read:\n" );
		fprintf( stderr, "\tlog dir: %s\n", parms->log_dir );
		fprintf( stderr, "\tconfig_dir: %s\n", parms->config_dir );
		fprintf( stderr, "\tlog_level: %d\n", parms->log_level );
		fprintf( stderr, "\tlog_keep: %d\n", parms->log_keep );
		fprintf( stderr, "\tfifo: %s\n", parms->fifo_path );
	}

	
	// ensure that we get a failure if the file isn't there
	parms = read_parms( "/hosuchdir/nosuchfile" );
	if( parms == NULL ) {
		fprintf( stderr, "[OK]   attempt to open a nonexistant file was rejected, as expected\n" );
	} else {
		fprintf( stderr, "[FAIL] attempt to open a nonexistant file was NOT rejected as expected\n" );
		rc = 1;
	}

	exit( rc );		// bad exit if we failed a test
}
