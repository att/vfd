/*
=================================================================================================
	(c) Copyright 1995-2011 By E. Scott Daniels. All rights reserved.

	Redistribution and use in source and binary forms, with or without modification, are
	permitted provided that the following conditions are met:
	
   		1. Redistributions of source code must retain the above copyright notice, this list of
      		conditions and the following disclaimer.
		
   		2. Redistributions in binary form must reproduce the above copyright notice, this list
      		of conditions and the following disclaimer in the documentation and/or other materials
      		provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED BY E. Scott Daniels ``AS IS'' AND ANY EXPRESS OR IMPLIED
	WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
	FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL E. Scott Daniels OR
	CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
	CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
	SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
	ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
	NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
	ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
	
	The views and conclusions contained in the software and documentation are those of the
	authors and should not be interpreted as representing official policies, either expressed
	or implied, of E. Scott Daniels.
=================================================================================================
*/
/*
------------------------------------------------------------------------------
Mnemonic: symtab.c
Abstract: Basic symbol table routines.
Date:     11 Feb 2000
Author:   E. Scott Daniels
Mod:		2016 23 Feb - converted Symtab refs so that caller need only a
				void pointer to use and struct does not need to be exposed.
------------------------------------------------------------------------------
*/

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>

#include "symtab.h"

//-----------------------------------------------------------------------------------------------

typedef struct Sym_ele
{
	struct Sym_ele *next;          /* pointer at next element in list */
	struct Sym_ele *prev;          /* larger table, easier deletes */
	unsigned char *name;           /* symbol name */
	void *val;                     /* user data associated with name */
	unsigned long mcount;          /* modificaitons to value */
	unsigned long rcount;          /* references to symbol */
	unsigned int flags; 
	unsigned int class;		/* helps divide things up and allows for duplicate names */
} Sym_ele;

typedef struct Sym_tab {
	Sym_ele **symlist;			/* pointer to list of element pointerss */
	long	inhabitants;             	/* number of active residents */
	long	deaths;                 	/* number of deletes */
	long	size;	
} Sym_tab;

/* ----- private functions ---- */
static int sym_hash( unsigned char *n, long size )
{
	unsigned char *p;
	unsigned long t = 0;
	unsigned long tt = 0;
	unsigned long x = 79;

	for( p = n; *p; p++ )      /* a bit of magic */
		t = (t * 79 ) + *p;

	if( t < 0 )
		t = ~t;

	return (int ) (t % size);
}

/* delete element pointed to by eptr at hash loc hv */
static void del_ele( Sym_tab *table, int hv, Sym_ele *eptr )
{
	Sym_ele **sym_tab;

	sym_tab = table->symlist;

	if( eptr )         /* unchain it */
	{
		if( eptr->prev )
			eptr->prev->next = eptr->next;
		else
			sym_tab[hv] = eptr->next;

		if( eptr->next )
			eptr->next->prev = eptr->prev;

		if( eptr->val && eptr->flags & UT_FL_FREE )
			free( eptr->val );
		if( eptr->name )
			free( eptr->name );
		free( eptr );

		table->deaths++;
		table->inhabitants--;
	}
}

static int same( unsigned int c1, unsigned int c2, char *s1, char* s2 )
{
	if( c1 != c2 )
		return 0;		/* different class - not the same */

	if( *s1 == *s2 && strcmp( s1, s2 ) == 0 )
		return 1;
	return 0;
}

/* generic rtn to put something into the table */
/* called by sym_map or sym_put, but not by the user! */
static int putin( Sym_tab *table, char *name, unsigned int class, void *val, int flags )
{
	Sym_ele *eptr;    	/* pointer into hash table */ 
	Sym_ele **sym_tab;    	/* pointer into hash table */ 
	int hv;                  /* hash value */
	int rc = 0;              /* assume it existed */

	sym_tab = table->symlist;

	hv = sym_hash( name, table->size );


	for(eptr=sym_tab[hv]; eptr && ! same( class, eptr->class, eptr->name, name); eptr=eptr->next );

	if( ! eptr )    /* new symbol for the table */
	{
		rc++;
		table->inhabitants++;
	
		eptr = (Sym_ele *) malloc( sizeof( Sym_ele) );
		if( ! eptr )
		{
			fprintf( stderr, "symtab/putin: out of memory\n" );
			exit( 1 );
		} 

		eptr->flags = flags & (UT_FL_FREE | UT_FL_COPY) ? UT_FL_FREE : 0;		/* set free flag if we made a copy of things */
		eptr->prev = NULL;
		eptr->class = class;
		eptr->mcount = eptr->rcount = 0;	/* init counters */
		eptr->val = NULL;                	/* add to head of the list */
		eptr->name = strdup( name );
		eptr->next = sym_tab[hv];
		sym_tab[hv] = eptr;
		if( eptr->next )
			eptr->next->prev = eptr;         /* chain back to new one */
	}

	eptr->mcount++;

	if( flags & UT_FL_COPY )          /* data expected to be a chr string */
	{
		if( eptr->val )		/* assuming we added it last time round */
			free( eptr->val );

		eptr->val = strdup( val );    /* make a copy so user can change */
	}
	else
		eptr->val = val;

	return rc;
}

