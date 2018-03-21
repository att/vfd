
/*
	Mnemonic:	fifo.c
	Abstract:	Functions for dealing with our fifo.
	Author:		E. Scott Daniels
	Date:		04 Mar 2016

	Mods:		07 Apr 2017 - Correct default mode on open/create.
				29 Nov 2017 - Fix possible buffer overrun, add blocking and timeout
					oriented read functions.
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "vfdlib.h"

#define RBUF_SIZE 8192		// our read buffer sizes

typedef struct {
	int 	fd;					// fd of the open fifo
	int		wfd;				// write file des to ensure we don't block
	int		close_on_data;		// flag that will cause write fd to close on first read so we detect external writer close
	char*	fname;				// filename -- policy is to unlink on close so we need to track this
	void*	flow;				// the managing flow 'handle'
	char*	rbuf;				// raw read buffer
} fifo_t;


/*
	This is the support for the create or open rifio functions.
*/
static void* copen_pipe( char* fname, int mode, int unlink_first ) {
	int fd;
	fifo_t*	fifo = NULL;

	if( mode == 0 ) {
		mode = 0660;
	}

	if( unlink_first ) {
		unlink( fname );								// hard unlink and we don't care if this fails
		if( mkfifo( fname, mode ) < 0 ) {
			return NULL;								// can't make send back err errno still set
		}
	}

	if( (fd =  open( fname, O_RDONLY | O_NONBLOCK, mode )) >= 0 ) {				// open for reading (create) and in non-blocking mode so we continue initialisation
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

		fifo->wfd =  open( fname, O_WRONLY | O_NONBLOCK, mode );		// open writer to ensure prevent 0 len reads

		fcntl( fd, F_SETPIPE_SZ, 1024 * 60 );		// ensure a 60k buffer (atomic writes are still just 4k)
		fifo->fname = strdup( fname );
	}

	return fifo;
}

/*
	Create our read fifo. We use flow manager to actually buffer and 
	parse out full json blocks from the fifo so that the application 
	can issue a read block and get a block, or get nothing. Thus the 
	return is a void pointer which is actually a pointer that our functions
	can use.
*/
extern void* rfifo_create( char* fname, int mode ) {
	return copen_pipe( fname, mode, 1 );
}

/*
	Opens the named pipe if it exists, creates it otherwise. If there is a file
	with the given name, and it's not a fifo, then we'll force it to unlink
	in the copen_pipe() call.
	Returns a 'handle' that can be passed to other functions here, or NULL
	on error.
*/
extern void* rfifo_open( char* fname, int mode ) {
	struct stat fstats;

	if( fname == NULL ) {
		return NULL;
	}

	if( stat( fname, &fstats ) == 0 ) {
		if( S_ISFIFO( fstats.st_mode ) ) {
			return copen_pipe( fname, mode, 0 );	// open without unlinking
		}
	} 

	return copen_pipe( fname, mode, 1 );			// not there, not pipe, unlink if something there and then create
}


/*
	Causes us to detect when the last writer has closed the pipe and to return
	a non-nil buffer, with a single 0 to indicate this.
*/
extern void rfifo_detect_close( void* vfifo ) {
	fifo_t*	fifo;

	if( (fifo = (fifo_t *) vfifo ) == NULL ) {
		return;
	}
	
	fifo->close_on_data = 1;
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

	CAUTION:
	This has issues if the pipe buffer is small (it shouldn't be, but 
	if something cannot be sized to a large buffer and the sender must
	write multiple times, this could not behave as expected. Use either
	rfifo_blk_rdln() to read a line at a time with the assumption that 
	an empty line (one byte returned which is a 0) marks the end of the 
	block. The rfifo_readln() function can also be used if non-blocking
	behavour is desired. A NULL return indciates nothing read as long
	as errno == EAGAIN; a single character buffer (0) indicates an 
	empty line was read.
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
	memset( rbuf, 0, sizeof( char ) * RBUF_SIZE );						// only to keep valgrind from twisting its knickers

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

/*
	Non-blocking read, reads up to the next newline. This returns nil if no data 
	was available to read (when errno is EAGAIN, error otherwise). When an empty 
	buffer is read, the pointer returned will not be nil and the buffer will have
	a single character (0).
*/
extern char* rfifo_readln( void* vfifo ) {
	fifo_t* fifo;
	char	*rbuf = NULL;		// return buffer
	char	*nb;				// next buffer from flow manager
	int		len;				// actual byte count read from fifo
	int		tlen = 0;			// total bytes put into buffer for caller

	if( (fifo = (fifo_t *) vfifo) == NULL ) {
		return NULL;
	}

	do {
		if( (nb = ng_flow_get( fifo->flow, '\n' )) != NULL ) {
			len = strlen( nb );
			if( (rbuf = (char *) malloc( sizeof( char ) * (len + 2)  )) == NULL ) {
				return NULL;
			}
			memcpy( rbuf, nb, len );
			rbuf[len] = '\n';
			rbuf[len+1] = 0;

			return rbuf;
		}	

		if( (len = read( fifo->fd, fifo->rbuf, RBUF_SIZE )) > 0 ) {		// something read
			ng_flow_ref( fifo->flow, fifo->rbuf, len );					// register the buffer (zero copy)
			if( fifo->wfd >= 0 && fifo->close_on_data ) {
				close( fifo->wfd );
				fifo->wfd = -1;
			}
		} else {
			if( len == 0 ) {
				return strdup( "" );
			}
		}
	} while( len > 0 );

	return rbuf;			// errno will be left set when we return here; EAGAIN indicates would block and anything else is an error.
}

/*
	Read from the pipe and stash into the flow buffer until we see a newline at
	which point a buffer (newline and zero terminated) is returned. The fifo is
	open in non-blocking mode; this function will block until a complete new-line
	buffer is read. The caller is responsible for freeing the returned buffer.

	If the return is nil, the caller can assume an error; errno should indicate
	the problem.
*/
extern char* rfifo_blk_readln( void* vfifo ) {
	char* nb = NULL;

	while( ! nb ) {
		if( ! (nb = rfifo_readln( vfifo )) ) {
			usleep( 250000 );					// ~.25 of a second
		}
	}

	return nb;
}

/*
	Blocking read with a timeout. Timeout is in tenths of seconds and is approximate
	as we don't check for interrupts shorting out a sleep call.
*/
extern char* rfifo_to_readln( void* vfifo, int to ) {
	char* nb = NULL;

	if( to <= 0 ) {									// full on blocking if timeout is 0
		nb = rfifo_blk_readln( vfifo );
	} else {
		while( ! nb && to > 0  ) {
			if( ! (nb = rfifo_readln( vfifo )) ) {
				usleep( 100000 );					// ~.1 of a second
				to--;
			}
		}
	}

	return nb;
}

