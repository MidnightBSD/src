/*-
 * Copyright (c) 2003-2007 Tim Kientzle
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "bsdtar_platform.h"
__FBSDID("$FreeBSD: release/7.0.0/usr.bin/tar/bsdtar.c 172095 2007-09-09 00:07:18Z kientzle $");

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};
#define	no_argument 0
#define	required_argument 1
#endif
#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_PATHS_H
#include <paths.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "bsdtar.h"

/*
 * Per POSIX.1-1988, tar defaults to reading/writing archives to/from
 * the default tape device for the system.  Pick something reasonable here.
 */
#ifdef __linux
#define	_PATH_DEFTAPE "/dev/st0"
#endif

#ifndef _PATH_DEFTAPE
#define	_PATH_DEFTAPE "/dev/tape"
#endif

/* External function to parse a date/time string (from getdate.y) */
time_t get_date(const char *);

static int		 bsdtar_getopt(struct bsdtar *, const char *optstring,
    const struct option **poption);
static void		 long_help(struct bsdtar *);
static void		 only_mode(struct bsdtar *, const char *opt,
			     const char *valid);
static char **		 rewrite_argv(struct bsdtar *,
			     int *argc, char ** src_argv,
			     const char *optstring);
static void		 set_mode(struct bsdtar *, char opt);
static void		 version(void);

/*
 * The leading '+' here forces the GNU version of getopt() (as well as
 * both the GNU and BSD versions of getopt_long) to stop at the first
 * non-option.  Otherwise, GNU getopt() permutes the arguments and
 * screws up -C processing.
 */
static const char *tar_opts = "+Bb:C:cf:HhI:jkLlmnOoPprtT:UuvW:wX:xyZz";

/*
 * Most of these long options are deliberately not documented.  They
 * are provided only to make life easier for people who also use GNU tar.
 * The only long options documented in the manual page are the ones
 * with no corresponding short option, such as --exclude, --nodump,
 * and --fast-read.
 *
 * On systems that lack getopt_long, long options can be specified
 * using -W longopt and -W longopt=value, e.g. "-W nodump" is the same
 * as "--nodump" and "-W exclude=pattern" is the same as "--exclude
 * pattern".  This does not rely the GNU getopt() "W;" extension, so
 * should work correctly on any system with a POSIX-compliant getopt().
 */

/* Fake short equivalents for long options that otherwise lack them. */
enum {
	OPTION_CHECK_LINKS=1,
	OPTION_EXCLUDE,
	OPTION_FAST_READ,
	OPTION_FORMAT,
	OPTION_HELP,
	OPTION_INCLUDE,
	OPTION_NEWER_CTIME,
	OPTION_NEWER_CTIME_THAN,
	OPTION_NEWER_MTIME,
	OPTION_NEWER_MTIME_THAN,
	OPTION_NODUMP,
	OPTION_NO_SAME_OWNER,
	OPTION_NO_SAME_PERMISSIONS,
	OPTION_NULL,
	OPTION_ONE_FILE_SYSTEM,
	OPTION_POSIX,
	OPTION_STRIP_COMPONENTS,
	OPTION_TOTALS,
	OPTION_USE_COMPRESS_PROGRAM,
	OPTION_VERSION
};

/*
 * If you add anything, be very careful to keep this list properly
 * sorted, as the -W logic relies on it.
 */
