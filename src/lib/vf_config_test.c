/*
	Mneminic:	vf_config_test.c
	Abstract: 	Unit test for vf config file parsing  (part of config.c).
				Tests obvious things, may miss edge cases.
	Date:		04 February 2016
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
	char*	fname = "vf.cfg";
	vf_config_t* vfc;
	int	rc = 0;
	int i;

	if( argv[1] != NULL ) {
		fname = argv[1];
	}
	vfc = read_config( fname );
	if( vfc == NULL ) {
		fprintf( stderr, "[FAIL] unable to read and/or parse config file %s: %s\n", fname, strerror( errno ) );
		rc = 1;
	} else {
		fprintf( stderr, "[OK]   Able to open, read and parse json in config file\n" );
		fprintf( stderr, "  configs read:\n" );
		fprintf( stderr, "\towner: %d\n", vfc->owner );
		fprintf( stderr, "\tname: %s\n", vfc->name );
		fprintf( stderr, "\tvfid: %d\n", vfc->vfid );
		fprintf( stderr, "\tpciid: %s\n", vfc->pciid );
		fprintf( stderr, "\tstart_cb: %s\n", vfc->start_cb );
		fprintf( stderr, "\tstop_cb: %s\n", vfc->stop_cb );
		fprintf( stderr, "\tstrip_stag: %d\n", vfc->strip_stag );
		fprintf( stderr, "\tallow_bcast: %d\n", vfc->allow_bcast );
		fprintf( stderr, "\tallow_mcast: %d\n", vfc->allow_mcast );
		fprintf( stderr, "\tallow_un_ucast: %d\n", vfc->allow_un_ucast );
		fprintf( stderr, "\tantispoof_mac: %d\n", vfc->antispoof_mac );
		fprintf( stderr, "\tantispoof_vlan: %d\n", vfc->antispoof_vlan );
		fprintf( stderr, "\tvm_mac: %s\n", vfc->vm_mac );
		fprintf( stderr, "\tlink_status: %s\n", vfc->link_status );

		fprintf( stderr, "\tnvlans: %d\n", vfc->nvlans );
		for( i = 0; i < vfc->nvlans; i++ ) {
			fprintf( stderr, "\t\tvlan[%d] = %d\n", i, vfc->vlans[i] );
		}
		fprintf( stderr, "\tnmacs: %d\n", vfc->nmacs );
		for( i = 0; i < vfc->nmacs; i++ ) {
			fprintf( stderr, "\t\tmac[%d] = %s\n", i, vfc->macs[i] );
		}

		free_config( vfc );
	}

	
	// ensure that we get a failure if the file isn't there
	vfc = read_config( "/hosuchdir/nosuchfile" );
	if( vfc == NULL ) {
		fprintf( stderr, "[OK]   attempt to open a nonexistant file was rejected, as expected\n" );
	} else {
		fprintf( stderr, "[FAIL] attempt to open a nonexistant file was NOT rejected as expected\n" );
		rc = 1;
	}

	exit( rc );		// bad exit if we failed a test
}
