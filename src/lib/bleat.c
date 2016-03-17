// :vi sw=4 ts=4:
/*
	Mnemonic:	bleat.c
	Abstract:	Simple bleater to write verbose messages.
	Author:		E. Scott Daniels
	Date:		09 March 2016
*/

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdarg.h>

#include "vfdlib.h"


static 			cur_level = 0;
static 			old_level = 0;
static FILE*	log = NULL;
static			log_is_std = 1;		// set when log is stderr so we don't close it

/*
	Return a pretty time for the message
*/
static char* pretty_time( time_t ts ) {
	char	buf[128];	
	struct tm	t;

	gmtime_r( (const time_t *) &ts, &t );
	
	snprintf( buf, sizeof( buf ), "%d/%02d/%02d %02d:%02d:%02dZ", t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec );

	return strdup( buf );
}

/*
	Generate a filename given a 'base' with a suffix of the current date.
*/
static char* add_date( time_t ts, char* fbase ) {
	char	buf[128];	
	struct tm	t;

	gmtime_r( (const time_t *) &ts, &t );
	
	snprintf( buf, sizeof( buf ), "%s.%d%02d%02d", fbase, t.tm_year+1900, t.tm_mon+1, t.tm_mday );

	return strdup( buf );
}


/*
	set the level; does not affect the value saved with a push.
*/
extern int bleat_set_lvl( int l ) {
	int r;

	r = cur_level;
	if( l >= 0 ) {
		cur_level = l;
	}

	return r;
}

/*
	set level to l, and save the current level (temporary changes)
*/
extern void bleat_push_lvl( int l ) {
	
	old_level = cur_level;
	if( l >= 0 ) {
		cur_level = l;
	}
}


/*
	Pushes the current level, or l; which ever is greater. 
*/
extern void bleat_push_glvl( int l ) {
	old_level = cur_level;
	if( l > cur_level ) {
		cur_level = l;
	}
}

/*
	Pop the old level; leaves old level set, so multiple pops
	result in the same value until another push is called.
*/
extern void bleat_pop_lvl( ) {
	cur_level = old_level;
}

/*
	Returns true if bleat_printf() is called with level 1 and would
	result in a message being written (good for a loop of writes
	without the small overhead of checking the level for each write).
*/
extern int bleat_will_it( int l ) {
	return l <= cur_level;
}

/*
	Set the file where we will write and open it.
	Returns 0 if good; !0 otherwise. If ad_flag is true then we add
	a datestamp to the log file.
*/
extern int bleat_set_log( char* fname, int ad_flag ) {
	FILE*	f;

	if( strcmp( fname, "stderr" ) == 0 ) {
		if( ! log_is_std ) {
			fclose( log );
		}

		log_is_std = 1;
		log = stderr;
		return 0;
	}

	if( ad_flag ) {
		fname = add_date( time( NULL ), fname );		// build the filename with a date suffix
	}
	if( (f = fopen( fname, "a" )) == NULL ) {
		if( ad_flag ) {
			free( fname );
		}
		return 1;
	}

	if( ! log_is_std ) {
		fclose( log  );				// open ok, can safely close the old one, but only if not stderr
	}
	log_is_std = 0;
	log = f;

	if( ad_flag ) {
		free( fname );
	}
	return 0;
}

/*
	Send a message  to the log file if the level indicated is >= to the 
	current level, otherwise nothing.

	(Shamelessly stolen from Ningaui, and then modified.)
*/
void bleat_printf( int vlevel, const char *fmt, ... )
{
	va_list	argp;			/* pointer at variable arguments */
	char	obuf[8192];		/* final msg buf - allow ng_buffer to caller, 1k for header*/
	time_t	 gmt;			// timestamp
	char	*uidx; 			/* index into output buffer for user message */
	int	space; 				/* amount of space in obuf for user message */
	int	hlen;  				/* size of header in output buffer */
	char*	ptime;

	if( log == NULL ) {		// first call; initialise if not set
		log = stderr;
		log_is_std = 1;
	}

	if( vlevel > cur_level  )		// mod -- ningaui caps at 0x0f
		return;

 	gmt = time(  NULL );				// current time
	ptime = pretty_time( gmt );
 
	snprintf( obuf, sizeof( obuf ), "%lld %s [%d] ", (long long) gmt, ptime, vlevel );
	free( ptime );

	hlen = strlen( obuf );          
	space = sizeof( obuf ) - hlen;        /* space for user message */
	uidx = obuf + hlen;                   /* point past header stuff */

	va_start( argp, fmt );                      /* point to first variable arg */
	vsnprintf( uidx, space - 1, fmt, argp );	// bang the user message onto our header (argp not valid after call)
	va_end( argp );                             /* cleanup of variable arg stuff */

	fprintf(  log, "%s\n", obuf );
	fflush( log );
}


