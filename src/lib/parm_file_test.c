/*
	Mneminic:	parm_file_test.c
	Abstract: 	Unit test for parm file parsing (part of config.c).
				Tests obvious things, may miss edge cases.
				usage:  parm_file_test cfg-file-name

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


void pprint( parms_t* parms ) {
	int  i, j, k, l;

	fprintf( stderr, "  parms read:\n" );
	fprintf( stderr, "\tlog dir: %s\n", parms->log_dir );
	fprintf( stderr, "\tconfig_dir: %s\n", parms->config_dir );
	fprintf( stderr, "\tlog_level: %d\n", parms->log_level );
	fprintf( stderr, "\tlog_keep: %d\n", parms->log_keep );
	fprintf( stderr, "\tdelete_keep: %d\n", parms->delete_keep );
	fprintf( stderr, "\tfifo: %s\n", parms->fifo_path );
	fprintf( stderr, "\tcpu_mask: %s\n", parms->cpu_mask );
	fprintf( stderr, "\tdpdk_log_level: %d\n", parms->dpdk_log_level );
	fprintf( stderr, "\tdpdk_init_log_level: %d\n", parms->dpdk_init_log_level );

	fprintf( stderr, "\tnpciids: %d\n", parms->npciids );
	for( i = 0; i < parms->npciids; i++ ) {
		fprintf( stderr, "\tpciid[%d]: %s %d flags=%02x\n", i, parms->pciids[i].id, parms->pciids[i].mtu, parms->pciids[i].flags );
        for( j = 0; j < parms->pciids[i].ntcs; j++ ) {
            if( parms->pciids[i].tcs[j] != 0 ) {
                fprintf( stderr, "\ttclasses[%d]: %s, flags=%02x, max_bw: %d, min_bw: %d\n", j, parms->pciids[i].tcs[j]->hr_name, parms->pciids[i].tcs[j]->flags, parms->pciids[i].tcs[j]->max_bw, parms->pciids[i].tcs[j]->min_bw );
            }
        }
        for( k = 0; k < sizeof(parms->pciids[i].bw_grps)/sizeof(bw_grp_t); k++) {
            fprintf(stderr, "\tbw_grps[%d]\n", k);
            for( l = 0; l < parms->pciids[i].bw_grps[k].ntcs; l++) {
                fprintf( stderr, "\t\t%d\n", parms->pciids[i].bw_grps[k].tcs[l]);
            }
        }
	}
}

int main( int argc, char** argv ) {
	char*	fname = argv[1];
	parms_t* parms;
	int	rc = 0;

	if( argv[1] != NULL ) {
		fname = argv[1];
	}
	parms = read_parms( fname );
	if( parms == NULL ) {
		fprintf( stderr, "[FAIL] unable to read parm file %s: %s\n", fname, strerror( errno ) );
		pprint( parms );
		free_parms( parms );
		rc = 1;
	} else {
		fprintf( stderr, "[OK]   Able to open, read and parse json in parm file\n" );
		pprint( parms );
	}
	free_parms( parms );
	
	parms = read_parms( "/hosuchdir/nosuchfile" );			// should return defaults all round rather than failing
	if( parms != NULL ) {
		fprintf( stderr, "[OK] opening nonexisting file resulted in default struct:\n" );
		pprint( parms );
		free_parms( parms );
	} else {
		fprintf( stderr, "[FAIL] attempt to open a nonexistant file was rejected; should have returned a struct with defaults\n" );
		rc = 1;
	}

	exit( rc );		// bad exit if we failed a test
}
