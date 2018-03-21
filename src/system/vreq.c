// :vi noet tw=4 ts=4:
/*
	Mnemonic:	vreq.c
	Abstract:	This is a tool to provide a limited interface to VFd through its pipe 
				in such a manner that a regular user can execute the command
				without sudo (the intent is that this command is installed suid root).
				Vreq will accept only the commands which are allowed by all users
				as opposed to explicitly checking to see if the executing user is 
				indeed root and allowing privledged commands if the uid/euid is root;
				the limit makes it easier to vet from a security point of view at the 
				expense of some duplication of the code with iplex (maybe iplex should 
				invoke this for the generic user commands).
	Author:		E. Scott Daniels
	Date:		03 April 2017
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <vfdlib.h>

#define VERSION "v1.0"			// pull from mk file eventually

typedef struct {
	int		argc;				// number of unparsed command line positional parms
	char	**argv;				// first positional parm
	char*	vfd_channel;		// channel to vfd (fifo file name most likely)
	char*	resp_channel;		// where we create fifo for response
} cl_parms_t;

/*
	Present a usage message.
*/
static void usage( void ) {
	const char *version = VERSION "    build: " __DATE__ " " __TIME__;

	fprintf( stdout, "vreq [-c channel-path] {dump | show {all|n|ex|pfs} | ping}\n" );
}

/*
	Get the parm pointed to by pidx unless it's out of range. If oor
	then we abort and error. pidx is a pointer to the index of 
	the next parameter in argv to use. It must be >=1 and < argc.
*/
static char* get_nxt( int argc, char** argv, int* pidx ) {
	if( *pidx >= argc || *pidx <= 0 || argv[*pidx] == NULL ) {
		fprintf( stderr, "abort: missing command line data; unable to parse command line\n" );
		usage( );
		exit( 1 );
	}

	(*pidx)++;
	return argv[(*pidx-1)];
}

/*
	Crack the command line args leaving the parms argc/argv info at the positional
	parms.
*/
extern cl_parms_t* crack_args( int argc, char** argv ) {
	cl_parms_t*	parms;
	int		parg = 1;		// arg being parsed
	char*	opt;			// next option string to parse
	char	wbuf[1024];		// working buffer
	

	if( (parms = (cl_parms_t *) malloc( sizeof( cl_parms_t ) )) == NULL ) {
		fprintf( stderr, "abort: cannot allocate space for parms\n" );
		exit( 1 );
	}
	memset( parms, 0, sizeof( *parms ) );
	parms->vfd_channel = "/var/lib/vfd/request";		// the standard place

	while( parg < argc ) {
		opt = argv[parg++];						// parg at the next parameter
		if( *opt != '-' ) { 
			parg--;
			break;
		} else {
			if( strcmp( opt, "--" ) == 0 ) {
				break;
			}
		}

		for( opt++; *opt; opt++ ) {
			switch( *opt ) {
				case 'c':							// alternate fifo (channel) that VFd is reading from
					parms->vfd_channel = get_nxt( argc, argv, &parg );		// get parm and inc parg
					break;
				
				case '?':
					usage();
					exit( 0 );
					break;


				default:
					fprintf( stderr, "unrecognised commandline flag: %c\n", *opt );
					usage();
					exit( 1 );
			}
		}
	}

	snprintf( wbuf, sizeof( wbuf ), "%s_vreq.%d", parms->vfd_channel, getpid() );		// base respons on the inbound channel name
	parms->resp_channel = strdup( wbuf );

	parms->argc = argc - parg;	// set up positional parameter info
	if( parg < argc ) {
		parms->argv = &argv[parg];
	} else {
		parms->argv = NULL;
	}
	
	return parms;
}


/*
	Open the VFd request channel.
*/
int open_rchannel( char* v_chan ) {
	int vfifo;

	if( (vfifo = open( v_chan, O_RDWR, 0 )) < 0 ) {
		fprintf( stderr, "unable to open VFd request channel: %s: %s\n", v_chan, strerror( errno ) );
		return -1;
	}

	return vfifo;
}