static const struct option tar_longopts[] = {
	{ "absolute-paths",     no_argument,       NULL, 'P' },
	{ "append",             no_argument,       NULL, 'r' },
	{ "block-size",         required_argument, NULL, 'b' },
	{ "bunzip2",            no_argument,       NULL, 'j' },
	{ "bzip",               no_argument,       NULL, 'j' },
	{ "bzip2",              no_argument,       NULL, 'j' },
	{ "cd",                 required_argument, NULL, 'C' },
	{ "check-links",        no_argument,       NULL, OPTION_CHECK_LINKS },
	{ "confirmation",       no_argument,       NULL, 'w' },
	{ "create",             no_argument,       NULL, 'c' },
	{ "dereference",	no_argument,	   NULL, 'L' },
	{ "directory",          required_argument, NULL, 'C' },
	{ "exclude",            required_argument, NULL, OPTION_EXCLUDE },
	{ "exclude-from",       required_argument, NULL, 'X' },
	{ "extract",            no_argument,       NULL, 'x' },
	{ "fast-read",          no_argument,       NULL, OPTION_FAST_READ },
	{ "file",               required_argument, NULL, 'f' },
	{ "files-from",         required_argument, NULL, 'T' },
	{ "format",             required_argument, NULL, OPTION_FORMAT },
	{ "gunzip",             no_argument,       NULL, 'z' },
	{ "gzip",               no_argument,       NULL, 'z' },
	{ "help",               no_argument,       NULL, OPTION_HELP },
	{ "include",            required_argument, NULL, OPTION_INCLUDE },
	{ "interactive",        no_argument,       NULL, 'w' },
	{ "keep-old-files",     no_argument,       NULL, 'k' },
	{ "list",               no_argument,       NULL, 't' },
	{ "modification-time",  no_argument,       NULL, 'm' },
	{ "newer",		required_argument, NULL, OPTION_NEWER_CTIME },
	{ "newer-ctime",	required_argument, NULL, OPTION_NEWER_CTIME },
	{ "newer-ctime-than",	required_argument, NULL, OPTION_NEWER_CTIME_THAN },
	{ "newer-mtime",	required_argument, NULL, OPTION_NEWER_MTIME },
	{ "newer-mtime-than",	required_argument, NULL, OPTION_NEWER_MTIME_THAN },
	{ "newer-than",		required_argument, NULL, OPTION_NEWER_CTIME_THAN },
	{ "nodump",             no_argument,       NULL, OPTION_NODUMP },
	{ "norecurse",          no_argument,       NULL, 'n' },
	{ "no-recursion",       no_argument,       NULL, 'n' },
	{ "no-same-owner",	no_argument,	   NULL, OPTION_NO_SAME_OWNER },
	{ "no-same-permissions",no_argument,	   NULL, OPTION_NO_SAME_PERMISSIONS },
	{ "null",		no_argument,	   NULL, OPTION_NULL },
	{ "one-file-system",	no_argument,	   NULL, OPTION_ONE_FILE_SYSTEM },
	{ "posix",		no_argument,	   NULL, OPTION_POSIX },
	{ "preserve-permissions", no_argument,     NULL, 'p' },
	{ "read-full-blocks",	no_argument,	   NULL, 'B' },
	{ "same-permissions",   no_argument,       NULL, 'p' },
	{ "strip-components",	required_argument, NULL, OPTION_STRIP_COMPONENTS },
	{ "to-stdout",          no_argument,       NULL, 'O' },
	{ "totals",		no_argument,       NULL, OPTION_TOTALS },
	{ "unlink",		no_argument,       NULL, 'U' },
	{ "unlink-first",	no_argument,       NULL, 'U' },
	{ "update",             no_argument,       NULL, 'u' },
	{ "use-compress-program",
				required_argument, NULL, OPTION_USE_COMPRESS_PROGRAM },
	{ "verbose",            no_argument,       NULL, 'v' },
	{ "version",            no_argument,       NULL, OPTION_VERSION },
	{ NULL, 0, NULL, 0 }
};

/* A basic set of security flags to request from libarchive. */
#define	SECURITY					\
	(ARCHIVE_EXTRACT_SECURE_SYMLINKS		\
	 | ARCHIVE_EXTRACT_SECURE_NODOTDOT)

