/*
 * Copyright (C) 1993-2001, 2003 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 */
#if !defined(lint)
static const char sccsid[] = "%W% %G% (C) 1993-2000 Darren Reed";
static const char rcsid[] = "@(#)$Id$";
#endif

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/cpuvar.h>
#include <sys/open.h>
#include <sys/ioctl.h>
#include <sys/filio.h>
#include <sys/systm.h>
#include <sys/cred.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/ksynch.h>
#include <sys/kmem.h>
#include <sys/mkdev.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/dditypes.h>
#include <sys/cmn_err.h>
#include <net/if.h>
#include <net/af.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/tcpip.h>
#include <netinet/ip_icmp.h>
#include "netinet/ip_compat.h"
#ifdef	USE_INET6
# include <netinet/icmp6.h>
#endif
#include "netinet/ip_fil.h"
#include "netinet/ip_nat.h"
#include "netinet/ip_frag.h"
#include "netinet/ip_state.h"
#include "netinet/ip_auth.h"
#include "netinet/ip_proxy.h"
#ifdef	IPFILTER_LOOKUP
# include "netinet/ip_lookup.h"
#endif
#include <inet/ip_ire.h>

#include "md5.h"

extern	int	fr_flags, fr_active;


static	int	fr_send_ip __P((fr_info_t *fin, mblk_t *m));

ipfmutex_t	ipl_mutex, ipf_authmx, ipf_rw, ipf_stinsert;
ipfmutex_t	ipf_nat_new, ipf_natio, ipf_timeoutlock;
ipfrwlock_t	ipf_mutex, ipf_global, ipf_ipidfrag;
ipfrwlock_t	ipf_frag, ipf_state, ipf_nat, ipf_natfrag, ipf_auth;
kcondvar_t	iplwait, ipfauthwait;
#if SOLARIS2 >= 7
timeout_id_t	fr_timer_id;
u_int		*ip_ttl_ptr;
u_int		*ip_mtudisc;
# if SOLARIS2 >= 8
int		*ip_forwarding;
u_int		*ip6_forwarding;
# else
u_int		*ip_forwarding;
# endif
#else
int		fr_timer_id;
u_long		*ip_ttl_ptr;
u_long		*ip_mtudisc;
u_long		*ip_forwarding;
#endif
int		ipf_locks_done = 0;


/* ------------------------------------------------------------------------ */
/* Function:    ipldetach                                                   */
/* Returns:     int - 0 == success, else error.                             */
/* Parameters:  Nil                                                         */
/*                                                                          */
/* This function is responsible for undoing anything that might have been   */
/* done in a call to iplattach().  It must be able to clean up from a call  */
/* to iplattach() that did not succeed.  Why might that happen?  Someone    */
/* configures a table to be so large that we cannot allocate enough memory  */
/* for it.                                                                  */
/* ------------------------------------------------------------------------ */
int ipldetach()
{

	ASSERT(rw_read_locked(&ipf_global.ipf_lk) == 0);

	if (fr_refcnt)
		return EBUSY;

	if (fr_control_forwarding & 2) {
		*ip_forwarding = 0;
#if SOLARIS2 >= 8
		*ip6_forwarding = 0;
#endif
	}

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "ipldetach()\n");
#endif

	fr_deinitialise();

	(void) frflush(IPL_LOGIPF, FR_INQUE|FR_OUTQUE|FR_INACTIVE);
	(void) frflush(IPL_LOGIPF, FR_INQUE|FR_OUTQUE);

	if (ipf_locks_done == 1) {
		MUTEX_DESTROY(&ipf_timeoutlock);
		MUTEX_DESTROY(&ipf_rw);
		RW_DESTROY(&ipf_ipidfrag);
		ipf_locks_done = 0;
	}
	return 0;
}


