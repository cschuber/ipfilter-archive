/*
 * Copyright (C) 1993-2001 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */
#if !defined(lint)
static const char sccsid[] = "@(#)ip_fil.c	2.41 6/5/96 (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id$";
#endif

#ifndef	SOLARIS
#define	SOLARIS	(defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#include <sys/param.h>
#if defined(__FreeBSD__) && !defined(__FreeBSD_version)
# if defined(IPFILTER_LKM)
#  ifndef __FreeBSD_cc_version
#   include <osreldate.h>
#  else
#   if __FreeBSD_cc_version < 430000
#    include <osreldate.h>
#   endif
#  endif
# endif
#endif
#include <sys/errno.h>
#if defined(__hpux) && (HPUXREV >= 1111) && !defined(_KERNEL)
# include <sys/kern_svcs.h>
#endif
#include <sys/types.h>
#define _KERNEL
#define KERNEL
#ifdef __OpenBSD__
struct file;
#endif
#include <sys/uio.h>
#undef _KERNEL
#undef KERNEL
#include <sys/file.h>
#include <sys/ioctl.h>
#ifdef __sgi
# include <sys/ptimers.h>
#endif
#include <sys/time.h>
#if !SOLARIS
# if (NetBSD > 199609) || (OpenBSD > 199603) || (__FreeBSD_version >= 300000)
#  include <sys/dirent.h>
# else
#  include <sys/dir.h>
# endif
#else
# include <sys/filio.h>
#endif
#ifndef linux
# include <sys/protosw.h>
#endif
#include <sys/socket.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <fcntl.h>

#ifdef __hpux
# define _NET_ROUTE_INCLUDED
#endif
#include <net/if.h>
#ifdef sun
# include <net/af.h>
#endif
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#ifdef __sgi
#include <sys/debug.h>
# ifdef IFF_DRVRLOCK /* IRIX6 */
#include <sys/hashing.h>
# endif
#endif
#include <netinet/in.h>
#if !(defined(__sgi) && !defined(IFF_DRVRLOCK)) /* IRIX < 6 */ && \
    !defined(__hpux) && !defined(linux)
# include <netinet/in_var.h>
#endif
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#if defined(__osf__)
# include <netinet/tcp_timer.h>
#endif
#if defined(__osf__) || defined(__hpux) || defined(__sgi)
# include "radix_ipf_local.h"
# define _RADIX_H_
#endif
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>
#ifdef __hpux
# undef _NET_ROUTE_INCLUDED
#endif
#include "netinet/ip_compat.h"
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_proxy.h"
#include "netinet/ip_auth.h"
#ifdef	IPFILTER_SYNC
#include "netinet/ip_sync.h"
#endif
#ifdef	IPFILTER_SCAN
#include "netinet/ip_scan.h"
#endif
#include "netinet/ip_pool.h"
#ifdef IPFILTER_COMPILED
# include "netinet/ip_rules.h"
#endif
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 300000)
# include <sys/malloc.h>
#endif
#if defined(__hpux) || SOLARIS
struct rtentry;
#endif
#include "md5.h"


#if !defined(__osf__) && !defined(__linux__)
extern	struct	protosw	inetsw[];
#endif

#include "ipt.h"
static	struct	ifnet **ifneta = NULL;
static	int	nifs = 0;

static	void	ipf_setifpaddr __P((struct ifnet *, char *));
void	init_ifp __P((void));
#if defined(__sgi) && (IRIX < 60500)
static int 	no_output __P((struct ifnet *, struct mbuf *,
			       struct sockaddr *));
static int	write_output __P((struct ifnet *, struct mbuf *,
				  struct sockaddr *));
#else
# if TRU64 >= 1885
static int 	no_output __P((struct ifnet *, struct mbuf *,
			       struct sockaddr *, struct rtentry *, char *));
static int	write_output __P((struct ifnet *, struct mbuf *,
				  struct sockaddr *, struct rtentry *, char *));
# else
static int 	no_output __P((struct ifnet *, struct mbuf *,
			       struct sockaddr *, struct rtentry *));
static int	write_output __P((struct ifnet *, struct mbuf *,
				  struct sockaddr *, struct rtentry *));
# endif
#endif


int   
ipfattach(softc)
	ipf_main_softc_t *softc;
{
	return 0;
}


int
ipfdetach(softc)
	ipf_main_softc_t *softc;
{
	return 0;
}     


/*
 * Filter ioctl interface.
 */