int
main(int argc, char **argv)
{
	struct bsdtar		*bsdtar, bsdtar_storage;
	const struct option	*option;
	int			 opt, t;
	char			 option_o;
	char			 possible_help_request;
	char			 buff[16];

	/*
	 * Use a pointer for consistency, but stack-allocated storage
	 * for ease of cleanup.
	 */
	bsdtar = &bsdtar_storage;
	memset(bsdtar, 0, sizeof(*bsdtar));
	bsdtar->fd = -1; /* Mark as "unused" */
	option_o = 0;

	/* Need bsdtar->progname before calling bsdtar_warnc. */
	if (*argv == NULL)
		bsdtar->progname = "bsdtar";
	else {
		bsdtar->progname = strrchr(*argv, '/');
		if (bsdtar->progname != NULL)
			bsdtar->progname++;
		else
			bsdtar->progname = *argv;
	}

	if (setlocale(LC_ALL, "") == NULL)
		bsdtar_warnc(bsdtar, 0, "Failed to set default locale");
#if defined(HAVE_NL_LANGINFO) && defined(HAVE_D_MD_ORDER)
	bsdtar->day_first = (*nl_langinfo(D_MD_ORDER) == 'd');
#endif
	possible_help_request = 0;

	/* Look up uid of current user for future reference */
	bsdtar->user_uid = geteuid();

	/* Default: open tape drive. */
	bsdtar->filename = getenv("TAPE");
	if (bsdtar->filename == NULL)
		bsdtar->filename = _PATH_DEFTAPE;

	/* Default: preserve mod time on extract */
	bsdtar->extract_flags = ARCHIVE_EXTRACT_TIME;

	/* Default: Perform basic security checks. */
	bsdtar->extract_flags |= SECURITY;

	/* Defaults for root user: */
	if (bsdtar->user_uid == 0) {
		/* --same-owner */
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_OWNER;
		/* -p */
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_PERM;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_ACL;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_XATTR;
		bsdtar->extract_flags |= ARCHIVE_EXTRACT_FFLAGS;
	}

	/* Rewrite traditional-style tar arguments, if used. */
	argv = rewrite_argv(bsdtar, &argc, argv, tar_opts);

	bsdtar->argv = argv;
	bsdtar->argc = argc;

	/* Process all remaining arguments now. */
	/*
	 * Comments following each option indicate where that option
	 * originated:  SUSv2, POSIX, GNU tar, star, etc.  If there's
	 * no such comment, then I don't know of anyone else who
	 * implements that option.
	 */
	while ((opt = bsdtar_getopt(bsdtar, tar_opts, &option)) != -1) {
		switch (opt) {
		case 'B': /* GNU tar */
			/* libarchive doesn't need this; just ignore it. */
			break;
		case 'b': /* SUSv2 */
			t = atoi(optarg);
			if (t <= 0 || t > 1024)
				bsdtar_errc(bsdtar, 1, 0,
				    "Argument to -b is out of range (1..1024)");
			bsdtar->bytes_per_block = 512 * t;
			break;
		case 'C': /* GNU tar */
			set_chdir(bsdtar, optarg);
			break;
		case 'c': /* SUSv2 */
			set_mode(bsdtar, opt);
			break;
		case OPTION_CHECK_LINKS: /* GNU tar */
			bsdtar->option_warn_links = 1;
			break;
		case OPTION_EXCLUDE: /* GNU tar */
			if (exclude(bsdtar, optarg))
				bsdtar_errc(bsdtar, 1, 0,
				    "Couldn't exclude %s\n", optarg);
			break;
		case OPTION_FORMAT: /* GNU tar, others */
			bsdtar->create_format = optarg;
			break;
		case 'f': /* SUSv2 */
			bsdtar->filename = optarg;
			if (strcmp(bsdtar->filename, "-") == 0)
				bsdtar->filename = NULL;
			break;
		case OPTION_FAST_READ: /* GNU tar */
			bsdtar->option_fast_read = 1;
			break;
		case 'H': /* BSD convention */
			bsdtar->symlink_mode = 'H';
			break;
		case 'h': /* Linux Standards Base, gtar; synonym for -L */
			bsdtar->symlink_mode = 'L';
			/* Hack: -h by itself is the "help" command. */
			possible_help_request = 1;
			break;
		case OPTION_HELP: /* GNU tar, others */
			long_help(bsdtar);
			exit(0);
			break;
		case 'I': /* GNU tar */
			/*
			 * TODO: Allow 'names' to come from an archive,
			 * not just a text file.  Design a good UI for
			 * allowing names and mode/owner to be read
			 * from an archive, with contents coming from
			 * disk.  This can be used to "refresh" an
			 * archive or to design archives with special
			 * permissions without having to create those
			 * permissions on disk.
			 */
			bsdtar->names_from_file = optarg;
			break;
		case OPTION_INCLUDE:
			/*
			 * Noone else has the @archive extension, so
			 * noone else needs this to filter entries
			 * when transforming archives.
			 */
			if (include(bsdtar, optarg))
				bsdtar_errc(bsdtar, 1, 0,
				    "Failed to add %s to inclusion list",
				    optarg);
			break;
		case 'j': /* GNU tar */
#if HAVE_LIBBZ2
			if (bsdtar->create_compression != '\0')
				bsdtar_errc(bsdtar, 1, 0,
				    "Can't specify both -%c and -%c", opt,
				    bsdtar->create_compression);
			bsdtar->create_compression = opt;
#else
			bsdtar_warnc(bsdtar, 0, "-j compression not supported by this version of bsdtar");
			usage(bsdtar);
#endif
			break;
		case 'k': /* GNU tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_NO_OVERWRITE;
			break;
		case 'L': /* BSD convention */
			bsdtar->symlink_mode = 'L';
			break;
	        case 'l': /* SUSv2 and GNU conflict badly here */
			if (getenv("POSIXLY_CORRECT") != NULL) {
				/* User has asked for POSIX/SUS behavior. */
				bsdtar->option_warn_links = 1;
			} else {
				fprintf(stderr,
"Error: -l has different behaviors in different tar programs.\n");
				fprintf(stderr,
"  For the GNU behavior, use --one-file-system instead.\n");
				fprintf(stderr,
"  For the POSIX behavior, use --check-links instead.\n");
				usage(bsdtar);
			}
			break;
		case 'm': /* SUSv2 */
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_TIME;
			break;
		case 'n': /* GNU tar */
			bsdtar->option_no_subdirs = 1;
			break;
	        /*
		 * Selecting files by time:
		 *    --newer-?time='date' Only files newer than 'date'
		 *    --newer-?time-than='file' Only files newer than time
		 *         on specified file (useful for incremental backups)
		 * TODO: Add corresponding "older" options to reverse these.
		 */
		case OPTION_NEWER_CTIME: /* GNU tar */
			bsdtar->newer_ctime_sec = get_date(optarg);
			break;
		case OPTION_NEWER_CTIME_THAN:
			{
				struct stat st;
				if (stat(optarg, &st) != 0)
					bsdtar_errc(bsdtar, 1, 0,
					    "Can't open file %s", optarg);
				bsdtar->newer_ctime_sec = st.st_ctime;
				bsdtar->newer_ctime_nsec =
				    ARCHIVE_STAT_CTIME_NANOS(&st);
			}
			break;
		case OPTION_NEWER_MTIME: /* GNU tar */
			bsdtar->newer_mtime_sec = get_date(optarg);
			break;
		case OPTION_NEWER_MTIME_THAN:
			{
				struct stat st;
				if (stat(optarg, &st) != 0)
					bsdtar_errc(bsdtar, 1, 0,
					    "Can't open file %s", optarg);
				bsdtar->newer_mtime_sec = st.st_mtime;
				bsdtar->newer_mtime_nsec =
				    ARCHIVE_STAT_MTIME_NANOS(&st);
			}
			break;
		case OPTION_NODUMP: /* star */
			bsdtar->option_honor_nodump = 1;
			break;
		case OPTION_NO_SAME_OWNER: /* GNU tar */
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_OWNER;
			break;
		case OPTION_NO_SAME_PERMISSIONS: /* GNU tar */
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_PERM;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_ACL;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_XATTR;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_FFLAGS;
			break;
		case OPTION_NULL: /* GNU tar */
			bsdtar->option_null++;
			break;
		case 'O': /* GNU tar */
			bsdtar->option_stdout = 1;
			break;
		case 'o': /* SUSv2 and GNU conflict here, but not fatally */
			option_o = 1; /* Record it and resolve it later. */
			break;
		case OPTION_ONE_FILE_SYSTEM: /* GNU tar */
			bsdtar->option_dont_traverse_mounts = 1;
			break;
#if 0
		/*
		 * The common BSD -P option is not necessary, since
		 * our default is to archive symlinks, not follow
		 * them.  This is convenient, as -P conflicts with GNU
		 * tar anyway.
		 */
		case 'P': /* BSD convention */
			/* Default behavior, no option necessary. */
			break;
#endif
		case 'P': /* GNU tar */
			bsdtar->extract_flags &= ~SECURITY;
			bsdtar->option_absolute_paths = 1;
			break;
		case 'p': /* GNU tar, star */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_PERM;
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_ACL;
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_XATTR;
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_FFLAGS;
			break;
		case OPTION_POSIX: /* GNU tar */
			bsdtar->create_format = "pax";
			break;
		case 'r': /* SUSv2 */
			set_mode(bsdtar, opt);
			break;
		case OPTION_STRIP_COMPONENTS: /* GNU tar 1.15 */
			bsdtar->strip_components = atoi(optarg);
			break;
		case 'T': /* GNU tar */
			bsdtar->names_from_file = optarg;
			break;
		case 't': /* SUSv2 */
			set_mode(bsdtar, opt);
			bsdtar->verbose++;
			break;
		case OPTION_TOTALS: /* GNU tar */
			bsdtar->option_totals++;
			break;
		case 'U': /* GNU tar */
			bsdtar->extract_flags |= ARCHIVE_EXTRACT_UNLINK;
			bsdtar->option_unlink_first = 1;
			break;
		case 'u': /* SUSv2 */
			set_mode(bsdtar, opt);
			break;
		case 'v': /* SUSv2 */
			bsdtar->verbose++;
			break;
		case OPTION_VERSION: /* GNU convention */
			version();
			break;
#if 0
		/*
		 * The -W longopt feature is handled inside of
		 * bsdtar_getop(), so -W is not available here.
		 */
		case 'W': /* Obscure, but useful GNU convention. */
			break;
#endif
		case 'w': /* SUSv2 */
			bsdtar->option_interactive = 1;
			break;
		case 'X': /* GNU tar */
			if (exclude_from_file(bsdtar, optarg))
				bsdtar_errc(bsdtar, 1, 0,
				    "failed to process exclusions from file %s",
				    optarg);
			break;
		case 'x': /* SUSv2 */
			set_mode(bsdtar, opt);
			break;
		case 'y': /* FreeBSD version of GNU tar */
#if HAVE_LIBBZ2
			if (bsdtar->create_compression != '\0')
				bsdtar_errc(bsdtar, 1, 0,
				    "Can't specify both -%c and -%c", opt,
				    bsdtar->create_compression);
			bsdtar->create_compression = opt;
#else
			bsdtar_warnc(bsdtar, 0, "-y compression not supported by this version of bsdtar");
			usage(bsdtar);
#endif
			break;
		case 'Z': /* GNU tar */
			if (bsdtar->create_compression != '\0')
				bsdtar_errc(bsdtar, 1, 0,
				    "Can't specify both -%c and -%c", opt,
				    bsdtar->create_compression);
			bsdtar->create_compression = opt;
			break;
		case 'z': /* GNU tar, star, many others */
#if HAVE_LIBZ
			if (bsdtar->create_compression != '\0')
				bsdtar_errc(bsdtar, 1, 0,
				    "Can't specify both -%c and -%c", opt,
				    bsdtar->create_compression);
			bsdtar->create_compression = opt;
#else
			bsdtar_warnc(bsdtar, 0, "-z compression not supported by this version of bsdtar");
			usage(bsdtar);
#endif
			break;
		case OPTION_USE_COMPRESS_PROGRAM:
			bsdtar->compress_program = optarg;
			break;
		default:
			usage(bsdtar);
		}
	}

	/*
	 * Sanity-check options.
	 */

	/* If no "real" mode was specified, treat -h as --help. */
	if ((bsdtar->mode == '\0') && possible_help_request) {
		long_help(bsdtar);
		exit(0);
	}

	/* Otherwise, a mode is required. */
	if (bsdtar->mode == '\0')
		bsdtar_errc(bsdtar, 1, 0,
		    "Must specify one of -c, -r, -t, -u, -x");

	/* Check boolean options only permitted in certain modes. */
	if (bsdtar->option_dont_traverse_mounts)
		only_mode(bsdtar, "--one-file-system", "cru");
	if (bsdtar->option_fast_read)
		only_mode(bsdtar, "--fast-read", "xt");
	if (bsdtar->option_honor_nodump)
		only_mode(bsdtar, "--nodump", "cru");
	if (option_o > 0) {
		switch (bsdtar->mode) {
		case 'c':
			/*
			 * In GNU tar, -o means "old format."  The
			 * "ustar" format is the closest thing
			 * supported by libarchive.
			 */
			bsdtar->create_format = "ustar";
			/* TODO: bsdtar->create_format = "v7"; */
			break;
		case 'x':
			/* POSIX-compatible behavior. */
			bsdtar->option_no_owner = 1;
			bsdtar->extract_flags &= ~ARCHIVE_EXTRACT_OWNER;
			break;
		default:
			only_mode(bsdtar, "-o", "xc");
			break;
		}
	}
	if (bsdtar->option_no_subdirs)
		only_mode(bsdtar, "-n", "cru");
	if (bsdtar->option_stdout)
		only_mode(bsdtar, "-O", "xt");
	if (bsdtar->option_unlink_first)
		only_mode(bsdtar, "-U", "x");
	if (bsdtar->option_warn_links)
		only_mode(bsdtar, "--check-links", "cr");

	/* Check other parameters only permitted in certain modes. */
	if (bsdtar->create_compression == 'Z' && bsdtar->mode == 'c') {
		bsdtar_warnc(bsdtar, 0, ".Z compression not supported");
		usage(bsdtar);
	}
	if (bsdtar->create_compression != '\0') {
		strcpy(buff, "-?");
		buff[1] = bsdtar->create_compression;
		only_mode(bsdtar, buff, "cxt");
	}
	if (bsdtar->create_format != NULL)
		only_mode(bsdtar, "--format", "c");
	if (bsdtar->symlink_mode != '\0') {
		strcpy(buff, "-?");
		buff[1] = bsdtar->symlink_mode;
		only_mode(bsdtar, buff, "cru");
	}
	if (bsdtar->strip_components != 0)
		only_mode(bsdtar, "--strip-components", "xt");

	bsdtar->argc -= optind;
	bsdtar->argv += optind;

	switch(bsdtar->mode) {
	case 'c':
		tar_mode_c(bsdtar);
		break;
	case 'r':
		tar_mode_r(bsdtar);
		break;
	case 't':
		tar_mode_t(bsdtar);
		break;
	case 'u':
		tar_mode_u(bsdtar);
		break;
	case 'x':
		tar_mode_x(bsdtar);
		break;
	}

	cleanup_exclusions(bsdtar);
	if (bsdtar->return_value != 0)
		bsdtar_warnc(bsdtar, 0,
		    "Error exit delayed from previous errors.");
	return (bsdtar->return_value);
}

