#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/zfs/zpool/add/doesnt_exist.t 185029 2008-11-17 20:49:29Z pjd $

dir=`dirname $0`
. ${dir}/../../misc.sh

echo "1..6"

disks_create 1
names_create 1

expect_fl ${ZPOOL} add ${name0} ${disk0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

expect_fl ${ZPOOL} add -f ${name0} ${disk0}
expect_fl ${ZPOOL} status -x ${name0}
expect_fl ${ZPOOL} destroy ${name0}

disks_destroy
