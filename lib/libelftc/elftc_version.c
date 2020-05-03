/* $FreeBSD: stable/11/lib/libelftc/elftc_version.c 338414 2018-08-31 17:36:45Z emaste $ */

#include <sys/types.h>
#include <libelftc.h>

const char *
elftc_version(void)
{
	return "elftoolchain r3614M";
}
