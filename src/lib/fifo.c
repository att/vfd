
/*
	Mnemonic:	fifo.c
	Abstract:	Functions for dealing with our fifo.
	Author:		E. Scott Daniels
	Date:		04 Mar 2016
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "vfdlib.h"

#define RBUF_SIZE 8192		// our read buffer sizes

typedef struct {
	int 	fd;			// fd of the open fifo
	char*	fname;		// filename -- policy is to unlink on close so we need to track this
	void*	flow;		// the managing flow 'handle'
	char*	rbuf;		// raw read buffer
} fifo_t;

/*
	Create our read fifo. We use flow manager to actually buffer and 
	parse out full json blocks from the fifo so that the application 
	can issue a read block and get a block, or get nothing. Thus the 
	return is a void pointer which is actually a pointer that our functions
	can use.
*/
extern void* rfifo_create( char* fname, int mode ) {
	int fd;
	fifo_t*	fifo = NULL;

	if( mode == 0 ) {
		mode = 0x660;
	}

	unlink( fname );								// hard unlink and we don't care if this fails
	if( mkfifo( fname, mode ) < 0 ) {
		return NULL;								// can't make send back err errno still set
	}

	if( (fd =  open( fname, O_RDWR | O_NONBLOCK, mode )) >= 0 ) {				// open in read AND write mode so that we don't block
		if( (fifo = malloc( sizeof( *fifo ) )) == NULL ) {
			return NULL;
		}

		fifo->fd = fd;
		if( (fifo->flow = ng_flow_open( RBUF_SIZE )) == NULL ) {		// create the buffered flow
			free( fifo );
			return NULL;
		}

		if( (fifo->rbuf = (char *) malloc( sizeof( char ) * RBUF_SIZE )) == NULL ) {
			ng_flow_close( fifo->flow );
			free( fifo );
			return NULL;
		}

		fifo->fname = strdup( fname );
	}

	return fifo;
}

/*
	Close the fifo and clean up the flow. Ultimately unlink the fifo.
*/
extern void rfifo_close( void* vfifo ) {
	fifo_t* fifo;

	if( (fifo = (fifo_t *) vfifo ) == NULL ) {
		return;
	}

	if( fifo->fd >= 0 ) {
		close( fifo->fd );
	}	

	if( fifo->flow != NULL ) {
		ng_flow_close( fifo->flow );
	}

	if( fifo->fname != NULL ) {
		unlink( fifo->fname );
		free( fifo->fname );
	}

	free( fifo );
}

/*
	Read a complete "block" from the fifo. A block is 
	all data up to a double newline (\n\n). Single newlines are left
	and a final newline is added to the buffer.  The buffer is nil 
	terminated.

	We don't expect a high rate of requests so we aren't concerned 
	about the additional copies, length checking and newline smashing
	that we do here.

	Caller must free the pointer returned.
*/
extern char* rfifo_read( void* vfifo ) {
	fifo_t* fifo;
	char	*rbuf;				// return buffer
	char	*nb;				// next buffer from flow manager
	int		len;				// actual byte count read from fifo
	int		tlen = 0;			// total bytes put into buffer for caller

	if( (fifo = (fifo_t *) vfifo) == NULL ) {
		return NULL;
	}

	if( (rbuf = (char *) malloc( sizeof( char ) * RBUF_SIZE )) == NULL ) {
		return NULL;
	}

	*rbuf = 0;
	while( 1 ) {
		while( (nb = ng_flow_get( fifo->flow, '\n' )) != NULL ) {
			if( tlen > 0 ) {
				strcat( rbuf, "\n" );
			}

			if( *nb == 0 ) {
				return rbuf;
			}
			
			tlen += strlen( nb );
			if( tlen > RBUF_SIZE-2 ) {									// give us room to blindly add \n on next go round if we don't overflow
				return rbuf;											// buffer overrun avoided, but data is lost. 
			}
			strcat( rbuf, nb );											// we could use some trickery to avoid this copy, but speed here isn't crutial
		}	

		if( (len = read( fifo->fd, fifo->rbuf, RBUF_SIZE )) < 0 ) {		// nothing to read
			return rbuf;
		}

		ng_flow_ref( fifo->flow, fifo->rbuf, len );						// register the buffer (zero copy)
	}

	return rbuf;			// shouldn't get here, but this keeps the compiler from tossing a warning.
}
