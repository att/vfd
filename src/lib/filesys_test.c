// :vi ts=4 sw=4 noet :
/*
	Mneminic:	file_test.c
	Abstract: 	Unit test for the file module.
				Tests obvious things, may miss edge cases.


	Date:		14 February 2018
	Author:		E. Scott Daniels

	Mods:
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "vfdlib.h"

#include "filesys.c"

#define FALSE 0
#define TRUE 1

static int verbose = 0;

int main( int argc, char** argv ) {
	char*	fname = "";
	char*	target = "";
	int	rc = 0;
	int errors = 0;
	char*	ok_str = "[OK]  ";
	char*	fail_str = "[FAIL]";
	char*	dir;
	char	wbuf[1024];


	if( argc < 6  ) {
			fprintf( stderr, "%s delete-file move-file dir1 dir-2 backup-file\n", argv[0] );
			fprintf( stderr, "delete file will be deleted, move file will be renamed, and backup file will be renamed with trailing -\n" );
			fprintf( stderr, "target directory is a directory in the same filesystem as move-file and will be where the file is moved\n" );
			fprintf( stderr, "both dir1 and dir2 should NOT exist, and one of dir1 and dir2 should be a fully quallified path, and the other should be relative\n" );
			fprintf( stderr, "dir1 will be used to move the move-file to\n" );
			exit( 0 );
	}

	errno = 0;
	dir = argv[3];
	rc = ensure_dir( dir );
	fprintf( stderr, "%s creating directory: %s %s\n", rc ? ok_str : fail_str, dir, strerror( errno ) );
	errors += rc ? 0 : 1;

	if( rc ) {		// only try this if we were sucessful above
		errno = 0;					// second test to create same file should also succeed :)
		dir = argv[3];
		rc = ensure_dir( dir );
		fprintf( stderr, "%s creating existing directory: %s %s\n", rc ? ok_str : fail_str, dir, strerror( errno ) );
		errors += rc ? 0 : 1;
	}

	errno = 0;
	dir = argv[4];					// assume fully qualified, or not if dir1 was, but tests all branches in ensure_dir()
	rc = ensure_dir( dir );
	fprintf( stderr, "%s creating directory: %s %s\n", rc ? ok_str : fail_str, dir, strerror( errno ) );
	errors += rc ? 0 : 1;

	errno = 0;
	fname = argv[1];
	rc = rm_file( fname, FALSE );
	fprintf( stderr, "%s deleting file: %s %s\n", rc ? ok_str : fail_str, fname, strerror( errno ) );
	errors += rc ? 0 : 1;

	errno = 0;
	fname = argv[2];
	target = argv[3];
	memset( wbuf, 0, sizeof( wbuf ) );							// keep valgrind happy by initialising all of the buffer
	snprintf( wbuf, sizeof( wbuf ), "%s/", target );			// ensure it's seen as a directory name
	rc = mv_file( fname, wbuf );
	fprintf( stderr, "%s moving file: %s to %s %s\n", rc ? ok_str : fail_str, fname, target, strerror( errno ) );
	errors += rc ? 0 : 1;


	errno = 0;
	fname = argv[5];
	rc = rm_file( fname, TRUE );
	fprintf( stderr, "%s backup file:  %s %s\n", rc ? ok_str : fail_str, fname, strerror( errno ) );
	errors += rc ? 0 : 1;



	exit( !!errors );
}
