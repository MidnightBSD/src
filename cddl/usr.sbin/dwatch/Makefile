# $FreeBSD: stable/11/cddl/usr.sbin/dwatch/Makefile 330559 2018-03-06 23:44:19Z dteske $

.include <src.opts.mk>

SUBDIR= libexec

.if ${MK_EXAMPLES} != "no"
SUBDIR+= examples
.endif

SCRIPTS= dwatch

MAN= dwatch.1

.include <bsd.prog.mk>
