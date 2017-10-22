/*
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: release/7.0.0/gnu/usr.bin/gdb/kgdb/main.c 154043 2006-01-04 23:17:52Z kan $");

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <inttypes.h>
#include <kvm.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* libgdb stuff. */
#include <defs.h>
#include <frame.h>
#include <frame-unwind.h>
#include <inferior.h>
#include <interps.h>
#include <cli-out.h>
#include <main.h>
#include <target.h>
#include <top.h>
#include <bfd.h>
#include <gdbcore.h>

extern void (*init_ui_hook)(char *);

extern frame_unwind_sniffer_ftype *kgdb_sniffer_kluge;

extern void symbol_file_add_main (char *args, int from_tty);

#include "kgdb.h"

kvm_t *kvm;
static char kvm_err[_POSIX2_LINE_MAX];

static int dumpnr;
static int verbose;

static char crashdir[PATH_MAX];
static char *kernel;
static char *remote;
static char *vmcore;

static void (*kgdb_new_objfile_chain)(struct objfile * objfile);

static void
kgdb_atexit(void)
{
	if (kvm != NULL)
		kvm_close(kvm);
}

static void
usage(void)
{

	fprintf(stderr,
	    "usage: %s [-afqv] [-d crashdir] [-c core | -n dumpnr | -r device]\n"
	    "\t[kernel [core]]\n", getprogname());
	exit(1);
}

static void
kernel_from_dumpnr(int nr)
{
	char path[PATH_MAX];
	FILE *info;
	char *s;
	struct stat st;
	int l;

	/*
	 * If there's a kernel image right here in the crash directory, then
	 * use it.  The kernel image is either called kernel.<nr> or is in a
	 * subdirectory kernel.<nr> and called kernel.  The latter allows us
	 * to collect the modules in the same place.
	 */
	snprintf(path, sizeof(path), "%s/kernel.%d", crashdir, nr);
	if (stat(path, &st) == 0) {
		if (S_ISREG(st.st_mode)) {
			kernel = strdup(path);
			return;
		}
		if (S_ISDIR(st.st_mode)) {
			snprintf(path, sizeof(path), "%s/kernel.%d/kernel",
			    crashdir, nr);
			if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
				kernel = strdup(path);
				return;
			}
		}
	}

	/*
	 * No kernel image here.  Parse the dump header.  The kernel object
	 * directory can be found there and we probably have the kernel
	 * image still in it.  The object directory may also have a kernel
	 * with debugging info (called kernel.debug).  If we have a debug
	 * kernel, use it.
	 */
	snprintf(path, sizeof(path), "%s/info.%d", crashdir, nr);
	info = fopen(path, "r");
	if (info == NULL) {
		warn(path);
		return;
	}
	while (fgets(path, sizeof(path), info) != NULL) {
		l = strlen(path);
		if (l > 0 && path[l - 1] == '\n')
			path[--l] = '\0';
		if (strncmp(path, "    ", 4) == 0) {
			s = strchr(path, ':');
			s = (s == NULL) ? path + 4 : s + 1;
			l = snprintf(path, sizeof(path), "%s/kernel.debug", s);
			if (stat(path, &st) == -1 || !S_ISREG(st.st_mode)) {
				path[l - 6] = '\0';
				if (stat(path, &st) == -1 ||
				    !S_ISREG(st.st_mode))
					break;
			}
			kernel = strdup(path);
			break;
		}
	}
	fclose(info);
}

static void
kgdb_new_objfile(struct objfile *objfile)
{
#if 0
	printf("XXX: %s(%p)\n", __func__, objfile);
	if (objfile != NULL) {
		goto out;
	}

out:
#endif
	if (kgdb_new_objfile_chain != NULL)
		kgdb_new_objfile_chain(objfile);
}

static CORE_ADDR
kgdb_parse(const char *exp)
{
	struct cleanup *old_chain;
	struct expression *expr;
	struct value *val;
	char *s;
	CORE_ADDR n;

	s = strdup(exp);
	old_chain = make_cleanup(free_current_contents, &expr);
	expr = parse_expression(s);
	val = (expr != NULL) ? evaluate_expression(expr) : NULL;
	n = (val != NULL) ? value_as_address(val) : 0;
	do_cleanups(old_chain);
	free(s);
	return (n);
}

#define	MSGBUF_SEQ_TO_POS(size, seq)	((seq) % (size))

