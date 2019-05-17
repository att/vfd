//vi: ts=4 sw=4 noet:
/*
	Mnemonic:	jw_xapi.c
	Abstract:	This is an extended api set for the 'primatives' in jwrapper.
	Author:		E. Scott Daniels
	Date:		11 Feb 2017

	Mods:
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define JSMN_STATIC 1		// jsmn no longer builds into a library; this pulls as static functions
#include <jsmn.h>

#include "vfdlib.h"
#include "symtab.h"


/*
	Look up a boolean returning the default if it's not a boolean or not defined.
*/
extern int jwx_get_bool( void* jblob, char const* field_name, int def_value ) {
	if( !jw_is_bool( jblob, field_name ) ) { 
		return def_value;
	}
	
	return jw_value( jblob, field_name );
}

/*
	Return the value from the json blob, or the default if it is not a value
*/
extern float jwx_get_value( void* jblob, char const* field_name, float def_value ) {
	if( !jw_is_value( jblob, field_name ) ) { 
		return def_value;
	}
	
	return jw_value( jblob, field_name );
}

/*
	Return the value from the json blob as an integer, or the default if it is not a value
*/
extern int jwx_get_ivalue( void* jblob, char const* field_name, int def_value ) {
	if( !jw_is_value( jblob, field_name ) ) { 
		return def_value;
	}
	
	return (int) jw_value( jblob, field_name );
}

/*
	There are cases where we need a 
	For cases where a 'value' needs to be used as a string, this will pull the 
	value from the blob and converts it using the format type passed in. If the 
	field name is a string in the blob it is returned as is. If the field name 
	does not exist, the default is returned.
	Fmt is one of the JWFMT_ constants.
*/
extern char* jwx_get_value_as_str( void* jblob, char const* field_name, char const* def_value, int  fmt ) {
	float	jvalue;				// value from json
	char	stuff[128];			// should be more than enough :)
	char*	jstr;				// if represented in json as a string

	if( jw_is_value( jblob, field_name ) ) { 
		jvalue = jw_value( jblob, field_name );
		switch( fmt ) {
			case JWFMT_INT:
				snprintf( stuff, sizeof( stuff ), "%d", (int) jvalue );
				break;
			case JWFMT_FLOAT:
				snprintf( stuff, sizeof( stuff ), "%f", jvalue );

			case JWFMT_HEX:
				snprintf( stuff, sizeof( stuff ), "0x%x", (int) jvalue );
		}
		return strdup( stuff );
	}

	if( (jstr = jw_string( jblob, field_name )) ) {		// if already a string, return that
		return strdup( jstr );
	}

	if( def_value ) {					// neither way in json, so use default if supplied
		return strdup( def_value );
	}
			
	return NULL;
}

/*
	Looks up field name and returns the value from jblob if its a string.
	If it's not a string in the json, then the def_value string is duplicated 
	and returned.
*/
extern char* jwx_get_str( void* jblob, char const* field_name, char const* def_value ) {
	char*	stuff;

	if( (stuff = jw_string( jblob, field_name )) ) {
		return strdup( stuff );
	}

	if( def_value ) {
		return strdup( def_value );
	}

	return NULL;
}
