#!/bin/sh
#
# qrun.sh <tag> <disk> <log> [snapshot]
#
# Legacy BIOS boot of <disk> in QEMU (SeaBIOS, i440FX, IDE disk) with the
# guest's serial console written to <log>.  QEMU exits when the guest powers
# off or reboots (-no-reboot).  With "snapshot", writes go to a temporary
# overlay and <disk> is left unchanged.
#
# Environment:
#	QTMO	timeout in seconds (default: 900)
#	MEM	guest memory (default: 2G)
#	CPUS	guest CPUs (default: 2)

set -u

if [ $# -lt 3 ]; then
	echo "usage: $0 tag disk log [snapshot]" >&2
	exit 64
fi

tag=$1
disk=$2
log=$3
snap=""
[ "${4:-}" = snapshot ] && snap="-snapshot"

QTMO=${QTMO:-900}
MEM=${MEM:-2G}
CPUS=${CPUS:-2}

if [ ! -f "${disk}" ]; then
	echo "qrun: no disk image ${disk}" >&2
	exit 1
fi

: > "${log}"
timeout "${QTMO}" qemu-system-x86_64 -machine pc -m "${MEM}" -smp "${CPUS}" \
    ${snap} -drive file="${disk}",format=raw,if=ide \
    -display none -monitor none -serial file:"${log}" -no-reboot \
    < /dev/null > "${log}.qemu" 2>&1
rc=$?
echo "### qemu rc=${rc} $(date)" >> "${log}"
echo "RESULT ${tag} qemu rc=${rc}"
exit 0