/*
	Send a show request to VFd and return good (1) if the caller should wait 
	for the response on our request channel. V_channel is the file name that
	VFd is listening on (pipe/channel).
*/
int do_show( int argc, char** argv, char* v_channel, char* r_channel ) {
	int	vfifo = -1;						// file des for VFd's fifo where we write
	int	rc = 0;								// 0 is bad
	int	log_level = 0;
	char	buf[2048];
	char	fmt[1024];


	if( (vfifo = open_rchannel( v_channel )) >= 0 ) {
		snprintf( fmt, sizeof( fmt ), "{ \"action\": \"show\", \"vfd_rid\": \"vreq-static-req\", \"params\": { \"resource\": \"%%s\", \"loglevel\": %d, \"r_fifo\": \"%s\"} }\n", 
			log_level, r_channel );

		switch( *(argv[0]) ) {
			case 'a':					// all
				snprintf( buf, sizeof( buf ), fmt, "all" );
				rc = 1;
				break;

			case 'e':					// extended stats
				snprintf( buf, sizeof( buf ), fmt, "extended" );
				rc = 1;
				break;

			case 'p':					// just pfs
				snprintf( buf, sizeof( buf ), fmt, "pfs" );
				rc = 1;
				break;
	
			default:
				fprintf( stderr, "unrecognised option: %s\n", argv[0] );
				break;
		}
	}

	if( rc ) {									// all is well above -- send it on
		write( vfifo, buf, strlen( buf ) );
	}

	if( vfifo >= 0 ) {
		close( vfifo );
	}

	return rc;

}

/*
	Send a dump request.
*/
int do_dump( char* v_channel, char* r_channel ) {
	int	vfifo = -1;						// file des for VFd's fifo where we write
	int	rc = 0;							// 0 is bad
	char	buf[2048];


	if( (vfifo = open_rchannel( v_channel )) >= 0 ) {
		snprintf( buf, sizeof( buf ), "{ \"action\": \"dump\", \"vfd_rid\": \"vreq-static-req\", \"params\": { \"resource\": null, \"loglevel\": 0, \"r_fifo\": \"%s\"} }\n", r_channel );
		write( vfifo, buf, strlen( buf ) );
		rc = 1;
	}

	if( vfifo >= 0 ) {
		close( vfifo );
	}

	return rc;
}

/*
	Send a ping request.
*/
int do_ping( char* v_channel, char* r_channel ) {
	int	vfifo = -1;						// file des for VFd's fifo where we write
	int	rc = 0;								// 0 is bad
	char	buf[2048];


	if( (vfifo = open_rchannel( v_channel )) >= 0 ) {
		snprintf( buf, sizeof( buf ), "{ \"action\": \"ping\", \"vfd_rid\": \"vreq-static-req\", \"params\": { \"resource\": null, \"loglevel\": 0, \"r_fifo\": \"%s\"} }\n", r_channel );
		write( vfifo, buf, strlen( buf ) );
		rc = 1;
	}

	if( vfifo >= 0 ) {
		close( vfifo );
	}

	return rc;
}


int main( int argc, char** argv ) {
	cl_parms_t*	parms;
	void*	resp_fifo;					// fifo where vfd will write it's response
	//char	resp_fname[128];
	int		ok2read = 0;

	parms = crack_args( argc, argv );

	//snprintf( resp_fname, sizeof( resp_fname ), "/tmp/PID%d.resp", getpid() );

	if( (resp_fifo = rfifo_create( parms->resp_channel, 0666 )) == NULL ) {
		fprintf( stderr, "unable to create response channel: %s: %s\n", parms->resp_channel, strerror( errno ) );
		exit( 1 );
	}
	rfifo_detect_close( resp_fifo );		// detect when other side closes the fifo; will give us an empty buffer on the next read


	if( parms == NULL || parms->argc < 1 ) {
		usage();
		exit( 1 );
	}

	switch( *(parms->argv[0]) ) {		// jump table based on first char faster than nested strcmps; for now all are unique on 1st char
		case 'd':
			ok2read = do_dump( parms->vfd_channel, parms->resp_channel );
			break;

		case 'p':				// for now we assume ping
			ok2read = do_ping( parms->vfd_channel, parms->resp_channel );
			break;
			
		case 's':				// for now we assume show
			ok2read = do_show( parms->argc-1, &parms->argv[1], parms->vfd_channel, parms->resp_channel );
			break;
			

		default:
			fprintf( stderr, "unrecognised request: %s\n", parms->argv[0] );
			usage();
			exit( 1 );
			break;
	}

	if( ok2read ) {
		char* rbuf;
		int		timeout = 100;			// wait 10 seconds for initial response

		while( ((rbuf = rfifo_to_readln( resp_fifo, timeout )) != NULL) && *rbuf ) {		// blocking read until we see an empty line or nil (error)
			fprintf( stdout, "%s", rbuf );													// buffers should be newline terminated
			free( rbuf );
			timeout = 0;																	// full blocking after initial read
		}
	} else {
		fprintf( stderr, "internal mishap: not waiting for response; above error messages may help determine the cause of the problem\n" );
	}

	rfifo_close( resp_fifo );
	unlink( parms->resp_channel );

	return 0;
}
