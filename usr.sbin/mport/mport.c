/*-
 * Copyright (c) 2010, 2011 Lucas Holt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $MidnightBSD: src/usr.sbin/mport/mport.c,v 1.5 2010/03/04 03:03:55 laffer1 Exp $
 */

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD: src/usr.sbin/mport/mport.c,v 1.5 2010/03/04 03:03:55 laffer1 Exp $");

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
		asprintf(&buf, "%s%s %s/%s/All/%s.mport",
			MPORT_TOOLS_PATH,
			"mport.install",
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
			"mport.delete -n",
			argv[2]);
        } else if (!strcmp(argv[1], "list")) {
		if (argc > 2 && strcmp(argv[2], "updates"))
			asprintf(&buf, "%s%s",
				MPORT_TOOLS_PATH,
				"mport.list -u");
		else
			asprintf(&buf, "%s%s",
                        	MPORT_TOOLS_PATH,
                        	"mport.list -v");
	} else {
		usage();
	}

#ifdef DEBUG
	printf("command: %s\n", buf);
#endif
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
