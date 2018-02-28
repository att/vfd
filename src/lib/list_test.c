
/*
	Mneminic:	list_test.c
	Abstract: 	Unit test for the list module.
				Tests obvious things, may miss edge cases.

				Parms:  directory suffix [suffix...]
	Date:		08 March 2016
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
	char**	list;
	int		llen;
	
	if( argv[1] == NULL ) {
		fprintf( stderr, "usage: %s dir-name suffix1 [suffix2...]\n", argv[0] );
		exit( 1 );
	}

	for( i = 2; i < argc; i++ ) {
		list = list_files( argv[1], argv[i], 1, &llen );
		if( list != NULL ) {
			fprintf( stderr, "list contains %d entries\n", llen );
			for( j = 0; j < llen; j++ ) {
				if( list[j] ) {
					fprintf( stderr, "[%d] %s\n", j, list[j] );
				} else {
					fprintf( stderr, "[%d] NULL\n", j );
				}
			}
		
		} else {
			fprintf( stderr, "list was null: %s\n", strerror( errno ) );
		}
	}
}
