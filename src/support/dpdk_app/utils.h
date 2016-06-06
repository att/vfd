/*
**
** az
**
*/

#ifndef __UTILS_H_
#define __UTILS_H_

#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

#include <netinet/in.h>

#include <stdint.h>
#include <time.h>

#define NETWORK_ENTRY                 0
#define NETMASK_ENTRY                 1
#define BROADCAST_ENTRY               2
#define NETMASK_V6_ENTRY              3
#define INVALIDNETMASK                -1

#define NETWORK_SIZE                  4  /* [0]=network, [1]=mask, [2]=broadcast [3]=mask v6 */
#define IPALLEN												4



#define TRACE_EMERG       0, __FILE__, __LINE__       /* system is unusable */
#define TRACE_ALERT       1, __FILE__, __LINE__       /* action must be taken immediately */
#define TRACE_CRIT        2, __FILE__, __LINE__       /* critical conditions */
#define TRACE_ERROR       3, __FILE__, __LINE__       /* error conditions */
#define TRACE_WARNING     4, __FILE__, __LINE__       /* warning conditions */
#define TRACE_NORMAL      5, __FILE__, __LINE__       /* normal but significant condition */
#define TRACE_INFO        6, __FILE__, __LINE__       /* informational */
#define TRACE_DEBUG       7, __FILE__, __LINE__       /* debug-level messages */


#define simpe_atomic_swap(var, newval)  __sync_lock_test_and_set(&var, newval)
#define barrier()                       __sync_synchronize()


typedef unsigned char __u8;
typedef unsigned int uint128_t __attribute__((mode(TI)));  


#define __UINT128__ 

int traceLevel;
int useSyslog;
int logFacility;
char * prog_name;

int trace_lock;

//static u_char syslog_opened = 0;

inline uint64_t ntohll(uint64_t in);
inline uint64_t htonll(uint64_t in);

inline void ntoh128(uint128_t  *src, uint128_t *dst);
inline void hton128(uint128_t  *src, uint128_t *dst);

//inline int xdigit (char c);

void hex_dump(const char *pref, u_int8_t *buf, size_t len, unsigned int width);
void str2eth(u_int8_t *addr, const char *str);
inline int int2bits(int number);
inline uint32_t str2addr(char *address);
inline void str2v6addr (char *str, struct in6_addr *addr);
inline int dotted2bits(char *mask);
inline int charMask2bits(char *mask);
inline uint32_t bits2mask(int bits);
inline uint128_t bits2mask128(int bits);
inline int lcmp(ulong *a, ulong *b);

//void print_stats(struct e1000_handle *h);
char * _intoaV4(unsigned int addr, char * buf, u_short bufLen);
void traceLog(int eventTraceLevel, const char * file, int line, const char * format, ...);
void daemonize(void);

#endif

