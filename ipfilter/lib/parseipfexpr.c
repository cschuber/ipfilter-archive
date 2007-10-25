#include "ipf.h"


typedef struct ipfopentry {
	int	ipoe_cmd;
	int	ipoe_nbasearg;
	int	ipoe_maxarg;
	char	*ipoe_word;
} ipfopentry_t;

static ipfopentry_t opwords[14] = {
	{ IPF_EXP_IP_ADDR, 2, 0, "ip.addr" },
	{ IPF_EXP_IP_PR, 1, 0, "ip.p" },
	{ IPF_EXP_IP_SRCADDR, 2, 0, "ip.src" },
	{ IPF_EXP_IP_DSTADDR, 2, 0, "ip.dst" },
	{ IPF_EXP_TCP_PORT, 1, 0, "tcp.port" },
	{ IPF_EXP_TCP_DPORT, 1, 0, "tcp.dport" },
	{ IPF_EXP_TCP_SPORT, 1, 0, "tcp.sport" },
	{ IPF_EXP_TCP_FLAGS, 2, 0, "tcp.flags" },
	{ IPF_EXP_UDP_PORT, 1, 0, "udp.port" },
	{ IPF_EXP_UDP_DPORT, 1, 0, "udp.dport" },
	{ IPF_EXP_UDP_SPORT, 1, 0, "udp.sport" },
	{ IPF_EXP_TCP_STATE, 1, 0, "tcp.state" },
	{ IPF_EXP_IDLE_GT, 1, 1, "idle-gt" },
	{ -1, 0, 0, NULL  }
};


int *parseipfexpr(line, errorptr)
	char *line;
	char **errorptr;
{
	int not, items, asize, *oplist, osize, i;
	char *temp, *arg, *s, *t, *ops, *error;
	ipfopentry_t *e;
	ipfexp_t *ipfe;

	asize = 0;
	error = NULL;
	oplist = NULL;

	temp = strdup(line);
	if (temp == NULL) {
		error = "strdup failed";
		goto parseerror;
	}

	/*
	 * Eliminate any white spaces to make parsing easier.
	 */
	for (s = temp; *s != '\0'; ) {
		if (isspace(*s))
			strcpy(s, s + 1);
		else
			s++;
	}

	/*
	 * Parse the string.
	 * It should be sets of "ip.dst=1.2.3.4/32;" things.
	 * There must be a "=" or "!=" and it must end in ";".
	 */
	if (temp[strlen(temp) - 1] != ';') {
		error = "last character not ';'";
		goto parseerror;
	}

	/*
	 * Work through the list of complete operands present.
	 */
	for (ops = strtok(temp, ";"); ops != NULL; ops = strtok(NULL, ";")) {
		arg = strchr(ops, '=');
		if ((arg < ops + 2) || (arg == NULL)) {
			error = "bad 'arg' vlaue";
			goto parseerror;
		}

		if (*(arg - 1) == '!') {
			*(arg - 1) = '\0';
			not = 1;
		} else {
			not = 0;
		}
		*arg++ = '\0';


		for (e = opwords; e->ipoe_word; e++) {
			if (strcmp(ops, e->ipoe_word) == 0)
				break;
		}
		if (e->ipoe_word == NULL) {
			error = malloc(32);
			if (error != NULL) {
				sprintf(error, "keyword (%.10s) not found",
					ops);
			}
			goto parseerror;
		}

		/*
		 * Count the number of commas so we know how big to
		 * build the array
		 */
		for (s = arg, items = 1; *s != '\0'; s++)
			if (*s == ',')
				items++;

		if ((e->ipoe_maxarg != 0) && (items > e->ipoe_maxarg)) {
			error = "too many items";
			goto parseerror;
		}

		/*
		 * osize will mark the end of where we have filled up to
		 * and is thus where we start putting new data.
		 */
		osize = asize;
		asize += 3 + (items * e->ipoe_nbasearg);
		if (oplist == NULL)
			oplist = calloc(1, sizeof(int) * (asize + 2));
		else
			oplist = realloc(oplist, sizeof(int) * (asize + 2));
		if (oplist == NULL) {
			error = "oplist alloc failed";
			goto parseerror;
		}
		ipfe = (ipfexp_t *)(oplist + osize);
		osize += 3;
		ipfe->ipfe_cmd = e->ipoe_cmd;
		ipfe->ipfe_not = not;
		ipfe->ipfe_narg = items * e->ipoe_nbasearg;

		for (s = arg; (*s != '\0') && (osize < asize); s = t) {
			/*
			 * Look for the end of this arg or the ',' to say
			 * there is another following.
			 */
			for (t = s; (*t != '\0') && (*t != ','); t++)
				;
			if (*t == ',')
				*t++ = '\0';

			if (!strcasecmp(ops, "ip.addr") ||
			    !strcasecmp(ops, "ip.src") ||
			    !strcasecmp(ops, "ip.dst")) {
				u_32_t mask, addr;
				char *delim;

				delim = strchr(s, '/');
				if (delim != NULL) {
					*delim++ = '\0';
					if (genmask(AF_INET, delim,
						    &mask) == -1) {
						error = "genmask failed";
						goto parseerror;
					}
				} else {
					mask = 0xffffffff;
				}
				if (gethost(s, &addr) == -1) {
					error = "gethost failed";
					goto parseerror;
				}

				oplist[osize++] = addr;
				oplist[osize++] = mask;

			} else if (!strcasecmp(ops, "ip.p")) {
				int p;

				p = getproto(s);
				if (p == -1)
					goto parseerror;
				oplist[osize++] = p;

			} else if (!strcasecmp(ops, "tcp.flags")) {
				u_32_t mask, flags;
				char *delim;

				delim = strchr(s, '/');
				if (delim != NULL) {
					*delim++ = '\0';
					mask = tcpflags(delim);
				} else {
					mask = 0xff;
				}
				flags = tcpflags(s);

				oplist[osize++] = flags;
				oplist[osize++] = mask;


			} else if (!strcasecmp(ops, "tcp.port") ||
			    !strcasecmp(ops, "tcp.sport") ||
			    !strcasecmp(ops, "tcp.dport") ||
			    !strcasecmp(ops, "udp.port") ||
			    !strcasecmp(ops, "udp.sport") ||
			    !strcasecmp(ops, "udp.dport")) {
				char proto[4];
				u_short port;

				strncpy(proto, ops, 3);
				proto[3] = '\0';
				if (getport(NULL, s, &port, proto) == -1)
					goto parseerror;
				oplist[osize++] = port;

			} else if (!strcasecmp(ops, "tcp.state")) {
				oplist[osize++] = atoi(s);

			} else {
				error = "unknown word";
				goto parseerror;
			}
		}
	}

	free(temp);

	if (errorptr != NULL)
		*errorptr = NULL;

	for (i = asize; i > 0; i--)
		oplist[i] = oplist[i - 1];

	oplist[0] = asize + 2;
	oplist[asize + 1] = IPF_EXP_END;

	return oplist;

parseerror:
	if (errorptr != NULL)
		*errorptr = error;
	if (oplist != NULL)
		free(oplist);
	if (temp != NULL)
		free(temp);
	return NULL;
}
