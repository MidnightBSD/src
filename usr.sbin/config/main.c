/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
  "$FreeBSD: release/7.0.0/usr.sbin/config/main.c 169647 2007-05-17 04:53:52Z imp $";
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sbuf.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/param.h>

#include <assert.h>
#include <ctype.h>
#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include <dirent.h>
#include "y.tab.h"
#include "config.h"
#include "configvers.h"

#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef FALSE
#define FALSE	(0)
#endif

#define	CDIR	"../compile/"

char *	PREFIX;
char 	destdir[MAXPATHLEN];
char 	srcdir[MAXPATHLEN];

int	debugging;
int	profiling;
int	found_defaults;
int	incignore;

/*
 * Preserve old behaviour in INCLUDE_CONFIG_FILE handling (files are included
 * literally).
 */
int	filebased = 0;

static void configfile(void);
static void get_srcdir(void);
static void usage(void);
static void cleanheaders(char *);
static void kernconfdump(const char *);

struct hdr_list {
	char *h_name;
	struct hdr_list *h_next;
} *htab;

/*
 * Config builds a set of files for building a UNIX
 * system given a description of the desired system.
 */
int
main(int argc, char **argv)
{

	struct stat buf;
	int ch, len;
	char *p;
	char xxx[MAXPATHLEN];
	char *kernfile;

	kernfile = NULL;
	while ((ch = getopt(argc, argv, "Cd:gpVx:")) != -1)
		switch (ch) {
		case 'C':
			filebased = 1;
			break;
		case 'd':
			if (*destdir == '\0')
				strlcpy(destdir, optarg, sizeof(destdir));
			else
				errx(2, "directory already set");
			break;
		case 'g':
			debugging++;
			break;
		case 'p':
			profiling++;
			break;
		case 'V':
			printf("%d\n", CONFIGVERS);
			exit(0);
		case 'x':
			kernfile = optarg;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (kernfile != NULL) {
		kernconfdump(kernfile);
		exit(EXIT_SUCCESS);
	}

	if (argc != 1)
		usage();

	PREFIX = *argv;
	if (freopen("DEFAULTS", "r", stdin) != NULL) {
		found_defaults = 1;
		yyfile = "DEFAULTS";
	} else {
		if (freopen(PREFIX, "r", stdin) == NULL)
			err(2, "%s", PREFIX);
		yyfile = PREFIX;
	}
	if (*destdir != '\0') {
		len = strlen(destdir);
		while (len > 1 && destdir[len - 1] == '/')
			destdir[--len] = '\0';
		get_srcdir();
	} else {
		strlcpy(destdir, CDIR, sizeof(destdir));
		strlcat(destdir, PREFIX, sizeof(destdir));
	}

	p = path((char *)NULL);
	if (stat(p, &buf)) {
		if (mkdir(p, 0777))
			err(2, "%s", p);
	} else if (!S_ISDIR(buf.st_mode))
		errx(2, "%s isn't a directory", p);

	SLIST_INIT(&cputype);
	SLIST_INIT(&mkopt);
	SLIST_INIT(&opt);
	SLIST_INIT(&rmopts);
	STAILQ_INIT(&cfgfiles);
	STAILQ_INIT(&dtab);
	STAILQ_INIT(&fntab);
	STAILQ_INIT(&ftab);
	STAILQ_INIT(&hints);
	if (yyparse())
		exit(3);

	/*
	 * Ensure that required elements (machine, cpu, ident) are present.
	 */
	if (machinename == NULL) {
		printf("Specify machine type, e.g. ``machine i386''\n");
		exit(1);
	}
	if (ident == NULL) {
		printf("no ident line specified\n");
		exit(1);
	}
	if (SLIST_EMPTY(&cputype)) {
		printf("cpu type must be specified\n");
		exit(1);
	}

	/*
	 * make symbolic links in compilation directory
	 * for "sys" (to make genassym.c work along with #include <sys/xxx>)
	 * and similarly for "machine".
	 */
	if (*srcdir == '\0')
		(void)snprintf(xxx, sizeof(xxx), "../../include");
	else
		(void)snprintf(xxx, sizeof(xxx), "%s/%s/include",
		    srcdir, machinename);
	(void) unlink(path("machine"));
	(void) symlink(xxx, path("machine"));
	if (strcmp(machinename, machinearch) != 0) {
		/*
		 * make symbolic links in compilation directory for
		 * machinearch, if it is different than machinename.
		 */
		if (*srcdir == '\0')
			(void)snprintf(xxx, sizeof(xxx), "../../../%s/include",
			    machinearch);
		else
			(void)snprintf(xxx, sizeof(xxx), "%s/%s/include",
			    srcdir, machinearch);
		(void) unlink(path(machinearch));
		(void) symlink(xxx, path(machinearch));
	}
	configfile();			/* put config file into kernel*/
	options();			/* make options .h files */
	makefile();			/* build Makefile */
	makeenv();			/* build env.c */
	makehints();			/* build hints.c */
	headers();			/* make a lot of .h files */
	cleanheaders(p);
	printf("Kernel build directory is %s\n", p);
	printf("Don't forget to do ``make cleandepend && make depend''\n");
	exit(0);
}

/*
 * get_srcdir
 *	determine the root of the kernel source tree
 *	and save that in srcdir.
 */
static void
get_srcdir(void)
{

	if (realpath("../..", srcdir) == NULL)
		errx(2, "Unable to find root of source tree");
}

static void
usage(void)
{

	fprintf(stderr, "usage: config [-CgpV] [-d destdir] sysname\n");
	fprintf(stderr, "       config -x kernel\n");
	exit(EX_USAGE);
}

/*
 * get_word
 *	returns EOF on end of file
 *	NULL on end of line
 *	pointer to the word otherwise
 */
char *
get_word(FILE *fp)
{
	static char line[80];
	int ch;
	char *cp;
	int escaped_nl = 0;

begin:
	while ((ch = getc(fp)) != EOF)
		if (ch != ' ' && ch != '\t')
			break;
	if (ch == EOF)
		return ((char *)EOF);
	if (ch == '\\'){
		escaped_nl = 1;
		goto begin;
	}
	if (ch == '\n') {
		if (escaped_nl){
			escaped_nl = 0;
			goto begin;
		}
		else
			return (NULL);
	}
	cp = line;
	*cp++ = ch;
	while ((ch = getc(fp)) != EOF) {
		if (isspace(ch))
			break;
		*cp++ = ch;
	}
	*cp = 0;
	if (ch == EOF)
		return ((char *)EOF);
	(void) ungetc(ch, fp);
	return (line);
}

/*
 * get_quoted_word
 *	like get_word but will accept something in double or single quotes
 *	(to allow embedded spaces).
 */
char *
get_quoted_word(FILE *fp)
{
	static char line[256];
	int ch;
	char *cp;
	int escaped_nl = 0;

begin:
	while ((ch = getc(fp)) != EOF)
		if (ch != ' ' && ch != '\t')
			break;
	if (ch == EOF)
		return ((char *)EOF);
	if (ch == '\\'){
		escaped_nl = 1;
		goto begin;
	}
	if (ch == '\n') {
		if (escaped_nl){
			escaped_nl = 0;
			goto begin;
		}
		else
			return (NULL);
	}
	cp = line;
	if (ch == '"' || ch == '\'') {
		int quote = ch;

		while ((ch = getc(fp)) != EOF) {
			if (ch == quote)
				break;
			if (ch == '\n') {
				*cp = 0;
				printf("config: missing quote reading `%s'\n",
					line);
				exit(2);
			}
			*cp++ = ch;
		}
	} else {
		*cp++ = ch;
		while ((ch = getc(fp)) != EOF) {
			if (isspace(ch))
				break;
			*cp++ = ch;
		}
		if (ch != EOF)
			(void) ungetc(ch, fp);
	}
	*cp = 0;
	if (ch == EOF)
		return ((char *)EOF);
	return (line);
}

/*
 * prepend the path to a filename
 */
char *
path(const char *file)
{
	char *cp = NULL;

	if (file)
		asprintf(&cp, "%s/%s", destdir, file);
	else
		cp = strdup(destdir);
	return (cp);
}

/*
 * Generate configuration file based on actual settings. With this mode, user
 * will be able to obtain and build conifguration file with one command.
 */
static void
configfile_dynamic(struct sbuf *sb)
{
	struct cputype *cput;
	struct device *d;
	struct opt *ol;
	char *lend;
	unsigned int i;

	asprintf(&lend, "\\n\\\n");
	assert(lend != NULL);
	sbuf_printf(sb, "options\t%s%s", OPT_AUTOGEN, lend);
	sbuf_printf(sb, "ident\t%s%s", ident, lend);
	sbuf_printf(sb, "machine\t%s%s", machinename, lend);
	SLIST_FOREACH(cput, &cputype, cpu_next)
		sbuf_printf(sb, "cpu\t%s%s", cput->cpu_name, lend);
	SLIST_FOREACH(ol, &mkopt, op_next)
		sbuf_printf(sb, "makeoptions\t%s=%s%s", ol->op_name,
		    ol->op_value, lend);
	SLIST_FOREACH(ol, &opt, op_next) {
		if (strncmp(ol->op_name, "DEV_", 4) == 0)
			continue;
		sbuf_printf(sb, "options\t%s", ol->op_name);
		if (ol->op_value != NULL) {
			sbuf_putc(sb, '=');
			for (i = 0; i < strlen(ol->op_value); i++) {
				if (ol->op_value[i] == '"')
					sbuf_printf(sb, "\\%c",
					    ol->op_value[i]);
				else
					sbuf_printf(sb, "%c",
					    ol->op_value[i]);
			}
			sbuf_printf(sb, "%s", lend);
		} else {
			sbuf_printf(sb, "%s", lend);
		}
	}
	/*
	 * Mark this file as containing everything we need.
	 */
	STAILQ_FOREACH(d, &dtab, d_next)
		sbuf_printf(sb, "device\t%s%s", d->d_name, lend);
	free(lend);
}

/*
 * Generate file from the configuration files.
 */
static void
configfile_filebased(struct sbuf *sb)
{
	FILE *cff;
	struct cfgfile *cf;
	int i;

	STAILQ_FOREACH(cf, &cfgfiles, cfg_next) {
		cff = fopen(cf->cfg_path, "r");
		if (cff == NULL) {
			warn("Couldn't open file %s", cf->cfg_path);
			continue;
		}
		while ((i = getc(cff)) != EOF) {
			if (i == '\n')
				sbuf_printf(sb, "\\n\\\n");
			else if (i == '"' || i == '\'')
				sbuf_printf(sb, "\\%c", i);
			else
				sbuf_putc(sb, i);
		}
		fclose(cff);
	}
}

static void
configfile(void)
{
	FILE *fo;
	struct sbuf *sb;
	char *p;

	/* Add main configuration file to the list of files to be included */
	cfgfile_add(PREFIX);
	p = path("config.c.new");
	fo = fopen(p, "w");
	if (!fo)
		err(2, "%s", p);
	sb = sbuf_new(NULL, NULL, 2048, SBUF_AUTOEXTEND);
	assert(sb != NULL);
	sbuf_clear(sb);
	/*
	 * Try to read all configuration files. Since those will be present as
	 * C string in the macro, we have to slash their ends then the line
	 * wraps.
	 */
	if (filebased) {
		/* Is needed, can be used for backward compatibility. */
		configfile_filebased(sb);
	} else {
		configfile_dynamic(sb);
	}
	sbuf_finish(sb);
	/* 
	 * We print first part of the tamplate, replace our tag with
	 * configuration files content and later continue writing our
	 * template.
	 */
	p = strstr(kernconfstr, KERNCONFTAG);
	if (p == NULL)
		errx(EXIT_FAILURE, "Something went terribly wrong!");
	*p = '\0';
	fprintf(fo, "%s", kernconfstr);
	fprintf(fo, "%s", sbuf_data(sb));
	p += strlen(KERNCONFTAG);
	fprintf(fo, "%s", p);
	sbuf_delete(sb);
	fclose(fo);
	moveifchanged(path("config.c.new"), path("config.c"));
	cfgfile_removeall();
}

/*
 * moveifchanged --
 *	compare two files; rename if changed.
 */
void
moveifchanged(const char *from_name, const char *to_name)
{
	char *p, *q;
	int changed;
	size_t tsize;
	struct stat from_sb, to_sb;
	int from_fd, to_fd;

	changed = 0;

	if ((from_fd = open(from_name, O_RDONLY)) < 0)
		err(EX_OSERR, "moveifchanged open(%s)", from_name);

	if ((to_fd = open(to_name, O_RDONLY)) < 0)
		changed++;

	if (!changed && fstat(from_fd, &from_sb) < 0)
		err(EX_OSERR, "moveifchanged fstat(%s)", from_name);

	if (!changed && fstat(to_fd, &to_sb) < 0)
		err(EX_OSERR, "moveifchanged fstat(%s)", to_name);

	if (!changed && from_sb.st_size != to_sb.st_size)
		changed++;

	tsize = (size_t)from_sb.st_size;

	if (!changed) {
		p = mmap(NULL, tsize, PROT_READ, MAP_SHARED, from_fd, (off_t)0);
#ifndef MAP_FAILED
#define MAP_FAILED ((caddr_t) -1)
#endif
		if (p == MAP_FAILED)
			err(EX_OSERR, "mmap %s", from_name);
		q = mmap(NULL, tsize, PROT_READ, MAP_SHARED, to_fd, (off_t)0);
		if (q == MAP_FAILED)
			err(EX_OSERR, "mmap %s", to_name);

		changed = memcmp(p, q, tsize);
		munmap(p, tsize);
		munmap(q, tsize);
	}
	if (changed) {
		if (rename(from_name, to_name) < 0)
			err(EX_OSERR, "rename(%s, %s)", from_name, to_name);
	} else {
		if (unlink(from_name) < 0)
			err(EX_OSERR, "unlink(%s)", from_name);
	}
}

static void
cleanheaders(char *p)
{
	DIR *dirp;
	struct dirent *dp;
	struct file_list *fl;
	struct hdr_list *hl;
	int i;

	remember("y.tab.h");
	remember("setdefs.h");
	STAILQ_FOREACH(fl, &ftab, f_next)
		remember(fl->f_fn);

	/*
	 * Scan the build directory and clean out stuff that looks like
	 * it might have been a leftover NFOO header, etc.
	 */
	if ((dirp = opendir(p)) == NULL)
		err(EX_OSERR, "opendir %s", p);
	while ((dp = readdir(dirp)) != NULL) {
		i = dp->d_namlen - 2;
		/* Skip non-headers */
		if (dp->d_name[i] != '.' || dp->d_name[i + 1] != 'h')
			continue;
		/* Skip special stuff, eg: bus_if.h, but check opt_*.h */
		if (index(dp->d_name, '_') &&
		    strncmp(dp->d_name, "opt_", 4) != 0)
			continue;
		/* Check if it is a target file */
		for (hl = htab; hl != NULL; hl = hl->h_next) {
			if (eq(dp->d_name, hl->h_name)) {
				break;
			}
		}
		if (hl)
			continue;
		printf("Removing stale header: %s\n", dp->d_name);
		if (unlink(path(dp->d_name)) == -1)
			warn("unlink %s", dp->d_name);
	}
	(void)closedir(dirp);
}

void
remember(const char *file)
{
	char *s;
	struct hdr_list *hl;

	if ((s = strrchr(file, '/')) != NULL)
		s = ns(s + 1);
	else
		s = ns(file);

	if (index(s, '_') && strncmp(s, "opt_", 4) != 0) {
		free(s);
		return;
	}
	for (hl = htab; hl != NULL; hl = hl->h_next) {
		if (eq(s, hl->h_name)) {
			free(s);
			return;
		}
	}
	hl = calloc(1, sizeof(*hl));
	hl->h_name = s;
	hl->h_next = htab;
	htab = hl;
}

/*
 * This one is quick hack. Will be probably moved to elf(3) interface.
 * It takes kernel configuration file name, passes it as an argument to
 * elfdump -a, which output is parsed by some UNIX tools...
 */
static void
kernconfdump(const char *file)
{
	struct stat st;
	FILE *fp, *pp;
	int error, len, osz, r;
	unsigned int off, size;
	char *cmd, *o;

	r = open(file, O_RDONLY);
	if (r == -1)
		errx(EXIT_FAILURE, "Couldn't open file '%s'", file);
	error = fstat(r, &st);
	if (error == -1)
		errx(EXIT_FAILURE, "fstat() failed");
	if (S_ISDIR(st.st_mode))
		errx(EXIT_FAILURE, "'%s' is a directory", file);
	fp = fdopen(r, "r");
	if (fp == NULL)
		errx(EXIT_FAILURE, "fdopen() failed");
	osz = 1024;
	o = calloc(1, osz);
	if (o == NULL)
		errx(EXIT_FAILURE, "Couldn't allocate memory");
	/* ELF note section header. */
	asprintf(&cmd, "/usr/bin/elfdump -c %s | grep -A 5 kern_conf"
	    "| tail -2 | cut -d ' ' -f 2 | paste - - -", file);
	if (cmd == NULL)
		errx(EXIT_FAILURE, "asprintf() failed");
	pp = popen(cmd, "r");
	if (pp == NULL)
		errx(EXIT_FAILURE, "popen() failed");
	free(cmd);
	len = fread(o, osz, 1, pp);
	pclose(pp);
	r = sscanf(o, "%d\t%d", &off, &size);
	free(o);
	if (r != 2)
		errx(EXIT_FAILURE, "File %s doesn't contain configuration "
		    "file. Either unsupported, or not compiled with "
		    "INCLUDE_CONFIG_FILE", file);
	r = fseek(fp, off, SEEK_CUR);
	if (r != 0)
		errx(EXIT_FAILURE, "fseek() failed");
	while ((r = fgetc(fp)) != EOF && size-- > 0)
		fputc(r, stdout);
	fclose(fp);
}
