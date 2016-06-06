/*
**
** az
**
*/

#include "utils.h"




void hex_dump(const char *pref, uint8_t *buf, size_t len, unsigned int width)
{
	unsigned long i;

	for (i=0; i < len; i++)
  {
		if (!(i % width))
			traceLog(TRACE_INFO, "%s%s", !i ? "" : "\n", pref);
			//printf("%s%s", !i ? "" : "\n", pref);
		traceLog(TRACE_INFO, "%2.2x ", buf[i]);
		//printf("%2.2x ", buf[i]);
	}
	if (len)
		traceLog(TRACE_INFO, "\n");
		//printf("\n");
}



void str2eth(uint8_t *addr, const char *str)
{
	unsigned int a[6], i;

	sscanf(str, "%x:%x:%x:%x:%x:%x",
		&a[0], &a[1], &a[2],
		&a[3], &a[4], &a[5]);

	for (i=0; i < 6; i++)
		addr[i] = a[i];
}




char * _intoaV4(unsigned int addr, char * buf, u_short bufLen)
{
  char *cp, *retStr;
  u_int byte;
  int n;

 // unsigned int haddr = ntohl(addr);
 
  addr = ntohl(addr);
  cp = &buf[bufLen];
  *--cp = '\0';

  n = 4;
  do
  {
    byte = addr & 0xff;
    *--cp = byte % 10 + '0';
    byte /= 10;
    if (byte > 0)
    {
      *--cp = byte % 10 + '0';
      byte /= 10;
      if (byte > 0)
	      *--cp = byte + '0';
    }
    *--cp = '.';
    addr >>= 8;
  } while (--n > 0);

  // Convert the string to lowercase
  retStr = (char*)(cp+1);

  return(retStr);
}



void traceLog(int eventTraceLevel, const char * file, int line, const char * format, ...)
{
  va_list va_ap;

  char buf[256], out_buf[256];
  //char *extra_msg = "";
  char extra_msg[10];

  //char theDate[32];
  //time_t theTime = time(NULL);
  
  
  strcpy(extra_msg, " ");

  if(eventTraceLevel <= traceLevel)
  {
    va_start (va_ap, format);


    memset(buf, 0, sizeof(buf));

    //memset(theDate, 0, sizeof(theDate));
    //strftime(theDate, sizeof(theDate), "%D %H:%M:%S", localtime(&theTime));


    vsnprintf(buf, sizeof(buf) - 1, format, va_ap);

    if(eventTraceLevel == 3 )
    {
      //extra_msg = "ERROR: ";
      strcpy(extra_msg, "ERROR: ");
    }
    else if(eventTraceLevel == 4 )
    {
      //extra_msg = "WARNING: ";
      strcpy(extra_msg, "WARNING: ");
    }

    
    while(buf[strlen(buf)-1] == '\n') buf[strlen(buf)-1] = '\0';

    //snprintf(out_buf, sizeof(out_buf), "%s [%s:%d] %s%s", theDate, file, line, extra_msg, buf);

    if(traceLevel > 6)
      snprintf(out_buf, sizeof(out_buf), "[%s:%d] %s%s", file, line, extra_msg, buf);
    else
      snprintf(out_buf, sizeof(out_buf), "%s%s", extra_msg, buf);


    if(useSyslog)
    {
  //    if(!syslog_opened)
      {
	      openlog(prog_name, LOG_PID, logFacility);
	//      syslog_opened = 1;
      }


      //syslog(LOG_INFO, out_buf);
      syslog(eventTraceLevel, "%s", out_buf);
    }
    else
    {
      printf("%s\n", out_buf);
    }
  }

  fflush(stdout);
  va_end(va_ap);
}



static void detachFromTerminal(void)
{
 // int rc;

//  if(doChdir) rc = chdir("/");
  setsid();  // detach from the terminal




  fclose(stdin);
  fclose(stdout);
  // fclose(stderr);

  // clear any inherited file mode creation mask
  umask(0);

  setvbuf(stdout, (char *)NULL, _IOLBF, 0);
}


void daemonize(void)
{
  int childpid;

  //signal(SIGHUP, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  if((childpid = fork()) < 0)
    traceLog(TRACE_ERROR, "INIT: Can not fork process (errno = %d)", errno);
  else
  {
#ifdef DEBUG
    traceEvent(TRACE_INFO, "DEBUG: after fork() in %s (%d)",
	       childpid ? "parent" : "child", childpid);
#endif
    if(!childpid)
    {
      // child
      traceLog(TRACE_INFO, "INIT: Starting Tcap daemon");
      detachFromTerminal();
    }
    else
    {
      // parent
      traceLog(TRACE_INFO, "INIT: Parent process exits");
      exit(EXIT_SUCCESS);
    }
  }
}


inline uint32_t str2addr(char *address)
{
  int a, b, c, d;

  if(sscanf(address, "%d.%d.%d.%d", &a, &b, &c, &d) != 4)
    return(0);
  else
    return(((a & 0xff) << 24) + ((b & 0xff) << 16) + ((c & 0xff) << 8) + (d & 0xff));
}


inline void str2v6addr (char *str, struct in6_addr *addr)
{
  int i;
  unsigned int x;

  /* %x must point to unsinged int */
  int y = 15;
  for (i = 0; i < 16; i++)
  {
    sscanf (str + (i * 2), "%02x", &x);
    addr->s6_addr[y] = x & 0xff;
    y--;
  }
}