/* --- public functions ---- */

/* delete all elements in the table */
void sym_clear( void *vtable )
{
	Sym_tab *table;
	Sym_ele **sym_tab;
	int i; 

	table = (Sym_tab *) vtable;
	sym_tab = table->symlist;

	for( i = 0; i < table->size; i++ )
		while( sym_tab[i] ) 
			del_ele( table, i, sym_tab[i] );
}

/*
	Clear and then free the whole thing.
*/
void sym_free( void *vtable ) {
	Sym_tab *table;

	table = (Sym_tab *) vtable;

	if( table == NULL )
		return;

	sym_clear( vtable );
	free( table->symlist );
	free( table );
}

void sym_dump( void *vtable )
{
	Sym_tab *table;
	int i; 
	Sym_ele *eptr;
	Sym_ele **sym_tab;

	table = (Sym_tab *) vtable;
	sym_tab = table->symlist;

	for( i = 0; i < table->size; i++ )
	{
		if( sym_tab[i] )
		for( eptr = sym_tab[i]; eptr; eptr = eptr->next )  
		{
			if( eptr->val && eptr->flags & UT_FL_FREE )
				fprintf( stderr, "%s %s\n", eptr->name, (char *) eptr->val );
			else
				fprintf( stderr, "%s -> %p\n", eptr->name, eptr->val );
		}
	}
}

/* allocate a table the size requested - best if size is prime */
/* returns a pointer to the management block */
void *sym_alloc( int size )
{
	int i;
	Sym_tab *table;

	if( size < 11 )     /* provide a bit of sanity */
		size = 11;

	if( (table = (Sym_tab *) malloc( sizeof( Sym_tab ))) == NULL )
	{
		fprintf( stderr, "sym_alloc: unable to get memory for symtable (%d elements)", size );
		exit( 1 );
	}

	memset( table, 0, sizeof( *table ) );

	if((table->symlist = (Sym_ele **) malloc( sizeof( Sym_ele *) * size ))) 
	{
		memset( table->symlist, 0, sizeof( Sym_ele *) * size );
		table->size = size;
	}
	else
	{
		fprintf( stderr, "sym_alloc: unable to get memory for %d elements", size );
		exit( 1 );
	}

	return (void *) table;    /* user might want to know what the size is */
}

/* delete a named element */
void sym_del( void *vtable, unsigned char *name, unsigned int class )
{
	Sym_tab	*table;
	Sym_ele **sym_tab;
	Sym_ele *eptr;    /* pointer into hash table */ 
	int hv;                  /* hash value */

	table = (Sym_tab *) vtable;
	sym_tab = table->symlist;

	hv = sym_hash( name, table->size );

	for(eptr=sym_tab[hv]; eptr && eptr->class != class &&  ! same(class, eptr->class, eptr->name, name); eptr=eptr->next );

	del_ele( table, hv, eptr );    /* ignors null ptr, so safe to always call */
}


void *sym_get( void *vtable, unsigned char *name, unsigned int class )
{
	Sym_tab	*table;
	Sym_ele **sym_tab;
	Sym_ele *eptr;    /* pointer into hash table */ 
	int hv;                  /* hash value */

	table = (Sym_tab *) vtable;
	sym_tab = table->symlist;

	hv = sym_hash( name, table->size );

	for(eptr=sym_tab[hv]; eptr && ! same( eptr->class, class, eptr->name, name); eptr=eptr->next );

	if( eptr )
	{
		eptr->rcount++;
		return eptr->val;
	}

	return NULL;
}

