#	$NetBSD: Makefile,v 1.4 2006/11/09 15:25:37 jmmv Exp $
# $FreeBSD: release/10.0.0/tools/regression/tmpfs/Makefile 204585 2010-03-02 16:58:04Z uqs $

tests=	t_mount
tests+=	t_statvfs
tests+=	t_mkdir
tests+=	t_vnode_leak
tests+=	t_setattr
tests+=	t_rmdir
tests+=	t_trail_slash
tests+=	t_dots
tests+=	t_create
tests+=	t_remove
tests+=	t_link
tests+=	t_rename
tests+=	t_read_write
tests+=	t_exec
tests+=	t_truncate
tests+=	t_sizes
tests+=	t_times
tests+=	t_symlink
tests+=	t_pipes
tests+=	t_sockets
tests+=	t_readdir
tests+=	t_vnd

regress: ${tests}
.for t in ${tests}
	@SUBRDIR=${.CURDIR} /bin/sh ${.CURDIR}/${t} ${TEST_ARGS}
.endfor

PROG=	h_tools
NO_MAN=	# defined
WARNS?=	4

t_sizes t_sockets t_statvfs: h_tools

.include <bsd.prog.mk>
