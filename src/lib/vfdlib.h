
// vim: ts=4 sw=4 :

#ifndef _vfdlib_h_
#define _vfdlib_h_
#include <stdint.h>
#include <time.h>
#include <sys/types.h>

/*
	Things that need to be visible to vfd
*/

// ----- jw_xapi --------------------------
#define JWFMT_HEX		1
#define JWFMT_INT		2
#define JWFMT_FLOAT		3

//----------------- config.c --------------------------------------------------------------------------
#define MIRROR_OFF			0		// mirror directions
#define MIRROR_IN			1		// mirror just inbound traffic
#define MIRROR_OUT			2		// mirror just outbound traffic
#define MIRROR_ALL			3		// mirror both directions

                                    // tc_class_t struct flags
#define TCF_LOW_LATENCY 0x01
#define TCF_BW_STRICTP  0x02
#define TCF_LNK_STRICTP 0x04
									// pfdef_t struct flags
#define PFF_LOOP_BACK	0x01		// loop back enabled flag
#define PFF_VF_OVERSUB  0x02        // vf_oversubscription enabled flag
#define PFF_PROMISC  	0x04        // promisc should be set for the PF

									// flags set in parm struct related to running state
#define RF_ENABLE_QOS	0x01		// enable qos
#define RF_INITIALISED	0x02		// init has finished
#define RF_ENABLE_FC	0x04		// enable flow control for all PFs
#define RF_NO_HUGE		0x08		// disable huget pages

#define MAX_TCS			8			// max number of traffic classes supported (0 - 7)
#define NUM_BWGS		8			// number of bandwidth groups

typedef char const* const_str;		// pointer to an unmutable string

typedef struct {
    char* hr_name;          // human readable name used for diagnostics
    unsigned int flags;     // TCF_ flasg constants
    int32_t max_bw;         // percentage of link bandwidth (value 0-100) (default == 100)
    int32_t min_bw;        // percentage of link bandwidth (value 0-100)
} tc_class_t;

typedef struct {
    int32_t ntcs;           // number of TCs in the group
    int32_t tcs[MAX_TCS];	// priority of each TC in the group (index into tcs array in pfdef_t)
} bw_grp_t;

/*
	pf_def_t -- definition info picked up from the parm file for a PF.
	Traffic classes (tcs) exist as either 4 or 8 and are contiguous in
	the array (0-3 or 0-7). The position is the priority and generally
	accepted practice is that the higher the number the higher the
	priority. If a traffic class is not supplied in the parm file
	it might be represented by a nil pointer, or a pointer to a
	default struct.
*/
typedef struct {
	char*	id;
	int		mtu;
	int		hw_strip_crc;			// set hardware to strip crc when true
	unsigned int flags;				// PFF_ flag constants
									// QoS members
    int32_t ntcs;					// number of TCs (4 or 8)
    tc_class_t* tcs[MAX_TCS];		// defined TCs (0-3 or 0-7) position in the array is the priority (from pri in the json)
    bw_grp_t    bw_grps[NUM_BWGS];	// definition of each bandwidth group
} pfdef_t;

/*
        Parameter file contents parsed from json
*/
typedef struct {
	char*	log_dir;        		// directory where log files should be written
	int		log_level;      		// verbose (bleat) log level (set after initialisation)
	int		init_log_level; 		// vlog level used during initialisation
	int		dpdk_log_level;			// log level passed to dpdk; allow it to be different than verbose level
	int		dpdk_init_log_level;	// log level for dpdk during initialisation
	char*	fifo_path;      		// path to fifo that cli will write to
	int		log_keep;       		// number of days of logs to keep (do we need this?)
	int		delete_keep;			// if true we will keep the deleted config files in the confid directory (marked with trailing -)
	double	cpu_alrm_thresh;		// we'll alarm if our cpu usage is over this amount
	char*	cpu_alrm_type;			// allow user to decide if these are critical, errors, or just warnings; default is warn
	char*	config_dir;     		// directory where nova writes pf config files
	char*	stats_path;				// filename where we might dump stats
	char*	pid_fname;				// if we daemonise we should write our pid here.
	char*	cpu_mask;				// should be something like 0x04, but could be decimal.  string so it can have lead 0x
	char*	numa_mem;				// something like 64 or 64,64 or 64,128.  For our little app, the default 64,64 should be fine

									// these things have no defaults
	int		npciids;				// number of pciids specified for us to configure
	pfdef_t*	pciids;				// list of the pciid and mtu settings from the config file.


									// these are NOT populated from the file, but are added so the struct can be the one stop shopping place for info
	void*	rfifo;					// the read fifo 'handle' where we 'listen' for requests
	int		forreal;				// if not set we don't execute any dpdk calls
	//int		initialised;			// all things have been initialised
	int		rflags;					// running flags (RF_ constants)
} parms_t;

