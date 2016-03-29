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


// --------------------- no way round these ------------------
static int		cur_level = 0;
static int		old_level = 0;
static FILE*	log = NULL;
static int		log_is_std = 1;		// set when log is stderr so we don't close it
static	time_t	time2flip = 0;		// time at which we need to flip the log (or first write after)
static time_t	log_cycle = 0;		// cycle time for the log if dated
static char*	fname_base = NULL;	// base name of the file that we add the date to

// -- private -------------------------------------------------------------------------
/*
	Compute the next time we need to flip the log. The base is the roll time
	(e.g. every 3600 seconds for hourly, 86400 seconds for daily).
*/
static time_t get_flip_time( int base ) {
	time_t now;

	now = time( NULL );
	return  (now - (now % base)) + base;
}

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
	Cycle is the number of seconds we're rolling the log.  The filename will be set
	such that it is the most recent multiple of cycle (e.g. at 7pm, with a 6 hour 
	cycle, the file would be stamped 18:00).
*/
static char* add_date( char* fbase, time_t cycle ) {
	char	buf[128];	
	struct tm	t;
	time_t	base_time;			// time baed on cycle

	base_time = time( NULL );
	base_time -= base_time % cycle;

	gmtime_r( (const time_t *) &base_time, &t );
	
	if( cycle > 86399 ) {
		snprintf( buf, sizeof( buf ), "%s.%d%02d%02d", fbase, t.tm_year+1900, t.tm_mon+1, t.tm_mday );
	} else {
		if( cycle > 3599 ) {
			snprintf( buf, sizeof( buf ), "%s.%d%02d%02d.%02d", fbase, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour );
		} else {	
			snprintf( buf, sizeof( buf ), "%s.%d%02d%02d.%02d%02d", fbase, t.tm_year+1900, t.tm_mon+1, t.tm_mday, t.tm_hour, t.tm_min );
		}
	}

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
	Return the timestamp when the next flip will happen.
*/
extern time_t bleat_next_roll( ) {
	return time2flip;
}

/*
	Set the file where we will write and open it.
	Returns 0 if good; !0 otherwise. If ad_flag is true then we add
	a datestamp to the log file and cause the log to roll at midnght.
	Add flag is a cycle value 86400 causes the file to be cycled every
	midnight, n*6300 causes it to be cycled every n hours (offset off of 
	midnight), and n*60 causes it to be cycled every n minutes). File names
	are suffixed with a suitble date/time stamp when ad_flag >0. 
*/
extern int bleat_set_log( char* fname, int ad_flag ) {
	FILE*	f;

	if( fname == NULL ) {
		return 1;
	}

	if( ad_flag && ad_flag < 60 ) {
		ad_flag = 60;
	}

	if( strcmp( fname, "stderr" ) == 0 ) {
		if( ! log_is_std ) {
			fclose( log );
		}

		log_is_std = 1;
		log = stderr;
		time2flip = 0;
		log_cycle = 0;
		if( fname_base ) {
			free( fname_base );
			fname_base = NULL;
		}
		if( fname ) {
			free( fname );
		}
		return 0;
	}

	if( ad_flag ) {
		if( fname_base && strcmp( fname, fname_base ) != 0 ) {		// user could rename, don't leak
			free( fname_base );
		}
		fname_base = strdup( fname );
		fname = add_date( fname_base, ad_flag );		// build the filename with a date suffix
		time2flip = get_flip_time( ad_flag );			// right now we'll flip on midnight boundaries
		log_cycle = ad_flag;
	}
	if( (f = fopen( fname, "a" )) == NULL ) {
		if( ad_flag ) {
			free( fname );
			free( fname_base );
			fname_base = NULL;
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
	} else {
		if( time2flip && time2flip < time( NULL ) ) {			// first bleat after flip time, close and reoopen the log
			bleat_set_log( fname_base, log_cycle );
		}
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


