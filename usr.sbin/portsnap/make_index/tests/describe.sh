#!/bin/sh
# Run as: sh describe.sh /path/to/make_index
set -eu
make_index=${1:?path to make_index required}
tmp=$(mktemp -d)
trap 'rm -r "$tmp"' EXIT

row()
{
	printf '%s|/ports/%s|/usr/local|comment|/descr|maintainer|devel||||%s|%s|https://example.org' \
	    "$1" "$2" "$3" "$4"
	if [ "$#" -eq 6 ]; then
		printf '|%s|%s' "$5" "$6"
	fi
	printf '\n'
}

{
	row alternate provider '' /ports/leaf alternate normal
	row default provider '' '' normal normal
	row leaf leaf '' '' '' ''
	row consumer consumer '/ports/provider /ports/provider@alternate' /ports/provider@normal
	row a a '' /ports/b default default
	row b b '' /ports/a@default
	# Grow the runtime dependency list before following its self-edge.
	row self self '' '/ports/provider@alternate /ports/self'
} > "$tmp/describe"

"$make_index" /dev/stdin < "$tmp/describe" > "$tmp/index" 2> "$tmp/err"
grep -q 'Breaking dependency cycle' "$tmp/err"
awk -F'|' '
	NF != 13 { bad = 1 }
	$1 == "consumer" && $8 == "alternate default leaf" && $9 == "default" {
		consumer = 1
	}
	$1 == "alternate" && $2 == "/ports/provider" && $9 == "leaf" {
		alternate = 1
	}
	$1 == "a" && $9 ~ /(^| )b( |$)/ { a = 1 }
	$1 == "b" && $9 ~ /(^| )a( |$)/ { b = 1 }
	$1 == "self" && $9 == "alternate leaf self" { self = 1 }
	END { exit !(NR == 7 && !bad && consumer && alternate && a && b && self) }
' "$tmp/index"

reject()
{
	if "$make_index" "$tmp/bad" > "$tmp/index" 2> "$tmp/err"; then
		echo "Unexpectedly accepted invalid describe input" >&2
		exit 1
	fi
	grep -Fq "$1" "$tmp/err"
	test ! -s "$tmp/index"
}

cp "$tmp/describe" "$tmp/bad"
row missing missing '' /ports/provider@missing >> "$tmp/bad"
reject 'no entry for /ports/provider@missing'

{
	row alternate provider '' '' alternate normal
	row consumer consumer '' /ports/provider
} > "$tmp/bad"
reject 'no entry for /ports/provider'

{
	cat "$tmp/describe"
	row duplicate provider '' '' normal normal
} > "$tmp/bad"
reject 'duplicate origin: /ports/provider'

row malformed malformed '' '' | sed 's/$/|/' > "$tmp/bad"
reject 'Port describe line is corrupt'

{
	row legacy-one legacy '' ''
	row legacy-two legacy '' ''
	row consumer consumer '' /ports/legacy
} > "$tmp/legacy"
"$make_index" "$tmp/legacy" > "$tmp/index"
awk -F'|' '
	NF != 13 { bad = 1 }
	$1 == "consumer" && $9 == "legacy-two" { consumer = 1 }
	END { exit !(NR == 3 && !bad && consumer) }
' "$tmp/index"

"$make_index" /dev/null > "$tmp/index"
test ! -s "$tmp/index"
