
/*
	Mnemonic:	jwrapper.c
	Abstract:	A wrapper interface to the jsmn library which makes it a bit easier
				to use.  Parses a json string capturing the contents in a symtab.
	Author:		E. Scott Daniels
	Date:		23 Feb 2016

	Mods:		04 Mar 2016 : Added missing/exists functions.
								Fixed bug in value array save.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jsmn.h>

#include "symtab.h"

#define JSON_SYM_NAME	"_jw_json_string"
#define MAX_THINGS		1024		// max objects/elements

/*
	This is what we will manage in the symtab. Right now we store all values (primatives)
	as float, but we could be smarter about it and look for a decimal. Unsigned and 
	differences between long, long long etc are tough.
*/
typedef struct jthing {
	int jsmn_type;				// propigated type from jsmn (jsmn constants)
	int	nele;					// number of elements if applies
	union {
		float fv;
		void *pv;
	} v;
} jthing_t;


/*
	Given the json token, 'extract' the element by marking the end with a
	nil character, and returning a pointer to the start.  We do this so that
	we don't create a bunch of small buffers that must be found and freed; we
	can just release the json string and we'll be done (read won't leak).
*/
static char* extract( char* buf, jsmntok_t *jtoken ) {
	buf[jtoken->end] = 0;
	return &buf[jtoken->start];
}

/*
	create a new jthing and add a reference to it in the symbol table st.
	sets the number of elements to 1 by default.
*/
static jthing_t *mk_thing( void *st, char *name, int jsmn_type ) {
	jthing_t 	*jtp;

	if( st == NULL ) {
		return;
	}

	jtp = (jthing_t *) malloc( sizeof( *jtp ) );
	if( jtp == NULL ) {
		return;
	}

	jtp->jsmn_type = jsmn_type;
	jtp->nele = 1;
	jtp->v.fv = 0;

	sym_fmap( st, (unsigned char *) name, 0, jtp );
	return jtp;
}


/*
	Find the named array. Returns a pointer to the jthing that represents
	the array (type, size and pointer to actual array of jthings).
*/
static jthing_t* suss_array( void* st, char* name ) {
	jthing_t* jtp;									// thing that is referenced by the symtab

	if( st == NULL ) {
		return NULL;
	}

	if( (jtp = (jthing_t *) sym_get( st, name, 0 )) == NULL ) {
		return NULL;
	}

	if( jtp->jsmn_type != JSMN_ARRAY ) {
		return NULL;
	}

	return jtp;
}

/*
	Private function to suss an array from the hash and return the ith
	element.
*/
static jthing_t* suss_element( void* st, char* name, int idx ) {
	jthing_t* jtp;									// thing that is referenced by the symtab
	jthing_t* jarray;

	if( (jtp = suss_array( st, name )) == NULL ) {
		return NULL;
	}
	
	if( idx < 0 || idx >= jtp->nele ) {				// out of range
		return NULL;
	}

	if( (jarray = jtp->v.pv)  == NULL ) {
		return NULL;
	}

	return &jarray[idx];
}


/*
	Invoked for each thing in the symtab; we free the things that actually point to 
	allocated data (e.g. arrays).
*/
static void nix_things( void* st, void* se, char* name,  void* ele, void *data ) {
	jthing_t* j;

	j = (jthing_t *) ele;
	if( j ) {
		switch( j->jsmn_type ) {
			case JSMN_ARRAY:
				free( j->v.pv );			// must free the array (arrays aren't nested, so all things in the array don't reference allocated mem)
				break;
		}
	}
}

