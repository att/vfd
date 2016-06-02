
/*
	Mneminic:	pfx_list_test.c
	Abstract: 	Unit test for the prefix list files function.
				Tests obvious things, may miss edge cases.
	Date:		02 June 2016
	Author:		E. Scott Daniels
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "vfdlib.h"


int main( int argc, char** argv ) {
	int i;
	int j;
	int seconds;
	char**	list;
	int		llen;
	
	if( argv[1] == NULL ) {
		fprintf( stderr, "usage: %s prefix-string dir1 [dir2...]\n", argv[0] );
		exit( 1 );
	}

	seconds = atoi( argv[1] );

	for( i = 2; i < argc; i++ ) {
		list = list_pfiles( argv[i], argv[1], 10, &llen );				// 10 tests the ability to properly set the flag
		if( list != NULL ) {
			fprintf( stderr, "list contains %d entries\n", llen );
			for( j = 0; j < llen; j++ ) {
				if( list[j] ) {
					fprintf( stderr, "[%d] %s\n", j, list[j] );
				} else {
					fprintf( stderr, "[%d] NULL\n", j );
				}
			}

			free_list( list, llen );
			fprintf( stderr, "\n" );
		} else {
			fprintf( stderr, "list was null: %s\n", strerror( errno ) );
		}
	}

	fprintf( stderr, "test complete\n" );
}
