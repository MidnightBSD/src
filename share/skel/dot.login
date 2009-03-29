# $FreeBSD: src/share/skel/dot.login,v 1.16 2001/06/25 20:40:02 nik Exp $
# $MidnightBSD: src/share/skel/dot.login,v 1.2 2006/10/31 19:20:02 laffer1 Exp $
#
# .login - csh login script, read by login shell, after `.cshrc' at login.
#
# see also csh(1), environ(7).
#

[ -x /usr/games/fortune ] && /usr/games/fortune fortunes
