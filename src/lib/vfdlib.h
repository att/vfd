
// vim: ts=4 sw=4 :

/*
	Things that need to be visible to vfd
*/


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
	Parm file functions
*/
extern parms_t* read_parms( char* fname );