static void
set_mode(struct bsdtar *bsdtar, char opt)
{
	if (bsdtar->mode != '\0' && bsdtar->mode != opt)
		bsdtar_errc(bsdtar, 1, 0,
		    "Can't specify both -%c and -%c", opt, bsdtar->mode);
	bsdtar->mode = opt;
}

/*
 * Verify that the mode is correct.
 */
static void
only_mode(struct bsdtar *bsdtar, const char *opt, const char *valid_modes)
{
	if (strchr(valid_modes, bsdtar->mode) == NULL)
		bsdtar_errc(bsdtar, 1, 0,
		    "Option %s is not permitted in mode -%c",
		    opt, bsdtar->mode);
}


/*-
 * Convert traditional tar arguments into new-style.
 * For example,
 *     tar tvfb file.tar 32 --exclude FOO
 * will be converted to
 *     tar -t -v -f file.tar -b 32 --exclude FOO
 *
 * This requires building a new argv array.  The initial bundled word
 * gets expanded into a new string that looks like "-t\0-v\0-f\0-b\0".
 * The new argv array has pointers into this string intermingled with
 * pointers to the existing arguments.  Arguments are moved to
 * immediately follow their options.
 *
 * The optstring argument here is the same one passed to getopt(3).
 * It is used to determine which option letters have trailing arguments.
 */
