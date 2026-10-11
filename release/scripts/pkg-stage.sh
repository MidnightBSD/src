#!/bin/sh
#
# SPDX-License-Identifier: BSD-2-Clause
#
# Stage a subset of a blessed mport repository on installation media.
#

set -eu

ROOTDIR=${ROOTDIR:-"$PWD/dvd"}
MPORT_REPOSITORY=${MPORT_REPOSITORY:-"/usr/mports/Packages/$(uname -p)/All"}
DVD_PACKAGE_ROOTS=${DVD_PACKAGE_ROOTS:-"midnightbsd-desktop bash zsh sudo screen tmux nano vim rsync git"}

INDEX=${MPORT_REPOSITORY}/index.db
PACKAGE_DEST=${ROOTDIR}/packages
TAB=$(printf '\t')

fail()
{
	echo "pkg-stage: $*" >&2
	exit 1
}

validate_atom()
{
	case "$1" in
	""|*[!A-Za-z0-9_.,+~:-]*)
		fail "unsafe package name or version: $1"
		;;
	esac
}

case "$ROOTDIR" in
""|/)
	fail "refusing unsafe ROOTDIR: $ROOTDIR"
	;;
esac

[ -r "$INDEX" ] || fail "missing blessed mport index: $INDEX"
[ -n "$DVD_PACKAGE_ROOTS" ] || fail "DVD_PACKAGE_ROOTS is empty"
command -v sqlite3 >/dev/null 2>&1 || fail "sqlite3 is required"
command -v sha256 >/dev/null 2>&1 || fail "sha256 is required"
duplicate=$(sqlite3 -readonly "$INDEX" \
    "SELECT pkg FROM packages GROUP BY pkg HAVING count(*) > 1 LIMIT 1;")
[ -z "$duplicate" ] ||
	fail "package '$duplicate' has multiple entries in the blessed index"

workdir=$(mktemp -d -t pkg-stage.XXXXXX)
trap 'rm -rf "$workdir"' EXIT HUP INT TERM
roots=${workdir}/roots
closure_raw=${workdir}/closure.raw
closure=${workdir}/closure
: > "$roots"
: > "$closure_raw"

for root in $DVD_PACKAGE_ROOTS; do
	validate_atom "$root"
	count=$(sqlite3 -readonly "$INDEX" \
	    "SELECT count(*) FROM packages WHERE pkg='$root' AND type=0;")
	[ "$count" -eq 1 ] ||
		fail "application root '$root' has $count matching index entries"

	row=$(sqlite3 -readonly -noheader -tabs "$INDEX" \
	    "SELECT pkg, version,
	    coalesce(nullif(replace(replace(replace(comment, char(9), ' '),
	    char(10), ' '), char(13), ' '), ''), pkg)
	    FROM packages WHERE pkg='$root' AND type=0;")
	state=off
	[ "$root" = midnightbsd-desktop ] && state=on
	printf '%s\t%s\n' "$row" "$state" >> "$roots"

	version=$(printf '%s\n' "$row" | awk -F "$TAB" '{ print $2 }')
	validate_atom "$version"
	sql="WITH RECURSIVE closure(pkg, version) AS (VALUES('$root', '$version') UNION SELECT d.d_pkg, d.d_version FROM depends AS d JOIN closure AS c ON d.pkg=c.pkg AND d.version=c.version) SELECT pkg, version FROM closure;"
	sqlite3 -readonly -noheader -tabs "$INDEX" "$sql" >> "$closure_raw"
done

sort -u "$closure_raw" > "$closure"

rm -rf "$PACKAGE_DEST"
mkdir -p "$PACKAGE_DEST"
install -m 0644 "$INDEX" "$PACKAGE_DEST/index.db"
install -m 0644 "$roots" "$PACKAGE_DEST/installer.tsv"

sqlite3 -readonly -noheader -tabs "$INDEX" \
    "SELECT pkg, version,
    coalesce(nullif(replace(replace(replace(comment, char(9), ' '),
    char(10), ' '), char(13), ' '), ''), pkg), bundlefile
    FROM packages WHERE type=0 ORDER BY pkg, version;" \
    > "$PACKAGE_DEST/catalog.tsv"
chmod 0644 "$PACKAGE_DEST/catalog.tsv"

while IFS="$TAB" read -r pkg version; do
	validate_atom "$pkg"
	validate_atom "$version"
	count=$(sqlite3 -readonly "$INDEX" \
	    "SELECT count(*) FROM packages WHERE pkg='$pkg' AND version='$version';")
	[ "$count" -eq 1 ] ||
		fail "dependency '$pkg-$version' has $count matching index entries"

	row=$(sqlite3 -readonly -noheader -tabs "$INDEX" \
	    "SELECT bundlefile, hash FROM packages WHERE pkg='$pkg' AND version='$version';")
	bundle=${row%%"$TAB"*}
	hash=${row#*"$TAB"}
	case "$bundle" in
	""|*/*|*[!A-Za-z0-9_.,+~:-]*)
		fail "unsafe bundle filename for '$pkg-$version': $bundle"
		;;
	*.mport)
		;;
	*)
		fail "unexpected bundle filename for '$pkg-$version': $bundle"
		;;
	esac

	source_file=${MPORT_REPOSITORY}/${bundle}
	[ -f "$source_file" ] || fail "missing bundle: $source_file"
	actual_hash=$(sha256 -q "$source_file")
	[ "$actual_hash" = "$hash" ] || fail "hash mismatch for $bundle"
	install -m 0644 "$source_file" "$PACKAGE_DEST/$bundle"
done < "$closure"

package_count=$(wc -l < "$closure" | tr -d ' ')
echo "Staged $package_count packages and the full blessed index in $PACKAGE_DEST"
