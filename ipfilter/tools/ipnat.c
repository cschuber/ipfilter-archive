/*
 * Copyright (C) 2001-2007 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * Added redirect stuff and a variety of bug fixes. (mcn@EnGarde.com)
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#if !defined(__SVR4) && !defined(__svr4__)
#include <strings.h>
#else
#include <sys/byteorder.h>
#endif
#include <sys/time.h>
#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/file.h>
#define _KERNEL
#include <sys/uio.h>
#undef _KERNEL
#include <sys/socket.h>
#include <sys/ioctl.h>
#if defined(sun) && (defined(__svr4__) || defined(__SVR4))
# include <sys/ioccom.h>
# include <sys/sysmacros.h>
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <net/if.h>
#if __FreeBSD_version >= 300000
# include <net/if_var.h>
#endif
#include <netdb.h>
#include <arpa/nameser.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <ctype.h>
#if defined(linux)
# include <linux/a.out.h>
#else
# include <nlist.h>
#endif
#include "ipf.h"
#include "netinet/ipl.h"
#include "kmem.h"

#ifdef	__hpux
# define	nlist	nlist64
#endif

#if	defined(sun) && !SOLARIS2
# define	STRERROR(x)	sys_errlist[x]
extern	char	*sys_errlist[];
#else
# define	STRERROR(x)	strerror(x)
#endif

#if !defined(lint)
static const char sccsid[] ="@(#)ipnat.c	1.9 6/5/96 (C) 1993 Darren Reed";
static const char rcsid[] = "@(#)$Id$";
#endif


#if	SOLARIS
#define	bzero(a,b)	memset(a,0,b)
#endif
int	use_inet6 = 0;
char	thishost[MAXHOSTNAMELEN];

extern	char	*optarg;

void	dostats __P((int, natstat_t *, int, int, int *));
void	dotable __P((natstat_t *, int, int, int, char *));
void	flushtable __P((int, int, int *));
void	usage __P((char *));
int	main __P((int, char*[]));
void	showhostmap __P((natstat_t *nsp));
void	natstat_dead __P((natstat_t *, char *));
void	dostats_live __P((int, natstat_t *, int, int *));
void	showhostmap_dead __P((natstat_t *));
void	showhostmap_live __P((int, natstat_t *));
void	dostats_dead __P((natstat_t *, int, int *));
int	nat_matcharray __P((nat_t *, int *));

int		opts;
int		nohdrfields = 0;
wordtab_t	*nat_fields = NULL;

void usage(name)
	char *name;
{
	fprintf(stderr, "Usage: %s [-CFhlnrRsv] [-f filename]\n", name);
	exit(1);
}


int main(argc, argv)
	int argc;
	char *argv[];
{
	int fd, c, mode, *natfilter;
	char *file, *core, *kernel;
	natstat_t ns, *nsp;
	ipfobj_t obj;

	fd = -1;
	opts = 0;
	nsp = &ns;
	file = NULL;
	core = NULL;
	kernel = NULL;
	mode = O_RDWR;
	natfilter = NULL;

	assigndefined(getenv("IPNAT_PREDEFINED"));

	while ((c = getopt(argc, argv, "CdFf:hlm:M:N:nO:rRsv")) != -1)
		switch (c)
		{
		case 'C' :
			opts |= OPT_CLEAR;
			break;
		case 'd' :
			opts |= OPT_DEBUG;
			break;
		case 'f' :
			file = optarg;
			break;
		case 'F' :
			opts |= OPT_FLUSH;
			break;
		case 'h' :
			opts |=OPT_HITS;
			break;
		case 'l' :
			opts |= OPT_LIST;
			mode = O_RDONLY;
			break;
		case 'm' :
			natfilter = parseipfexpr(optarg, NULL);
			break;
		case 'M' :
			core = optarg;
			break;
		case 'N' :
			kernel = optarg;
			break;
		case 'n' :
			opts |= OPT_DONOTHING|OPT_DONTOPEN;
			mode = O_RDONLY;
			break;
		case 'O' :
			nat_fields = parsefields(natfields, optarg);
			break;
		case 'R' :
			opts |= OPT_NORESOLVE;
			break;
		case 'r' :
			opts |= OPT_REMOVE;
			break;
		case 's' :
			opts |= OPT_STAT;
			mode = O_RDONLY;
			break;
		case 'v' :
			opts |= OPT_VERBOSE;
			break;
		default :
			usage(argv[0]);
		}

	initparse();

	if ((kernel != NULL) || (core != NULL)) {
		(void) setgid(getgid());
		(void) setuid(getuid());
	}

	if (!(opts & OPT_DONOTHING)) {
		if (((fd = open(IPNAT_NAME, mode)) == -1) &&
		    ((fd = open(IPNAT_NAME, O_RDONLY)) == -1)) {
			(void) fprintf(stderr, "%s: open: %s\n", IPNAT_NAME,
				STRERROR(errno));
			exit(1);
		}
	}

	bzero((char *)&ns, sizeof(ns));

	if ((opts & OPT_DONOTHING) == 0) {
		if (checkrev(IPL_NAME) == -1) {
			fprintf(stderr, "User/kernel version check failed\n");
			exit(1);
		}
	}

	if (!(opts & OPT_DONOTHING) && (kernel == NULL) && (core == NULL)) {
		bzero((char *)&obj, sizeof(obj));
		obj.ipfo_rev = IPFILTER_VERSION;
		obj.ipfo_type = IPFOBJ_NATSTAT;
		obj.ipfo_size = sizeof(*nsp);
		obj.ipfo_ptr = (void *)nsp;
		if (ioctl(fd, SIOCGNATS, &obj) == -1) {
			ipferror(fd, "ioctl(SIOCGNATS)");
			exit(1);
		}
		(void) setgid(getgid());
		(void) setuid(getuid());
	} else if ((kernel != NULL) || (core != NULL)) {
		if (openkmem(kernel, core) == -1)
			exit(1);

		natstat_dead(nsp, kernel);
		if (opts & (OPT_LIST|OPT_STAT))
			dostats(fd, nsp, opts, 0, natfilter);
		exit(0);
	}

	if (opts & (OPT_FLUSH|OPT_CLEAR))
		flushtable(fd, opts, natfilter);
	if (file) {
		ipnat_parsefile(fd, ipnat_addrule, ioctl, file);
	}
	if (opts & (OPT_LIST|OPT_STAT))
		dostats(fd, nsp, opts, 1, natfilter);
	return 0;
}


/*
 * Read NAT statistic information in using a symbol table and memory file
 * rather than doing ioctl's.
 */
