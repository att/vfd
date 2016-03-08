
/*
	Mnemonic:	listfiles.c
	Abstract:	Simple directory read to list config files in a given directory.
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
#include <dirent.h>

#include "vfdlib.h"

/*
	Given a list, free it.  Assumes the pointers in the list are
	malloced memory. The first size entries are freed and then 
	the list itself is freed.
*/
extern void free_list( char** list, int size ) {
	char**	lp;

	for( lp = list; size; size-- ) {
		if( *lp ) {
			free( *lp );
		}

		lp++;
	}

	free( list );
}

/*
	Given a directory name and a suffix (e.g. .cfg), return an array of 
	pointers to filenames in that directory which are of the form 
	*.<suffix> (caller is expected to stat the file to determine type 
	if that is needed.   Suffix is taken as a literal NOT a pattern
	or glob string. The list is terminated with a nil pointer and 
	length is set if the caller provides the len parm.

	If qualifiy is set, then each file name in the list is appended
	to the directory name with a slant inserted. 

	A NULL list is returned on error and errno set in most cases.

	NOTE:  Right now the list has a 1K max. It is doubtful that there
		will be a need for more than this, so I didn't bother with
		expansion logic if we encounter more.
*/
extern char** list_files( char* dname, char* suffix, int qualify, int* len ) {
	char**	list = NULL;
	int		lidx = 0;			// current index into the list
	struct dirent fentry;		// file entry filled in by dirread()
	struct dirent* fep;			// pointer to struct returned by dirread()
	DIR*	dfd;				// directory "fd"
	int		state;				// read state
	char*	cp;					// pointer into the filename
	char*	qbuf;				// buffer for building qualified path in
	int		qblen = 0;			// length of the space allocated for qbuf


	if( dname == NULL || suffix == NULL ) {
		errno = ENOENT;										// simulate system level error
		return NULL;
	}

	if( (dfd = opendir( (const char*) dname )) == NULL ) {
		return NULL;
	}

	if( (list = (char **) malloc( sizeof( *list ) * 1024 )) == NULL ) {			// in a hurry not doing list expansion at the moment, so max 1024 supported
		errno = ENOMEM;
		return NULL;
	}

	if( qualify ) {
		qblen = (strlen( dname ) + 258 ); 										//258 = 256 is max len of filename in dirent, + 1 for slant + 1 for nil ch
		if( (qbuf = (char *) malloc( sizeof( char ) * qblen )) == NULL ) {		
			errno = ENOMEM;
			return NULL;
		}
	}

	while( (state = readdir_r( dfd, &fentry, &fep )) == 0 ) {
		if( fep == NULL ) {							 							// succes and nil pointer indicate no more entries
			list[lidx] = NULL;
			if( len != NULL ) {
				*len = lidx;													// return the number of entries if user supplied pointer
			}

			closedir( dfd );
			if( qbuf ) {
				free( qbuf );
			}
			return list;
		}

		cp = strrchr( fentry.d_name, '.' );				// find last separator
		if( cp  && strcmp( cp+1, suffix ) == 0 ) {		// match
			if( qualify ) {
				snprintf( qbuf, qblen, "%s/%s", dname, fentry.d_name );
				list[lidx] = strdup( qbuf );
			} else {
				list[lidx] = strdup( fentry.d_name );
			}

			lidx++;
			if( lidx >= 1024 ) {
				errno = ENOMEM;							// otherwise it will appear as a success
				free_list( list, lidx );
				closedir( dfd );
				if( qbuf ) {
					free( qbuf );
				}
				return NULL;
			}
		}
	}

	errno = state;
	free_list( list, lidx );
	closedir( dfd );
	if( qbuf ) {
		free( qbuf );
	}
	return NULL;
}
