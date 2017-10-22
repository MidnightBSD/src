#!/usr/bin/sed -E -n -f
# $FreeBSD: release/7.0.0/sys/conf/makeLINT.sed 174854 2007-12-22 06:32:46Z cvs2svn $

/^(machine|ident|device|nodevice|makeoptions|nomakeoption|options|option|nooption|profile|cpu|maxusers)[[:space:]]/ {
    s/[[:space:]]*#.*$//
    p
}
