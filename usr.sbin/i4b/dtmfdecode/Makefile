#---------------------------------------------------------------------------
#
# $FreeBSD: release/7.0.0/usr.sbin/i4b/dtmfdecode/Makefile 80029 2001-07-20 06:20:32Z obrien $
#
#	last edit-date: [Tue Dec 14 21:17:46 1999]
#
#---------------------------------------------------------------------------

PROG=	dtmfdecode

CFLAGS+= -DDEBUG

# libm is only necessary if USE_COS is defined in the source
#LDADD=	-lm

test:   ${PROG}
	uudecode -p < dtmfsounds.al.uu | ./${PROG}

.include <bsd.prog.mk>