/*
	Real work for parsing an object ({...}) from the json.   Called by jw_new() and 
	recurses to deal with sub-objects.
*/
void* parse_jobject( void* st, char *json, char* prefix ) {
	jthing_t	*jtp; 			// json thing that we just created
	int 	i;
	int 	n;					
	char	*name;				// name in the json
	char	*data;				// data string from the json
	jthing_t*	jarray;			// array of jthings we'll coonstruct
	int		size;
	int		njtokens; 			// tokens actually sussed out
	jsmn_parser jp;				// 'parser' object
	jsmntok_t *jtokens;			// pointer to tokens returned by the parser
	char	pname[1024];		// name with prefix

	jsmn_init( &jp );			// does this have a failure mode?

	jtokens = (jsmntok_t *) malloc( sizeof( *jtokens ) * MAX_THINGS );
	if( jtokens == NULL ) {
		fprintf( stderr, "abort: cannot allocate tokens array\n" );
		exit( 1 );
	}

	njtokens = jsmn_parse( &jp, json, strlen( json ), jtokens, MAX_THINGS );

	if( jtokens[0].type != JSMN_OBJECT ) {				// if it's not an object then we can't parse it.
		fprintf( stderr, "warn: badly formed json; initial opening bracket ({) not detected\n" );
		return NULL;
	}

	for( i = 1; i < njtokens-1; i++ ) {					// we'll silently skip the last token if its "name" without a value
		if( jtokens[i].type != JSMN_STRING ) {
			fprintf( stderr, "warn: badly formed json; expected name (string)\n" );
			sym_free( st );
			return NULL;
		}
		name = extract( json, &jtokens[i] );
		if( *prefix != 0 ) {
			snprintf( pname, sizeof( pname ), "%s.%s", prefix, name );
			name = pname;
		}

		size = jtokens[i].size;

		i++;										// at the data token now
		switch( jtokens[i].type ) {
			case JSMN_UNDEFINED: 
				fprintf( stderr, "warn: element [%d] in json is undefined\n", i );
				break;

    		case JSMN_OBJECT:				// object + size*2 elements
				parse_jobject( st, extract( json, &jtokens[i] ), name );					// recurse to add the object as objectname.xxxx elements
				
				size = jtokens[i].end;									// done with them, we need to skip them 
				i++;
				while( i < njtokens-1  &&  jtokens[i].end < size ) {
					//fprintf( stderr, "\tskip: [%d] object element start=%d end=%d (%s)\n", i, jtokens[i].start, jtokens[i].end, extract( json, &jtokens[i])  );
					i++;
				} 

				i--;						// must allow loop to bump past the last
				break;

    		case JSMN_ARRAY:
				size = jtokens[i].size;		// size is burried here, and not with the name
				jtp = mk_thing( st, name, jtokens[i].type );

				i++;						// first thing is the whole array string which I don't grock the need for, but it's their code...
				if( jtp == NULL ) {
					fprintf( stderr, "warn: memory alloc error processing element [%d] in json\n", i );
					sym_free( st );
					return NULL;
				}
				jarray = jtp->v.pv = (jsmntok_t *) malloc( sizeof( *jarray ) * size );		// allocate the array
				jtp->nele = size;

				for( n = 0; n < size; n++ ) {
					switch( jtokens[i+n].type ) {
						case JSMN_UNDEFINED:
							fprintf( stderr, "warn: [%d] array element %d is not valid type (undefined) is not string or primative\n", i, n );
							break;

						case JSMN_OBJECT:
							fprintf( stderr, "warn: [%d] array element %d is not valid type (object) is not string or primative\n", i, n );
							sym_free( st );
							return NULL;			// FIXME -- for now we'll bail out, but we should really just skip over this

						case JSMN_ARRAY:
							fprintf( stderr, "warn: [%d] array element %d is not valid type (array) is not string or primative\n", i, n );
							n += jtokens[i].size;			// this should skip the nested array
							break;

						case JSMN_STRING:
							data = extract( json, &jtokens[i+n] );
							jarray[n].v.pv = (void *) data;
							jarray[n].jsmn_type = JSMN_STRING;
							break;

						case JSMN_PRIMITIVE:
							data = extract( json, &jtokens[i+n] );
							switch( *data ) {
								case 0:
									jarray[n].v.fv = 0;
									break;

								case 'T':
								case 't':
									jarray[n].v.fv = 1;
									break;

								case 'F':
								case 'f':
								case 'N':										// assume null, nil, or some variant
								case 'n':
									jarray[n].v.fv = 0;
									break;

								default:
									jarray[n].v.fv = strtof( data, NULL ); 		// store all numerics as float
									break;
							}

							jarray[n].jsmn_type = JSMN_PRIMITIVE;
							break;
						
						default:
							fprintf( stderr, "warn: [%d] array element %d is not valid type (unknown=%d) is not string or primative\n", i, n, jtokens[i].type  );
							sym_free( st );
							return NULL;
							break;
					}
				}

				i += size - 1;		// must allow loop to push to next
				break;

    		case JSMN_STRING:
				data = extract( json, &jtokens[i] );
				jtp = mk_thing( st, name, jtokens[i].type );
				if( jtp == NULL ) {
					fprintf( stderr, "warn: memory alloc error processing element [%d] in json\n", i );
					sym_free( st );
					return NULL;
				}
				jtp->v.pv =  (void *) data;						// just point into the large json string
				break;

    		case JSMN_PRIMITIVE:
				data = extract( json, &jtokens[i] );
				jtp = mk_thing( st, name, jtokens[i].type );
				if( jtp == NULL ) {
					fprintf( stderr, "warn: memory alloc error processing element [%d] in json\n", i );
					sym_free( st );
					return NULL;
				}
				switch( *data ) {								// assume T|t is true and F|f is false
					case '0':
						jtp->v.fv = 1; 
						break;

					case 'T':
					case 't':
						jtp->v.fv = 1; 
						break;

					case 'F':
					case 'f':
					case 'N':									// Null or some form of that
					case 'n':
						jtp->v.fv = 0; 
						break;

					default:
						jtp->v.fv = strtof( data, NULL ); 		// store all numerics as float
						break;
				}
				break;

			default:
				fprintf( stderr, "unknown type at %d\n", i );
				break;
		}
	}

	free( jtokens );
	return st;
}

