#
# $FreeBSD: stable/11/usr.sbin/pmcstat/Makefile 275054 2014-11-25 16:57:27Z bapt $
#

PROG=	pmcstat
MAN=	pmcstat.8

LIBADD=	elf kvm pmc m ncursesw

SRCS=	pmcstat.c pmcstat.h pmcstat_log.c \
pmcpl_callgraph.c pmcpl_gprof.c pmcpl_annotate.c \
pmcpl_annotate_cg.c pmcpl_calltree.c

.include <bsd.prog.mk>
