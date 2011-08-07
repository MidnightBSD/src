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
__MBSDID("$MidnightBSD: src/libexec/msearch.index/msearch.index.c,v 1.6 2011/08/06 23:14:51 laffer1 Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <msearch.h>
#include <unistd.h>

static void usage(void);

int
main(int argc, char *argv[]) {
	msearch_index *index;
	msearch_fulltext *findex;
	int i;
	int fflag, pflag, rflag, tflag, ch;

	fflag = pflag = rflag = tflag = 0;

	while ((ch = getopt(argc, argv, "prtf:")) != -1) {
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
			case 't':
				tflag = 1;
				break;
             		case '?':
             		default:
                     		usage();
             	}
	}
	argc -= optind;
	argv += optind;

	if (geteuid()  == 0) {
		fprintf(stderr, "msearch.index should not be run as root.\n");
		usage();
	}

	if (tflag && pflag)
		usage();

	if (rflag) {
		if (tflag)
			unlink(MSEARCH_DEFAULT_FULLTEXT_FILE);
		else if (pflag)
			unlink(MSEARCH_DEFAULT_INDEX_FILE);
		else {
			unlink(MSEARCH_DEFAULT_INDEX_FILE);
			unlink(MSEARCH_DEFAULT_FULLTEXT_FILE);
		}
	}

	index = msearch_index_open(MSEARCH_DEFAULT_INDEX_FILE);
	msearch_index_create(index);

	findex = msearch_fulltext_open(MSEARCH_DEFAULT_FULLTEXT_FILE);
	msearch_fulltext_create(findex);

	if (fflag) {
		for (i = 1; i < argc; i++) {
			msearch_index_file(index, argv[i], 0);
		}
	} else if (pflag) {
		msearch_index_path(index, argv[1]);
	} else if (tflag) {
		msearch_fulltext_index(findex, index);
	} else {
		msearch_index_path(index, "/");
		msearch_fulltext_index(findex, index);
	}

	msearch_index_close(index);
	msearch_fulltext_close(findex);

	return 0;
}

static void
usage() {
	fprintf(stderr, "msearch.index [-r] [-p path | -f files ...]\n");
	exit(1); 
}
