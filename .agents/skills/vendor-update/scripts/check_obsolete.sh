#!/bin/sh
#
# Report ObsoleteFiles.inc entries that installworld still installs.
#
# make check-old lists every OLD_FILES/OLD_DIRS entry that exists on the
# system, so an entry for something installworld puts back is reported
# after every upgrade.  This checks:
#
#   - OLD_DIRS against the directories the etc/mtree specs create (exact)
#   - OpenSSL man page OLD_FILES against the libcrypto/openssl Makefiles
#     (exact; they install under /usr/share/openssl/man)
#   - libc++ header OLD_FILES against lib/libc++/Makefile (exact)
#   - other man page OLD_FILES against every MAN/MLINKS line in the tree
#     (by name and section only; review each hit by hand)
#
# Usage: check_obsolete.sh [srctop]
# Exits 1 if any exact overlap is found.

set -u

SRCTOP=${1:-$(git rev-parse --show-toplevel 2>/dev/null || pwd)}
OBS=${SRCTOP}/ObsoleteFiles.inc
MTREE=${MTREE:-/usr/sbin/mtree}

if [ ! -f "${OBS}" ]; then
	echo "check_obsolete: ${OBS} not found" >&2
	exit 2
fi

TMP=$(mktemp -d "${TMPDIR:-/tmp}/check_obsolete.XXXXXX") || exit 2
trap 'rm -rf "${TMP}"' EXIT INT TERM
status=0

# Print the OLD_FILES or OLD_DIRS entries, one path per line, sorted.
old_entries() {
	sed -n "s/^[[:space:]]*$1+=[[:space:]]*//p" "${OBS}" |
	    sed 's/[[:space:]]*$//' | sort -u
}

# Print the lines of a sorted list that also appear in another.
report() {
	title=$1
	hits=$(comm -12 "$2" "$3")
	if [ -n "${hits}" ]; then
		echo "==> ${title}"
		echo "${hits}"
		echo
		status=1
	fi
}

old_entries OLD_FILES > "${TMP}/old_files"
old_entries OLD_DIRS > "${TMP}/old_dirs"

#
# Directories.  Apply every spec where etc/Makefile's distrib-dirs does,
# then list what was created.
#
if [ -x "${MTREE}" ]; then
	root=${TMP}/root
	while read -r spec dest; do
		[ -f "${SRCTOP}/etc/mtree/${spec}" ] || continue
		mkdir -p "${root}${dest}"
		if ! "${MTREE}" -deUW -f "${SRCTOP}/etc/mtree/${spec}" \
		    -p "${root}${dest}" >/dev/null 2>"${TMP}/mtree.err"; then
			echo "check_obsolete: mtree failed on ${spec}:" >&2
			cat "${TMP}/mtree.err" >&2
			status=1
		fi
	done <<EOF
BSD.root.dist /
BSD.var.dist /var
BSD.usr.dist /usr
BSD.include.dist /usr/include
BSD.debug.dist /usr/lib
BSD.lib32.dist /usr
BSD.lib32.dist /usr/lib/debug/usr
BSD.libsoft.dist /usr
BSD.libsoft.dist /usr/lib/debug/usr
BSD.tests.dist /usr/tests
BSD.tests.dist /usr/lib/debug/usr/tests
BSD.sendmail.dist /
EOF
	(cd "${root}" && find . -type d) | sed -n 's|^\./||p' | sort -u \
	    > "${TMP}/mtree_dirs"
	report "OLD_DIRS still created by etc/mtree (drop from the spec)" \
	    "${TMP}/mtree_dirs" "${TMP}/old_dirs"
else
	echo "check_obsolete: ${MTREE} not found; skipping the OLD_DIRS check" >&2
fi

