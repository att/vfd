// :vi ts=4 sw=4:
/*
	Mneminic:	bleat_test.c
	Abstract: 	Unit test for the bleat module.
				Tests obvious things, may miss edge cases.
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
	
	id = getppid();

	// these should go to stderr
	bleat_set_lvl( 1 );
	bleat_printf( 1, "this is a level 1 message should be SEEN" );
	bleat_printf( 2, "this message should NOT be seen it is level 2" );
	bleat_printf( 0, "this is a level 0 should be SEEN with pid: %d",  id );

	bleat_push_lvl( 2 );
	bleat_printf( 2, "level set to 2 so this should be SEEN " );
	bleat_pop_lvl( );
	bleat_printf( 2, "level reset so this should NOT be seen " );

	// these should to to foo.log in the current directory
	bleat_printf( 0, "setting log to foo.log, look there for other messages" );
	bleat_set_log( "foo.log", 0 );
	bleat_printf( 1, "this is a level 1 message should be SEEN" );
	bleat_printf( 2, "this message should NOT be seen it is level 2" );
	bleat_printf( 0, "this is a level 0 should be SEEN with pid: %d",  id );

	// these should to to foo.log.<date> in the current directory, hms should be added and the 
	// log should 'roll' on 5 minute boundaries
	bleat_printf( 0, "setting log to foo.log.<date>, look there for other messages" );
	bleat_set_log( "foo.log", 300 );
	bleat_printf( 1, "this is a level 1 message should be SEEN" );
	bleat_printf( 2, "this message should NOT be seen it is level 2" );
	bleat_printf( 0, "this is a level 0 should be SEEN with pid: %d",  id );

	fprintf( stderr, "sleeping 300 sec to test log file date rolling, next roll = %ld\n", (long )bleat_next_roll()  );
	sleep( 300 );
	bleat_printf( 0, "this bleat should be seen in the rolled log file" );
}
