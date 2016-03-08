/*
* ======================================================================== v1.0/0a157 
*                                                         
*       This software is part of the AT&T Ningaui distribution 
*	
*       Copyright (c) 2001-2009 by AT&T Intellectual Property. All rights reserved
*	AT&T and the AT&T logo are trademarks of AT&T Intellectual Property.
*	
*       Ningaui software is licensed under the Common Public
*       License, Version 1.0  by AT&T Intellectual Property.
*                                                                      
*       A copy of the License is available at    
*       http://www.opensource.org/licenses/cpl1.0.txt             
*                                                                      
*       Information and Software Systems Research               
*       AT&T Labs 
*       Florham Park, NJ                            
*	
*	Send questions, comments via email to ningaui_support@research.att.com.
*	
*                                                                      
* ======================================================================== 0a229
*/

/*
------------------------------------------------------------------
  Mnemonic:	flowmgr - flow buffer manager
  Abstract:	helps to manage a flow of buffers where multiple 
		records may be received in a single buffer (e.g. from 
		tcp) with the possiblity of the last bit of the 
		buffer being an incomplete record.
		Opening a flow creates the needed managment 
		datastructure, and each received buffer can be 
		registered, and then 'read' from using flow_get.
		The caller needs to exhaust the currently registered
		buffer before registering another.  Minimal copying
		is done -- only the last portion of a truncated
		record, and the first portion of the next are 
		ever coppied. 

		void *ng_flow_open( size );  (size of partial buffer allocated; 8k if 0 supplied)
		void ng_flow_close( handle );
		void ng_flow_ref( handle, buf, len );
		char *ng_flow_get( handle, sep );	(retuns null if no more complete records were found)

  Date:		28 Aug 2003
  Author: 	E. Scott Daniels
 -------------------------------------------------------------------
*/

#include <stdio.h>              /* standard io */
#include <errno.h>              /* error number constants */
#include <fcntl.h>              /* file control */
#include <string.h>
#include <stdlib.h>
                
//#include        <sfio.h>
                
//#include        <ningaui.h>
//#include        <ng_ext.h>

#define NG_BUFFER 8192



#define COOKIE	0x060403000	/* oatmeal */

typedef struct {
	char	*buf;		/* pointer to the buffer */
	char	*head;		/* pointer to where the next read will start */
	char	*tail;		/* pointer to the last chr inserted */
	char	*partial;	/* partial buffer from last go round */
	long	psize;		/* size of the partial buffer */
	char	*pnext;		/* next insertion point in partial */
	int	cookie;		/* helps us verify the pointer they pass in */
} Flowboss_t;


static void add2partial( Flowboss_t *f,  char *msg, int mlen )
{
	int	avail;

	if( f == NULL || msg == NULL )	
		return;
		
	avail = f->psize - (f->pnext - f->partial);
	
	if( avail > mlen )
	{
		memcpy( f->pnext, msg, mlen );		/* tack on the message then deal with it */
		f->pnext += mlen;
	}
	else
	{
		//ng_bleat( 0, "flowmgr: buffer overrun filling partial: %.100s(had...) %s(got)", f->partial, msg );
		f->pnext = f->partial;
		
	}
}

void *ng_flow_open(  int size )
{
	Flowboss_t *f;

	//f = (Flowboss_t *) ng_malloc( sizeof( Flowboss_t ), "flowboss block" );
	f = (Flowboss_t *) malloc( sizeof( Flowboss_t ) );
	memset( f, 0, sizeof( *f ) );

	f->psize = size ? size : NG_BUFFER;
	//f->partial = ng_malloc( f->psize+2, "partial buffer" );
	f->partial = malloc( f->psize+2 );
	f->pnext = f->partial;

	f->cookie = COOKIE;
	
	return (void *) f;
}

void ng_flow_close( void *vf )
{
	Flowboss_t *f;

	f = (Flowboss_t *) vf;
	if( !f ||  f->cookie != COOKIE )
		return;

	free( f->partial );
	f->cookie = 0;			
	free( f );
}

void ng_flow_ref( void *vf, char *buf, long len )
{
	Flowboss_t *f;

	f = (Flowboss_t *) vf;
	if( !f ||  f->cookie != COOKIE )
		return;

	f->buf = buf;				/* reference their buffer */
	f->head = buf;
	f->tail = buf + len;			/* points one past the end of the buffer */
}

char *ng_flow_get( void *vf, char sep )
{
	Flowboss_t *f;
	char	*cp;				/* pointer in search of end of message */
	char	*rp;				/* pointer to return */

	f = (Flowboss_t *) vf;
	if( !f ||  f->cookie != COOKIE || ! f->head )
		return NULL;

	for( cp = f->head; cp < f->tail && *cp && *cp != sep; cp++ );		/* find end */

	if( cp >= f->tail )			/* off the edge without finding a seperator -- add to partial */
	{
		add2partial( f, f->head, (cp - f->head) );		
		f->head = NULL;
		return NULL;
	}
	
	
	*cp = 0;				/* convert to string */
	rp = f->head;
	f->head = cp + 1;
	if( f->head  > f->tail )
		f->head = NULL;

	if( f->pnext != f->partial )		/* something in the partial buffer */
	{
		add2partial( f, rp, (cp - rp)+1 );		/* add the first part of new buffer */
		f->pnext = f->partial;				/* used the partial buf -- empty it */
		return f->partial;				/* let them go for the partital buffer */
	}
	else
		return rp;
		
}

