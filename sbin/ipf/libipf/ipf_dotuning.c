
/*
 * Copyright (C) 2012 by Darren Reed.
 *
 * See the IPFILTER.LICENCE file for details on licencing.
 *
 * $Id$
 */

#include "ipf.h"
#include "netinet/ipl.h"
#include <sys/ioctl.h>

int
ipf_dotuning(int fd, char *tuneargs, ioctlfunc_t iocfn)
{
	ipfobj_t obj;
	ipftune_t tu;
	char *end, *s, *t;

	memset(&obj, 0, sizeof(obj));
	bzero((char *)&tu, sizeof(tu));
	obj.ipfo_rev = IPFILTER_VERSION;
	obj.ipfo_size = sizeof(tu);
	obj.ipfo_ptr = (void *)&tu;
	obj.ipfo_type = IPFOBJ_TUNEABLE;

	for (s = strtok(tuneargs, ","); s != NULL; s = strtok(NULL, ",")) {
		if (!strcmp(s, "list")) {
			tu.ipft_cookie = NULL;
			while (1) {
				if ((*iocfn)(fd, SIOCIPFGETNEXT, &obj) == -1) {
					ipf_perror_fd(fd, iocfn,
						      "ioctl(SIOCIPFGETNEXT)");
					return (-1);
				}
				if (tu.ipft_cookie == NULL)
					break;

				tu.ipft_name[sizeof(tu.ipft_name) - 1] = '\0';
				printtunable(&tu);
			}
			continue;
		}

		t = strchr(s, '=');
		if (t != NULL)
			*t++ = '\0';
		if (*s == '\0' ||
		    strlcpy(tu.ipft_name, s, sizeof(tu.ipft_name)) >=
		    sizeof(tu.ipft_name)) {
			(void)fprintf(stderr, "invalid tunable name '%s'\n", s);
			return (-1);
		}
		tu.ipft_cookie = NULL;
		if (t != NULL) {
			errno = 0;
			tu.ipft_vlong = strtoul(t, &end, 10);
			if (end != t && *end == '\0' && errno == 0) {
				if ((*iocfn)(fd, SIOCIPFSET, &obj) == -1) {
					ipf_perror_fd(fd, iocfn,
						      "ioctl(SIOCIPFSET)");
					return (-1);
				}
			} else {
				(void)fprintf(stderr, "invalid value '%s' for %s\n", t, s);
				return (-1);
			}
		} else {
			if ((*iocfn)(fd, SIOCIPFGET, &obj) == -1) {
				ipf_perror_fd(fd, iocfn, "ioctl(SIOCIPFGET)");
				return (-1);
			}
			if (tu.ipft_cookie == NULL) {
				fprintf(stderr, "Null cookie for %s\n", s);
				return (-1);
			}

			tu.ipft_name[sizeof(tu.ipft_name) - 1] = '\0';
			printtunable(&tu);
		}
	}
	return (0);
}