inline uint64_t ntohll(uint64_t in)
{
	return (ntohl(in >> 32) | (uint64_t) ntohl(in) << 32);
}

inline uint64_t htonll(uint64_t in)
{
	return (htonl(in >> 32) | (uint64_t) htonl(in) << 32);
}


// inline void ntoh128_0(const struct in6_addr  *src, struct in6_addr *dst)
// {	
// 	int i;
// 	for (i = 0; i < 16; i++)
// 		dst->s6_addr[15 - i] = src->s6_addr[i];	
// }


inline void ntoh128(uint128_t * src, uint128_t * dst)
{
	__u8 * s = ( __u8 * ) src;
	__u8 * d = ( __u8 * ) dst;
	
	int i;
	for (i = 0; i < 16; i++)
		d[15 - i] = s[i];	
}


// inline void hton128_0(const struct in6_addr  *src, struct in6_addr *dst)
// {
// 	int i;
// 	for (i = 0; i < 16; i++)
// 		dst->s6_addr[15 - i] = src->s6_addr[i];	
// }


inline void hton128(uint128_t * src, uint128_t * dst)
{
	__u8 * s = ( __u8 * ) src;
	__u8 * d = ( __u8 * ) dst;
	
	int i;
	for (i = 0; i < 16; i++)
		d[15 - i] = s[i];	
}

inline int lcmp(ulong *a, ulong *b)
{
	int i;
	
	for(i = 0; i < IPALLEN; i++)
	{
		if(a[i] > b[i])
			return 1;
			
		if(a[i] < b[i])
			return -1;
	}
	return 0;
}



inline int int2bits(int number)
{
  int bits = 8;
  int test;

  if((number > 255) || (number < 0))
    return(INVALIDNETMASK);
  else
  {
    test = ~number & 0xff;
    while (test & 0x1)
    {
	    bits --;
	    test = test >> 1;
    }

    if(number != ((~(0xff >> bits)) & 0xff))
      return(INVALIDNETMASK);
    else
      return(bits);
  }
}


inline int charMask2bits(char *mask)
{
	int ret = atoi(mask);
	
	if(ret < 0 || ret > 128)
	{
		return -1;
	}
	
	return ret;
}


inline int dotted2bits(char *mask)
{
  int		fields[4];
  int		fields_num, field_bits;
  int		bits = 0;
  int		i;

  fields_num = sscanf(mask, "%d.%d.%d.%d", &fields[0], &fields[1], &fields[2], &fields[3]);
  if((fields_num == 1) && (fields[0] <= 32) && (fields[0] >= 0))
  {
#ifdef DEBUG
      traceLog(TRACE_INFO, "DEBUG: dotted2bits (%s) = %d", mask, fields[0]);
#endif
      return(fields[0]);
  }
  for (i = 0; i < fields_num; i++)
  {
    /* We are in a dotted quad notation. */
    field_bits = int2bits (fields[i]);
    switch (field_bits)
    {
      case INVALIDNETMASK:
        return(INVALIDNETMASK);

      case 0:
        /* whenever a 0 bits field is reached there are no more */
        /* fields to scan                                       */
        /* In this case we are in a bits (not dotted quad) notation */
        return(bits /* fields[0] - L.Deri 08/2001 */);

      default:
        bits += field_bits;
    }
  }
  return(bits);
}



inline uint32_t bits2mask(int bits)
{
  int i;

  uint32_t num = 0xffffffff;

  if(bits > 32 || bits < 0)
  {
    traceLog(TRACE_ERROR, "Invalid net mask");
    return 0;
  }

  for(i = 0; i < (32 - bits); i++)
  {
    num = num << 1;
  }

  return num;
}


inline uint128_t bits2mask128(int bits)
{
  int i;
  
  //uint128_t num = ( __uint128_t ) 0xffffffffffffffffffffffffffffffffULL;
  uint128_t num = 1;
 

  if(bits > 128 || bits < 0)
  {
    traceLog(TRACE_ERROR, "Invalid net mask");
    return 0;
  }

  for(i = 0; i < (128 - bits); i++)
  {
    num = num << 1;
  }

  return num;
}


/*
* ffz - find first zero in word.
* @word: The word to search
*
* Undefined if no zero exists, so code should check against ~0UL first.
*/
static inline unsigned long ffz(unsigned long word)
{
  __asm__("bsfl %1,%0"
          :"=r" (word)
          :"r" (~word));

  return word;
}

/*
*
* This function returns number of th low-order zero-bits in mask.
* When the value is 0, the value 32 is returnrd. Otherwise, the value is changed from network
* byte order to host byte order and ffz finds first zero
* see /usr/src/linux/include/asm-i386/bitops.h
* 0xFFFFFFFF yields 0, 0xFFFFFFFE yields 1, 0x80000000 yields 31
*
*
*/
static inline int logmask(uint32_t mask)
{
  if(!(mask = htonl(mask)))
    return 32;

  return ffz(~mask);
}



/*
*
* Inverse of the function above. It takes number that reresents the number of zero-bits
* to be generated at the low-order end of the network address. Any value greater or euqal to
* 32 yields a result of 0, while a value of 0 results in an all-ones mask. The mask is placed in
* network-byte order.
*
*/

static inline uint32_t mask(int logmask)
{
  if(logmask >= 32)
    return 0;

  return htonl(~((1 << logmask) -1 ));
}
