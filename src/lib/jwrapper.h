extern void jw_nuke( void* st );
extern void* jw_new( char* json );
extern char* jw_string( void* st, char* name );
extern float jw_value( void* st, char* name );
extern char* jw_string_ele( void* st, char* name, int idx );
extern float jw_value_ele( void* st, char* name, int idx );
extern int jw_array_len( void* st, char* name );