char **
rewrite_argv(struct bsdtar *bsdtar, int *argc, char **src_argv,
    const char *optstring)
{
	char **new_argv, **dest_argv;
	const char *p;
	char *src, *dest;

	if (src_argv[0] == NULL ||
	    src_argv[1] == NULL || src_argv[1][0] == '-')
		return (src_argv);

	*argc += strlen(src_argv[1]) - 1;
	new_argv = malloc((*argc + 1) * sizeof(new_argv[0]));
	if (new_argv == NULL)
		bsdtar_errc(bsdtar, 1, errno, "No Memory");

	dest_argv = new_argv;
	*dest_argv++ = *src_argv++;

	dest = malloc(strlen(*src_argv) * 3);
	if (dest == NULL)
		bsdtar_errc(bsdtar, 1, errno, "No memory");
	for (src = *src_argv++; *src != '\0'; src++) {
		*dest_argv++ = dest;
		*dest++ = '-';
		*dest++ = *src;
		*dest++ = '\0';
		/* If option takes an argument, insert that into the list. */
		for (p = optstring; p != NULL && *p != '\0'; p++) {
			if (*p != *src)
				continue;
			if (p[1] != ':')	/* No arg required, done. */
				break;
			if (*src_argv == NULL)	/* No arg available? Error. */
				bsdtar_errc(bsdtar, 1, 0,
				    "Option %c requires an argument",
				    *src);
			*dest_argv++ = *src_argv++;
			break;
		}
	}

	/* Copy remaining arguments, including trailing NULL. */
	while ((*dest_argv++ = *src_argv++) != NULL)
		;

	return (new_argv);
}

