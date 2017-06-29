//:vi sw=4 ts=4 noet:

/*
	Mnemonic:	id_mgr.c
	Abstract:	Functions to manage a range of numeric IDs.
				Create a new id manager with mk_idm() and then the caller can allocate
				new IDs in the range of 0 through num_ids-1, and can return them
				when finshed so they can be allocated again. 

	Author:		E. Scott Daniels
	Date:		28 June 2017

	Mod:
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


typedef struct id {
	int	num_ids;				// number of ids supported
	int	nallocated;				// number of ids allocated
	int pool_len;				// bytes allocated in the pool
	unsigned char *bit_pool;		// bits that represent the allocated ids
} idm_t;

/*
	Make a new manager block, ititialise it and return. Num_ids is the number
	of total IDs to support.
*/
extern void* mk_idm( int num_ids ) {
	idm_t* id;

	id = (idm_t *) malloc( sizeof( idm_t ) );
	if( id == NULL ) {
		return NULL;
	}

	id->pool_len = sizeof( unsigned char) * (((num_ids-1) / 8) + 1);
	id->bit_pool = (unsigned char *) malloc( id->pool_len );
	if( id->bit_pool == NULL ) {
		free( id );
		return NULL;
	}

	memset( id->bit_pool, 0, id->pool_len );
	id->nallocated = 0;
	id->num_ids = num_ids;

	return (void *) id;
}

/*
	Allocate an unused ID. Returns -1 if there are none.
*/
extern int idm_alloc( void* vid ) {
	idm_t *id;
	int i;
	int j;
	unsigned char bit;
	int k;

	if( vid == NULL ) {
		return -1;
	}

	id  = (idm_t *) vid;
	if( id->nallocated >= id->num_ids ) {
		return -1;
	}

	for( i = 0; i < id->pool_len; i++ ) {
		if( id->bit_pool[i] != 0xff ) {			// hole in this one
			bit = 1;

			for( j = 0; j < 8; j++ ) {
				if( !(id->bit_pool[i] & bit) ) {		// this is a hole
					k = (8 * i) + j;					// possible id, but could be out of range
					if( k <= id->num_ids ) {
						id->bit_pool[i] |= bit;			// fill the hole
						id->nallocated++;
						return k;						// and return the id number
					} else {
						return -1;						// extra bits in the last pool bytes can't be used
					}
				}

				bit <<= 1;
			}
		}
	}

	return -1;
}

/*
	Allows the user to allocate a specific ID; returns 1 if
	the ID was unused and is now marked in use, and 0 if the ID
	was already assigned and should not otherwise be used. A return 
	of -1 indicated an error (bad value passed in etc.).
*/
extern int idm_use( void* vid, int id_val ) {
	idm_t*	id;
	int i;
	unsigned char bit;

	if( vid == NULL ){
		return -1;
	}

	id = (idm_t *) vid;

	if( id_val > id->num_ids || id_val < 0 ) {
		return -1;
	}
	
	i = id_val / 8;					// offset into bit poool
	if( i > id->nallocated ) {		// shouldn't happen, but prevent accidents
		return -1;
	}

	bit = 1 << (id_val % 8);					// bit to flip
	if( id->bit_pool[i] & bit ) {				// set, so return in use
		return 0;
	}

	id->bit_pool[i] |= bit;						//  mark used
	id->nallocated++;
	
	return 1;
}

/*
	Returns 1 if in use, 0 if not.  Returns -1 on error (bad value).
*/
extern int idm_is_used( void* vid, int id_val ) {
	idm_t*	id;
	int i;
	unsigned char bit;

	if( vid == NULL ){
		return -1;
	}

	id = (idm_t *) vid;

	if( id_val > id->num_ids || id_val < 0 ) {
		return -1;
	}
	
	i = id_val / 8;					// offset into bit poool
	if( i > id->nallocated ) {		// shouldn't happen, but prevent accidents
		return -1;
	}

	bit = 1 << (id_val % 8);					// bit to flip
	if( id->bit_pool[i] & bit ) {				// set, so return in use
		return 1;
	}

	return 0;									// not in use
}


/*
	Return the id value to the pool.
*/
extern void idm_return( void* vid, int id_val ) {
	idm_t*	id;
	int i;
	unsigned char bit;

	if( vid == NULL ){
		return;
	}

	id = (idm_t *) vid;

	if( id_val > id->num_ids || id_val < 0 ) {
		return;
	}
	
	i = id_val / 8;				// offset into bit poool
	if( i > id->nallocated ) {		// shouldn't happen, but prevent accidents
		return;
	}

	bit = 1 << (id_val % 8);				// bit to flip
	if( !( id->bit_pool[i] & bit) ) {				// not set, do nothing
		return;
	}

	id->bit_pool[i] &= ~bit;				// open the hole up
	if( id->nallocated > 0 ) {
		id->nallocated--;
	}

}

/*
	Free the associated storage.
*/
extern void idm_free( void* vid ) {
	idm_t*	id;

	if( vid == NULL ) {
		return;
	}

	id = (idm_t *) vid;

	free( id->bit_pool );
	free( id );

	return;
}

