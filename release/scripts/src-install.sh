#!/bin/sh
#
# $FreeBSD: src/release/scripts/src-install.sh,v 1.10 2004/08/06 08:42:05 cperciva Exp $
# $MidnightBSD: src/release/scripts/src-install.sh,v 1.2 2007/03/17 16:44:45 laffer1 Exp $

if [ "`id -u`" != "0" ]; then
	echo "Sorry, this must be done as root."
	exit 1
fi
if [ $# -lt 1 ]; then
	echo "You must specify which components of src to extract"
	echo "possible subcomponents are:"
	echo
	echo "base bin cddl compat contrib crypto etc games gnu include krb5"
	echo "lib libexec nrelease release rescue sbin secure share sys tools"
	echo "ubin usbin"
	echo
	echo "You may also specify all to extract all subcomponents."
	exit 1
fi

if [ "$1" = "all" ]; then
	dists="base bin cddl compat contrib crypto etc games gnu include krb5 lib libexec nrelease release rescue sbin secure share sys tools ubin usbin"
else
	dists="$*"
fi

echo "Extracting sources into ${DESTDIR}/usr/src..."
for i in $dists; do
	echo "  Extracting source component: $i"
	cat s${i}.?? | tar --unlink -xpzf - -C ${DESTDIR}/usr/src
done
echo "Done extracting sources."
exit 0