int
ipfioctl(softc, dev, cmd, data, mode)
	ipf_main_softc_t *softc;
	int dev;
	ioctlcmd_t cmd;
	caddr_t data;
	int mode;
{
	int error = 0, unit = 0, uid;

	uid = getuid();
	unit = dev;

	SPL_NET(s);

	error = ipf_ioctlswitch(softc, unit, data, cmd, mode, uid, NULL);
	if (error != -1) {
		SPL_X(s);
		return error;
	}
	SPL_X(s);
	return error;
}


void
ipf_forgetifp(softc, ifp)
	ipf_main_softc_t *softc;
	void *ifp;
{
	register frentry_t *f;

	WRITE_ENTER(&softc->ipf_mutex);
	for (f = softc->ipf_acct[0][softc->ipf_active]; (f != NULL);
	     f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = softc->ipf_acct[1][softc->ipf_active]; (f != NULL);
	     f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = softc->ipf_rules[0][softc->ipf_active]; (f != NULL);
	     f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	for (f = softc->ipf_rules[1][softc->ipf_active]; (f != NULL);
	     f = f->fr_next)
		if (f->fr_ifa == ifp)
			f->fr_ifa = (void *)-1;
	RWLOCK_EXIT(&softc->ipf_mutex);
	ipf_nat_sync(softc, ifp);
}


static int
#if defined(__sgi) && (IRIX < 60500)
no_output(ifp, m, s)
#else
# if TRU64 >= 1885
no_output (ifp, m, s, rt, cp)
	char *cp;
# else
no_output(ifp, m, s, rt)
# endif
	struct rtentry *rt;
#endif
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *s;
{
	return 0;
}


static int
#if defined(__sgi) && (IRIX < 60500)
write_output(ifp, m, s)
#else
# if TRU64 >= 1885
write_output (ifp, m, s, rt, cp)
	char *cp;
# else
write_output(ifp, m, s, rt)
# endif
	struct rtentry *rt;
#endif
	struct ifnet *ifp;
	struct mbuf *m;
	struct sockaddr *s;
{
	char fname[32];
	mb_t *mb;
	ip_t *ip;
	int fd;

	mb = (mb_t *)m;
	ip = MTOD(mb, ip_t *);

#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
    (defined(OpenBSD) && (OpenBSD >= 199603)) || defined(linux) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 501113))
	sprintf(fname, "/tmp/%s", ifp->if_xname);
#else
	sprintf(fname, "/tmp/%s%d", ifp->if_name, ifp->if_unit);
#endif
	fd = open(fname, O_WRONLY|O_APPEND);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	write(fd, (char *)ip, ntohs(ip->ip_len));
	close(fd);
	return 0;
}


static void
ipf_setifpaddr(ifp, addr)
	struct ifnet *ifp;
	char *addr;
{
#ifdef __sgi
	struct in_ifaddr *ifa;
#else
	struct ifaddr *ifa;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	if (ifp->if_addrlist.tqh_first != NULL)
#else
# ifdef __sgi
	if (ifp->in_ifaddr != NULL)
# else
	if (ifp->if_addrlist != NULL)
# endif
#endif
		return;

	ifa = (struct ifaddr *)malloc(sizeof(*ifa));
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	ifp->if_addrlist.tqh_first = ifa;
#else
# ifdef __sgi
	ifp->in_ifaddr = ifa;
# else
	ifp->if_addrlist = ifa;
# endif
#endif

	if (ifa != NULL) {
		struct sockaddr_in *sin;

#ifdef __sgi
		sin = (struct sockaddr_in *)&ifa->ia_addr;
#else
		sin = (struct sockaddr_in *)&ifa->ifa_addr;
#endif
#ifdef USE_INET6
		if (index(addr, ':') != NULL) {
			struct sockaddr_in6 *sin6;

			sin6 = (struct sockaddr_in6 *)&ifa->ifa_addr;
			sin6->sin6_family = AF_INET6;
			inet_pton(AF_INET6, addr, &sin6->sin6_addr);
		} else
#endif
		{
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = inet_addr(addr);
			if (sin->sin_addr.s_addr == 0)
				abort();
		}
	}
}