void
usage(struct bsdtar *bsdtar)
{
	const char	*p;

	p = bsdtar->progname;

	fprintf(stderr, "Usage:\n");
	fprintf(stderr, "  List:    %s -tf <archive-filename>\n", p);
	fprintf(stderr, "  Extract: %s -xf <archive-filename>\n", p);
	fprintf(stderr, "  Create:  %s -cf <archive-filename> [filenames...]\n", p);
#ifdef HAVE_GETOPT_LONG
	fprintf(stderr, "  Help:    %s --help\n", p);
#else
	fprintf(stderr, "  Help:    %s -h\n", p);
#endif
	exit(1);
}

static void
version(void)
{
	printf("bsdtar %s - %s\n", PACKAGE_VERSION, archive_version());
	exit(1);
}

static const char *long_help_msg =
	"First option must be a mode specifier:\n"
	"  -c Create  -r Add/Replace  -t List  -u Update  -x Extract\n"
	"Common Options:\n"
	"  -b #  Use # 512-byte records per I/O block\n"
	"  -f <filename>  Location of archive (default " _PATH_DEFTAPE ")\n"
	"  -v    Verbose\n"
	"  -w    Interactive\n"
	"Create: %p -c [options] [<file> | <dir> | @<archive> | -C <dir> ]\n"
	"  <file>, <dir>  add these items to archive\n"
	"  -z, -j  Compress archive with gzip/bzip2\n"
	"  --format {ustar|pax|cpio|shar}  Select archive format\n"