static void
kgdb_init_target(void)
{
	CORE_ADDR bufp;
	bfd *kern_bfd;
	int size, rseq, wseq;
	int kern_desc;
	char c;

	kern_desc = open(kernel, O_RDONLY);
	if (kern_desc == -1)
		errx(1, "couldn't open a kernel image");

	kern_bfd = bfd_fdopenr(kernel, gnutarget, kern_desc);
	if (kern_bfd == NULL) {
		close(kern_desc);
		errx(1, "\"%s\": can't open to probe ABI: %s.", kernel,
			bfd_errmsg (bfd_get_error ()));
	}
	bfd_set_cacheable(kern_bfd, 1);

	if (!bfd_check_format (kern_bfd, bfd_object)) {
		bfd_close(kern_bfd);
		errx(1, "\"%s\": not in executable format: %s", kernel,
			bfd_errmsg(bfd_get_error()));
        }

	set_gdbarch_from_file (kern_bfd);
	bfd_close(kern_bfd);

	symbol_file_add_main (kernel, 0);
	if (remote)
		push_remote_target (remote, 0);
	else
		kgdb_target();

	/*
	 * Display the unread portion of the message buffer. This gives the
	 * user a some initial data to work from.
	 */
	bufp = kgdb_parse("msgbufp->msg_ptr");
	size = (int)kgdb_parse("msgbufp->msg_size");
	rseq = (int)kgdb_parse("msgbufp->msg_rseq");
	wseq = (int)kgdb_parse("msgbufp->msg_wseq");
	rseq = MSGBUF_SEQ_TO_POS(size, rseq);
	wseq = MSGBUF_SEQ_TO_POS(size, wseq);
	if (bufp == 0 || size == 0 || rseq == wseq)
		return;

	printf("\nUnread portion of the kernel message buffer:\n");
	while (rseq < wseq) {
		read_memory(bufp + rseq, &c, 1);
		putchar(c);
		rseq++;
		if (rseq == size)
			rseq = 0;
	}
	if (c != '\n')
		putchar('\n');
	putchar('\n');
}

static void
kgdb_interp_command_loop(void *data)
{
	static int once = 0;

	if (!once) {
		once = 1;
		kgdb_init_target();
		print_stack_frame(get_selected_frame(),
		    frame_relative_level(get_selected_frame()), 1);
	}
	command_loop();
}

static void
kgdb_init(char *argv0 __unused)
{
	static struct interp_procs procs = {
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		kgdb_interp_command_loop
	};
	struct interp *kgdb;
	kgdb = interp_new("kgdb", NULL, cli_out_new(gdb_stdout), &procs);
	interp_add(kgdb);

	set_prompt("(kgdb) ");
	kgdb_new_objfile_chain = target_new_objfile_hook;
	target_new_objfile_hook = kgdb_new_objfile;
}

