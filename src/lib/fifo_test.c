
/*
	Mneminic:	fifo_test.c
	Abstract: 	Unit test for the fifo module.
				Tests obvious things, may miss edge cases.

				run with argv[1] as:
					loop  - test read() which should return a whole block (foo\nbar\ngoo\n\n should
							result in a single buffer with embedded newlines).
					bloop - test the blocking read which should return a line at a time
							The foo\nbar\n... string should result in 4 separate buffers.
					lnloop- test the non-lbocking read which should return a line at a time
							The foo\nbar\n... string should result in 4 separate buffers.

	Date:		04 February 2016
	Author:		E. Scott Daniels

	Mods:		29 Nov 2017 - Added support to test blocking and non-blocking functions.
*/

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "vfdlib.h"

static int verbose = 0;

/*
	Loops reading the fifo. Will read forever on the pipe and echo what it gets back 
	to stdout.
*/
static void loop_test( char* fname ) {
	char	*rbuf;			// read buffer
	void*	fifo;			// fifo 'handle'

	if( fname == NULL ) {
		fprintf( stderr, "argv2 was nil: requires loop pipe-name\n" );
		exit( 1 );
	}

	fifo = rfifo_create( fname, 0664 );
	if( fifo == NULL ) {
		fprintf( stderr, "[FAIL] unable to create fifo: %s\n", strerror( errno ) );
		exit( 1 );
	}
	fprintf( stderr, "[OK]  fifo (%s) created successfully, entering read loop.\n", fname );
	fprintf( stderr, "      echo/print strings to the pipe and they should be written to stderr here\n\n" );
	
	while( 1 ) {
		rbuf = rfifo_read( fifo );
		if( rbuf != NULL ) {
			if( strlen( rbuf ) > 0 ) {
				fprintf( stderr, "read block returned %d bytes in the buffer: %s", (int) strlen( rbuf ), rbuf );   // buf should have \n
			} else {
				if( verbose ) {
					fprintf( stderr, "[OK]   empty buffer returned from read; this should happen if there is nothing to read\n" );
				}
			}

			free( rbuf );
		} else {
			if( verbose ) {
				fprintf( stderr, "[OK]   nothing returned from read; this should happen if there is nothing to read\n" );
			}
		}

		usleep( 250000 );		// ~.25 second
	}
}

/*
	Tests the blocking readln, or the non-blocking readln function in the same way loop tests the non-blocking function.
	The blocking parm is set to 1 when testing blk_readln().
*/
static void blk_loop_test( char* fname, int blocking ) {
	char	*rbuf;			// read buffer
	void*	fifo;			// fifo 'handle'

	if( fname == NULL ) {
		fprintf( stderr, "argv2 was nil: requires loop pipe-name\n" );
		exit( 1 );
	}

	fifo = rfifo_create( fname, 0664 );
	if( fifo == NULL ) {
		fprintf( stderr, "[FAIL] unable to create fifo: %s\n", strerror( errno ) );
		exit( 1 );
	}
	rfifo_detect_close( fifo );
	fprintf( stderr, "[OK]  fifo (%s) created successfully, entering blocking read loop.\n", fname );
	fprintf( stderr, "      echo/print strings to the pipe and they should be written to stderr here\n\n" );
	
	while( 1 ) {
		if( blocking ) {
			rbuf = rfifo_blk_readln( fifo );
		} else {
			rbuf = rfifo_readln( fifo );
		}
		if( rbuf != NULL ) {
			if( strlen( rbuf ) > 0 ) {
				fprintf( stderr, "%sblocking read returned %d bytes in the buffer: %s", blocking ? "" : "non-", (int) strlen( rbuf ), rbuf ); // buffer should be newline terminated
			} else {
				sleep( 1 );
			}
			free( rbuf );
		} else {
			if( blocking ) {
				fprintf( stderr, "[FAIL] nothing returned from blocking read; this should NOT happen\n" );
			}

			sleep( 1 );
		}
	}
}

