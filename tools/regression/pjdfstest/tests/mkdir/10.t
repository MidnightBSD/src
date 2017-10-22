#!/bin/sh
# $FreeBSD: release/10.0.0/tools/regression/pjdfstest/tests/mkdir/10.t 211186 2010-08-11 17:34:58Z pjd $

desc="mkdir returns EEXIST if the named file exists"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..21"

n0=`namegen`

for type in regular dir fifo block char socket symlink; do
	create_file ${type} ${n0}
	expect EEXIST mkdir ${n0} 0755
	if [ "${type}" = "dir" ]; then
		expect 0 rmdir ${n0}
	else
		expect 0 unlink ${n0}
	fi
done
