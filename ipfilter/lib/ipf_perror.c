#include "ipf.h"

void
ipf_perror(err, string)
	int err;
	char *string;
{
	if (err == 0)
		fprintf(stderr, "%s\n", string);
	else
		fprintf(stderr, "%s\n", ipf_strerror(err));
}
