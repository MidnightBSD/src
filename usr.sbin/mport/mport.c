/*
 * License forthcoming.. BSD 2 clause
 * Copyright (C) 2010 Lucas Holt
 */

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/usr.sbin/mport/mport.c,v 1.1 2010/02/09 01:41:45 laffer1 Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MPORT_TOOLS_PATH "/usr/libexec/"
#define MPORT_LOCAL_PKG_PATH "/usr/mports/Packages"

void usage(void);

int 
main(int argc, char *argv[]) {
	char *buf = NULL;

	if (argc < 2)
		usage();

	if (!strcmp(argv[1], "install")) {
		asprintf(&buf, "%s%s %s%s%s",
			MPORT_TOOLS_PATH,
			"install",
			MPORT_LOCAL_PKG_PATH,
#if defined(__i386__)
			"i386",
#elif defined(__amd64__)
			"amd64",
#else
			"sparc64",
#endif
			argv[2]); 	
	} else if (!strcmp(argv[1], "delete")) {
		asprintf(&buf, "%s%s %s",
			MPORT_TOOLS_PATH,
			"delete",
			argv[3]);
        } else if (!strcmp(argv[1], "list")) {
		asprintf(&buf, "%s%s",
                        MPORT_TOOLS_PATH,
                        "mport.list -v");
	} else {
		usage();
	}

	system(buf);

	return 0;
}

void
usage(void) {
	fprintf( stderr, 
		"usage: mport <command> args:\n"
		"       mport delete [package name]\n"
		"       mport install [package name]\n"
		"       mport list\n"
	);
	exit(1);
} 
