#!/usr/bin/sed -E -n -f
# $FreeBSD: release/10.0.0/sys/conf/makeLINT.sed 226013 2011-10-04 17:11:38Z marcel $

/^(machine|files|ident|(no)?device|(no)?makeoption(s)?|(no)?option(s)?|profile|cpu|maxusers)[[:space:]]/ {
    s/[[:space:]]*#.*$//
    p
}
