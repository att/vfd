// vi: sw=4 ts=4 noet:

/*
	Mnemonic:	vfd_rif.h
	Abstract:	Request interface header.
	Author:		E. Scott Daniels
	Date:		11 October 2016
*/

#ifndef _VFD_RIF_H
#define _VFD_RIF_H

#define ADDED	1				// updated states
#define DELETED (-1)
#define UNCHANGED 0
#define RESET	2

#define RESP_ERROR	1			// states for response bundler
#define RESP_OK		0

#define RT_NOP	0				// request types
#define RT_ADD	1
#define RT_DEL	2
#define RT_SHOW 3
#define RT_PING 4
#define RT_VERBOSE 5
#define RT_DUMP 6
#define RT_MIRROR 7				// mirror on/off command
#define RT_CPU_ALARM 8			// set the cpu alarm threshold

#define BUF_1K	1024			// simple buffer size constants
#define BUF_10K BUF_1K * 10

typedef struct request {
	int		rtype;				// type: RT_ const
	char*	resource;			// parm file name, show target, etc.
	char*	resp_fifo;			// name of the return pipe
	int		log_level;			// for verbose
	char*	vfd_rid;			// request id that must be placed into the response (allows single response pipe by request process)
} req_t;

// ------------------ prototypes ---------------------------------------------
extern int vfd_init_fifo( parms_t* parms );
extern int check_tcs( struct sriov_port_s* port, uint8_t *tc_pctgs );
extern void vfd_add_ports( parms_t* parms, sriov_conf_t* conf );
extern int vfd_add_vf( sriov_conf_t* conf, char* fname, char** reason );
extern void vfd_add_all_vfs(  parms_t* parms, sriov_conf_t* conf );
extern int vfd_del_vf( parms_t* parms, sriov_conf_t* conf, char* fname, char** reason );
extern int vfd_write( int fd, const char* buf, int len );
extern void vfd_response( char* rpipe, int state, const_str vfd_rid, const char* msg );
extern void vfd_free_request( req_t* req );
extern req_t* vfd_read_request( parms_t* parms );
extern int vfd_req_if( parms_t *parms, sriov_conf_t* conf, int forever );


#endif
