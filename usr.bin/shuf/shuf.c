/*
 * Copyright (c) 2017-2020 Brian Callahan <bcallah@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHERWISE) ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <capsicum_helpers.h>

static FILE    *ofile;
static char 	delimiter = '\n';
static int 	eflag, most = -1, rflag, zflag;

static void __dead2 usage(void);
static void __dead2 version(void);
static void printshuf(char *args[], int ind);
static void randomshuf(int argn, char *args[]);
static void shuf(int argn, char *args[]);
static void shuffile(const char *input, int argn, size_t inputlen);
static void shufintegers(int range, int lo);

/*
 * shuf -- output a random permutation of input lines
 */

static void
printshuf(char *args[], int ind)
{
	size_t size;

	for (size = 0; args[ind][size] != delimiter; size++)
		;

	fwrite(args[ind], eflag ? size : size + 1, 1, ofile);

	if (eflag) {
		if (zflag)
			fputc('\0', ofile);
		else
			fputc('\n', ofile);
	}
}

static void
randomshuf(int argn, char *args[])
{

	while ((most == -1 ? 1 : most-- > 0))
		printshuf(args, arc4random_uniform(argn));
}

static void
shuf(int argn, char *args[])
{
	char   *argt;
	int 	i, j;

	for (i = argn - 1; i > 0; i--) {
		j = arc4random_uniform(i + 1);
		argt = args[j];
		args[j] = args[i];
		args[i] = argt;
	}

	most = (most == -1 ? argn : most > argn ? argn : most);

	for (i = 0; i < most; i++)
		printshuf(args, i);
}

static void
shuffile(const char *input, int argn, size_t inputlen)
{
	const char     *s;
	char	      **args = NULL, *argt;
	int 			i = 0;
	size_t 		len;

	if ((args = reallocarray(args, argn, sizeof(char *))) == NULL)
		err(1, "could not create shuffile buffer");

	while (i < argn) {
		for (s = input; *s != delimiter; s++) {
			if (inputlen-- == 0)
				break;
		}
		len = s - input;

		if ((args[i] = malloc(len + 1)) == NULL)
			err(1, "could not create shuffile item buffer");
		argt = args[i++];

		while (len-- > 0)
			*argt++ = *input++;
		*argt++ = delimiter;

		input++;
		--inputlen;
	}

	most = (most < argn ? most : argn);

	if (rflag)
		randomshuf(argn, args);
	else
		shuf(argn, args);

	for (i = 0; i < argn; i++) {
		free(args[i]);
		args[i] = NULL;
	}

	free(args);
	args = NULL;
}

static void
shufintegers(int range, int lo)
{
	int    *args = NULL, argt, i, j;

	if ((args = reallocarray(args, range, sizeof(int))) == NULL)
		err(1, "range size will exhaust memory");

	if (rflag) {
		while ((most == -1 ? 1 : most-- > 0)) {
			fprintf(ofile, "%u%c",
				arc4random_uniform(range) + lo, delimiter);
		}
	} else {
		for (i = 0; i < range; i++)
			args[i] = lo + i;

		for (i = range - 1; i > 0; i--) {
			j = arc4random_uniform(i + 1);
			argt = args[j];
			args[j] = args[i];
			args[i] = argt;
		}

		most = (most == -1 ? range : most > range ? range : most);

		for (i = 0; i < most; i++)
			fprintf(ofile, "%d%c", args[i], delimiter);
	}

	free(args);
	args = NULL;
}

static void __dead2
usage(void)
{
	const char     *name;

	name = getprogname();

	fprintf(stderr, "usage: %s [-hv] [-n count] [-o file] [-rz] [file]\n"
		"       %s [-hv] -e [-n count] [-o file] [-rz] [args ...]\n"
		"       %s [-hv] -i lo-hi [-n count] [-o file] [-rz]\n",
		name, name, name);

	exit(1);
}

