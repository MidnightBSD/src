# $FreeBSD: src/share/skel/dot.login,v 1.16 2001/06/25 20:40:02 nik Exp $
# $MidnightBSD: src/share/skel/dot.login,v 1.3 2009/03/29 03:10:41 laffer1 Exp $
#
# .login - csh login script, read by login shell, after `.cshrc' at login.
#
# see also csh(1), environ(7).
#

if ( -x /usr/games/fortune ) /usr/games/fortune fortunes
