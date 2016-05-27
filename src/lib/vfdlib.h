
// vim: ts=4 sw=4 :

#ifndef _vfdlib_h_
#define _vfdlib_h_
#include <time.h>
#include <sys/types.h>

/*
	Things that need to be visible to vfd
*/

//----------------- config.c --------------------------------------------------------------------------
/*
	pf_def -- definition info picked up from the parm file for a PF
*/
typedef struct {
	char*	id;
	int		mtu;
} pfdef_t;

/*
        Parameter file contents parsed from json
*/
typedef struct {
	char*	log_dir;        // directory where log files should be written
	int		log_level;      // verbose (bleat) log level (set after initialisation)
	int		init_log_level; // vlog level used during initialisation
	int		dpdk_log_level;			// log level passed to dpdk; allow it to be different than verbose level
	int		dpdk_init_log_level;	// log level for dpdk during initialisation
	char*	fifo_path;      // path to fifo that cli will write to
	int		log_keep;       // number of days of logs to keep (do we need this?)
	int		delete_keep;	// if true we will keep the deleted config files in the confid directory (marked with trailing -)
	char*	config_dir;     // directory where nova writes pf config files
	char*	stats_path;		// filename where we might dump stats
	char*	pid_fname;		// if we daemonise we should write our pid here.
	char*	cpu_mask;		// should be something like 0x04, but could be decimal.  string so it can have lead 0x

							// these things have no defaults
	int		npciids;		// number of pciids specified for us to configure
	pfdef_t*	pciids;		// list of the pciid and mtu settings from the config file. 


							// these are NOT populated from the file, but are added so the struct can be the one stop shopping place for info
	void*	rfifo;			// the read fifo 'handle' where we 'listen' for requests
	int		forreal;		// if not set we don't execute any dpdk calls
	int		initialised;	// all things have been initialised
} parms_t;

/*
	vf config file data
*/
typedef struct {
	uid_t	owner;			// user id that owns the file (used for pre/post command execution)
	char*	name;			// nova supplied name or id; mostly ignored by us, but possibly useful
	char*	pciid;			// physical interface id (0000:07:00.1)
	int		vfid;			// the vf on the pf 1-32
	int		strip_stag;		// bool
	int		allow_bcast;	// bool
	int		allow_mcast;	// bool
	int		allow_un_ucast;	// bool
	int		antispoof_mac;	//	bool -- forced to true but here for future
	int		antispoof_vlan;	//	bool -- forced to true but here for future
	int		allow_untagged;	//	bool -- forced to true but here for future
	char*	link_status;	// on, off, auto
	char*	start_cb;		// external command/script to execute on the owner's behalf after we start up
	char*	stop_cb;		// external command/script to execute on the owner's behalf just before we shutdown
	char*	vm_mac;			// the mac to force onto the VF (optional)
	int*	vlans;			// array of vlan IDs
	int		nvlans;			// number of vlans allocated
	char**	macs;			// array of mac addresses (filter)
	int		nmacs;			// number of mac addresses
	float	rate;			// percentage of the total link speed this to be confined to (rate limiting)
	// ignoring mirrors right now
	/*
    "mirror":           [ { "vlan": 100; "vfid": 3 },
                          { "vlan": 430; "vfid": 6 } ]
	*/
} vf_config_t;

/*
	Parm file functions
*/
extern parms_t* read_parms( char* fname );
extern vf_config_t*	read_config( char* fname );
extern void free_config( vf_config_t* );

//------------------ ng_flowmgr --------------------------------------------------------------------------
void ng_flow_close( void *vf );
void ng_flow_flush( void *vf );
char* ng_flow_get( void *vf, char sep );
void *ng_flow_open(  int size );
void ng_flow_ref( void *vf, char *buf, long len );


// ---------------- fifo ---------------------------------------------------------------------------------
extern void* rfifo_create( char* fname, int mode );
extern void rfifo_close( void* vfifo );
extern char* rfifo_read( void* vfifo );


// --------------- list ----------------------------------------------------------------------------------
#define LF_QUALIFED		1				// list_files should return qualified names
#define LF_UNQUALIFIED	0

extern char** list_files( char* dname, const char* suffix, int qualify, int* len );
extern void free_list( char** list, int size );

// --------------- bleat ----------------------------------------------------------------------------------
#define BLEAT_ADD_DATE	1
#define BLEAT_NO_DATE	0

extern int bleat_set_lvl( int l );
extern time_t bleat_next_roll( void );
extern void bleat_push_lvl( int l );
extern void bleat_push_glvl( int l );
extern void bleat_pop_lvl( void );
extern int bleat_will_it( int l );
extern int bleat_set_log( char* fname, int add_date );
extern void bleat_printf( int level, const char* fmt, ... );

//---------------- hot_plug -------------------------------------------------------------------------------
extern int user_cmd( uid_t uid, char* cmd );

//---------------- jwrapper -------------------------------------------------------------------------------
extern void jw_nuke( void* st );
extern void* jw_new( char* json );
extern int jw_missing( void* st, const char* name );
extern int jw_exists( void* st, const char* name );
extern char* jw_string( void* st, const char* name );
extern float jw_value( void* st, const char* name );
extern void* jw_blob( void* st, const char* name );
extern char* jw_string_ele( void* st, const char* name, int idx );
extern float jw_value_ele( void* st, const char* name, int idx );
extern void* jw_obj_ele( void* st, const char* name, int idx );
extern int jw_array_len( void* st, const char* name );
#endif
