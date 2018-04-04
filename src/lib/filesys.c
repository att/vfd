
/*
	Mnemonic:	filesys.c
	Abstract:	A collection of filesystem chores wrapped with error checking
				and additional functions.
	Date:		14 February 2018
	Author:		E. Scott Daniels
*/

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "vfdlib.h"

/*
	Delete the file. If backup is true, then the file is 
	just renamed to have a trailing dash (-).  Returns true
	if successful. 
*/
extern int rm_file( const_str fname, int backup ) {
	char wbuf[4096];

	if( backup ) {											// need to keep the old by renaming it with a trailing -
		snprintf( wbuf, sizeof( wbuf ), "%s-", fname );
		return rename( fname, wbuf ) >= 0;
	}

	return unlink( fname ) >= 0;
}

/*
	Move the file to the indicated file or directory. If target
	has a trailing slash we assume it's a directory and bang on
	the filename, otherwise we use the name as is.

	We assume that the files can be moved with the rname() system
	call which might require that the files are in the same filesystem.

	Returns true on success; false otherwise and errno should be
	set to reflect the underlying system difficulty.
*/
extern int mv_file( const_str fname, char* target ) {
	char	wbuf[4096];
	char*	esp;			// pointer to the last slant in the file name
	const_str base;			// if fname is a full path, then this points just to the base

	esp = strrchr( target, '/' );
	if( esp != NULL && *(esp+1) == 0 ) {								// it's a directory name 
		if( (base = strrchr( fname, '/')) != NULL ) {
			if( ! ++base ) {											// src can't have a trailing slant
				errno = EINVAL;
				return 0;
			}
		} else {
			base = fname;							// filename has no path component, just use it
		}

		snprintf( wbuf, sizeof( wbuf ), "%s%s", target, base );		// build dest name
	} else {
		snprintf( wbuf, sizeof( wbuf ), "%s", target );
	}

	return rename( fname, wbuf ) >=0;
}

/*
	Ensure that a directory exists; create it if it does not.
	Returns false on failure; true on success. On failure errno should
	reflect the reason.
*/
extern int ensure_dir( const_str pathname ) {
	struct	stat fsts;
	int		state;
	char*	tok;
	char*	dpath;					// duplicate path so we preserve caller's
	char	wbuf[4096];
	char*	stp;					// pointer to tokeniser state
	int		make_path = 0;			// we set this when we encounter the first node in the path that doesn't exist

	if( (state = stat( pathname, &fsts )) < 0 ) {
		if( errno != ENOENT ) {							// fatal if anything other than no entry
			return 0;
		}

		if( (dpath = strdup( pathname )) == NULL ) {		// dup so we can chop it
			errno = ENOMEM;
			return 0;
		}

		*wbuf = 0;
		tok = dpath;
		strtok_r( *dpath == '/' ? dpath+1 : dpath, "/", &stp );		// split the first token off
		while( tok ) {
			strcat( wbuf, tok );									// work down the path chain
			if( ! make_path ) {
				if( (state = stat( wbuf, &fsts )) < 0 ) {			// see if this is there
					if( errno != ENOENT ) {
						return 0;
					}

					make_path = 1;
				}
			}

			if( make_path ) {										// need to start building from here down
				if( mkdir( wbuf, 0775 ) < 0 ) {
					return 0;
				}
			}

			tok = strtok_r( NULL, "/", &stp );						// get next token
			strcat( wbuf, "/" );
		}

		errno = 0;
		return 1;
	}

	return S_ISDIR( fsts.st_mode );
}

/*
	Returns the file mode or 0 if there is no file.
*/
extern mode_t file_mode( const_str pathname ) {
	struct	stat fsts;
	int		state;
	char*	tok;
	char*	dpath;					// duplicate path so we preserve caller's
	char	wbuf[4096];
	char*	stp;					// pointer to tokeniser state
	int		make_path = 0;			// we set this when we encounter the first node in the path that doesn't exist

	if( (state = stat( pathname, &fsts )) >= 0 ) {
		return fsts.st_mode;
	}

	return 0;
}

/*
	Convenience funcitons
*/
extern int is_dir( const_str pathname ) {
	mode_t mode;

	if( (mode = file_mode( pathname )) != 0 ) {
		return (int) S_ISDIR( mode );
	}

	return 0;
}

/*
	Return true if it's a plain file
*/
extern int is_file( const_str pathname ) {
	unsigned long mode;

	if( (mode = file_mode( pathname )) != 0 ) {
		return S_ISREG( mode );
	}

	return 0;
}

extern int is_fifo( const_str pathname ) {
	unsigned long mode;

	if( (mode = file_mode( pathname )) != 0 ) {
		return S_ISFIFO( mode );
	}

	return 0;
}

/*
	Returns true if the file exists in some shape or form; zero if it
	does not.
*/
extern int file_exists( const_str pathname ) {
	return file_mode( pathname ) != 0;
}

/*
	Copy path1 (src) to path2 (dest) and if rm_src is set to true path1 is unlinked on
	a successful copy (effectively a move which can span filesystems).
	If path2 is a directory, then the basename from path1 will be appended
	to the path2 string. On error errno should be set; ENOENT indicates the
	source path did not exist.
*/
extern int cp_file( const_str path1, const_str path2, int rm_src ) {
	char*	dest = NULL;
	const_str	cp = NULL;
	int		len = 0;
	int		sfd;				// soruce file des
	int		dfd;				// dest file des
	char	rbuf[4096];

	if( ! file_exists( path1 ) ) {
		errno = ENOENT;
		return 0;
	}

	if( is_dir( path2 ) ) {
		len = strlen( path1 ) + strlen( path2 ) + 2;		// max room needed for target string (including / and nil)
		if( (dest =  (char*) malloc( sizeof( char ) * len )) == NULL ) {
			errno = ENOMEM;
			return 0;
		}

		
		if( (cp = strrchr( path1, '/' )) == NULL ) {		// base name
			cp = path1;
		} else {
			cp++;
		}

		snprintf( dest, len, "%s/%s", path2, cp );		// build the full dest path
	} else {
		dest = strdup( path2 );								// so we can unditionally free later
	}

	if( (sfd = open( path1, O_RDONLY )) < 0 ) {
		free( dest );
		return 0;
	}

	if( (dfd = open( dest, O_WRONLY | O_CREAT | O_TRUNC, 0644 )) < 0 ) {
		close( sfd );
		free( dest );
		return 0;
	}

	while( (len = read( sfd, rbuf, sizeof( rbuf ) )) > 0 ) {
		write( dfd, rbuf, len );
	}

	free( dest );
	close( sfd );
	if( close( dfd ) ) {		// error if not zero, report and bail
		return 0;
	}

	if( rm_src ) {
		return unlink( path1 ) >= 0;
	}

	errno = 0;
	return 1;
}