int iplattach __P((void))
{
	int i;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplattach()\n");
#endif

	ASSERT(rw_read_locked(&ipf_global.ipf_lk) == 0);

	bzero((char *)frcache, sizeof(frcache));
	MUTEX_INIT(&ipf_rw, "ipf rw mutex");
	MUTEX_INIT(&ipf_timeoutlock, "ipf timeout lock mutex");
	RWLOCK_INIT(&ipf_ipidfrag, "ipf IP NAT-Frag rwlock");
	ipf_locks_done = 1;

	if (fr_initialise() == -1)
		return -1;

#if SOLARIS2 >= 8
	ip_forwarding = &ip_g_forward;
#endif
	/*
	 * XXX - There is no terminator for this array, so it is not possible
	 * to tell if what we are looking for is missing and go off the end
	 * of the array.
	 */

	for (i = 0; ; i++) {
		if (!strcmp(ip_param_arr[i].ip_param_name, "ip_def_ttl")) {
			ip_ttl_ptr = &ip_param_arr[i].ip_param_value;
		} else if (!strcmp(ip_param_arr[i].ip_param_name,
			    "ip_path_mtu_discovery")) {
			ip_mtudisc = &ip_param_arr[i].ip_param_value;
		}
#if SOLARIS2 < 8
		else if (!strcmp(ip_param_arr[i].ip_param_name,
			    "ip_forwarding")) {
			ip_forwarding = &ip_param_arr[i].ip_param_value;
		}
#else
		else if (!strcmp(ip_param_arr[i].ip_param_name,
			    "ip6_forwarding")) {
			ip6_forwarding = &ip_param_arr[i].ip_param_value;
		}
#endif

		if (ip_mtudisc != NULL && ip_ttl_ptr != NULL &&
#if SOLARIS2 >= 8
		    ip6_forwarding != NULL &&
#endif
		    ip_forwarding != NULL)
			break;
	}

	if (fr_control_forwarding & 1) {
		*ip_forwarding = 1;
#if SOLARIS2 >= 8
		*ip6_forwarding = 1;
#endif
	}

	return 0;
}


/*
 * Filter ioctl interface.
 */
/*ARGSUSED*/
int iplioctl(dev, cmd, data, mode, cp, rp)
dev_t dev;
int cmd;
#if SOLARIS2 >= 7
intptr_t data;
#else
int *data;
#endif
int mode;
cred_t *cp;
int *rp;
{
	int error = 0, tmp;
	friostat_t fio;
	minor_t unit;
	u_int enable;

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplioctl(%x,%x,%x,%d,%x,%d)\n",
		dev, cmd, data, mode, cp, rp);
#endif
	unit = getminor(dev);
	if (IPL_LOGMAX < unit)
		return ENXIO;

	if (fr_running <= 0) {
		if (unit != IPL_LOGIPF)
			return EIO;
		if (cmd != SIOCIPFGETNEXT && cmd != SIOCIPFGET &&
		    cmd != SIOCIPFSET && cmd != SIOCFRENB && cmd != SIOCGETFS)
			return EIO;
	}

	READ_ENTER(&ipf_global);

	error = fr_ioctlswitch(unit, (caddr_t)data, cmd, mode);
	if (error != -1) {
		RWLOCK_EXIT(&ipf_global);
		return error;
	}
	error = 0;

	switch (cmd)
	{
	case SIOCFRENB :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = COPYIN((caddr_t)data, (caddr_t)&enable,
				       sizeof(enable));
			if (error != 0) {
				error = EFAULT;
				break;
			}

			RWLOCK_EXIT(&ipf_global);
			WRITE_ENTER(&ipf_global);
			if (enable) {
				if (fr_running > 0)
					error = 0;
				else
					error = iplattach();
				if (error == 0)
					fr_running = 1;
				else
					(void) ipldetach();
			} else {
				error = ipldetach();
				if (error == 0)
					fr_running = -1;
			}
		}
		break;
	case SIOCIPFSET :
		if (!(mode & FWRITE)) {
			error = EPERM;
			break;
		}
		/* FALLTHRU */
	case SIOCIPFGETNEXT :
	case SIOCIPFGET :
		error = fr_ipftune(cmd, (void *)data);
		break;
	case SIOCSETFF :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = COPYIN((caddr_t)data, (caddr_t)&fr_flags,
			       sizeof(fr_flags));
			if (error != 0)
				error = EFAULT;
		}
		break;
	case SIOCGETFF :
		error = COPYOUT((caddr_t)&fr_flags, (caddr_t)data,
			       sizeof(fr_flags));
		if (error != 0)
			error = EFAULT;
		break;
	case SIOCFUNCL :
		error = fr_resolvefunc((void *)data);
		break;
	case SIOCINAFR :
	case SIOCRMAFR :
	case SIOCADAFR :
	case SIOCZRLST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, (caddr_t)data,
					  fr_active, 1);
		break;
	case SIOCINIFR :
	case SIOCRMIFR :
	case SIOCADIFR :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = frrequest(unit, cmd, (caddr_t)data,
					  1 - fr_active, 1);
		break;
	case SIOCSWAPA :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			WRITE_ENTER(&ipf_mutex);
			bzero((char *)frcache, sizeof(frcache[0]) * 2);
			error = COPYOUT((caddr_t)&fr_active, (caddr_t)data,
				       sizeof(fr_active));
			if (error != 0)
				error = EFAULT;
			else
				fr_active = 1 - fr_active;
			RWLOCK_EXIT(&ipf_mutex);
		}
		break;
	case SIOCGETFS :
		fr_getstat(&fio);
		error = fr_outobj((void *)data, &fio, IPFOBJ_IPFSTAT);
		break;
	case SIOCFRZST :
		if (!(mode & FWRITE))
			error = EPERM;
		else
			error = fr_zerostats((caddr_t)data);
		break;
	case	SIOCIPFFL :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			error = COPYIN((caddr_t)data, (caddr_t)&tmp,
				       sizeof(tmp));
			if (!error) {
				tmp = frflush(unit, tmp);
				error = COPYOUT((caddr_t)&tmp, (caddr_t)data,
					       sizeof(tmp));
				if (error != 0)
					error = EFAULT;
			} else
				error = EFAULT;
		}
		break;
	case SIOCSTLCK :
		error = COPYIN((caddr_t)data, (caddr_t)&tmp, sizeof(tmp));
		if (error == 0) {
			fr_state_lock = tmp;
			fr_nat_lock = tmp;
			fr_frag_lock = tmp;
			fr_auth_lock = tmp;
		} else
			error = EFAULT;
	break;