/*
	Manages configuration information read from a specific vf config file.
*/
typedef struct {
	uid_t	owner;					// user id that owns the file (used for pre/post command execution)
	char*	name;					// nova supplied name or id; mostly ignored by us, but possibly useful
	char*	pciid;					// physical interface id (0000:07:00.1)
	int		vfid;					// the vf on the pf 1-32
	int		strip_stag;				// bool
	int		strip_ctag;				// bool
	int		allow_bcast;			// bool
	int		allow_mcast;			// bool
	int		allow_un_ucast;			// bool
	int		antispoof_mac;			//	bool -- forced to true but here for future
	int		antispoof_vlan;			//	bool -- forced to true but here for future
	int		allow_untagged;			//	bool -- forced to true but here for future
	char*	link_status;			// on, off, auto
	char*	start_cb;				// external command/script to execute on the owner's behalf after we start up
	char*	stop_cb;				// external command/script to execute on the owner's behalf just before we shutdown
	char*	vm_mac;					// the mac to force onto the VF (optional)
	int*	vlans;					// array of vlan IDs
	int		nvlans;					// number of vlans allocated
	char**	macs;					// array of mac addresses (filter)
	int		nmacs;					// number of mac addresses
	float	rate;					// percentage of the total link speed this to be confined to (rate limiting)
	int		mirror_target;			// vf number of the target for mirroring
	int		mirror_dir;				// direction (in/out/both/off)
	float	min_rate;				// percentage of the total link speed that is guaranteed (BW guarantee)
	uint8_t	qshare[MAX_TCS];		// share (percentage) of each traffic class
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
extern void free_parms( parms_t* parms );

//------------------ ng_flowmgr --------------------------------------------------------------------------
void ng_flow_close( void *vf );
void ng_flow_flush( void *vf );
char* ng_flow_get( void *vf, char sep );
void *ng_flow_open(  int size );
void ng_flow_ref( void *vf, char *buf, long len );


// ---------------- fifo ---------------------------------------------------------------------------------
extern void* rfifo_create( char* fname, int mode );
extern void rfifo_close( void* vfifo );
extern void rfifo_detect_close( void* vfifo );
extern void* rfifo_open( char* fname, int mode );
extern char* rfifo_read( void* vfifo );
extern char* rfifo_readln( void* vfifo );
extern char* rfifo_blk_readln( void* vfifo );
extern char* rfifo_to_readln( void* vfifo, int to );


// --------------- list ----------------------------------------------------------------------------------
#define LF_QUALIFED		1				// list_files should return qualified names
#define LF_UNQUALIFIED	0

extern char** list_files( char* dname, const char* suffix, int qualify, int* len );
extern char** list_pfiles( char* dname, const char* prefix, int qualify, int* len );
extern char** list_old_files( char* dname, int qualify, int seconds, int* len );
extern char** rm_new_files( char** flist, int seconds, int *ulen );
extern void free_list( char** list, int size );

// --------------- bleat ----------------------------------------------------------------------------------
#define BLEAT_ADD_DATE	1
#define BLEAT_NO_DATE	0

extern int bleat_set_lvl( int l );
extern void bleat_set_purge( const char* dname, const char* prefix, int seconds );
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

extern int jw_is_value( void* st, const char* name );
extern int jw_is_bool( void* st, const char* name );
extern int jw_is_null( void* st, const char* name );
extern int jw_is_value_ele( void* st, const char* name, int idx );
extern int jw_is_bool_ele( void* st, const char* name, int idx );

// ---------------- jw_xapi ---------------------------------------------------------------------------------
extern int jwx_get_bool( void* jblob, char const* field_name, int def_value );
extern float jwx_get_value( void* jblob, char const* field_name, float def_value );
extern int jwx_get_ivalue( void* jblob, char const* field_name, int def_value );
extern char* jwx_get_value_as_str( void* jblob, char const* field_name, char const* def_value, int  fmt );
extern char* jwx_get_str( void* jblob, char const* field_name, char const* def_value );

//----------------- idmgr -----------------------------------------------------------------------------------
extern void* mk_idm( int num_ids );
extern int idm_alloc( void* vid );
extern int idm_use( void* vid, int id_val );
extern int idm_is_used( void* vid, int id_val );
extern void idm_return( void* vid, int id_val );
extern void idm_free( void* vid );

//----------------- filesys  -----------------------------------------------------------------------------------
extern int rm_file( const_str fname, int backup );
extern int mv_file( const_str fname, char* target );
extern int ensure_dir( const_str pathname );
extern int is_dir( const_str pathname );
extern int is_file( const_str pathname );
extern int is_fifo( const_str pathname );
extern int file_exists( const_str pathname );
extern int cp_file( const_str path1, const_str path2, int rm_src );



#endif