static void __dead2
version(void)
{

	fputs("shuf 3.0\n"
	      "Copyright (c) 2017-2020 Brian Callahan <bcallah@openbsd.org>\n"
	      "\nPermission to use, copy, modify, and distribute this software"
	      " for any\npurpose with or without fee is hereby granted, "
	      "provided that the above\ncopyright notice and this permission "
	      "notice appear in all copies.\n\nTHE SOFTWARE IS PROVIDED "
	      " AS\nIS\" AND THE AUTHOR DISCLAIMS ALL WARRANTIES"
	      " WITH REGARD TO\nTHIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF"
	      "\nMERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE "
	      "LIABLE FOR\nANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL "
	      "DAMAGES OR ANY DAMAGES\nWHATSOEVER RESULTING FROM LOSS OF USE, "
	      "DATA OR PROFITS, WHETHER IN AN\nACTION OF CONTRACT, NEGLIGENCE "
	      "OR OTHER TORTIOUS ACTION, ARISING OUT OF\nOR IN CONNECTION "
	      "WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n", stderr);

	exit(1);
}

int
main(int argc, char *argv[])
{
	FILE	       *ifile;
	const char     *errstr;
	char	       *argp, *buf, *nbuf;
	int 			argn = 1, ch, hi = 0, lo = 0, prev = 1;
	int 			iflag = 0, oflag = 0;
	size_t 		buflen = 0, bufsize = 8192, nbufsize;

#ifdef HAVE_PLEDGE
	if (pledge("stdio rpath wpath cpath", NULL) == -1)
		err(1, "pledge");
#endif

	if (caph_enter() < 0)
		err(1, "cap_enter");

	while ((ch = getopt(argc, argv, "ehi:n:o:rvz")) != -1) {
		switch (ch) {
		case 'e':
			eflag = 1;
			break;
		case 'h':
			usage();
		case 'i':
			if (iflag++)
				errx(1, "cannot have multiple -i options");

			if ((argp = strchr(optarg, '-')) == NULL)
				errx(1, "must provide lo and hi for -i option");
			*argp = '\0';

			lo = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "invalid or out of range lo");

			hi = strtonum(argp + 1, 0, INT_MAX, &errstr);
			if (errstr != NULL)
				errx(1, "invalid or out of range hi");

			if (lo >= hi)
				errx(1, "lo is greater than or equal to hi");
			if (hi == INT_MAX && lo == 0)
				errx(1, "lo-hi range too large for -i option");
			break;
		case 'n':
			most = strtonum(optarg, 0, INT_MAX, &errstr);
			if (errstr != NULL) {
				errx(1,
				     "-n count must be from 0 to %d, not %s",
				     INT_MAX, optarg);
			}
			break;
		case 'o':
			if (oflag++)
				errx(1, "cannot have multiple -o options");

			if ((ofile = fopen(optarg, "w")) == NULL)
				err(1, "could not open output file %s", optarg);

			break;
		case 'r':
			rflag = 1;
			break;
		case 'v':
			version();
		case 'z':
			zflag = 1;
			delimiter = '\0';
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (eflag && iflag)
		errx(1, "cannot combine -e and -i options");

	if (oflag == 0)
		ofile = stdout;

	if (eflag == 0 && ((iflag == 1 && argc != 0) || argc > 1))
		errx(1, "extra operand '%s'", iflag == 1 ? *argv : *++argv);

	if (eflag) {
		if (argc == 0)
			goto out;
		delimiter = '\0';
		if (rflag)
			randomshuf(argc, argv);
		else
			shuf(argc, argv);
		goto out;
	}

	if (iflag) {
		shufintegers(hi - lo + 1, lo);
		goto out;
	}

	if (argc == 0 || (argv[0][0] == '-' && argv[0][1] == '\0')) {
		ifile = stdin;
	} else {
		if ((ifile = fopen(*argv, "r")) == NULL)
			err(1, "could not open %s", *argv);
	}

	if ((buf = malloc(bufsize)) == NULL)
		err(1, "could not create initial shuffle buffer");

	while ((ch = fgetc(ifile)) != EOF) {
		buf[buflen++] = ch;

		if (buflen == bufsize) {
			nbufsize = bufsize << 1;
			if ((nbuf = realloc(buf, nbufsize)) == NULL) {
				free(buf);
				buf = NULL;
				err(1, "could not resize shuffle buffer");
			}
			buf = nbuf;
			bufsize = nbufsize;
		}
		if (prev == delimiter)
			++argn;
		prev = ch;
	}
	buf[buflen] = '\0';

	if (ifile != stdin)
		fclose(ifile);

	if (buflen > 0)
		shuffile(buf, argn, buflen);

	free(buf);
	buf = NULL;

out:
	if (oflag)
		fclose(ofile);

	return 0;
}