#ifdef	IPFILTER_LOG
	case	SIOCIPFFB :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			tmp = ipflog_clear(unit);
			error = COPYOUT((caddr_t)&tmp, (caddr_t)data,
				       sizeof(tmp));
			if (error)
				error = EFAULT;
		}
		break;
#endif /* IPFILTER_LOG */
	case SIOCFRSYN :
		if (!(mode & FWRITE))
			error = EPERM;
		else {
			RWLOCK_EXIT(&ipf_global);
			WRITE_ENTER(&ipf_global);
			error = ipfsync();
		}
		break;
	case SIOCGFRST :
		error = fr_outobj((void *)data, fr_fragstats(),
				  IPFOBJ_FRAGSTAT);
		break;
	case FIONREAD :
#ifdef	IPFILTER_LOG
		tmp = (int)iplused[IPL_LOGIPF];

		error = COPYOUT((caddr_t)&tmp, (caddr_t)data, sizeof(tmp));
		if (error != 0)
			error = EFAULT;
#endif
		break;
	default :
		cmn_err(CE_NOTE, "Unknown: cmd %#x data %p", cmd, (void *)data);
		error = EINVAL;
		break;
	}
	RWLOCK_EXIT(&ipf_global);
	return error;
}


void	*get_unit(name, v)
char	*name;
int	v;
{
	void *ifp;
	qif_t *qf;
	int sap;

	if (v == 4)
		sap = 0x0800;
	else if (v == 6)
		sap = 0x86dd;
	else
		return NULL;
	rw_enter(&pfil_rw, RW_READER);
	qf = qif_iflookup(name, sap);
	rw_exit(&pfil_rw);
	return qf;
}


/*
 * routines below for saving IP headers to buffer
 */
/*ARGSUSED*/
int iplopen(devp, flags, otype, cred)
dev_t *devp;
int flags, otype;
cred_t *cred;
{
	minor_t min = getminor(*devp);

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplopen(%x,%x,%x,%x)\n", devp, flags, otype, cred);
#endif
	if (!(otype & OTYP_CHR))
		return ENXIO;

	min = (IPL_LOGMAX < min) ? ENXIO : 0;
	return min;
}


/*ARGSUSED*/
int iplclose(dev, flags, otype, cred)
dev_t dev;
int flags, otype;
cred_t *cred;
{
	minor_t	min = getminor(dev);

#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplclose(%x,%x,%x,%x)\n", dev, flags, otype, cred);
#endif

	min = (IPL_LOGMAX < min) ? ENXIO : 0;
	return min;
}

#ifdef	IPFILTER_LOG
/*
 * iplread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
/*ARGSUSED*/
int iplread(dev, uio, cp)
dev_t dev;
register struct uio *uio;
cred_t *cp;
{
# ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplread(%x,%x,%x)\n", dev, uio, cp);
# endif
# ifdef	IPFILTER_SYNC
	if (getminor(dev) == IPL_LOGSYNC)
		return ipfsync_read(uio);
# endif

	return ipflog_read(getminor(dev), uio);
}
#endif /* IPFILTER_LOG */


#ifdef	IPFILTER_SYNC
/*
 * iplread/ipllog
 * both of these must operate with at least splnet() lest they be
 * called during packet processing and cause an inconsistancy to appear in
 * the filter lists.
 */