void natstat_dead(nsp, kernel)
	natstat_t *nsp;
	char *kernel;
{
	struct nlist nat_nlist[10] = {
		{ "nat_table" },		/* 0 */
		{ "nat_list" },
		{ "maptable" },
		{ "ipf_nattable_sz" },
		{ "ipf_natrules_sz" },
		{ "ipf_rdrrules_sz" },		/* 5 */
		{ "ipf_hostmap_sz" },
		{ "nat_instances" },
		{ NULL }
	};
	void *tables[2];

	if (nlist(kernel, nat_nlist) == -1) {
		fprintf(stderr, "nlist error\n");
		return;
	}

	/*
	 * Normally the ioctl copies all of these values into the structure
	 * for us, before returning it to userland, so here we must copy each
	 * one in individually.
	 */
	kmemcpy((char *)&tables, nat_nlist[0].n_value, sizeof(tables));
	nsp->ns_side[0].ns_table = tables[0];
	nsp->ns_side[1].ns_table = tables[1];

	kmemcpy((char *)&nsp->ns_list, nat_nlist[1].n_value,
		sizeof(nsp->ns_list));
	kmemcpy((char *)&nsp->ns_maptable, nat_nlist[2].n_value,
		sizeof(nsp->ns_maptable));
	kmemcpy((char *)&nsp->ns_nattab_sz, nat_nlist[3].n_value,
		sizeof(nsp->ns_nattab_sz));
	kmemcpy((char *)&nsp->ns_rultab_sz, nat_nlist[4].n_value,
		sizeof(nsp->ns_rultab_sz));
	kmemcpy((char *)&nsp->ns_rdrtab_sz, nat_nlist[5].n_value,
		sizeof(nsp->ns_rdrtab_sz));
	kmemcpy((char *)&nsp->ns_hostmap_sz, nat_nlist[6].n_value,
		sizeof(nsp->ns_hostmap_sz));
	kmemcpy((char *)&nsp->ns_instances, nat_nlist[7].n_value,
		sizeof(nsp->ns_instances));
}


/*
 * Issue an ioctl to flush either the NAT rules table or the active mapping
 * table or both.
 */
