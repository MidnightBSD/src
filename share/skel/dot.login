# $MidnightBSD: src/share/skel/dot.login,v 1.4 2011/11/23 12:48:52 laffer1 Exp $
#
# .login - csh login script, read by login shell, after `.cshrc' at login.
#
# see also csh(1), environ(7).
#

if ( -x /usr/games/fortune ) /usr/games/fortune fortunes
