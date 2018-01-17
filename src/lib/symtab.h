

/* --------- symtab ---------------- */
#define UT_FL_NOCOPY 0x00          /* use user pointer */
#define UT_FL_COPY 0x01            /* make a copy of the string data */
#define UT_FL_FREE 0x02            /* free val when deleting */


/* ------------ symtab ----------------------------- */
extern void sym_clear( void *s );
extern void sym_dump( void *s );
extern void *sym_alloc( int size );
extern void sym_del( void *s, const char *name, unsigned int class );
extern void sym_free( void *vtable );
extern int sym_fmap( void *vtable, const char *name, unsigned int class, void *val );
extern void *sym_get( void *s,  const char *name, unsigned int class );
extern int sym_put( void *s,  const char *name, unsigned int class, void *val );
extern int sym_map( void *s,  const char *name, unsigned int class, void *val );
extern void sym_stats( void *s, int level );
//extern void sym_foreach_class( void *st, unsigned int space, void (* user_fun)(), void *user_data );
extern void sym_foreach_class( void *vst, unsigned int space, void (* user_fun)( void*, void*, const char*, void*, void* ), void *user_data );