struct ifnet *
get_unit(name, family)
	char *name;
	int family;
{
	struct ifnet *ifp, **ifpp, **old_ifneta;
	char *addr;
#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
    (defined(OpenBSD) && (OpenBSD >= 199603)) || defined(linux) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 501113))

	if (name == NULL)
		name = "anon0";

	addr = strchr(name, '=');
	if (addr != NULL)
		*addr++ = '\0';

	for (ifpp = ifneta; ifpp && (ifp = *ifpp); ifpp++) {
		if (!strcmp(name, ifp->if_xname)) {
			if (addr != NULL)
				ipf_setifpaddr(ifp, addr);
			return ifp;
		}
	}
#else
	char *s, ifname[LIFNAMSIZ+1];

	if (name == NULL)
		name = "anon0";

	addr = strchr(name, '=');
	if (addr != NULL)
		*addr++ = '\0';

	for (ifpp = ifneta; ifpp && (ifp = *ifpp); ifpp++) {
		COPYIFNAME(family, ifp, ifname);
		if (!strcmp(name, ifname)) {
			if (addr != NULL)
				ipf_setifpaddr(ifp, addr);
			return ifp;
		}
	}
#endif

	if (!ifneta) {
		ifneta = (struct ifnet **)malloc(sizeof(ifp) * 2);
		if (!ifneta)
			return NULL;
		ifneta[1] = NULL;
		ifneta[0] = (struct ifnet *)calloc(1, sizeof(*ifp));
		if (!ifneta[0]) {
			free(ifneta);
			return NULL;
		}
		nifs = 1;
	} else {
		old_ifneta = ifneta;
		nifs++;
		ifneta = (struct ifnet **)realloc(ifneta,
						  (nifs + 1) * sizeof(ifp));
		if (!ifneta) {
			free(old_ifneta);
			nifs = 0;
			return NULL;
		}
		ifneta[nifs] = NULL;
		ifneta[nifs - 1] = (struct ifnet *)malloc(sizeof(*ifp));
		if (!ifneta[nifs - 1]) {
			nifs--;
			return NULL;
		}
	}
	ifp = ifneta[nifs - 1];

#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	TAILQ_INIT(&ifp->if_addrlist);
#endif
#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
    (defined(OpenBSD) && (OpenBSD >= 199603)) || defined(linux) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 501113))
	(void) strncpy(ifp->if_xname, name, sizeof(ifp->if_xname));
#else
	for (s = name; *s && !ISDIGIT(*s); s++)
		;
	if (*s && ISDIGIT(*s)) {
		ifp->if_unit = atoi(s);
		ifp->if_name = (char *)malloc(s - name + 1);
		(void) strncpy(ifp->if_name, name, s - name);
		ifp->if_name[s - name] = '\0';
	} else {
		ifp->if_name = strdup(name);
		ifp->if_unit = -1;
	}
#endif
	ifp->if_output = (void *)no_output;

	if (addr != NULL) {
		ipf_setifpaddr(ifp, addr);
	}

	return ifp;
}


char *
get_ifname(ifp)
	struct ifnet *ifp;
{
	static char ifname[LIFNAMSIZ];

#if defined(__OpenBSD__) || defined(__NetBSD__) || defined(linux) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 501113))
	sprintf(ifname, "%s", ifp->if_xname);
#else
	sprintf(ifname, "%s%d", ifp->if_name, ifp->if_unit);
#endif
	return ifname;
}



void
init_ifp()
{
	struct ifnet *ifp, **ifpp;
	char fname[32];
	int fd;

#if (defined(NetBSD) && (NetBSD <= 1991011) && (NetBSD >= 199606)) || \
    (defined(OpenBSD) && (OpenBSD >= 199603)) || defined(linux) || \
    (defined(__FreeBSD__) && (__FreeBSD_version >= 501113))
	for (ifpp = ifneta; ifpp && (ifp = *ifpp); ifpp++) {
		ifp->if_output = (void *)write_output;
		sprintf(fname, "/tmp/%s", ifp->if_xname);
		fd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC, 0600);
		if (fd == -1)
			perror("open");
		else
			close(fd);
	}
#else

	for (ifpp = ifneta; ifpp && (ifp = *ifpp); ifpp++) {
		ifp->if_output = (void *)write_output;
		sprintf(fname, "/tmp/%s%d", ifp->if_name, ifp->if_unit);
		fd = open(fname, O_WRONLY|O_CREAT|O_EXCL|O_TRUNC, 0600);
		if (fd == -1)
			perror("open");
		else
			close(fd);
	}
#endif
}


