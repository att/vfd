/*
	Mneminic:	parm_file_test.c
	Abstract: 	Unit test for parm file parsing (part of config.c).
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

	if( argv[1] != NULL ) {
		fname = argv[1];
	}
	parms = read_parms( fname );
	if( parms == NULL ) {
		fprintf( stderr, "abort: unable to read parm file %s: %s\n", fname, strerror( errno ) );
		exit( 1 );
	}

	fprintf( stderr, "parms read:\n" );
	fprintf( stderr, "\tlog dir: %s\n", parms->log_dir );
	fprintf( stderr, "\tconfig_dir: %s\n", parms->config_dir );
	fprintf( stderr, "\tlog_level: %d\n", parms->log_level );
	fprintf( stderr, "\tlog_keep: %d\n", parms->log_keep );
	fprintf( stderr, "\tfifo: %s\n", parms->fifo_path );
}
