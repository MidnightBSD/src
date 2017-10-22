/*
 * Copyright (c) 1993 Paul Kranenburg
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
__FBSDID("$FreeBSD$");

#include <sys/wait.h>

#include <machine/elf.h>

#include <arpa/inet.h>

#include <a.out.h>
#include <dlfcn.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"

#ifdef COMPAT_32BIT
#define	LD_	"LD_32_"
#else
#define	LD_	"LD_"
#endif

/*
 * 32-bit ELF data structures can only be used if the system header[s] declare
 * them.  There is no official macro for determining whether they are declared,
 * so check for the existence of one of the 32-macros defined in elf(5).
 */
#ifdef ELF32_R_TYPE
#define	ELF32_SUPPORTED
#endif

static int	is_executable(const char *fname, int fd, int *is_shlib,
		    int *type);
static void	usage(void);

#define	TYPE_UNKNOWN	0
#define	TYPE_AOUT	1
#define	TYPE_ELF	2	/* Architecture default */
#if __ELF_WORD_SIZE > 32 && defined(ELF32_SUPPORTED)
#define	TYPE_ELF32	3	/* Explicit 32 bits on architectures >32 bits */

#define	_PATH_LDD32	"/usr/bin/ldd32"

static int
execldd32(char *file, char *fmt1, char *fmt2, int aflag, int vflag)
{
	char *argv[8];
	int i, rval, status;

	unsetenv(LD_ "TRACE_LOADED_OBJECTS");
	rval = 0;
	i = 0;
	argv[i++] = strdup(_PATH_LDD32);
	if (aflag)
		argv[i++] = strdup("-a");
	if (vflag)
		argv[i++] = strdup("-v");
	if (fmt1 != NULL) {
		argv[i++] = strdup("-f");
		argv[i++] = strdup(fmt1);
	}
	if (fmt2 != NULL) {
		argv[i++] = strdup("-f");
		argv[i++] = strdup(fmt2);
	}
	argv[i++] = strdup(file);
	argv[i++] = NULL;

	switch (fork()) {
	case -1:
		err(1, "fork");
		break;
	case 0:
		execv(_PATH_LDD32, argv);
		warn("%s", _PATH_LDD32);
		_exit(127);
		break;
	default:
		if (wait(&status) < 0)
			rval = 1;
		else if (WIFSIGNALED(status))
			rval = 1;
		else if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
			rval = 1;
		break;
	}
	while (i--)
		free(argv[i]);
	setenv(LD_ "TRACE_LOADED_OBJECTS", "yes", 1);
	return (rval);
}
#endif

