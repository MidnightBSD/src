# $MidnightBSD$
# $FreeBSD: src/etc/root/dot.login,v 1.22 2000/07/15 03:25:14 rwatson Exp $
#
# .login - csh login script, read by login shell, after `.cshrc' at login.
#
# See also csh(1), environ(7).
#

# Query terminal size; useful for serial lines.
if ( -x /usr/bin/resizewin ) /usr/bin/resizewin -z

# Uncomment to display a random cookie each login:
# if ( -x /usr/games/fortune ) /usr/games/fortune -s
