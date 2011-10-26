#!/bin/sh -e
#
# Copyright (C) 2004, 2006-2011  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000-2003  Internet Software Consortium.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
# REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
# AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
# LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
# OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
# PERFORMANCE OF THIS SOFTWARE.

# $Id: sign.sh,v 1.1.1.2 2011-10-26 11:58:38 laffer1 Exp $

SYSTEMTESTTOP=../..
. $SYSTEMTESTTOP/conf.sh

RANDFILE=../random.data

zone=.
infile=root.db.in
zonefile=root.db

(cd ../ns2 && sh sign.sh )

cp ../ns2/dsset-example. .
cp ../ns2/dsset-dlv. .
grep "8 [12] " ../ns2/dsset-algroll. > dsset-algroll.

keyname=`$KEYGEN -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`

cat $infile $keyname.key dsset-example. dsset-dlv. dsset-algroll. > $zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null

# Configure the resolving server with a trusted key.

cat $keyname.key | $PERL -n -e '
local ($dn, $class, $type, $flags, $proto, $alg, @rest) = split;
local $key = join("", @rest);
print <<EOF
trusted-keys {
    "$dn" $flags $proto $alg "$key";
};
EOF
' > trusted.conf
cp trusted.conf ../ns2/trusted.conf
cp trusted.conf ../ns3/trusted.conf
cp trusted.conf ../ns4/trusted.conf
cp trusted.conf ../ns6/trusted.conf
cp trusted.conf ../ns7/trusted.conf
