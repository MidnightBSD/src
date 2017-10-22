/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Robert Paul Corbett.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if 0
#ifndef lint
static char sccsid[] = "@(#)main.c	5.5 (Berkeley) 5/24/93";
#endif
#endif

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <paths.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "defs.h"

char dflag;
char lflag;
char rflag;
char tflag;
char vflag;

const char *symbol_prefix;
const char *file_prefix = "y";
char temp_form[] = "yacc.XXXXXXXXXXX";

int lineno;
int outline;

char *action_file_name;
char *code_file_name;
char *defines_file_name;
const char *input_file_name = "";
char *output_file_name;
char *text_file_name;
char *union_file_name;
char *verbose_file_name;

FILE *action_file;	/*  a temp file, used to save actions associated    */
			/*  with rules until the parser is written	    */
FILE *code_file;	/*  y.code.c (used when the -r option is specified) */
FILE *defines_file;	/*  y.tab.h					    */
FILE *input_file;	/*  the input file				    */
FILE *output_file;	/*  y.tab.c					    */
FILE *text_file;	/*  a temp file, used to save text until all	    */
			/*  symbols have been defined			    */
FILE *union_file;	/*  a temp file, used to save the union		    */
			/*  definition until all symbol have been	    */
			/*  defined					    */
FILE *verbose_file;	/*  y.output					    */

int nitems;
int nrules;
int nsyms;
int ntokens;
int nvars;

int   start_symbol;
char  **symbol_name;
short *symbol_value;
short *symbol_prec;
char  *symbol_assoc;

short *ritem;
short *rlhs;
short *rrhs;
short *rprec;
char  *rassoc;
short **derives;
char *nullable;

static void create_file_names(void);
static void getargs(int, char **);
static void onintr(int);
static void open_files(void);
static void set_signals(void);
static void usage(void);

volatile sig_atomic_t sigdie;

__dead2 void
done(int k)
{
    if (action_file) { fclose(action_file); unlink(action_file_name); }
    if (text_file) { fclose(text_file); unlink(text_file_name); }
    if (union_file) { fclose(union_file); unlink(union_file_name); }
    if (sigdie) { _exit(k); }
    exit(k);
}


static void
onintr(int signo __unused)
{
    sigdie = 1;
    done(1);
}


static void
set_signals(void)
{
#ifdef SIGINT
    if (signal(SIGINT, SIG_IGN) != SIG_IGN)
	signal(SIGINT, onintr);
#endif
#ifdef SIGTERM
    if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
	signal(SIGTERM, onintr);
#endif
#ifdef SIGHUP
    if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
	signal(SIGHUP, onintr);
#endif
}


static void
usage(void)
{
    fprintf(stderr, "%s\n%s\n",
		"usage: yacc [-dlrtv] [-b file_prefix] [-o output_filename]",
		"            [-p symbol_prefix] filename");
    exit(1);
}


static void
getargs(int argc, char *argv[])
{
    int ch;

    while ((ch = getopt(argc, argv, "b:dlo:p:rtvy")) != -1)
    {
	switch (ch)
	{
	case 'b':
	    file_prefix = optarg;
	    break;

	case 'd':
	    dflag = 1;
	    break;

	case 'l':
	    lflag = 1;
	    break;

	case 'o':
	    output_file_name = optarg;
	    break;

	case 'p':
	    symbol_prefix = optarg;
	    break;

	case 'r':
	    rflag = 1;
	    break;

	case 't':
	    tflag = 1;
	    break;

	case 'v':
	    vflag = 1;
	    break;

	case 'y':
	    /* for bison compatibility -- byacc is already POSIX compatible */
	    break;

	default:
	    usage();
	}
    }

    if (optind + 1 != argc)
	usage();
    if (strcmp(argv[optind], "-") == 0)
	input_file = stdin;
    else
	input_file_name = argv[optind];
}


void *
allocate(size_t n)
{
    void *p;

    p = NULL;
    if (n)
    {
	p = calloc(1, n);
	if (!p) no_space();
    }
    return (p);
}