int
ipf_fastroute(m, mpp, fin, fdp)
	mb_t *m, **mpp;
	fr_info_t *fin;
	frdest_t *fdp;
{
	struct ifnet *ifp = fdp->fd_ptr;
	ip_t *ip = fin->fin_ip;
	int error = 0;
	frentry_t *fr;
	void *sifp;

	if (!ifp)
		return 0;	/* no routing table out here */

	fr = fin->fin_fr;
	ip->ip_sum = 0;

	if (fin->fin_out == 0) {
		sifp = fin->fin_ifp;
		fin->fin_ifp = ifp;
		fin->fin_out = 1;
		(void) ipf_acctpkt(fin, NULL);
		fin->fin_fr = NULL;
		if (!fr || !(fr->fr_flags & FR_RETMASK)) {
			u_32_t pass;

			(void) ipf_state_check(fin, &pass);
		}

		switch (ipf_nat_checkout(fin, NULL))
		{
		case 0 :
			break;
		case 1 :
			ip->ip_sum = 0;
			break;
		case -1 :
			error = -1;
			goto done;
			break;
		}

		fin->fin_ifp = sifp;
		fin->fin_out = 0;
	}

#if defined(__sgi) && (IRIX < 60500)
	(*ifp->if_output)(ifp, (void *)ip, NULL);
# if TRU64 >= 1885
	(*ifp->if_output)(ifp, (void *)m, NULL, 0, 0);
# else
	(*ifp->if_output)(ifp, (void *)m, NULL, 0);
# endif
#endif
done:
	return error;
}


int
ipf_send_reset(fin)
	fr_info_t *fin;
{
	ipfkverbose("- TCP RST sent\n");
	return 0;
}


int
ipf_send_icmp_err(type, fin, dst)
	int type;
	fr_info_t *fin;
	int dst;
{
	ipfkverbose("- ICMP unreachable sent\n");
	return 0;
}


void
m_freem(m)
	mb_t *m;
{
	return;
}

#ifdef STES
void
m_copydata(m, off, len, cp)
	mb_t *m;
	int off, len;
	caddr_t cp;
{
	bcopy((char *)m + off, cp, len);
}
#endif

int
ipfuiomove(buf, len, rwflag, uio)
	caddr_t buf;
	int len, rwflag;
	struct uio *uio;
{
	int left, ioc, num, offset;
	struct iovec *io;
	char *start;

	if (rwflag == UIO_READ) {
		left = len;
		ioc = 0;

		offset = uio->uio_offset;

		while ((left > 0) && (ioc < uio->uio_iovcnt)) {
			io = uio->uio_iov + ioc;
			num = io->iov_len;
			if (num > left)
				num = left;
			start = (char *)io->iov_base + offset;
			if (start > (char *)io->iov_base + io->iov_len) {
				offset -= io->iov_len;
				ioc++;
				continue;
			}
			bcopy(buf, start, num);
			uio->uio_resid -= num;
			uio->uio_offset += num;
			left -= num;
			if (left > 0)
				ioc++;
		}
		if (left > 0)
			return EFAULT;
	}
	return 0;
}


u_32_t
ipf_newisn(fin)
	fr_info_t *fin;
{
	static int iss_seq_off = 0;
	u_char hash[16];
	u_32_t newiss;
	MD5_CTX ctx;

	/*
	 * Compute the base value of the ISS.  It is a hash
	 * of (saddr, sport, daddr, dport, secret).
	 */
	MD5Init(&ctx);

	MD5Update(&ctx, (u_char *) &fin->fin_fi.fi_src,
		  sizeof(fin->fin_fi.fi_src));
	MD5Update(&ctx, (u_char *) &fin->fin_fi.fi_dst,
		  sizeof(fin->fin_fi.fi_dst));
	MD5Update(&ctx, (u_char *) &fin->fin_dat, sizeof(fin->fin_dat));

	/* MD5Update(&ctx, ipf_iss_secret, sizeof(ipf_iss_secret)); */

	MD5Final(hash, &ctx);

	memcpy(&newiss, hash, sizeof(newiss));

	/*
	 * Now increment our "timer", and add it in to
	 * the computed value.
	 *
	 * XXX Use `addin'?
	 * XXX TCP_ISSINCR too large to use?
	 */
	iss_seq_off += 0x00010000;
	newiss += iss_seq_off;
	return newiss;
}


