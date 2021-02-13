/*-
 * Copyright (c) 2007 Chris Reinhardt
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
 */

#include <sys/cdefs.h>
__MBSDID("$MidnightBSD$");

#include <stdlib.h>
#include <stdio.h>
#include <err.h>
#include <string.h>
#include <unistd.h>
#include <mport.h>

static void usage(void);

int
main(int argc, char *argv[]) 
{
	int ch;
	char *prefix = NULL;
	mportInstance *mport;
	__block int error_code = 0;

	dispatch_queue_t mainq = dispatch_get_main_queue();
        dispatch_group_t grp = dispatch_group_create();
        dispatch_queue_t q = dispatch_queue_create("print", NULL);
		
	while ((ch = getopt(argc, argv, "p:")) != -1) {
		switch (ch) {
			case 'p':
				prefix = optarg;
				break;
			case '?':
			default:
				usage();
				break; 
		}
	} 

	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	mport_init_queues();

	mport = mport_instance_new();

	if (mport_instance_init(mport, NULL) != MPORT_OK) {
		warnx("Init failed: %s", mport_err_string());
		return 1;
	}

	for (int i = 0; i < argc; i++) {
		dispatch_group_async(grp, q, ^{
			if (mport_install_primative(mport, argv[i], prefix) != MPORT_OK) {
				warnx("install failed: %s", mport_err_string());
				mport_instance_free(mport);
				exit(1);
			}
		});
	}
 
	dispatch_group_wait(grp, DISPATCH_TIME_FOREVER);
        dispatch_async(mainq, ^{
                mport_instance_free(mport);
                exit(error_code);
        });

        dispatch_main();
}

static 
void usage(void) 
{

	fprintf(stderr, "Usage: mport.install [-p prefix] pkgfile1 pkgfile2 ...\n");
	exit(1);
}
