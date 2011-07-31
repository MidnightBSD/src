/*-
 * Copyright (c) 2011 Lucas Holt
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
__MBSDID("$MidnightBSD: src/libexec/msearch.index/msearch.index.c,v 1.2 2011/07/31 21:25:52 laffer1 Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <msearch.h>
#include <unistd.h>

static void usage(void);

int
main(int argc, char *argv[]) {
	msearch_index *index;
	int i;
	int fflag, pflag, rflag, ch;

	fflag = pflag = rflag = 0;

	while ((ch = getopt(argc, argv, "prf:")) != -1) {
		switch (ch) {
			case 'f':
         			fflag = 1;
				break;
			case 'p':
				pflag = 1;
				break;
			case 'r':
				rflag = 1;
				break;
             		case '?':
             		default:
                     		usage();
             	}
	}
	argc -= optind;
	argv += optind;

	if (rflag)
		unlink(MSEARCH_DEFAULT_INDEX_FILE);

	index = msearch_index_open(MSEARCH_DEFAULT_INDEX_FILE);
	msearch_index_create(index);

	if (fflag) {
		for (i = 1; i < argc; i++) {
			msearch_index_file(index, argv[i], 0);
		}
	} else if (pflag) {
		msearch_index_path(index, argv[1]);
	} else {
		msearch_index_path(index, "/");
	}

	msearch_index_close(index);

	return 0;
}

static void
usage() {
	fprintf(stderr, "msearch.index [-r] [-p path | -f files ...]\n");
	exit(1); 
}
