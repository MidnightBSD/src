# $MidnightBSD$
# $FreeBSD: stable/10/share/mk/local.sys.mk 294043 2016-01-14 21:53:06Z bdrewery $

.if defined(.PARSEDIR)
SRCTOP:= ${.PARSEDIR:tA:H:H}
.else
SRCTOP:= ${.MAKEFILE_LIST:M*/local.sys.mk:H:H:H}
.endif

.if ${.CURDIR} == ${SRCTOP}
RELDIR = .
.elif ${.CURDIR:M${SRCTOP}/*}
RELDIR := ${.CURDIR:S,${SRCTOP}/,,}
.endif