// --------------- public functions -----------------------------------------------------------------

/*
	Destroy all operating structures assocaited with the symtab pointer passed in.
*/
extern void jw_nuke( void* st ) {
	char* 	buf;					// pointer to the original json to free
	if( st == NULL ) {
		return;
	}

	sym_foreach_class( st, 0, nix_things, NULL );			// free anything that the symtab references
	sym_free( st );											// free the symtab itself
}

/*
	Given a json string, parse it, and put the things into a symtab.
	return the symtab pointer to the caller. They pass the symtab
	pointer back to the various get functions.

	This is the entry point. It sets up the symbol table and invokes the parse object
	funtion to start at the first level. Parse object will recurse for nested objects
	if present.
*/
extern void* jw_new( char* json ) {
	void	*st;				// symbol table

	st = sym_alloc( MAX_THINGS );
	if( st == NULL ) {
		return NULL;
	}

	json = strdup( json );												// allows user to free/overlay their buffer as needed
	sym_fmap( st, (unsigned char *) JSON_SYM_NAME, 0, json );			// save a pointer so we can free on nuke (fmap flags the data to be free'd when deleted)

	return parse_jobject( st,  json, "" );								// empty prefix for the root object
}

/*
	Returns true (1) if the named field is missing. 
*/
extern int jw_missing( void* st, char* name ) {
	return sym_get( st, name, 0 ) == NULL;
}

/*
	Returns true (1) if the named field is in the blob;
*/
extern int jw_exists( void* st, char* name ) {
	return sym_get( st, name, 0 ) != NULL;
}

/*
	Look up the name in the symtab and return the string (data).
*/
extern char* jw_string( void* st, char* name ) {
	jthing_t* jtp;									// thing that is referenced by the symtab

	jtp = (jthing_t *) sym_get( st, name, 0 );		// get it or NULL

	if( ! jtp ) {
		return NULL;
	}

	if( jtp->jsmn_type != JSMN_STRING ) {
		return NULL;
	}

	return (char *) jtp->v.pv;
}

/*
	Look up name and return the value.
*/
extern float jw_value( void* st, char* name ) {
	jthing_t* jtp;									// thing that is referenced by the symtab

	jtp = (jthing_t *) sym_get( st, name, 0 );		// get it or NULL

	if( ! jtp ) {
		return 0;
	}

	if( jtp->jsmn_type != JSMN_PRIMITIVE ) {
		return 0;
	}

	return jtp->v.fv;
}

/*
	Look up array element as a string. Returns NULL if:
		name is not an array
		name is not in the hash
		index is out of range
		element is not a string
*/
extern char* jw_string_ele( void* st, char* name, int idx ) {
	jthing_t* jtp;									// thing that is referenced by the symtab entry

	if( (jtp = suss_element( st, name, idx )) == NULL ) {
		return NULL;
	}

	if( jtp->jsmn_type != JSMN_STRING ) {
		return NULL;
	}

	return (char *) jtp->v.pv;
}

/*
	Look up array element as a value. Returns 0 if:
		name is not an array
		name is not in the hash
		index is out of range
		element is not a value
*/
extern float jw_value_ele( void* st, char* name, int idx ) {
	jthing_t* jtp;									// thing that is referenced by the symtab entry

	if( (jtp = suss_element( st, name, idx )) == NULL ) {
		return 0;
	}

	if( jtp->jsmn_type != JSMN_PRIMITIVE ) {
		return 0;
	}

fprintf( stderr, ">>> jwrapper returning %.2f\n", jtp->v.fv  );
	return jtp->v.fv;
}

/*
	Return the size of the array named. Returns -1 if the thing isn't an array, 
	and returns the number of elements otherwise.
*/
extern int jw_array_len( void* st, char* name ) {
	jthing_t* jtp;									// thing that is referenced by the symtab entry

	if( (jtp = suss_array( st, name )) == NULL ) {
		return -1;
	}

	return jtp->nele;
}