int
main(int argc, char *argv[])
{
	char *fmt1, *fmt2;
	int rval, c, aflag, vflag;

	aflag = vflag = 0;
	fmt1 = fmt2 = NULL;

	while ((c = getopt(argc, argv, "af:v")) != -1) {
		switch (c) {
		case 'a':
			aflag++;
			break;
		case 'f':
			if (fmt1 != NULL) {
				if (fmt2 != NULL)
					errx(1, "too many formats");
				fmt2 = optarg;
			} else
				fmt1 = optarg;
			break;
		case 'v':
			vflag++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (vflag && fmt1 != NULL)
		errx(1, "-v may not be used with -f");

	if (argc <= 0) {
		usage();
		/* NOTREACHED */
	}

#ifdef __i386__
	if (vflag) {
		for (c = 0; c < argc; c++)
			dump_file(argv[c]);
		exit(error_count == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
	}
#endif

	rval = 0;
	for (; argc > 0; argc--, argv++) {
		int fd, status, is_shlib, rv, type;

		if ((fd = open(*argv, O_RDONLY, 0)) < 0) {
			warn("%s", *argv);
			rval |= 1;
			continue;
		}
		rv = is_executable(*argv, fd, &is_shlib, &type);
		close(fd);
		if (rv == 0) {
			rval |= 1;
			continue;
		}

		switch (type) {
		case TYPE_ELF:
		case TYPE_AOUT:
			break;
#if __ELF_WORD_SIZE > 32 && defined(ELF32_SUPPORTED)
		case TYPE_ELF32:
			rval |= execldd32(*argv, fmt1, fmt2, aflag, vflag);
			continue;
#endif
		case TYPE_UNKNOWN:
		default:
			/*
			 * This shouldn't happen unless is_executable()
			 * is broken.
			 */
			errx(EDOOFUS, "unknown executable type");
		}

		/* ld.so magic */
		setenv(LD_ "TRACE_LOADED_OBJECTS", "yes", 1);
		if (fmt1 != NULL)
			setenv(LD_ "TRACE_LOADED_OBJECTS_FMT1", fmt1, 1);
		if (fmt2 != NULL)
			setenv(LD_ "TRACE_LOADED_OBJECTS_FMT2", fmt2, 1);

		setenv(LD_ "TRACE_LOADED_OBJECTS_PROGNAME", *argv, 1);
		if (aflag)
			setenv(LD_ "TRACE_LOADED_OBJECTS_ALL", "1", 1);
		else if (fmt1 == NULL && fmt2 == NULL)
			/* Default formats */
			printf("%s:\n", *argv);
		fflush(stdout);

		switch (fork()) {
		case -1:
			err(1, "fork");
			break;
		default:
			if (wait(&status) < 0) {
				warn("wait");
				rval |= 1;
			} else if (WIFSIGNALED(status)) {
				fprintf(stderr, "%s: signal %d\n", *argv,
				    WTERMSIG(status));
				rval |= 1;
			} else if (WIFEXITED(status) &&
			    WEXITSTATUS(status) != 0) {
				fprintf(stderr, "%s: exit status %d\n", *argv,
				    WEXITSTATUS(status));
				rval |= 1;
			}
			break;
		case 0:
			if (is_shlib == 0) {
				execl(*argv, *argv, (char *)NULL);
				warn("%s", *argv);
			} else {
				dlopen(*argv, RTLD_TRACE);
				warnx("%s: %s", *argv, dlerror());
			}
			_exit(1);
		}
	}

	return rval;
}

static void
usage(void)
{

	fprintf(stderr, "usage: ldd [-a] [-v] [-f format] program ...\n");
	exit(1);
}

static int
is_executable(const char *fname, int fd, int *is_shlib, int *type)
{
	union {
		struct exec aout;
#if __ELF_WORD_SIZE > 32 && defined(ELF32_SUPPORTED)
		Elf32_Ehdr elf32;
#endif
		Elf_Ehdr elf;
	} hdr;
	int n;

	*is_shlib = 0;
	*type = TYPE_UNKNOWN;

	if ((n = read(fd, &hdr, sizeof(hdr))) == -1) {
		warn("%s: can't read program header", fname);
		return (0);
	}

	if ((size_t)n >= sizeof(hdr.aout) && !N_BADMAG(hdr.aout)) {
		/* a.out file */
		if ((N_GETFLAG(hdr.aout) & EX_DPMASK) != EX_DYNAMIC
#if 1 /* Compatibility */
		    || hdr.aout.a_entry < __LDPGSZ
#endif
			) {
			warnx("%s: not a dynamic executable", fname);
			return (0);
		}
		*type = TYPE_AOUT;
		return (1);
	}

#if __ELF_WORD_SIZE > 32 && defined(ELF32_SUPPORTED)
	if ((size_t)n >= sizeof(hdr.elf32) && IS_ELF(hdr.elf32) &&
	    hdr.elf32.e_ident[EI_CLASS] == ELFCLASS32) {
		/* Handle 32 bit ELF objects */
		Elf32_Phdr phdr;
		int dynamic, i;

		dynamic = 0;
		*type = TYPE_ELF32;

		if (lseek(fd, hdr.elf32.e_phoff, SEEK_SET) == -1) {
			warnx("%s: header too short", fname);
			return (0);
		}
		for (i = 0; i < hdr.elf32.e_phnum; i++) {
			if (read(fd, &phdr, hdr.elf32.e_phentsize) !=
			    sizeof(phdr)) {
				warnx("%s: can't read program header", fname);
				return (0);
			}
			if (phdr.p_type == PT_DYNAMIC) {
				dynamic = 1;
				break;
			}
		}

		if (!dynamic) {
			warnx("%s: not a dynamic ELF executable", fname);
			return (0);
		}
		if (hdr.elf32.e_type == ET_DYN) {
			if (hdr.elf32.e_ident[EI_OSABI] == ELFOSABI_FREEBSD) {
				*is_shlib = 1;
				return (1);
			}
			warnx("%s: not a FreeBSD ELF shared object", fname);
			return (0);
		}

		return (1);
	}
#endif

	if ((size_t)n >= sizeof(hdr.elf) && IS_ELF(hdr.elf) &&
	    hdr.elf.e_ident[EI_CLASS] == ELF_TARG_CLASS) {
		/* Handle default ELF objects on this architecture */
		Elf_Phdr phdr;
		int dynamic, i;

		dynamic = 0;
		*type = TYPE_ELF;

		if (lseek(fd, hdr.elf.e_phoff, SEEK_SET) == -1) {
			warnx("%s: header too short", fname);
			return (0);
		}
		for (i = 0; i < hdr.elf.e_phnum; i++) {
			if (read(fd, &phdr, hdr.elf.e_phentsize)
			   != sizeof(phdr)) {
				warnx("%s: can't read program header", fname);
				return (0);
			}
			if (phdr.p_type == PT_DYNAMIC) {
				dynamic = 1;
				break;
			}
		}

		if (!dynamic) {
			warnx("%s: not a dynamic ELF executable", fname);
			return (0);
		}
		if (hdr.elf.e_type == ET_DYN) {
			if (hdr.elf.e_ident[EI_OSABI] == ELFOSABI_FREEBSD) {
				*is_shlib = 1;
				return (1);
			}
			warnx("%s: not a FreeBSD ELF shared object", fname);
			return (0);
		}

		return (1);
	}

	warnx("%s: not a dynamic executable", fname);
	return (0);
}