void flushtable(fd, opts, match)
	int fd, opts, *match;
{
	int n = 0;

	if (opts & OPT_FLUSH) {
		n = 0;
		if (!(opts & OPT_DONOTHING)) {
			if (match != NULL) {
				ipfobj_t obj;

				obj.ipfo_rev = IPFILTER_VERSION;
				obj.ipfo_size = match[0] * sizeof(int);
				obj.ipfo_type = IPFOBJ_IPFEXPR;
				obj.ipfo_ptr = match;
				if (ioctl(fd, SIOCMATCHFLUSH, &obj) == -1) {
					ipferror(fd, "ioctl(SIOCMATCHFLUSH)");
					n = -1;
				} else {
					n = obj.ipfo_retval;
				}
			} else if (ioctl(fd, SIOCIPFFL, &n) == -1) {
				ipferror(fd, "ioctl(SIOCIPFFL)");
				n = -1;
			}
		}
		if (n >= 0)
			printf("%d entries flushed from NAT table\n", n);
	}

	if (opts & OPT_CLEAR) {
		n = 1;
		if (!(opts & OPT_DONOTHING) && ioctl(fd, SIOCIPFFL, &n) == -1)
			ipferror(fd, "ioctl(SIOCCNATL)");
		else
			printf("%d entries flushed from NAT list\n", n);
	}
}


/*
 * Display NAT statistics.
 */
void dostats_dead(nsp, opts, filter)
	natstat_t *nsp;
	int opts, *filter;
{
	nat_t *np, nat;
	ipnat_t	ipn;
	int i;

	if (nat_fields == NULL) {
		printf("List of active MAP/Redirect filters:\n");
		while (nsp->ns_list) {
			if (kmemcpy((char *)&ipn, (long)nsp->ns_list,
				    sizeof(ipn))) {
				perror("kmemcpy");
				break;
			}
			if (opts & OPT_HITS)
				printf("%lu ", ipn.in_hits);
			printnat(&ipn, opts & (OPT_DEBUG|OPT_VERBOSE));
			nsp->ns_list = ipn.in_next;
		}
	}

	if (nat_fields == NULL) {
		printf("\nList of active sessions:\n");

	} else if (nohdrfields == 0) {
		for (i = 0; nat_fields[i].w_value != 0; i++) {
			printfieldhdr(natfields, nat_fields + i);
			if (nat_fields[i + 1].w_value != 0)
				printf("\t");
		}
		printf("\n");
	}

	for (np = nsp->ns_instances; np; np = nat.nat_next) {
		if (kmemcpy((char *)&nat, (long)np, sizeof(nat)))
			break;
		if ((filter != NULL) && (nat_matcharray(&nat, filter) == 0))
			continue;
		if (nat_fields != NULL) {
			for (i = 0; nat_fields[i].w_value != 0; i++) {
				printnatfield(&nat, nat_fields[i].w_value);
				if (nat_fields[i + 1].w_value != 0)
					printf("\t");
			}
			printf("\n");
		} else {
			printactivenat(&nat, opts, 0, nsp->ns_ticks);
			if (nat.nat_aps)
				printaps(nat.nat_aps, opts);
		}
	}

	if (opts & OPT_VERBOSE)
		showhostmap_dead(nsp);
}


void dotable(nsp, fd, alive, which, side)
	natstat_t *nsp;
	int fd, alive, which;
	char *side;
{
	int sz, i, used, maxlen, minlen, totallen;
	ipftable_t table;
	u_int *buckets;
	ipfobj_t obj;

	sz = sizeof(*buckets) * nsp->ns_nattab_sz;
	buckets = (u_int *)malloc(sz);
	if (buckets == NULL) {
		fprintf(stderr,
			"cannot allocate memory (%d) for buckets\n", sz);
		return;
	}

	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_GTABLE;
	obj.ipfo_size = sizeof(table);
	obj.ipfo_ptr = &table;

	if (which == 0) {
		table.ita_type = IPFTABLE_BUCKETS_NATIN;
	} else if (which == 1) {
		table.ita_type = IPFTABLE_BUCKETS_NATOUT;
	}
	table.ita_table = buckets;

	if (alive) {
		if (ioctl(fd, SIOCGTABL, &obj) != 0) {
			perror("SIOCFTABL");
			free(buckets);
			return;
		}
	} else {
		if (kmemcpy((char *)buckets, (u_long)nsp->ns_nattab_sz, sz)) {
			free(buckets);
			return;
		}
	}

	minlen = nsp->ns_side[which].ns_inuse;
	totallen = 0;
	maxlen = 0;
	used = 0;

	for (i = 0; i < nsp->ns_nattab_sz; i++) {
		if (buckets[i] > maxlen)
			maxlen = buckets[i];
		if (buckets[i] < minlen)
			minlen = buckets[i];
		if (buckets[i] != 0)
			used++;
		totallen += buckets[i];
	}

	printf("%d%%\thash efficiency %s\n",
	       totallen ? used * 100 / totallen : 0, side);
	printf("%2.2f%%\tbucket usage %s\n",
	       ((float)used / nsp->ns_nattab_sz) * 100.0, side);
	printf("%d\tminimal length %s\n", minlen, side);
	printf("%d\tmaximal length %s\n", maxlen, side);
	printf("%.3f\taverage length %s\n",
	       used ? ((float)totallen / used) : 0.0, side);

	free(buckets);
}


