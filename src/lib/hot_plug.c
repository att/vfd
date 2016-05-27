// :vim sw=4 ts=4 nocindent noexpandtab:

/*
	Mnemonic:	hot_plug.c
	Abstract:	Functions that allow the creator of a VF configuration file
				to have a command executed just after VFd restart, and just
				before VFd shutdown.  This allows for a device to be hot-unplugged, 
				reinserted, etc. if it is necessary. Some drivers must have the device
				removed/added by the virtulalisation manager in order for the VM to 
				continue working after VFd has been restarted. 
	Author:		E. Scott Daniels
	Date:		26 May 2016

	Mods:
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

// -------------------------------------------------------------------------------------
#define SFREE(p) if((p)){free(p);}			// safe free (free shouldn't balk on nil, but don't chance it)



/*
	Run the user command as the user given (e.g. sudo -u user command). User command output goes to 
	either stderr or stdout and won't go to a bleat log file.
*/
int user_cmd( uid_t uid, char* cmd ) {
	char*	cmd_buf;
	int		cmd_len;
	int		rc;

	if( uid < 0 ) {
		return -1;
	}

	cmd_len = strlen( cmd ) + 128;
	cmd_buf = (char *) malloc( sizeof( char ) * cmd_len ); 
	if( cmd_buf == NULL ) {
		return -1;
	}

	snprintf( cmd_buf, cmd_len, "sudo -u '#%d' %s", uid, cmd );
	rc =  system( cmd_buf );
	free( cmd_buf );
	return rc;
}
