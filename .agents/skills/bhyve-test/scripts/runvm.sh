#!/bin/sh
#
# runvm.sh <vmname> <userboot|olduserboot|uefi> <timeout> <log> <disk1> [disk2]
#
# Boot disk1 (and optionally disk2) in bhyve with the chosen loader and send
# the serial console to <log>.  Non-interactive: stdin is a FIFO that never
# delivers input.
#
# Environment:
#	NEWUB	new userboot.so (default: from OBJ)
#	OBJ	stand objdir root, e.g. /tmp/obj/<srcdir>/amd64.amd64
#	FW	UEFI firmware (default: /usr/local/share/uefi-firmware/BHYVE_UEFI.fd)
#	MEM	guest memory (default: 4G)
#	CPUS	guest CPUs (default: 4)
#	LDRTMO	loader timeout in seconds (default: 180)

set -u

if [ $# -lt 5 ]; then
	echo "usage: $0 vmname userboot|olduserboot|uefi timeout log disk1 [disk2]" >&2
	exit 64
fi

name=$1
ldr=$2
tmo=$3
log=$4
d1=$5
d2=${6:-}

NEWUB=${NEWUB:-${OBJ:-}/stand/userboot/userboot_lua/userboot_lua.so}
OLDUB=/boot/userboot.so
FW=${FW:-/usr/local/share/uefi-firmware/BHYVE_UEFI.fd}
MEM=${MEM:-4G}
CPUS=${CPUS:-4}
LDRTMO=${LDRTMO:-180}

fifo=$(mktemp -u "${TMPDIR:-/tmp}/runvm.XXXXXX")
mkfifo "${fifo}" || exit 1
# Hold the FIFO open without ever writing to it.
sleep 100000 > "${fifo}" &
sp=$!

cleanup()
{
	bhyvectl --destroy --vm="${name}" > /dev/null 2>&1
	kill "${sp}" 2> /dev/null
	rm -f "${fifo}"
}
trap cleanup EXIT

disks="-s 4,ahci-hd,${d1}"
[ -n "${d2}" ] && disks="${disks} -s 5,ahci-hd,${d2}"

bhyvectl --destroy --vm="${name}" > /dev/null 2>&1
: > "${log}"
echo "### runvm ${name} loader=${ldr} d1=${d1} d2=${d2} $(date)" >> "${log}"

boot=""
case ${ldr} in
userboot|olduserboot)
	ub=${NEWUB}
	[ "${ldr}" = olduserboot ] && ub=${OLDUB}
	if [ ! -f "${ub}" ]; then
		echo "runvm: no userboot at ${ub}" >&2
		exit 1
	fi
	echo "### bhyveload -l ${ub}" >> "${log}"
	dl="-d ${d1}"
	[ -n "${d2}" ] && dl="${dl} -d ${d2}"
	timeout "${LDRTMO}" bhyveload -l "${ub}" -m "${MEM}" ${dl} -c stdio \
	    "${name}" < "${fifo}" >> "${log}" 2>&1
	lrc=$?
	echo "### bhyveload rc=${lrc}" >> "${log}"
	if [ ${lrc} -ne 0 ]; then
		echo "RESULT loader-failed rc=${lrc}"
		exit 10
	fi
	;;
uefi)
	boot="-l bootrom,${FW}"
	;;
*)
	echo "runvm: unknown loader ${ldr}" >&2
	exit 64
	;;
esac

# bhyve resets the descriptor flags (drops O_APPEND), so log separately.
timeout "${tmo}" bhyve -c "${CPUS}" -m "${MEM}" -H -A -P -s 0,hostbridge \
    ${disks} -s 31,lpc -l com1,stdio ${boot} "${name}" \
    < "${fifo}" > "${log}.vm" 2>&1
brc=$?
cat "${log}.vm" >> "${log}" && rm -f "${log}.vm"
echo "### bhyve rc=${brc} $(date)" >> "${log}"
echo "RESULT bhyve rc=${brc}"
exit 0