int
main(int argc, char *argv[])
{
	char path[PATH_MAX];
	struct stat st;
	struct captured_main_args args;
	char *s;
	int a, ch, quiet, writecore;

	dumpnr = -1;

	strlcpy(crashdir, "/var/crash", sizeof(crashdir));
	s = getenv("KGDB_CRASH_DIR");
	if (s != NULL)
		strlcpy(crashdir, s, sizeof(crashdir));

	/* Convert long options into short options. */
	for (a = 1; a < argc; a++) {
		s = argv[a];
		if (s[0] == '-') {
			s++;
			/* Long options take either 1 or 2 dashes. */
			if (s[0] == '-')
				s++;
			if (strcmp(s, "quiet") == 0)
				argv[a] = "-q";
			else if (strcmp(s, "fullname") == 0)
				argv[a] = "-f";
		}
	}

	quiet = 0;
	writecore = 0;

	while ((ch = getopt(argc, argv, "ac:d:fn:qr:vw")) != -1) {
		switch (ch) {
		case 'a':
			annotation_level++;
			break;
		case 'c':	/* use given core file. */
			if (vmcore != NULL) {
				warnx("option %c: can only be specified once",
				    optopt);
				usage();
				/* NOTREACHED */
			}
			vmcore = strdup(optarg);
			break;
		case 'd':	/* lookup dumps in given directory. */
			strlcpy(crashdir, optarg, sizeof(crashdir));
			break;
		case 'f':
			annotation_level = 1;
			break;
		case 'n':	/* use dump with given number. */
			dumpnr = strtol(optarg, &s, 0);
			if (dumpnr < 0 || *s != '\0') {
				warnx("option %c: invalid kernel dump number",
				    optopt);
				usage();
				/* NOTREACHED */
			}
			break;
		case 'q':
			quiet = 1;
			break;
		case 'r':	/* use given device for remote session. */
			if (remote != NULL) {
				warnx("option %c: can only be specified once",
				    optopt);
				usage();
				/* NOTREACHED */
			}
			remote = strdup(optarg);
			break;
		case 'v':	/* increase verbosity. */
			verbose++;
			break;
		case 'w':	/* core file is writeable. */
			writecore = 1;
			break;
		case '?':
		default:
			usage();
		}
	}

	if (((vmcore != NULL) ? 1 : 0) + ((dumpnr >= 0) ? 1 : 0) +
	    ((remote != NULL) ? 1 : 0) > 1) {
		warnx("options -c, -n and -r are mutually exclusive");
		usage();
		/* NOTREACHED */
	}

	if (verbose > 1)
		warnx("using %s as the crash directory", crashdir);

	if (argc > optind)
		kernel = strdup(argv[optind++]);

	if (argc > optind && (dumpnr >= 0 || remote != NULL)) {
		warnx("options -n and -r do not take a core file. Ignored");
		optind = argc;
	}

	if (dumpnr >= 0) {
		snprintf(path, sizeof(path), "%s/vmcore.%d", crashdir, dumpnr);
		if (stat(path, &st) == -1)
			err(1, path);
		if (!S_ISREG(st.st_mode))
			errx(1, "%s: not a regular file", path);
		vmcore = strdup(path);
	} else if (remote != NULL && remote[0] != ':' && remote[0] != '|') {
		if (stat(remote, &st) != 0) {
			snprintf(path, sizeof(path), "/dev/%s", remote);
			if (stat(path, &st) != 0) {
				err(1, "%s", remote);
				/* NOTREACHED */
			}
			free(remote);
			remote = strdup(path);
		}
		if (!S_ISCHR(st.st_mode) && !S_ISFIFO(st.st_mode)) {
			errx(1, "%s: not a special file, FIFO or socket",
			    remote);
			/* NOTREACHED */
		}
	} else if (argc > optind) {
		if (vmcore == NULL)
			vmcore = strdup(argv[optind++]);
		if (argc > optind)
			warnx("multiple core files specified. Ignored");
	} else if (vmcore == NULL && kernel == NULL) {
		vmcore = strdup(_PATH_MEM);
		kernel = strdup(getbootfile());
	}

	if (verbose) {
		if (vmcore != NULL)
			warnx("core file: %s", vmcore);
		if (remote != NULL)
			warnx("device file: %s", remote);
		if (kernel != NULL)
			warnx("kernel image: %s", kernel);
	}

	/*
	 * At this point we must either have a core file or have a kernel
	 * with a remote target.
	 */
	if (remote != NULL && kernel == NULL) {
		warnx("remote debugging requires a kernel");
		usage();
		/* NOTREACHED */
	}
	if (vmcore == NULL && remote == NULL) {
		warnx("need a core file or a device for remote debugging");
		usage();
		/* NOTREACHED */
	}

	/* If we don't have a kernel image yet, try to find one. */
	if (kernel == NULL) {
		if (dumpnr >= 0)
			kernel_from_dumpnr(dumpnr);

		if (kernel == NULL)
			errx(1, "couldn't find a suitable kernel image");
		if (verbose)
			warnx("kernel image: %s", kernel);
	}

	if (remote == NULL) {
		kvm = kvm_openfiles(kernel, vmcore, NULL,
		    writecore ? O_RDWR : O_RDONLY, kvm_err);
		if (kvm == NULL)
			errx(1, kvm_err);
		atexit(kgdb_atexit);
		kgdb_thr_init();
	}

	/* The libgdb code uses optind too. Reset it... */
	optind = 0;

	memset (&args, 0, sizeof args);
	args.argv = argv;
	args.argc = 1 + quiet;
	if (quiet)
		argv[1] = "-q";
	argv[args.argc] = NULL;
	args.use_windows = 0;
	args.interpreter_p = "kgdb";

	init_ui_hook = kgdb_init;

	kgdb_sniffer_kluge = kgdb_trgt_trapframe_sniffer;

	return (gdb_main(&args));
}
