// :vim sw=4 ts=4 nocindent noexpandtab:

/*
	Mnemonic:	hot_plug.c
	Abstract:	Functions to allow VFd to 'hot plug' (remove or add) a device given 
				a set of parms in the config file. 
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
	Run virsh detach-device with the supplied parameters. The parameters
	may not contian a ';' as this could allow command stacking/chaining
	which we consider to be an attack.  If the string contains a ';' then
	the command is not executed.

	Returns -1 if the command could not be executed, otherwise returns the 
	return code from the system() call.
*/
int virsh_detach( char* parms ) {
	char*	cmd_buf;
	int		cmd_len;
	int		rc;

	if( strchr( parms, ';' ) != NULL ) {
		return -1;
	}

	cmd_len = strlen( parms ) + 128;
	cmd_buf = (char *) malloc( sizeof( char ) * cmd_len ); 
	if( cmd_buf == NULL ) {
		return -1;
	}

	snprintf( cmd_buf, cmd_len, "virsh detach-device %s", parms );
	rc =  system( cmd_buf );
	free( cmd_buf );
	return rc;
}


/*
	Run virsh attach-interface with the supplied parameters. The parameters
	may not contian a ';' as this could allow command stacking/chaining
	which we consider to be an attack.  If the string contains a ';' then
	the command is not executed.

	Returns -1 if the command could not be executed, otherwise returns the 
	return code from the system() call.
*/
int virsh_attach( char* parms ) {
	char*	cmd_buf;
	int		cmd_len;
	int		rc;

	if( strchr( parms, ';' ) != NULL ) {
		return -1;
	}

	cmd_len = strlen( parms ) + 128;
	cmd_buf = (char *) malloc( sizeof( char ) * cmd_len ); 
	if( cmd_buf == NULL ) {
		return -1;
	}

	snprintf( cmd_buf, cmd_len, "virsh attach-interface %s", parms );
	rc =  system( cmd_buf );
	free( cmd_buf );
	return rc;
}

