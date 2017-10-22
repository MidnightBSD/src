#!/bin/sh
# $FreeBSD: stable/9/tools/regression/zfs/zpool/create/already_exists.t 185029 2008-11-17 20:49:29Z pjd $

dir=`dirname $0`
. ${dir}/../../misc.sh

echo "1..5"

disks_create 2
names_create 1

expect_ok ${ZPOOL} create ${name0} ${disk0}
exp=`(
  echo "  pool: ${name0}"
  echo " state: ONLINE"
  echo " scrub: none requested"
  echo "config:"
  echo "	NAME        STATE   READ WRITE CKSUM"
  echo "	${name0}    ONLINE     0     0     0"
  echo "	  ${disk0}  ONLINE     0     0     0"
  echo "errors: No known data errors"
)`
expect "${exp}" ${ZPOOL} status ${name0}
expect_fl ${ZPOOL} create ${name0} ${disk1}
expect "${exp}" ${ZPOOL} status ${name0}
expect_ok ${ZPOOL} destroy ${name0}

disks_destroy