/* ------------------------------------------------------------------------ */
/* Function:    ipf_nextipid                                                */
/* Returns:     int - 0 == success, -1 == error (packet should be droppped) */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Returns the next IPv4 ID to use for this packet.                         */
/* ------------------------------------------------------------------------ */
INLINE u_short
ipf_nextipid(fin)
	fr_info_t *fin;
{
	static u_short ipid = 0;
	ipf_main_softc_t *softc = fin->fin_main_soft;
	u_short id;

	MUTEX_ENTER(&softc->ipf_rw);
	id = ipid++;
	MUTEX_EXIT(&softc->ipf_rw);

	return id;
}


INLINE void
ipf_checkv4sum(fin)
	fr_info_t *fin;
{
	if (ipf_checkl4sum(fin) == -1)
		fin->fin_flx |= FI_BAD;
}


#ifdef	USE_INET6
INLINE void
ipf_checkv6sum(fin)
	fr_info_t *fin;
{
	if (ipf_checkl4sum(fin) == -1)
		fin->fin_flx |= FI_BAD;
}
#endif


#if 0
/*
 * See above for description, except that all addressing is in user space.
 */
int
copyoutptr(softc, src, dst, size)
	void *src, *dst;
	size_t size;
{
	caddr_t ca;

	bcopy(dst, (char *)&ca, sizeof(ca));
	bcopy(src, ca, size);
	return 0;
}


/*
 * See above for description, except that all addressing is in user space.
 */
int
copyinptr(src, dst, size)
	void *src, *dst;
	size_t size;
{
	caddr_t ca;

	bcopy(src, (char *)&ca, sizeof(ca));
	bcopy(ca, dst, size);
	return 0;
}
#endif


/*
 * return the first IP Address associated with an interface
 */
int
ipf_ifpaddr(softc, v, atype, ifptr, inp, inpmask)
	ipf_main_softc_t *softc;
	int v, atype;
	void *ifptr;
	i6addr_t *inp, *inpmask;
{
	struct ifnet *ifp = ifptr;
#ifdef __sgi
	struct in_ifaddr *ifa;
#else
	struct ifaddr *ifa;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	ifa = ifp->if_addrlist.tqh_first;
#else
# ifdef __sgi
	ifa = (struct in_ifaddr *)ifp->in_ifaddr;
# else
	ifa = ifp->if_addrlist;
# endif
#endif
	if (ifa != NULL) {
		if (v == 4) {
			struct sockaddr_in *sin, mask;

			mask.sin_addr.s_addr = 0xffffffff;

#ifdef __sgi
			sin = (struct sockaddr_in *)&ifa->ia_addr;
#else
			sin = (struct sockaddr_in *)&ifa->ifa_addr;
#endif

			return ipf_ifpfillv4addr(atype, sin, &mask,
						 &inp->in4, &inpmask->in4);
		}
#ifdef USE_INET6
		if (v == 6) {
			struct sockaddr_in6 *sin6, mask;

			sin6 = (struct sockaddr_in6 *)&ifa->ifa_addr;
			((i6addr_t *)&mask.sin6_addr)->i6[0] = 0xffffffff;
			((i6addr_t *)&mask.sin6_addr)->i6[1] = 0xffffffff;
			((i6addr_t *)&mask.sin6_addr)->i6[2] = 0xffffffff;
			((i6addr_t *)&mask.sin6_addr)->i6[3] = 0xffffffff;
			return ipf_ifpfillv6addr(atype, sin6, &mask,
						 inp, inpmask);
		}
#endif
	}
	return 0;
}


/*    
 * This function is not meant to be random, rather just produce a
 * sequence of numbers that isn't linear to show "randomness".
 */
u_32_t
ipf_random() 
{
	static unsigned int last = 0xa5a5a5a5;
	static int calls = 0;
	int number;

	calls++;

	/*
	 * These are deliberately chosen to ensure that there is some
	 * attempt to test whether the output covers the range in test n18.
	 */
	switch (calls)
	{
	case 1 :
		number = 0;
		break;
	case 2 :
		number = 4;
		break;
	case 3 :
		number = 3999;
		break;
	case 4 :
		number = 4000;
		break;
	case 5 :
		number = 48999;
		break;
	case 6 :
		number = 49000;
		break;
	default :
		number = last;
		last *= calls;
		last++;
		number ^= last;
		break;
	}
	return number;
}


int
ipf_verifysrc(fin)
	fr_info_t *fin;
{
	return 1;
}


int
ipf_inject(fin, m)   
	fr_info_t *fin; 
	mb_t *m;
{
	FREE_MB_T(m);

	return 0;
}     