void dostats(fd, nsp, opts, alive, filter)
	natstat_t *nsp;
	int fd, opts, alive, *filter;
{
	/*
	 * Show statistics ?
	 */
	if (opts & OPT_STAT) {
		printnatside("in", nsp, &nsp->ns_side[0]);
		dotable(nsp, fd, alive, 0, "in");

		printnatside("out", nsp, &nsp->ns_side[1]);
		dotable(nsp, fd, alive, 1, "out");

		printf("%lu\tlog successes\n", nsp->ns_side[0].ns_log);
		printf("%lu\tlog failures\n", nsp->ns_side[1].ns_log);
		printf("%lu\tadded in\n%lu\tadded out\n",
			nsp->ns_side[0].ns_added,
			nsp->ns_side[1].ns_added);
		printf("%lu\texpired\n", nsp->ns_expire);
		printf("%u\twilds\n", nsp->ns_wilds);
		if (opts & OPT_VERBOSE)
			printf("list %p\n", nsp->ns_list);
	}

	if (opts & OPT_LIST) {
		if (alive)
			dostats_live(fd, nsp, opts, filter);
		else
			dostats_dead(nsp, opts, filter);
	}
}


/*
 * Display NAT statistics.
 */
void dostats_live(fd, nsp, opts, filter)
	natstat_t *nsp;
	int fd, opts, *filter;
{
	ipfgeniter_t iter;
	ipfobj_t obj;
	ipnat_t	ipn;
	nat_t nat;
	int i;

	bzero((char *)&obj, sizeof(obj));
	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_GENITER;
	obj.ipfo_size = sizeof(iter);
	obj.ipfo_ptr = &iter;

	iter.igi_type = IPFGENITER_IPNAT;
	iter.igi_nitems = 1;
	iter.igi_data = &ipn;

	/*
	 * Show list of NAT rules and NAT sessions ?
	 */
	if (nat_fields == NULL) {
		printf("List of active MAP/Redirect filters:\n");
		while (nsp->ns_list) {
			if (ioctl(fd, SIOCGENITER, &obj) == -1)
				break;
			if (opts & OPT_HITS)
				printf("%lu ", ipn.in_hits);
			printnat(&ipn, opts & (OPT_DEBUG|OPT_VERBOSE));
			nsp->ns_list = ipn.in_next;
		}
	}

	if (nat_fields == NULL) {
		printf("\nList of active sessions:\n");

	} else if (nohdrfields == 0) {
		for (i = 0; nat_fields[i].w_value != 0; i++) {
			printfieldhdr(natfields, nat_fields + i);
			if (nat_fields[i + 1].w_value != 0)
				printf("\t");
		}
		printf("\n");
	}


	iter.igi_type = IPFGENITER_NAT;
	iter.igi_nitems = 1;
	iter.igi_data = &nat;

	while (nsp->ns_instances != NULL) {
		if (ioctl(fd, SIOCGENITER, &obj) == -1)
			break;
		if ((filter != NULL) && (nat_matcharray(&nat, filter) == 0))
			continue;
		if (nat_fields != NULL) {
			for (i = 0; nat_fields[i].w_value != 0; i++) {
				printnatfield(&nat, nat_fields[i].w_value);
				if (nat_fields[i + 1].w_value != 0)
					printf("\t");
			}
			printf("\n");
		} else {
			printactivenat(&nat, opts, 1, nsp->ns_ticks);
			if (nat.nat_aps)
				printaps(nat.nat_aps, opts);
		}
		nsp->ns_instances = nat.nat_next;
	}

	if (opts & OPT_VERBOSE)
		showhostmap_live(fd, nsp);
}


