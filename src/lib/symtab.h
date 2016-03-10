

/* --------- symtab ---------------- */
#define UT_FL_NOCOPY 0x00          /* use user pointer */
#define UT_FL_COPY 0x01            /* make a copy of the string data */
#define UT_FL_FREE 0x02            /* free val when deleting */


/* ------------ symtab ----------------------------- */
extern void sym_clear( void *s );
extern void sym_dump( void *s );
extern void *sym_alloc( int size );
extern void sym_del( void *s, char *name, unsigned int class );
extern void *sym_get( void *s, char *name, unsigned int class );
extern int sym_put( void *s, char *name, unsigned int class, void *val );
extern int sym_map( void *s, char *name, unsigned int class, void *val );
extern void sym_stats( void *s, int level );
void sym_foreach_class( void *st, unsigned int class, void (* user_fun)(), void *user_data );

