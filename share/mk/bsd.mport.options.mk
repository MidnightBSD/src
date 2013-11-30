# $FreeBSD: src/share/mk/bsd.port.pre.mk,v 1.4 2002/04/19 07:42:41 ru Exp $
# $MidnightBSD: src/share/mk/bsd.port.pre.mk,v 1.2 2006/05/22 06:03:21 laffer1 Exp $
#
# bsd.mport.options.mk - Process OPTIONS
#
#
# When included in a port, this makefile will process the port's 
# OPTIONS, allowing them to be used before including bsd.port.pre.mk
#
#
# Usage:
#
#	.include <bsd.mport.options.mk>
# 		... do stuff with options ...
#   .include <bsd.port.pre.mk>
#       ... other work ...
#   .include <bsd.port.post.mk>
#   
#

USEOPTIONSMK=	yes
INOPTIONSMK=	yes

.include "bsd.port.mk"

.undef INOPTIONSMK
