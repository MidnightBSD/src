/*
 * This file is in public domain.
 * Written by Konstantin Belousov <kib@freebsd.org>
 *
 * $FreeBSD: stable/9/tools/test/auxinfo/auxinfo.c 238139 2012-07-05 15:41:31Z kib $
 */

#include <sys/mman.h>
#include <err.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

static void
test_pagesizes(void)
{
	size_t *ps;
	int i, nelem;

	nelem = getpagesizes(NULL, 0);
	if (nelem == -1)
		err(1, "getpagesizes(NULL, 0)");
	ps = malloc(nelem * sizeof(size_t));
	if (ps == NULL)
		err(1, "malloc");
	nelem = getpagesizes(ps, nelem);
	if (nelem == -1)
		err(1, "getpagesizes");
	printf("Supported page sizes:");
	for (i = 0; i < nelem; i++)
		printf(" %jd", (intmax_t)ps[i]);
	printf("\n");
}

static void
test_pagesize(void)
{

	printf("Pagesize: %d\n", getpagesize());
}

static void
test_osreldate(void)
{

	printf("OSRELDATE: %d\n", getosreldate());
}

static void
test_ncpus(void)
{

	printf("NCPUs: %ld\n", sysconf(_SC_NPROCESSORS_CONF));
}

int
main(int argc __unused, char *argv[] __unused)
{

	test_pagesizes();
	test_pagesize();
	test_osreldate();
	test_ncpus();
	return (0);
}
