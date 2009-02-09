/*
 * Copyright (C) 2000-2008 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"


void print_toif(tag, fdp)
	char *tag;
	frdest_t *fdp;
{
	switch (fdp->fd_type)
	{
	case FRD_NORMAL :
		printf("%s %s%s", tag, fdp->fd_name,
		       (fdp->fd_ptr || (long)fdp->fd_ptr == -1) ? "" : "(!)");
#ifdef	USE_INET6
		if (use_inet6 && IP6_NOTZERO(&fdp->fd_ip6.in6)) {
			char ipv6addr[80];

			inet_ntop(AF_INET6, &fdp->fd_ip6, ipv6addr,
				  sizeof(fdp->fd_ip6));
			printf(":%s", ipv6addr);
		} else
#endif
			if (fdp->fd_ip.s_addr)
				printf(":%s", inet_ntoa(fdp->fd_ip));
		putchar(' ');
		break;
	default :
		break;
	}
}
