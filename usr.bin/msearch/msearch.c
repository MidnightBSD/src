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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <unistd.h>
#include <msearch.h>

static void usage(void);

int
main(int argc, char *argv[]) {
	msearch_query *query;
	msearch_result *result;
	msearch_result *current;
	int results;
	int ch, zeroflag, cflag, rflag, tflag;
	int limit = 0;

	zeroflag = cflag = tflag = rflag = 0;

	while ((ch = getopt(argc, argv, "crtzl:")) != -1) {
		switch(ch) {
			case 'z': 
				zeroflag = 1;
				break;
			case 'c':
				cflag = 1;
				break;
			case 'l':
				limit = (int)strtol(optarg, (char **)NULL, 10);
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

	if (argc < 1) {
		usage();
	}

	if ((result = (msearch_result *) calloc(1, sizeof(msearch_result))) == NULL) {
		err(1, NULL);
	}

	if ((query = malloc(sizeof(msearch_query))) == NULL) {
		err(1, NULL);
	}

	if (tflag)
		query->type = MSEARCH_QUERY_TYPE_FULL;
	else
		query->type = MSEARCH_QUERY_TYPE_FILE;
	query->terms = argv;
	query->term_count = argc;
	query->limit = limit;

	results = msearch(query, result);

	if (cflag) {
		printf("%d\n", results);
	} else {
		current = result;
		while (current != NULL) {
			if (current->path != NULL) {
				if (tflag && rflag) {
					printf("%0.2f %s", current->weight, current->path);
					if (zeroflag)
						fputc('\0', stdout);
					else
						fputc('\n', stdout);
				} else {
					printf("%s", current->path);
					if (zeroflag)
						fputc('\0', stdout);
					else
						fputc('\n', stdout);
				}
			}
			current = current->next;
		}
	}

	msearch_free(result);
	free(query);

	return 0;
}

void
usage() {
	fprintf(stderr, "usage: msearch [-crtz] [-l limit] pattern ...\n");
	exit(1);
}