int iplwrite(dev, uio, cp)
dev_t dev;
register struct uio *uio;
cred_t *cp;
{
#ifdef	IPFDEBUG
	cmn_err(CE_CONT, "iplwrite(%x,%x,%x)\n", dev, uio, cp);
#endif
	if (getminor(dev) != IPL_LOGSYNC)
		return ENXIO;
	return ipfsync_write(uio);
}
#endif /* IPFILTER_SYNC */


/*
 * fr_send_reset - this could conceivably be a call to tcp_respond(), but that
 * requires a large amount of setting up and isn't any more efficient.
 */
int fr_send_reset(fin)
fr_info_t *fin;
{
	tcphdr_t *tcp, *tcp2;
	int tlen, hlen;
	mblk_t *m;
#ifdef	USE_INET6
	ip6_t *ip6;
#endif
	ip_t *ip;

	tcp = fin->fin_dp;
	if (tcp->th_flags & TH_RST)
		return -1;

#ifndef	IPFILTER_CKSUM
	if (fr_checkl4sum(fin) == -1)
		return -1;
#endif

	tlen = (tcp->th_flags & (TH_SYN|TH_FIN)) ? 1 : 0;
#ifdef	USE_INET6
	if (fin->fin_v == 6)
		hlen = sizeof(ip6_t);
	else
#endif
		hlen = sizeof(ip_t);
	hlen += sizeof(*tcp2);
	if ((m = (mblk_t *)allocb(hlen + 64, BPRI_HI)) == NULL)
		return -1;

	m->b_rptr += 64;
	MTYPE(m) = M_DATA;
	m->b_wptr = m->b_rptr + hlen;
	bzero((char *)m->b_rptr, hlen);
	tcp2 = (struct tcphdr *)(m->b_rptr + hlen - sizeof(*tcp2));
	tcp2->th_dport = tcp->th_sport;
	tcp2->th_sport = tcp->th_dport;
	if (tcp->th_flags & TH_ACK) {
		tcp2->th_seq = tcp->th_ack;
		tcp2->th_flags = TH_RST;
	} else {
		tcp2->th_ack = ntohl(tcp->th_seq);
		tcp2->th_ack += tlen;
		tcp2->th_ack = htonl(tcp2->th_ack);
		tcp2->th_flags = TH_RST|TH_ACK;
	}
	tcp2->th_off = sizeof(struct tcphdr) >> 2;

	/*
	 * This is to get around a bug in the Solaris 2.4/2.5 TCP checksum
	 * computation that is done by their put routine.
	 */
#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6 = (ip6_t *)m->b_rptr;
		ip6->ip6_src = fin->fin_dst6;
		ip6->ip6_dst = fin->fin_src6;
		ip6->ip6_plen = htons(sizeof(*tcp));
		ip6->ip6_nxt = IPPROTO_TCP;
	} else
#endif
	{
		ip = (ip_t *)m->b_rptr;
		ip->ip_src.s_addr = fin->fin_daddr;
		ip->ip_dst.s_addr = fin->fin_saddr;
		ip->ip_id = fr_nextipid(fin);
		ip->ip_hl = sizeof(*ip) >> 2;
		ip->ip_p = IPPROTO_TCP;
		ip->ip_len = htons(sizeof(*ip) + sizeof(*tcp));
		ip->ip_tos = fin->fin_ip->ip_tos;
		tcp2->th_sum = fr_cksum(m, ip, IPPROTO_TCP, tcp2);
	}
	return fr_send_ip(fin, m);
}


/*ARGSUSED*/
static int fr_send_ip(fin, m)
fr_info_t *fin;
mblk_t *m;
{
	int i;

#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		ip6_t *ip6;

		ip6 = (ip6_t *)m->b_rptr;
		ip6->ip6_flow = 0;
		ip6->ip6_vfc = 0x60;
		ip6->ip6_hlim = 127;
	} else
#endif
	{
		ip_t *ip;

		ip = (ip_t *)m->b_rptr;
		ip->ip_v = IPVERSION;
		ip->ip_ttl = (u_char)(*ip_ttl_ptr);
		ip->ip_off = htons(*ip_mtudisc ? IP_DF : 0);
		ip->ip_sum = ipf_cksum((u_short *)ip, sizeof(*ip));
	}
	i = fr_fastroute(m, &m, fin, NULL);
	return i;
}


