
/*
	Mnemonic:	listfiles.c
	Abstract:	Simple directory read to list config files in a given directory.
	Author:		E. Scott Daniels
	Date:		04 Mar 2016

	Mod:		01 Jun 2016 - Added ability to create a set of old files
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
#include <time.h>

#include "vfdlib.h"


						// flags for mk_list
#define FL_QUALIFY		0x01	// MUST be 1 (qualify the name by adding directory)
#define FL_IS_SUFFIX	0x02	// match value passed is a suffix

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
	Internal funciton which supports either a prefix or suffix match

	Files in the directory (dname) are listed and added to the list which is 
	returned if:
		1) match is "" and the filename does NOT begin with a dot 
		OR
		2) match is not empty, and the first n characters of the file name
		   are the same as the match string (prefix match)
		OR
		3) match is not empty, and FL_IS_SUFFIX is set in flags, and the 
		   portion of the filename after the last dot matches the string in 
		   match exactly.  If the filename does not have a dot (.) then the 
		   match is considered false and the file is not included.


	If the FL_QUALIFY bit is set in flags, then the directory name is prepended
	to the file name in the list.

	If len is not NULL, then the number of files placed into the list is returned.
	If len is NULL, then we bail -- it's meaningless to return the list without
	communicating how long it is, and free_list() requires the lenght to work 
	properly.
*/
static char** mk_list( char* dname, const char* match, int flags, int* len ) {
	char**	list = NULL;
	int		lidx = 0;			// current index into the list
	struct dirent fentry;		// file entry filled in by dirread()
	struct dirent* fep;			// pointer to struct returned by dirread()
	DIR*	dfd;				// directory "fd"
	int		state;				// read state
	char*	cp;					// pointer into the filename
	char*	qbuf;				// buffer for building qualified path in
	int		qblen = 0;			// length of the space allocated for qbuf
	int		snarf;
	int		cmp_len;			// length for prefix compare


	memset( &fentry, 0, sizeof( fentry ) );					// probably unecessary, but keeps valgrind from complaining as readdir_r may not completely populate the struct

	if( dname == NULL || match == NULL || len == NULL ) {
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

	if( flags & FL_QUALIFY ) {
		qblen = (strlen( dname ) + 258 ); 										//258 = 256 is max len of filename in dirent, + 1 for slant + 1 for nil ch
		if( (qbuf = (char *) malloc( sizeof( char ) * qblen )) == NULL ) {		
			errno = ENOMEM;
			return NULL;
		}
	}

	cmp_len = strlen( match );

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

		snarf = 0;
		if( ! *match ) {
			if(  *fentry.d_name != '.' ) {					// empty match (any) and not an unimportant (.*) file
				snarf = 1;
			}
		} else {
			if( flags & FL_IS_SUFFIX ) {
				cp = strrchr( fentry.d_name, '.' );				// find last separator
				snarf = ( cp  && strcmp( cp+1, match ) == 0);	// save the name if sep is there and suffix matches
			} else {
				snarf = strncmp( fentry.d_name, match, cmp_len ) == 0;		// save if prefix matches
			}
		}
				
		if( snarf ) {
			if( flags & FL_QUALIFY ) {
				snprintf( qbuf, qblen, "%s/%s", dname, fentry.d_name );
				list[lidx] = strdup( qbuf );
			} else {
				list[lidx] = strdup( fentry.d_name );
			}

			lidx++;
			if( lidx >= 1024 ) {
				errno = ENOMEM;							// it will appear as a success if we don't force it
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

/*
	Given a directory name and a suffix (e.g. .cfg), return an array of 
	pointers to filenames in that directory which are of the form 
	*.<suffix> (caller is expected to stat the file to determine type 
	if that is needed.   Suffix is taken as a literal NOT a pattern
	or glob string. The list is terminated with a nil pointer and 
	length is set if the caller provides the len parm.

	If the suffix is given as "", then all files which do NOT begin
	with a dot (.) are returned. 

	If qualifiy is set, then each file name in the list is appended
	to the directory name with a slant inserted. Any other value is 

	A NULL list is returned on error and errno set in most cases.

	NOTE:  Right now the list has a 1K max. It is doubtful that there
		will be a need for more than this, so I didn't bother with
		expansion logic if we encounter more.
*/
extern char** list_files( char* dname, const char* suffix, int qualify, int* len ) {
	return mk_list( dname, suffix, (!!qualify) | FL_IS_SUFFIX, len );
}

/*
	Same as list_files, but only files with a matching prefix are returned.
*/
extern char** list_pfiles( char* dname, const char* prefix, int qualify, int* len ) {
	return mk_list( dname, prefix, (!!qualify), len );
}

/*
	Accepts a list of files and removes any which have a modification time 
	more recent than seconds ago. Ulen is assumed to be the length of the 
	list passed in, and will be used to return the length of the new list.
*/
extern char** rm_new_files( char** flist, int seconds, int *ulen ) {
	char**	olist; 				// list of old files
	int		flen;
	struct stat fstats;
	int i;
	int j;
	time_t	stale_before;		// timestamp -- files older than this are listed


	if( ulen == NULL ) {
		return NULL;
	}
	flen = *ulen;
	*ulen = 0;

	olist = (char **) malloc( sizeof( char * ) * flen );
	if( olist == NULL ) {
		return NULL;
	}
	
	stale_before = time( NULL ) - seconds;
	j = 0;
	for( i = 0; i < flen; i ++ ) {
		if( stat( flist[i], &fstats ) == 0 ) {
			if( S_ISREG( fstats.st_mode ) && fstats.st_mtime < stale_before ) {				// regular file
				olist[j++] = flist[i];
				flist[i] = NULL;
			}
		}	
	}

	*ulen = j;
	return olist;
}

/*
	Generate a list of files which have a last mod time more than seconds ago.
	If qualify is set the filenames returned in the list will be fully qualified.
	The list will contain all files that are important (. files excluded).
*/
extern char** list_old_files( char* dname, int qualify, int seconds, int *ulen ) {
	char**	flist;				// list of files in the named directory
	char**	olist; 				// list of old files
	int		flen;
	struct stat fstats;
	int i;
	int j;
	time_t	stale_before;		// timestamp -- files older than this are listed


	if( ulen == NULL ) {
		return NULL;
	}
	*ulen = 0;

	flist = list_files( dname, "", qualify, &flen );
	if( flist == NULL || flen <= 0 ) {
		return NULL;
	}

	*ulen = flen;
	olist = rm_new_files( flist, seconds, ulen );
	free_list( flist, flen );
	return olist;
}