#ifdef HAVE_GETOPT_LONG
	"  --exclude <pattern>  Skip files that match pattern\n"
#else
	"  -W exclude=<pattern>  Skip files that match pattern\n"
#endif
	"  -C <dir>  Change to <dir> before processing remaining files\n"
	"  @<archive>  Add entries from <archive> to output\n"
	"List: %p -t [options] [<patterns>]\n"
	"  <patterns>  If specified, list only entries that match\n"
	"Extract: %p -x [options] [<patterns>]\n"
	"  <patterns>  If specified, extract only entries that match\n"
	"  -k    Keep (don't overwrite) existing files\n"
	"  -m    Don't restore modification times\n"
	"  -O    Write entries to stdout, don't restore to disk\n"
	"  -p    Restore permissions (including ACLs, owner, file flags)\n";


/*
 * Note that the word 'bsdtar' will always appear in the first line
 * of output.
 *
 * In particular, /bin/sh scripts that need to test for the presence
 * of bsdtar can use the following template:
 *
 * if (tar --help 2>&1 | grep bsdtar >/dev/null 2>&1 ) then \
 *          echo bsdtar; else echo not bsdtar; fi
 */
static void
long_help(struct bsdtar *bsdtar)
{
	const char	*prog;
	const char	*p;

	prog = bsdtar->progname;

	fflush(stderr);

	p = (strcmp(prog,"bsdtar") != 0) ? "(bsdtar)" : "";
	printf("%s%s: manipulate archive files\n", prog, p);

	for (p = long_help_msg; *p != '\0'; p++) {
		if (*p == '%') {
			if (p[1] == 'p') {
				fputs(prog, stdout);
				p++;
			} else
				putchar('%');
		} else
			putchar(*p);
	}
	version();
}

