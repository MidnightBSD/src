# $MidnightBSD$

.if !defined(PORTSDIR)
# Autodetect if the command is being run in a mports tree that's not rooted
# in the default /usr/mports.  The ../../.. case is in case ports ever grows
# a third level.
.for RELPATH in . .. ../.. ../../..
.if !defined(_PORTSDIR) && exists(${.CURDIR}/${RELPATH}/Mk/bsd.port.mk)
_PORTSDIR=	${.CURDIR}/${RELPATH}
.endif
.endfor
_PORTSDIR?=	/usr/mports
.if defined(.PARSEDIR)
PORTSDIR=	${_PORTSDIR:tA}
.else # fmake doesn't have :tA
PORTSDIR!=	realpath ${_PORTSDIR}
.endif
.endif

BSDPORTSUBDIRMK?=	${PORTSDIR}/Mk/bsd.port.subdir.mk

.include "${BSDPORTSUBDIRMK}"