void ng_flow_flush( void *vf )
{
	Flowboss_t *f;

	f = (Flowboss_t *) vf;
	if( f && f->cookie == COOKIE )
	{
		f->pnext = f->partial;
	}
}


#ifdef SELF_TEST
main( int argc, char **argv )
{
	void *flow;
	int fd;
	int len;
	char b[NG_BUFFER];
	char	*p;

	if( (fd = open( argv[1], O_RDONLY, 0 )) >= 0 )
	{
		if( ! (flow = ng_flow_open( 0 )) )
		{
			printf( "could not open flow\n" );
			exit( 1 );
		}

		while( (len = read( fd, b, 29 )) > 0 )	
		{
			b[len] = 0;
			//ng_bleat( 2, "read %d bytes (%s)", len, b );
			ng_flow_ref( flow, b, len );
			while( p = ng_flow_get( flow, '\n' ) )
  				printf( "%s\n",  p ); 
		}

		ng_flow_close( flow );
	}
}
#endif

#ifdef KEEP
/* ---------- SCD: Self Contained Documentation ------------------- 
&scd_start
&doc_title(Flow manager:Buffer flow management)

&space
		void *ng_flow_open( long size )
&break
		void ng_flow_close( void *handle )
&break
		void ng_flow_ref( void *handle, buf, len )
&break
		char *ng_flow_get( void *handle, char seperator )
&break
		void ng_flow_flush( void *handle );

&space
&desc
	These routines assist programmes working with streaming input over  TCP/IP 
	to manage logical records that might be split across datagrams, or multiple
	messages contained in a single datagram.  These functions are used mostly 
	in conjunction with the socket interface (si) callback interface where 
	mapping the inbound file descriptor to Sfio is not an option. 
&space
	Once a flow is opened using &lit(ng_flow_open) buffers received by the 
	application are &ital(referenced) (ng_flow_ref), which allows multiple 
	&lit(ng_flow_get) calls to be made to extract logical records. &lit(ng_flow_get)
	returns a pointer to a null terminated string of bytes that were deliniated
	in the input buffer with the &lit(seperator). If a complete string is 
	not available in the buffer, NULL returned.  Any partial data is 
	saved, the next call to &lit(ng_flow_ref) will prepend that data to 
	the newly referenced buffer. 

&space
&subcat void *ng_flow_open( long size )
	This function opens a new flow and returns the handle to the caller. The
	handle must be passed into all other ng_flow functions.  The size allows
	the user to specify the maximum partial buffer size to be used on the 
	flow.  If 0 is supplied, then a default value of &lit(NG_BUFFER) is used. 
&space
&subcat void ng_flow_ref( void *handle, char *buf, int len )
	Registers a new buffer. If a partial buffer from a previous registration 
	and get calls exists the data is appended to it. A buffer must be registered
	to be used. The registered buffer will be &stress(used in place) and the 
	seperator characters in the buffer will be converted to nulls (zeros).
	This allows for minimal copying, and the user must be aware that calls to 
	the get function after buf has been freed will likely result in a crash.

&space
&subcat	char *ng_flow_get( void *handle, char seperator )
	Returns a pointer to the next null terminated string from the currently 
	registered buffer. The first occurance of &ital(seperator) will have 
	been converted into a null (0).  If the remaining bytes in the registered
	buffer do not contain &ital(seperator,) then a NULL pointer is returned,
	the remaining bytes are saved, and will be used when the next buffer 
	is registered.   Once a NULL pointer is returned, it is safe for the 
	user to free or reuse the registered buffer.

&space
&subcat void ng_flow_flush( void *handle );
	This function flushes any cached partial data that might have been 
	left from the last registered buffer. 

&space
&subcat void ng_flow_close( void *handle )
	Closes the flow and releases any allocated memory.


&space
&mods
&owner(Scott Daniels)
&lgterms
&term	28 Aug 2003 (sd) : Thin end of the wedge.
&term	22 Jan 2004 (sd) : broke out of zoo and made a lib routine.
&term	23 Jul 2004 (sd) : Corrected memory leak -- partial buffer was not
&term	08 Nov 2004 (sd) : Corrected man page.
	being freed.
&term	31 July 2005 (sd) : changes to prevent gcc warnings.
&term	21 Jun 2006 (sd) : Cookie value change. 
&endterms

&scd_end
------------------------------------------------------------------ */
#endif