#
# Man pages.  Print "<name>.<section>" for every page a Makefile installs
# through MAN, MANn or MLINKS, following backslash continuations.
#
MAN_AWK='
FNR == 1 { cont = 0 }
{
	line = $0
	if (!cont && line !~ /^[ \t]*(MAN[0-9]*|MLINKS)[ \t]*[+?:]?=/)
		next
	cont = (line ~ /\\$/)
	if (line ~ /^[ \t]*(MAN[0-9]*|MLINKS)[ \t]*[+?:]?=/)
		sub(/^[^=]*=/, "", line)
	n = split(line, w, /[ \t\\]+/)
	for (i = 1; i <= n; i++)
		if (w[i] ~ /^[^$\/]+\.[1-9n][a-z]*$/)
			print w[i]
}'

# OpenSSL: MANDIR is ${SHAREDIR}/openssl/man/man.
for d in secure/lib/libcrypto/man/man3 secure/lib/libcrypto/man/man5 \
    secure/lib/libcrypto/man/man7 secure/usr.bin/openssl; do
	for mk in "${SRCTOP}/${d}"/Makefile*; do
		[ -f "${mk}" ] && awk "${MAN_AWK}" "${mk}"
	done
done | awk '{ s = $0; sub(/^.*\./, "", s); s = substr(s, 1, 1)
	print "usr/share/openssl/man/man" s "/" $0 ".gz" }' | sort -u \
    > "${TMP}/openssl_man"
report "OpenSSL man pages still installed (drop from OLD_FILES)" \
    "${TMP}/openssl_man" "${TMP}/old_files"

# libc++: <GROUP>DIR= ${CXXINCLUDEDIR}/sub and <GROUP>_HEADERS+= name.
if [ -f "${SRCTOP}/lib/libc++/Makefile" ]; then
	awk '
	$1 ~ /^[A-Z]+DIR=$/ && $2 ~ /^\$\{CXXINCLUDEDIR\}/ {
		g = substr($1, 1, length($1) - 4)
		d = $2
		sub(/^\$\{CXXINCLUDEDIR\}/, "usr/include/c++/v1", d)
		dir[g] = d
	}
	$1 ~ /^[A-Z]+_HEADERS\+=$/ {
		g = substr($1, 1, length($1) - 10)
		if (g in dir)
			print dir[g] "/" $2
	}' "${SRCTOP}/lib/libc++/Makefile" | sort -u > "${TMP}/cxx_headers"
	report "libc++ headers still installed (drop from OLD_FILES)" \
	    "${TMP}/cxx_headers" "${TMP}/old_files"
fi

# Everything else under /usr/share/man, matched by name and section.
# contrib/, crypto/ and secure/ are skipped (contrib Makefiles are not ours,
# and secure/ installs OpenSSL pages elsewhere), as are Makefiles that set
# MANSUBDIR, since those pages install under manN/<arch>.  Pages from
# directories that are not built still show up, so these are only
# candidates.
(cd "${SRCTOP}" && git ls-files) | grep -E '(^|/)Makefile[^/]*$' |
    grep -Ev '^(contrib|crypto|secure)/' |
    (cd "${SRCTOP}" && xargs awk '
	FNR == 1 { if (f != "" && !m) print f; f = FILENAME; m = 0 }
	/^MANSUBDIR/ { m = 1 }
	END { if (f != "" && !m) print f }') > "${TMP}/makefiles"
(cd "${SRCTOP}" && xargs awk "${MAN_AWK}" < "${TMP}/makefiles") |
    sort -u > "${TMP}/man_names"
sed -n 's|^usr/share/man/man[1-9n]/\([^/]*\)\.gz$|\1|p' "${TMP}/old_files" |
    sort -u > "${TMP}/old_man_names"
cand=$(comm -12 "${TMP}/man_names" "${TMP}/old_man_names")
if [ -n "${cand}" ]; then
	echo "==> Other man pages to review (a page with this name and section is"
	echo "    installed somewhere; check MANDIR and whether it is built)"
	for m in ${cand}; do
		s=${m##*.}
		s=$(printf '%.1s' "${s}")
		grep -n "^[[:space:]]*OLD_FILES+=usr/share/man/man${s}/${m}\.gz" \
		    "${OBS}" | sed 's/^/    ObsoleteFiles.inc:/'
	done
	echo
fi

if [ ${status} -eq 0 ]; then
	echo "check_obsolete: no exact overlaps found"
fi
exit ${status}