int fr_send_icmp_err(type, fin, dst)
int type;
fr_info_t *fin;
int dst;
{
	struct in_addr dst4;
	struct icmp *icmp;
	qpktinfo_t *qpi;
	int hlen, code;
	u_short sz;
#ifdef	USE_INET6
	mblk_t *mb;
#endif
	mblk_t *m;
#ifdef	USE_INET6
	ip6_t *ip6;
#endif
	ip_t *ip;

	if ((type < 0) || (type > ICMP_MAXTYPE))
		return -1;

	code = fin->fin_icode;
#ifdef USE_INET6
	if ((code < 0) || (code > sizeof(icmptoicmp6unreach)/sizeof(int)))
		return -1;
#endif

#ifndef	IPFILTER_CKSUM
	if (fr_checkl4sum(fin) == -1)
		return -1;
#endif

	qpi = fin->fin_qpi;

#ifdef	USE_INET6
	mb = fin->fin_qfm;

	if (fin->fin_v == 6) {
		sz = sizeof(ip6_t);
		sz += MIN(mb->b_wptr - mb->b_rptr, 512);
		hlen = sizeof(ip6_t);
		type = icmptoicmp6types[type];
		if (type == ICMP6_DST_UNREACH)
			code = icmptoicmp6unreach[code];
	} else
#endif
	{
		if ((fin->fin_p == IPPROTO_ICMP) &&
		    !(fin->fin_flx & FI_SHORT))
			switch (ntohs(fin->fin_data[0]) >> 8)
			{
			case ICMP_ECHO :
			case ICMP_TSTAMP :
			case ICMP_IREQ :
			case ICMP_MASKREQ :
				break;
			default :
				return 0;
			}

		sz = sizeof(ip_t) * 2;
		sz += 8;		/* 64 bits of data */
		hlen = sizeof(ip_t);
	}

	sz += offsetof(struct icmp, icmp_ip);
	if ((m = (mblk_t *)allocb((size_t)sz + 64, BPRI_HI)) == NULL)
		return -1;
	MTYPE(m) = M_DATA;
	m->b_rptr += 64;
	m->b_wptr = m->b_rptr + sz;
	bzero((char *)m->b_rptr, (size_t)sz);
	icmp = (struct icmp *)(m->b_rptr + hlen);
	icmp->icmp_type = type & 0xff;
	icmp->icmp_code = code & 0xff;
#ifdef	icmp_nextmtu
	if (type == ICMP_UNREACH && (qpi->qpi_max_frag != 0) &&
	    fin->fin_icode == ICMP_UNREACH_NEEDFRAG)
		icmp->icmp_nextmtu = htons(qpi->qpi_max_frag);
#endif

#ifdef	USE_INET6
	if (fin->fin_v == 6) {
		struct in6_addr dst6;
		int csz;

		if (dst == 0) {
			if (fr_ifpaddr(6, FRI_NORMAL, qpi->qpi_ill,
				       (struct in_addr *)&dst6, NULL) == -1) {
				FREE_MB_T(m);
				return -1;
			}
		} else
			dst6 = fin->fin_dst6;

		csz = sz;
		sz -= sizeof(ip6_t);
		ip6 = (ip6_t *)m->b_rptr;
		ip6->ip6_plen = htons((u_short)sz);
		ip6->ip6_nxt = IPPROTO_ICMPV6;
		ip6->ip6_src = dst6;
		ip6->ip6_dst = fin->fin_src6;
		sz -= offsetof(struct icmp, icmp_ip);
		bcopy((char *)mb->b_rptr, (char *)&icmp->icmp_ip, sz);
		icmp->icmp_cksum = csz - sizeof(ip6_t);
	} else
#endif
	{
		ip = (ip_t *)m->b_rptr;
		ip->ip_hl = sizeof(*ip) >> 2;
		ip->ip_p = IPPROTO_ICMP;
		ip->ip_id = fin->fin_ip->ip_id;
		ip->ip_tos = fin->fin_ip->ip_tos;
		ip->ip_len = htons((u_short)sz);
		if (dst == 0) {
			if (fr_ifpaddr(4, FRI_NORMAL, qpi->qpi_ill,
				       &dst4, NULL) == -1) {
				FREE_MB_T(m);
				return -1;
			}
		} else
			dst4 = fin->fin_dst;
		ip->ip_src = dst4;
		ip->ip_dst = fin->fin_src;
		bcopy((char *)fin->fin_ip, (char *)&icmp->icmp_ip,
		      sizeof(*fin->fin_ip));
		bcopy((char *)fin->fin_ip + fin->fin_hlen,
		      (char *)&icmp->icmp_ip + sizeof(*fin->fin_ip), 8);
		icmp->icmp_cksum = ipf_cksum((u_short *)icmp,
					     sz - sizeof(ip_t));
	}

	/*
	 * Need to exit out of these so we don't recursively call rw_enter
	 * from fr_qout.
	 */
	return fr_send_ip(fin, m);
}


/*
 * return the first IP Address associated with an interface
 */
