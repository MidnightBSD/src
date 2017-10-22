# $FreeBSD: release/10.0.0/usr.bin/procstat/Makefile 235642 2012-05-19 05:07:03Z marcel $

PROG=	procstat
MAN=	procstat.1
SRCS=	procstat.c		\
	procstat_args.c		\
	procstat_auxv.c		\
	procstat_basic.c	\
	procstat_bin.c		\
	procstat_cred.c		\
	procstat_files.c	\
	procstat_kstack.c	\
	procstat_rlimit.c	\
	procstat_sigs.c		\
	procstat_threads.c	\
	procstat_vm.c

LDADD+=	-lutil -lprocstat -lkvm
DPADD+=	${LIBUTIL} ${LIBPROCSTAT} ${LIBKVM}

.include <bsd.prog.mk>
