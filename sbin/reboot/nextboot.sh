#! /bin/sh
#
# Copyright 2002. Gordon Tetlow.
# gordon@FreeBSD.org
#
# $FreeBSD$

delete="NO"
force="NO"
nextboot_file="/boot/nextboot.conf"

display_usage() {
	echo "Usage: nextboot [-f] [-o options] -k kernel"
	echo "       nextboot -D"
}

while getopts "Dfk:o:" argument ; do
	case "${argument}" in
	D)
		delete="YES"
		;;
	f)
		force="YES"
		;;
	k)
		kernel="${OPTARG}"
		;;
	o)
		kernel_options="${OPTARG}"
		;;
	*)
		display_usage
		exit 1
		;;
	esac
done

if [ ${delete} = "YES" ]; then
	rm -f ${nextboot_file}
	exit 0
fi

if [ "xxx${kernel}" = "xxx" ]; then
	display_usage
	exit 1
fi

if [ ${force} = "NO" -a ! -d /boot/${kernel} ]; then
	echo "Error: /boot/${kernel} doesn't exist. Use -f to override."
	exit 1
fi

df -Tn "/boot/" 2>/dev/null | while read _fs _type _other ; do
	[ "zfs" = "${_type}" ] || continue
	cat 1>&2 <<-EOF
		WARNING: loader(8) has only R/O support for ZFS
		nextboot.conf will NOT be reset in case of kernel boot failure
	EOF
done

cat > ${nextboot_file} << EOF
nextboot_enable="YES"
kernel="${kernel}"
kernel_options="${kernel_options}"
EOF