/*ARGSUSED*/
int fr_ifpaddr(v, atype, ifptr, inp, inpmask)
int v, atype;
void *ifptr;
struct in_addr *inp, *inpmask;
{
#ifdef	USE_INET6
	struct sockaddr_in6 sin6, mask6;
#endif
	struct sockaddr_in sin, mask;
	qif_t *qif;

	qif = ifptr;

#ifdef	USE_INET6
	if (v == 6) {
		in6_addr_t *inp6;
		ipif_t *ipif;
		ill_t *ill;

		ill = qif->qf_ill;

		/*
		 * First is always link local.
		 */
		for (ipif = ill->ill_ipif; ipif; ipif = ipif->ipif_next) {
			inp6 = &ipif->ipif_v6lcl_addr;
			if (!IN6_IS_ADDR_LINKLOCAL(inp6) &&
			    !IN6_IS_ADDR_LOOPBACK(inp6))
				break;
		}
		if (ipif == NULL)
			return -1;

		mask6.sin6_addr = ipif->ipif_v6net_mask;
		if (atype == FRI_BROADCAST)
			sin6.sin6_addr = ipif->ipif_v6brd_addr;
		else if (atype == FRI_PEERADDR)
			sin6.sin6_addr = ipif->ipif_v6pp_dst_addr;
		else
			sin6.sin6_addr = *inp6;
		return fr_ifpfillv6addr(atype, &sin6, &mask6, inp, inpmask);
	}
#endif

	switch (atype)
	{
	case FRI_BROADCAST :
		sin.sin_addr.s_addr = QF_V4_BROADCAST(qif);
		break;
	case FRI_PEERADDR :
		sin.sin_addr.s_addr = QF_V4_PEERADDR(qif);
		break;
	default :
		sin.sin_addr.s_addr = QF_V4_ADDR(qif);
		break;
	}
	mask.sin_addr.s_addr = QF_V4_NETMASK(qif);

	return fr_ifpfillv4addr(atype, &sin, &mask, inp, inpmask);
}


u_32_t fr_newisn(fin)
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

	MD5Update(&ctx, ipf_iss_secret, sizeof(ipf_iss_secret));

	MD5Final(hash, &ctx);

	bcopy(hash, &newiss, sizeof(newiss));

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
/* Function:    fr_nextipid                                                 */
/* Returns:     int - 0 == success, -1 == error (packet should be droppped) */
/* Parameters:  fin(I) - pointer to packet information                      */
/*                                                                          */
/* Returns the next IPv4 ID to use for this packet.                         */
/* ------------------------------------------------------------------------ */
INLINE u_short fr_nextipid(fin)
fr_info_t *fin;
{
	static u_short ipid = 0;
	ipstate_t *is;
	nat_t *nat;
	u_short id;

	MUTEX_ENTER(&ipf_rw);
	if (fin->fin_state != NULL) {
		is = fin->fin_state;
		id = (u_short)(is->is_pkts[(fin->fin_rev << 1) + 1] & 0xffff);
	} else if (fin->fin_nat != NULL) {
		nat = fin->fin_nat;
		id = (u_short)(nat->nat_pkts[fin->fin_out] & 0xffff);
	} else
		id = ipid++;
	MUTEX_EXIT(&ipf_rw);

	return id;
}


#ifndef IPFILTER_CKSUM
/* ARGSUSED */
#endif
INLINE void fr_checkv4sum(fin)
fr_info_t *fin;
{
#ifdef IPFILTER_CKSUM
	if (fr_checkl4sum(fin) == -1)
		fin->fin_flx |= FI_BAD;
#endif
}


#ifdef USE_INET6
# ifndef IPFILTER_CKSUM
/* ARGSUSED */
# endif
INLINE void fr_checkv6sum(fin)
fr_info_t *fin;
{
# ifdef IPFILTER_CKSUM
	if (fr_checkl4sum(fin) == -1)
		fin->fin_flx |= FI_BAD;
# endif
}
#endif /* USE_INET6 */


/*
 * Function:    fr_verifysrc
 * Returns:     int (really boolean)
 * Parameters:  fin - packet information
 *
 * Check whether the packet has a valid source address for the interface on
 * which the packet arrived, implementing the "fr_chksrc" feature.
 * Returns true iff the packet's source address is valid.
 * Pre-Solaris 10, we call into the routing code to make the determination.
 * On Solaris 10 and later, we have a valid address set from pfild to check
 * against.
 */