/* 
The only difference between sym_put and sym_map is that put assumes the 
pointer is to a character string and it makes a copy of the data. sym_map 
causes the user's pointer to be put into the table and thus the user is 
responsible for freeing the data. There is some risk to the user as if 
the data location changes, or the user frees the space,  the symbol table 
will have the wrong pointer unless they replace it with another sym_map 
call, or delete the entry! 
*/

/* put an element, replace if there */
/* creates local copy of data (assumes string) */
/* returns 1 if new, 0 if existed */
int sym_put( void *vtable, unsigned char *name, unsigned int class, void *val )
{
	Sym_tab	*table;

	table = (Sym_tab *) vtable;
	return putin( table, name, class, val, UT_FL_COPY );
}

/* add/replace an element, map directly to user data, dont copy */
/* the data like sym_put does */
/* returns 1 if new, 0 if existed */
int sym_map( void *vtable, unsigned char *name, unsigned int class, void *val )
{
	Sym_tab	*table;

	table = (Sym_tab *) vtable;
	return putin( table, name, class, val, UT_FL_NOCOPY );
}

/* add/replace an element, map directly to user data, dont copy */
/* the data like sym_put does, but sets the free on delete flag. */
/* returns 1 if new, 0 if existed */
int sym_fmap( void *vtable, unsigned char *name, unsigned int class, void *val )
{
	Sym_tab	*table;

	table = (Sym_tab *) vtable;
	return putin( table, name, class, val, UT_FL_FREE );
}

/* dump some statistics to stderr dev. Higher level is the more info dumpped */
void sym_stats( void *vtable, int level )
{
	Sym_tab	*table;
	Sym_ele *eptr;    /* pointer into the elements */
	Sym_ele **sym_tab;
	int i;
	int empty = 0;
	long ch_count;
	long max_chain = 0;
	int maxi = 0;
	int twoper = 0;

	table = (Sym_tab *) vtable;
	sym_tab = table->symlist;

	for( i = 0; i < table->size; i++ )
	{
		ch_count = 0;
		if( sym_tab[i] )
		{
			for( eptr = sym_tab[i]; eptr; eptr = eptr->next )  
			{
				ch_count++;
				if( level > 3 )
				if( eptr->val && eptr->flags & UT_FL_FREE )
					fprintf( stderr, "sym: (%d) %s str=(%s)  ref=%ld mod=%lu\n", 
						i, eptr->name, (char *) eptr->val, eptr->rcount, eptr->mcount );
				else
					fprintf( stderr, "sym: (%d) %s ptr=%p  ref=%ld mod=%lu\n", 
						i, eptr->name, eptr->val, eptr->rcount, eptr->mcount );
			}
		}
		else
			empty++;

		if( ch_count > max_chain ) 
		{
			max_chain = ch_count;
			maxi = i;
		}
		if( ch_count > 1 )
			twoper++;

		if( level > 2 )
			fprintf( stderr, "sym: (%d) chained=%ld\n", i, ch_count );
	}

	if( level > 1 )
	{
		fprintf( stderr, "sym: largest chain: (%d) ", maxi );
		for( eptr = sym_tab[maxi]; eptr; eptr = eptr->next )
	 		fprintf( stderr, "%s ", eptr->name );
		fprintf( stderr, "\n" );
	}

	fprintf( stderr, "sym:%ld(size)  %ld(inhab) %ld(occupied) %ld(dead) %ld(maxch) %ld(>2per)\n", 
			table->size, table->inhabitants, table->size - empty, table->deaths, max_chain, twoper );
}

void sym_foreach_class( void *vst, unsigned int class, void (* user_fun)( void*, void*, char*, void*, void* ), void *user_data )
{
	Sym_tab	*st;
	Sym_ele **list;
	Sym_ele *se;
	Sym_ele *next;		/* allows user to delete the node(s) we return */
	int 	i;

	st = (Sym_tab *) vst;

	if( st && (list = st->symlist) != NULL && user_fun != NULL )
		for( i = 0; i < st->size; i++ )
			for( se = list[i]; se; se = next )		/* using next allows user to delet via this */
			{
				next = se->next;
				if( class == se->class )
					user_fun( st, se, se->name, se->val, user_data );
			}
}