/*
 * Display the active host mapping table.
 */
void showhostmap_dead(nsp)
	natstat_t *nsp;
{
	hostmap_t hm, *hmp, **maptable;
	u_int hv;

	printf("\nList of active host mappings:\n");

	maptable = (hostmap_t **)malloc(sizeof(hostmap_t *) *
					nsp->ns_hostmap_sz);
	if (kmemcpy((char *)maptable, (u_long)nsp->ns_maptable,
		    sizeof(hostmap_t *) * nsp->ns_hostmap_sz)) {
		perror("kmemcpy (maptable)");
		return;
	}

	for (hv = 0; hv < nsp->ns_hostmap_sz; hv++) {
		hmp = maptable[hv];

		while (hmp) {
			if (kmemcpy((char *)&hm, (u_long)hmp, sizeof(hm))) {
				perror("kmemcpy (hostmap)");
				return;
			}

			printhostmap(&hm, hv);
			hmp = hm.hm_next;
		}
	}
	free(maptable);
}


/*
 * Display the active host mapping table.
 */
void showhostmap_live(fd, nsp)
	int fd;
	natstat_t *nsp;
{
	ipfgeniter_t iter;
	hostmap_t hm;
	ipfobj_t obj;

	bzero((char *)&obj, sizeof(obj));
	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_type = IPFOBJ_GENITER;
	obj.ipfo_size = sizeof(iter);
	obj.ipfo_ptr = &iter;

	iter.igi_type = IPFGENITER_HOSTMAP;
	iter.igi_nitems = 1;
	iter.igi_data = &hm;

	printf("\nList of active host mappings:\n");

	while (nsp->ns_maplist != NULL) {
		if (ioctl(fd, SIOCGENITER, &obj) == -1)
			break;
		printhostmap(&hm, hm.hm_hv);
		nsp->ns_maplist = hm.hm_next;
	}
}


int nat_matcharray(nat, array)
	nat_t *nat;
	int *array;
{
	int i, n, *x, e, p;

	e = 0;
	n = array[0];
	x = array + 1;

	for (; n > 0; x += 3 + x[3]) {
		if (x[0] == IPF_EXP_END)
			break;
		e = 0;

		n -= x[3] + 3;

		p = x[0] >> 16;
		if (p != 0 && p != nat->nat_pr[1])
			break;

		switch (x[0])
		{
		case IPF_EXP_IP_PR :
			for (i = 0; !e && i < x[3]; i++) {
				e |= (nat->nat_pr[1] == x[i + 3]);
			}
			break;

		case IPF_EXP_IP_SRCADDR :
			for (i = 0; !e && i < x[3]; i++) {
				e |= ((nat->nat_nsrcaddr & x[i + 4]) ==
				      x[i + 3]);
			}
			break;

		case IPF_EXP_IP_DSTADDR :
			for (i = 0; !e && i < x[3]; i++) {
				e |= ((nat->nat_ndstaddr & x[i + 4]) ==
				      x[i + 3]);
			}
			break;

		case IPF_EXP_IP_ADDR :
			for (i = 0; !e && i < x[3]; i++) {
				e |= ((nat->nat_nsrcaddr & x[i + 4]) ==
				      x[i + 3]) ||
				     ((nat->nat_ndstaddr & x[i + 4]) ==
				      x[i + 3]);
			}
			break;

		case IPF_EXP_UDP_PORT :
		case IPF_EXP_TCP_PORT :
			for (i = 0; !e && i < x[3]; i++) {
				e |= (nat->nat_nsport == x[i + 3]) ||
				     (nat->nat_ndport == x[i + 3]);
			}
			break;

		case IPF_EXP_UDP_SPORT :
		case IPF_EXP_TCP_SPORT :
			for (i = 0; !e && i < x[3]; i++) {
				e |= (nat->nat_nsport == x[i + 3]);
			}
			break;

		case IPF_EXP_UDP_DPORT :
		case IPF_EXP_TCP_DPORT :
			for (i = 0; !e && i < x[3]; i++) {
				e |= (nat->nat_ndport == x[i + 3]);
			}
			break;
		}
		e ^= x[2];

		if (!e)
			break;
	}

	return e;
}
