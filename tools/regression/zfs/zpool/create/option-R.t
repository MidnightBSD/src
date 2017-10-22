#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/zfs/zpool/create/option-R.t 185029 2008-11-17 20:49:29Z pjd $

dir=`dirname $0`
. ${dir}/../../misc.sh

echo "1..9"

disks_create 1
names_create 2

expect_fl is_mountpoint /${name0}
expect_fl is_mountpoint /${name1}
expect_ok ${ZPOOL} create -R /${name1} ${name0} ${disk0}
exp=`(
  echo "NAME      PROPERTY  VALUE      SOURCE"
  echo "${name0}  altroot   /${name1}  local"
)`
expect "${exp}" ${ZPOOL} get altroot ${name0}
expect_fl is_mountpoint /${name0}
if [ -z "${no_mountpoint}" ]; then
	expect_ok is_mountpoint /${name1}
else
	expect_fl is_mountpoint /${name1}
fi
expect_ok ${ZPOOL} destroy ${name0}
expect_fl is_mountpoint /${name0}
expect_fl is_mountpoint /${name1}

disks_destroy