int fr_verifysrc(fin)
fr_info_t *fin;
{
	ire_t *dir;
	int result;

#if SOLARIS2 >= 6
	ire_t *gw;

	dir = ire_route_lookup(fin->fin_saddr, 0xffffffff, 0, 0, NULL,
			       &gw, NULL, MATCH_IRE_DSTONLY|MATCH_IRE_DEFAULT|
			       MATCH_IRE_RECURSIVE);
#else
	dir = ire_lookup(fin->fin_saddr);
#endif

	if (!dir)
		return 0;
	result = (ire_to_ill(dir) == fin->fin_ifp);
#if SOLARIS2 >= 8
	ire_refrele(dir);
#endif
	return result;
}


#if (SOLARIS2 < 7)
void fr_slowtimer()
#else
/*ARGSUSED*/
void fr_slowtimer __P((void *ptr))
#endif
{

	WRITE_ENTER(&ipf_global);
	if (fr_running <= 0) {
		if (fr_running == -1)
			fr_timer_id = timeout(fr_slowtimer, NULL,
					      drv_usectohz(500000));
		else
			fr_timer_id = NULL;
		RWLOCK_EXIT(&ipf_global);
		return;
	}
	MUTEX_DOWNGRADE(&ipf_global);

	fr_fragexpire();
	fr_timeoutstate();
	fr_natexpire();
	fr_authexpire();
	fr_ticks++;
	if (fr_running == -1 || fr_running == 1)
		fr_timer_id = timeout(fr_slowtimer, NULL, drv_usectohz(500000));
	else
		fr_timer_id = NULL;
	RWLOCK_EXIT(&ipf_global);
}


/*
 * Function:  fr_fastroute
 * Returns:    0: success;
 *            -1: failed
 * Parameters:
 *    mb: the message block where ip head starts
 *    mpp: the pointer to the pointer of the orignal
 *            packet message
 *    fin: packet information
 *    fdp: destination interface information
 *    if it is NULL, no interface information provided.
 *
 * This function is for fastroute/to/dup-to rules. It calls
 * pfil_make_lay2_packet to search route, make lay-2 header
 * ,and identify output queue for the IP packet.
 * The destination address depends on the following conditions:
 * 1: for fastroute rule, fdp is passed in as NULL, so the
 *    destination address is the IP Packet's destination address
 * 2: for to/dup-to rule, if an ip address is specified after
 *    the interface name, this address is the as destination
 *    address. Otherwise IP Packet's destination address is used
 */
