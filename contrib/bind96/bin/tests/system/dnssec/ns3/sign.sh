#!/bin/sh -e
#
# Copyright (C) 2004, 2006-2011  Internet Systems Consortium, Inc. ("ISC")
# Copyright (C) 2000-2002  Internet Software Consortium.
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

zone=secure.example.
infile=secure.example.db.in
zonefile=secure.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null

zone=bogus.example.
infile=bogus.example.db.in
zonefile=bogus.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null

zone=dynamic.example.
infile=dynamic.example.db.in
zonefile=dynamic.example.db

keyname1=`$KEYGEN -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`
keyname2=`$KEYGEN -r $RANDFILE -a RSAMD5 -b 1024 -n zone -f KSK $zone`

cat $infile $keyname1.key $keyname2.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null

zone=keyless.example.
infile=keyless.example.db.in
zonefile=keyless.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null

# Change the signer field of the a.b.keyless.example SIG A
# to point to a provably nonexistent KEY record.
mv $zonefile.signed $zonefile.tmp
<$zonefile.tmp perl -p -e 's/ keyless.example/ b.keyless.example/
    if /^a.b.keyless.example/../NXT/;' >$zonefile.signed
rm -f $zonefile.tmp

#
#  NSEC3/NSEC test zone
#
zone=secure.nsec3.example.
infile=secure.nsec3.example.db.in
zonefile=secure.nsec3.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null

#
#  NSEC3/NSEC3 test zone
#
zone=nsec3.nsec3.example.
infile=nsec3.nsec3.example.db.in
zonefile=nsec3.nsec3.example.db

keyname=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -r $RANDFILE -o $zone $zonefile > /dev/null

#
#  OPTOUT/NSEC3 test zone
#
zone=optout.nsec3.example.
infile=optout.nsec3.example.db.in
zonefile=optout.nsec3.example.db

keyname=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -A -r $RANDFILE -o $zone $zonefile > /dev/null

#
# A nsec3 zone (non-optout).
#
zone=nsec3.example.
infile=nsec3.example.db.in
zonefile=nsec3.example.db

keyname=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -g -3 - -r $RANDFILE -o $zone $zonefile > /dev/null

#
#  OPTOUT/NSEC test zone
#
zone=secure.optout.example.
infile=secure.optout.example.db.in
zonefile=secure.optout.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSAMD5 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null

#
#  OPTOUT/NSEC3 test zone
#
zone=nsec3.optout.example.
infile=nsec3.optout.example.db.in
zonefile=nsec3.optout.example.db

keyname=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -r $RANDFILE -o $zone $zonefile > /dev/null

#
#  OPTOUT/OPTOUT test zone
#
zone=optout.optout.example.
infile=optout.optout.example.db.in
zonefile=optout.optout.example.db

keyname=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -A -r $RANDFILE -o $zone $zonefile > /dev/null

#
# A optout nsec3 zone.
#
zone=optout.example.
infile=optout.example.db.in
zonefile=optout.example.db

keyname=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -g -3 - -A -r $RANDFILE -o $zone $zonefile > /dev/null

#
# A nsec3 zone (non-optout) with unknown hash algorithm.
#
zone=nsec3-unknown.example.
infile=nsec3-unknown.example.db.in
zonefile=nsec3-unknown.example.db

keyname=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -U -r $RANDFILE -o $zone $zonefile > /dev/null

#
# A optout nsec3 zone.
#
zone=optout-unknown.example.
infile=optout-unknown.example.db.in
zonefile=optout-unknown.example.db

keyname=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -3 - -U -A -r $RANDFILE -o $zone $zonefile > /dev/null

#
# A multiple parameter nsec3 zone.
#
zone=multiple.example.
infile=multiple.example.db.in
zonefile=multiple.example.db

keyname=`$KEYGEN -r $RANDFILE -a NSEC3RSASHA1 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null
mv $zonefile.signed $zonefile
$SIGNER -P -3 - -r $RANDFILE -o $zone $zonefile > /dev/null
mv $zonefile.signed $zonefile
$SIGNER -P -3 AAAA -r $RANDFILE -o $zone $zonefile > /dev/null
mv $zonefile.signed $zonefile
$SIGNER -P -3 BBBB -r $RANDFILE -o $zone $zonefile > /dev/null
mv $zonefile.signed $zonefile
$SIGNER -P -3 CCCC -r $RANDFILE -o $zone $zonefile > /dev/null
mv $zonefile.signed $zonefile
$SIGNER -P -3 DDDD -r $RANDFILE -o $zone $zonefile > /dev/null

#
# A RSASHA256 zone.
#
zone=rsasha256.example.
infile=rsasha256.example.db.in
zonefile=rsasha256.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSASHA256 -b 768 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null

#
# A RSASHA512 zone.
#
zone=rsasha512.example.
infile=rsasha512.example.db.in
zonefile=rsasha512.example.db

keyname=`$KEYGEN -r $RANDFILE -a RSASHA512 -b 1024 -n zone $zone`

cat $infile $keyname.key >$zonefile

$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null

#
# A zone with the expired signatures
#
zone=expired.example.
infile=expired.example.db.in
zonefile=expired.example.db

kskname=`$KEYGEN -r $RANDFILE -a RSASHA512 -b 1024 -n zone -f KSK $zone`
zskname=`$KEYGEN -r $RANDFILE -a RSASHA512 -b 1024 -n zone $zone`
cat $infile $kskname.key $zskname.key >$zonefile
$SIGNER -P -r $RANDFILE -o $zone -s +-10800 -e +3600 $zonefile > /dev/null 2>&1
rm -f $kskname.* $zskname.*

#
# A NSEC3 signed zone that will have a DNSKEY added to it via UPDATE.
#
zone=update-nsec3.example.
infile=update-nsec3.example.db.in
zonefile=update-nsec3.example.db

kskname=`$KEYGEN -r $RANDFILE -a RSASHA512 -b 1024 -n zone -f KSK $zone`
zskname=`$KEYGEN -r $RANDFILE -a RSASHA512 -b 1024 -n zone $zone`
cat $infile $kskname.key $zskname.key >$zonefile
$SIGNER -P -3 - -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1

#
# Secure below cname test zone.
#
zone=secure.below-cname.example.
infile=secure.below-cname.example.db.in
zonefile=secure.below-cname.example.db
keyname=`$KEYGEN -r $RANDFILE -a RSASHA1 -b 1024 -n zone $zone`
cat $infile $keyname.key >$zonefile
$SIGNER -P -r $RANDFILE -o $zone $zonefile > /dev/null 2>&1
