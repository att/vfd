// :vi ts=4 sw=4:
/*
	Mneminic:	bleat_test.c
	Abstract: 	Unit test for the bleat module.
				Tests obvious things, may miss edge cases.

				bleat_test [roll-seconds] [purge-seconds dir log_prefix]

				If seconds are given, then a test of purging log files
				in the current directory is made.  Log files older than
				seconds should be purged when the log file is rolled during
				the test.


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
	int	id = 0;
	int	psec = 0;
	int rsec = 0;			// seconds to wait when testing log roll

	
	id = getppid();

	if( argc > 1 ) {
		rsec = atoi( argv[1] );
		if( rsec < 300 ) {
			fprintf( stderr, "roll second test requires a wait of at least 300s; using 300\n" );
			rsec = 300;
		}
	}

	if( argc > 4 ) {								// if purg-seconds directory, prefix, seconds supplied
		if( (psec = atoi( argv[2] )) < 5 ) {
			fprintf( stderr, "purge seconds must be >= 5; using 5 seconds\n" );
			psec = 5;
		}
		fprintf( stderr, "setting purge threshold to %d\n", psec );
		bleat_set_purge( argv[3], argv[4], psec );
	}

	// these should go to stderr
	bleat_set_lvl( 1 );
	bleat_printf( 1, "this is a level 1 message should be SEEN" );
	bleat_printf( 2, "this message should NOT be seen it is level 2" );
	bleat_printf( 0, "this is a level 0 should be SEEN data: %d",  id );		// test ability to add printf style data

	bleat_push_lvl( 2 );
	bleat_printf( 2, "level set to 2 so this should be SEEN " );
	bleat_pop_lvl( );
	bleat_printf( 2, "level reset so this should NOT be seen " );

	bleat_set_lvl( 2 );			// hard level set test
	bleat_printf( 2, "level set to 2 so this should be SEEN " );
	bleat_printf( 3, "lvl 3 msg: but level is currently set to 2 so this should be NOT be seen" );
	bleat_set_lvl( 1 );			// hard set back
	bleat_printf( 2, "lvl 2 msg: level reverted to 1 so this should NOT be seen" );

	// these should to to foo.log (no rolling) in the current directory
	bleat_printf( 0, "setting log to foo.log, look there for other messages" );
	bleat_set_log( "foo.log", 0 );
	bleat_printf( 1, "this is a level 1 message should be SEEN" );
	bleat_printf( 2, "this message should NOT be seen it is level 2" );
	bleat_printf( 0, "this is a level 0 should be SEEN data: %d",  id );

	if( rsec > 0 ) {
		// these should to to foo.log.<date> in the current directory, hms should be added and the 
		// log should 'roll' on rsec boundaries
		bleat_printf( 0, "setting log to foo.log.<date>, look there for other messages" );
		bleat_set_log( "foo.log", rsec );
		bleat_printf( 1, "this is a level 1 message should be SEEN" );
		bleat_printf( 2, "this message should NOT be seen it is level 2" );
		bleat_printf( 0, "this is a level 0 should be SEEN data: %d",  id );
	
		fprintf( stderr, "sleeping %d sec to test log file date rolling, next roll = %ld\n", rsec, (long )bleat_next_roll()  );
		sleep( rsec );
		bleat_printf( 0, "this bleat should be SEEN in the rolled log file" );
	}
}
