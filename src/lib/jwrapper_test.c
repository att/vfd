/*
	Mnemonic:	jwrapper_test
	Abstract: 	This will attempt to put the jwrapper code through its paces.

				It accepts as argv[1] a string of json and causes it to be parsed.
				argv[2] is the name of an element to find in the json. 
				argv[3] (optional) is an array element and argv[4] is an index into
				the array of a string element to find. 

				Example:
				json='{ 
					"active_patient": true,
					"last_visit": "2015/02/03",

					"patient_info": {
						"first_name": "Fred",
						"last_name": "Flintsone",
						"dob": "1963/04/03",
						"sex": "M",
						"weight_kilo": 65,
						"drug_alergies": [ "asprin","darvaset" ]
					}

					"Contact_info": {
						"name": "Wilma", "relation": "wife", "phone": "972.612.8918"
					}
				}'

				jwrapper_test "$json" "patient_info.dob" "patient_info.drug_alergies" 1
				jwrapper_test "$json" "last_visit"
				jwrapper_test "$json" "patient_info.weight_kilo"
				

				
	Author:		E. Scott Daniels
	Date:		24 February 2016
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vfdlib.h"


int main( int argc, char **argv ) {
	void	*jblob;						// parsed json stuff
	char	*stuff;
	float	value;

	if( argc <= 2 )
		exit( 1 );

	if( (jblob = jw_new( argv[1] )) == NULL ) {
		fprintf( stderr, "failed to create wrapper\n" );
		exit( 1 );
	}
	
	if(  (stuff = jw_string( jblob, argv[2] )) ) {
		fprintf( stderr, "found name (as a string): %s = (%s)\n", argv[2], stuff );
	} else {
		if(  (value = jw_value( jblob, argv[2] )) ) {
			fprintf( stderr, "found name (as a value): %s = (%0.2f)\n", argv[2], value );
		} else {
			fprintf( stderr, "failed to find name: %s\n", argv[2] );
		}
	}

	if( argc > 3 ) {				// assume array and index are parms 3, 4
		fprintf( stderr, "array %s has %d elements\n", argv[3], jw_array_len( jblob, argv[3] ) );

		// TODO -- add a value element test though vf_config_test tests this too!
		if(  (stuff = jw_string_ele( jblob, argv[3], atoi(argv[4]) )) ) {
			fprintf( stderr, "found array element: %s[%s] = %s\n", argv[3], argv[4], stuff );
		} else {
			fprintf( stderr, "failed to find string element for: %s[%s]\n", argv[3], argv[4] );
		}
	}

	jw_nuke( jblob );
}