/*
	Tests the timeout readln by waiting up to 10 seconds for the first buffer, and then blocking hard
	after that.
*/
static void to_loop_test( char* fname ) {
	int		timeout = 100;	// tenths of seconds
	char	*rbuf;			// read buffer
	void*	fifo;			// fifo 'handle'

	if( fname == NULL ) {
		fprintf( stderr, "argv2 was nil: requires loop pipe-name\n" );
		exit( 1 );
	}

	fifo = rfifo_create( fname, 0664 );
	if( fifo == NULL ) {
		fprintf( stderr, "[FAIL] unable to create fifo: %s\n", strerror( errno ) );
		exit( 1 );
	}
	rfifo_detect_close( fifo );
	fprintf( stderr, "[OK]  fifo (%s) created successfully, entering blocking read loop.\n", fname );
	fprintf( stderr, "      echo/print strings to the pipe and they should be written to stderr here\n\n" );
	
	while( 1 ) {
		rbuf = rfifo_to_readln( fifo, timeout );			// abort after initial wait
		if( rbuf != NULL ) {
			if( strlen( rbuf ) > 0 ) {
				fprintf( stderr, "timeout read returned %d bytes in the buffer: %s", (int) strlen( rbuf ), rbuf ); // buffer should be newline terminated
			} else {
				sleep( 1 );
			}
			free( rbuf );
			timeout = 0;			// we wont' timeout after the first read
		} else {
			fprintf( stderr, "read timed out; ending\n" );
			return;
		}
	}
}

int main( int argc, char** argv ) {
	char*	fname = "test_pipe";
	int	rc = 0;
	int i;
	void*	fifo;			// we'll create the fifo with the library stuff and then...
	int		fd;				// we will open it again and write something, then call read
	char*	wbuf = "Mary didn't really have a lamb, she had a dog.\nIt was brown, lazy, and liked to play down by the river.\n\n";	// we should get both records back, each with a trailing newline
	char*	rbuf;			// read buffer


	if( argv[1] != NULL ) {
		if( strcmp( argv[1], "-?" ) == 0 ) {
			fprintf( stderr, "%s [{loop|bloop|lnloop|tloop}] [path[pipe-name]]\n", argv[0] );
			exit( 0 );
		}

		if( strcmp( argv[1], "loop" ) == 0 ) {
			if( argv[2] != NULL ) {
				fname = argv[2];
			}

			loop_test( fname );
			exit( 0 );
		}

		if( strcmp( argv[1], "tloop" ) == 0 ) {
			if( argv[2] != NULL ) {
				fname = argv[2];
			}

			fprintf( stderr, "starting timeout readln loop test\n" );
			to_loop_test( fname );
			exit( 0 );
		}

		if( strcmp( argv[1], "bloop" ) == 0 ) {
			if( argv[2] != NULL ) {
				fname = argv[2];
			}

			fprintf( stderr, "starting blocking readln loop test\n" );
			blk_loop_test( fname, 1 );
			exit( 0 );
		}

		if( strcmp( argv[1], "lnloop" ) == 0 ) {
			if( argv[2] != NULL ) {
				fname = argv[2];
			}

			fprintf( stderr, "starting readln loop test\n" );
			blk_loop_test( fname, 0 );
			exit( 0 );
		}

		fname = argv[1];
	}

	fifo = rfifo_create( fname, 0664 );
	if( fifo == NULL ) {
		fprintf( stderr, "[FAIL] unable to create fifo: %s\n", strerror( errno ) );
		exit( 1 );
	}
	fprintf( stderr, "[OK]  fifo created successfully\n" );

	fd = open( fname, O_WRONLY, 0 );			// we'll read from the fifo too


	fprintf( stderr, "reading before write to test nonblocking...\n" );
	rbuf = rfifo_read( fifo );
	if( rbuf != NULL ) {
		fprintf( stderr, "nb read got %d bytes in the buffer\n", (int) strlen( rbuf ) );
	} else {
		fprintf( stderr, "nb read got null pointer back\n" );
	}


	if( fd >= 0 ) {
		if( write( fd, wbuf, strlen( wbuf ) ) < strlen( wbuf ) ) {
			fprintf( stderr, "write to fifo failed\n" );
			exit( 1 );
		}
	} else {
		fprintf( stderr, "open for write failed\n" );
		exit( 1 );
	}

	fprintf( stderr, "reading...\n" );
	rbuf = rfifo_read( fifo );
	if( rbuf != NULL ) {
		fprintf( stderr, "read from fifo was successful: %ld bytes\n", (long) strlen( rbuf ) );
		fprintf( stderr, "(%s)\n", rbuf );
	} else {
		fprintf( stderr, "read from pipe failed\n" );
	}

	rfifo_close( fifo );
}
