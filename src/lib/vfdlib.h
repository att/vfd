
// vim: ts=4 sw=4 :

/*
	Things that need to be visible to vfd
*/

//----------------- config.c --------------------------------------------------------------------------
/*
        Parameter file contents parsed from json
*/
typedef struct {
	char*	log_dir;                                // directory where log files should be written
	int		log_level;                              // current log level
	char*	fifo_path;                              // path to fifo that cli will write to
	int		log_keep;                               // number of days of logs to keep (do we need this?)
	char*	config_dir;                             // directory where nova writes pf config files
} parms_t;

/*
	vf config file data
*/
typedef struct {
	char*	name;			// nova supplied name or id; mostly ignored by us, but possibly useful
	char*	pciid;			// physical interface id (0000:07:00.1)
	int		vfid;			// the vf on the pf 1-32
	int		strip_stag;		// bool
	int		allow_bcast;	// bool
	int		allow_mcast;	// bool
	int		allow_un_ucast;	// bool
	int		antispoof_mac;	//	bool -- forced to true but here for future
	int		antispoof_vlan;	//	bool -- forced to true but here for future
	char*	link_status;	// on, off, auto
	int*	vlans;			// array of vlan IDs
	int		nvlans;			// number of vlans allocated
	char**	macs;			// array of mac addresses
	int		nmacs;			// number of mac addresses
	// ignoring mirrors right now
	/*
    "mirror":           [ { "vlan": 100; "vf": 3 },
                          { "vlan": 430; "vf": 6 } ]
	*/
} vf_config_t;

/*
	Parm file functions
*/
extern parms_t* read_parms( char* fname );
extern vf_config_t*	read_config( char* fname );

//------------------ ng_flowmgr --------------------------------------------------------------------------
void ng_flow_close( void *vf );
void ng_flow_flush( void *vf );
char* ng_flow_get( void *vf, char sep );
void *ng_flow_open(  int size );
void ng_flow_ref( void *vf, char *buf, long len );


// ---------------- fifo ---------------------------------------------------------------------------------
extern void* rfifo_create( char* fname );
extern void rfifo_close( void* vfifo );
extern char* rfifo_read( void* vfifo );


// --------------- list ----------------------------------------------------------------------------------
#define LF_QUALIFED		1				// list_files should return qualified names
#define LF_UNQUALIFIED	0

extern char** list_files( char* dname, char* suffix, int qualify, int* len );
extern void free_list( char** list, int size );