int fr_fastroute(mb, mpp, fin, fdp)
mblk_t *mb, **mpp;
fr_info_t *fin;
frdest_t *fdp;
{
	struct in_addr dst;
	queue_t *q = NULL;
	mblk_t *mp = NULL;
	ire_t *dir, *gw;
	size_t hlen = 0;
	qpktinfo_t *qpi;
	frentry_t *fr;
	frdest_t fd;
	ill_t *ifp;
	u_char *s;
	ip_t *ip;
#ifndef	sparc
	u_short __iplen, __ipoff;
#endif
#ifdef	USE_INET6
	ip6_t *ip6 = (ip6_t *)fin->fin_ip;
	struct in6_addr dst6;
#endif

	fr = fin->fin_fr;
	ip = fin->fin_ip;
	qpi = fin->fin_qpi;

	/*
	 * If this is a duplicate mblk then we want ip to point at that
	 * data, not the original, if and only if it is already pointing at
	 * the current mblk data.
	 */
	if (ip == (ip_t *)qpi->qpi_m->b_rptr && qpi->qpi_m != mb)
		ip = (ip_t *)mb->b_rptr;

	/*
	 * If there is another M_PROTO, we don't want it
	 */
	if (*mpp != mb) {
		mp = unlinkb(*mpp);
		freeb(*mpp);
		*mpp = mp;
	}

	/*
	 * If the fdp is NULL then there is no set route for this packet.
	 */
	if (fdp == NULL) {
		ifp = fin->fin_ifp;

		switch (fin->fin_v)
		{
		case 4 :
			fd.fd_ip = ip->ip_dst;
			break;
#ifdef USE_INET6
		case 6 :
			fd.fd_ip6.in6 = ip6->ip6_dst;
			break;
#endif
		}
		fdp = &fd;
	} else {
		ifp = fdp->fd_ifp;

		if (ifp == NULL || ifp == (void *)-1)
			goto bad_fastroute;
	}

	/*
	 * In case we're here due to "to <if>" being used with
	 * "keep state", check that we're going in the correct
	 * direction.
	 */
	if ((fr != NULL) && (fin->fin_rev != 0)) {
		if ((ifp != NULL) && (fdp == &fr->fr_tif))
			return -1;
		dst.s_addr = fin->fin_fi.fi_daddr;
	} else {
		if (fin->fin_v == 4) {
			if (fdp->fd_ip.s_addr != 0)
				dst = fdp->fd_ip;
			else
				dst.s_addr = fin->fin_fi.fi_daddr;
		}
#ifdef USE_INET6
		else if (fin->fin_v == 6) {
			if (IP6_NOTZERO(&fdp->fd_ip))
				dst6 = fdp->fd_ip6.in6;
			else
				dst6 = fin->fin_dst6;
		}
#endif
	}

#if SOLARIS2 >= 6
	gw = NULL;
	if (fin->fin_v == 4) {
		dir = ire_route_lookup(dst.s_addr, 0xffffffff, 0, 0, NULL,
					&gw, NULL, MATCH_IRE_DSTONLY|
					MATCH_IRE_DEFAULT|MATCH_IRE_RECURSIVE);
	}
# ifdef	USE_INET6
	else if (fin->fin_v == 6) {
		dir = ire_route_lookup_v6(&ip6->ip6_dst, NULL, 0, 0,
					NULL, &gw, NULL, MATCH_IRE_DSTONLY|
					MATCH_IRE_DEFAULT|MATCH_IRE_RECURSIVE);
	}
# endif
#else
	dir = ire_lookup(dst.s_addr);
#endif
#if SOLARIS2 < 8
	if (dir != NULL)
		if (dir->ire_ll_hdr_mp == NULL || dir->ire_ll_hdr_length == 0)
			dir = NULL;
#else
	if (dir != NULL)
		if (dir->ire_fp_mp == NULL || dir->ire_dlureq_mp == NULL)
			dir = NULL;
#endif

	if (dir != NULL) {
#if SOLARIS2 < 8
		mp = dir->ire_ll_hdr_mp;
		hlen = dir->ire_ll_hdr_length;
#else
		mp = dir->ire_fp_mp;
		hlen = mp ? mp->b_wptr - mp->b_rptr : 0;
		if (mp == NULL)
			mp = dir->ire_dlureq_mp;
#endif

		if (fin->fin_out == 0) {
			void *saveifp;
			u_32_t pass;

			saveifp = fin->fin_ifp;
			fin->fin_ifp = ifp;
			fin->fin_out = 1;
			fr_acctpkt(fin, &pass);
			fin->fin_fr = NULL;
			if (!fr || !(fr->fr_flags & FR_RETMASK))
				(void) fr_checkstate(fin, &pass);
			(void) fr_checknatout(fin, NULL);
			fin->fin_out = 0;
			fin->fin_ifp = saveifp;
		}
#ifndef sparc
		if (fin->fin_v == 4) {
			__iplen = (u_short)ip->ip_len,
			__ipoff = (u_short)ip->ip_off;

			ip->ip_len = htons(__iplen);
			ip->ip_off = htons(__ipoff);
		}
#endif

		if (mp != NULL) {
			s = mb->b_rptr;
			if (
#if (SOLARIS2 >= 6) && defined(ICK_M_CTL_MAGIC)
			    (dohwcksum &&
			     ifp->ill_ick.ick_magic == ICK_M_CTL_MAGIC) ||
#endif
			    (hlen && (s - mb->b_datap->db_base) >= hlen)) {
				s -= hlen;
				mb->b_rptr = (u_char *)s;
				bcopy((char *)mp->b_rptr, (char *)s, hlen);
			} else {
				mblk_t *mp2;

				mp2 = copyb(mp);
				if (mp2 == NULL)
					goto bad_fastroute;
				linkb(mp2, mb);
				mb = mp2;
			}
		}
		*mpp = mb;

		if (dir->ire_stq != NULL)
			q = dir->ire_stq;
		else if (dir->ire_rfq != NULL)
			q = WR(dir->ire_rfq);
		if (q != NULL)
			q = q->q_next;
		if (q != NULL) {
			RWLOCK_EXIT(&ipf_global);
#if (SOLARIS2 >= 6) && defined(ICK_M_CTL_MAGIC)
			if ((fin->fin_p == IPPROTO_TCP) && dohwcksum &&
			    (ifp->ill_ick.ick_magic == ICK_M_CTL_MAGIC)) {
				tcphdr_t *tcp;
				u_32_t t;

				tcp = (tcphdr_t *)((char *)ip + fin->fin_hlen);
				t = ip->ip_src.s_addr;
				t += ip->ip_dst.s_addr;
				t += 30;
				t = (t & 0xffff) + (t >> 16);
				tcp->th_sum = t & 0xffff;
			}
#endif
			putnext(q, mb);
			ATOMIC_INCL(fr_frouteok[0]);
			READ_ENTER(&ipf_global);
			return 0;
		}
	}

bad_fastroute:
	freemsg(mb);
	ATOMIC_INCL(fr_frouteok[1]);
	return -1;
}