static int
bsdtar_getopt(struct bsdtar *bsdtar, const char *optstring,
    const struct option **poption)
{
	char *p, *q;
	const struct option *option;
	int opt;
	int option_index;
	size_t option_length;

	option_index = -1;
	*poption = NULL;

#ifdef HAVE_GETOPT_LONG
	opt = getopt_long(bsdtar->argc, bsdtar->argv, optstring,
	    tar_longopts, &option_index);
	if (option_index > -1)
		*poption = tar_longopts + option_index;
#else
	opt = getopt(bsdtar->argc, bsdtar->argv, optstring);
#endif

	/* Support long options through -W longopt=value */
	if (opt == 'W') {
		p = optarg;
		q = strchr(optarg, '=');
		if (q != NULL) {
			option_length = (size_t)(q - p);
			optarg = q + 1;
		} else {
			option_length = strlen(p);
			optarg = NULL;
		}
		option = tar_longopts;
		while (option->name != NULL &&
		    (strlen(option->name) < option_length ||
		    strncmp(p, option->name, option_length) != 0 )) {
			option++;
		}

		if (option->name != NULL) {
			*poption = option;
			opt = option->val;

			/* If the first match was exact, we're done. */
			if (strncmp(p, option->name, strlen(option->name)) == 0) {
				while (option->name != NULL)
					option++;
			} else {
				/* Check if there's another match. */
				option++;
				while (option->name != NULL &&
				    (strlen(option->name) < option_length ||
				    strncmp(p, option->name, option_length) != 0)) {
					option++;
				}
			}
			if (option->name != NULL)
				bsdtar_errc(bsdtar, 1, 0,
				    "Ambiguous option %s "
				    "(matches both %s and %s)",
				    p, (*poption)->name, option->name);

			if ((*poption)->has_arg == required_argument
			    && optarg == NULL)
				bsdtar_errc(bsdtar, 1, 0,
				    "Option \"%s\" requires argument", p);
		} else {
			opt = '?';
			/* TODO: Set up a fake 'struct option' for
			 * error reporting... ? ? ? */
		}
	}

	return (opt);
}
