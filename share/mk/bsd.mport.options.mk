# $MidnightBSD: src/share/mk/bsd.mport.options.mk,v 1.1.2.1 2008/10/29 18:41:56 ctriv Exp $
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

.include <bsd.port.mk>

.undef INOPTIONSMK