static void
create_file_names(void)
{
    int i, len;
    const char *tmpdir;

    if (!(tmpdir = getenv("TMPDIR")))
	tmpdir = _PATH_TMP;

    len = strlen(tmpdir);
    i = len + strlen(temp_form) + 1;
    if (len && tmpdir[len-1] != '/')
	++i;

    action_file_name = malloc(i);
    if (action_file_name == 0) no_space();
    text_file_name = malloc(i);
    if (text_file_name == 0) no_space();
    union_file_name = malloc(i);
    if (union_file_name == 0) no_space();

    strcpy(action_file_name, tmpdir);
    strcpy(text_file_name, tmpdir);
    strcpy(union_file_name, tmpdir);

    if (len && tmpdir[len - 1] != '/')
    {
	action_file_name[len] = '/';
	text_file_name[len] = '/';
	union_file_name[len] = '/';
	++len;
    }

    strcpy(action_file_name + len, temp_form);
    strcpy(text_file_name + len, temp_form);
    strcpy(union_file_name + len, temp_form);

    action_file_name[len + 5] = 'a';
    text_file_name[len + 5] = 't';
    union_file_name[len + 5] = 'u';

    if (output_file_name != 0)
    {
	file_prefix = output_file_name;
	len = strlen(file_prefix);
    }
    else
    {
	len = strlen(file_prefix);
	output_file_name = malloc(len + 7);
	if (output_file_name == 0)
	    no_space();
	strcpy(output_file_name, file_prefix);
	strcpy(output_file_name + len, OUTPUT_SUFFIX);
    }

    if (rflag)
    {
	code_file_name = malloc(len + 8);
	if (code_file_name == 0)
	    no_space();
	strcpy(code_file_name, file_prefix);
	if (file_prefix == output_file_name)
	{
	    /*
	     * XXX ".tab.c" here is OUTPUT_SUFFIX, but since its length is
	     * in various magic numbers, don't bother using the macro.
	     */
	    if (len >= 6 && strcmp(code_file_name + len - 6, ".tab.c") == 0)
		strcpy(code_file_name + len - 6, CODE_SUFFIX);
	    else if (len >= 2 && strcmp(code_file_name + len - 2, ".c") == 0)
		strcpy(code_file_name + len - 2, CODE_SUFFIX);
	    else
		strcpy(code_file_name + len, CODE_SUFFIX);
	}
	else
	    strcpy(code_file_name + len, CODE_SUFFIX);
    }
    else
	code_file_name = output_file_name;

    if (dflag)
    {
	defines_file_name = malloc(len + 7);
	if (defines_file_name == 0)
	    no_space();
	strcpy(defines_file_name, file_prefix);
	if (file_prefix == output_file_name)
	{
#define BISON_DEFINES_SUFFIX  ".h"
	    if (len >= 2 && strcmp(defines_file_name + len - 2, ".c") == 0)
		strcpy(defines_file_name + len - 2, BISON_DEFINES_SUFFIX);
	    else
		strcpy(defines_file_name + len, BISON_DEFINES_SUFFIX);
	}
	else
	    strcpy(defines_file_name + len, DEFINES_SUFFIX);
    }

    if (vflag)
    {
	verbose_file_name = malloc(len + 8);
	if (verbose_file_name == 0)
	    no_space();
	strcpy(verbose_file_name, file_prefix);
	if (file_prefix == output_file_name)
	{
	    if (len >= 6 && strcmp(verbose_file_name + len - 6, ".tab.c") == 0)
		strcpy(verbose_file_name + len - 6, VERBOSE_SUFFIX);
	    else if (len >= 2 && strcmp(verbose_file_name + len - 2, ".c") == 0)
		strcpy(verbose_file_name + len - 2, VERBOSE_SUFFIX);
	    else
		strcpy(verbose_file_name + len, VERBOSE_SUFFIX);
	}
	else
	    strcpy(verbose_file_name + len, VERBOSE_SUFFIX);
    }
}


static void
open_files(void)
{
    int fd;

    create_file_names();

    if (input_file == 0)
    {
	input_file = fopen(input_file_name, "r");
	if (input_file == 0)
	    open_error(input_file_name);
    }

    fd = mkstemp(action_file_name);
    if (fd < 0 || (action_file = fdopen(fd, "w")) == NULL) {
        if (fd >= 0)
	    close(fd);
	open_error(action_file_name);
    }
    fd = mkstemp(text_file_name);
    if (fd < 0 || (text_file = fdopen(fd, "w")) == NULL) {
        if (fd >= 0)
	    close(fd);
	open_error(text_file_name);
    }
    fd = mkstemp(union_file_name);
    if (fd < 0 || (union_file = fdopen(fd, "w")) == NULL) {
        if (fd >= 0)
	    close(fd);
	open_error(union_file_name);
    }

    text_file = fopen(text_file_name, "w");
    if (text_file == 0)
	open_error(text_file_name);

    if (vflag)
    {
	verbose_file = fopen(verbose_file_name, "w");
	if (verbose_file == 0)
	    open_error(verbose_file_name);
    }

    if (dflag)
    {
	defines_file = fopen(defines_file_name, "w");
	if (defines_file == 0)
	    open_error(defines_file_name);
	union_file = fopen(union_file_name, "w");
	if (union_file ==  0)
	    open_error(union_file_name);
    }

    output_file = fopen(output_file_name, "w");
    if (output_file == 0)
	open_error(output_file_name);

    if (rflag)
    {
	code_file = fopen(code_file_name, "w");
	if (code_file == 0)
	    open_error(code_file_name);
    }
    else
	code_file = output_file;
}


int
main(int argc, char *argv[])
{
    set_signals();
    getargs(argc, argv);
    open_files();
    reader();
    lr0();
    lalr();
    make_parser();
    verbose();
    output();
    done(0);
    /*NOTREACHED*/
    return (0);
}
