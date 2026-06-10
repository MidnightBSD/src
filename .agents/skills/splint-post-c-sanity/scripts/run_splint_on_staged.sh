#!/bin/sh
set -eu

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

die() { echo "error: $*" >&2; exit 2; }

if ! command -v splint >/dev/null 2>&1; then
  die "splint not found in PATH"
fi

if ! command -v rg >/dev/null 2>&1; then
  die "rg (ripgrep) not found in PATH"
fi

tmp_files="${TMPDIR:-/tmp}/splint-files.$$.txt"
tmp_out="${TMPDIR:-/tmp}/splint.$$.txt"
trap 'rm -f "$tmp_files" "$tmp_out"' EXIT

git diff --cached --name-only --diff-filter=ACMR -- '*.c' |
  rg -N '\\.c$' |
  rg -v '^sys/' >"$tmp_files" || true

if ! [ -s "$tmp_files" ]; then
  echo "No staged non-kernel *.c files; skipping splint."
  exit 0
fi

echo "Running splint on staged .c files..."
: >"$tmp_out"

while IFS= read -r f; do
  [ -f "$f" ] || continue
  echo "== $f ==" >>"$tmp_out"
  splint \
    -standard \
    +boundsread +boundswrite +likelybounds \
    +nullpass +nullret \
    +bufferoverflowhigh \
    +branchstate +evalorder \
    +mustfreefresh +freshtrans +onlytrans +temptrans \
    +usereleased \
    +compdestroy \
    -showcolumn \
    -I. -Ilibmport -Ilibexec \
    "$f" >>"$tmp_out" 2>&1 || true
done <"$tmp_files"

# Splint output isn't always labeled as warning/error; fail if it reports any file diagnostics.
# Heuristic: ignore file headers and blank lines; treat remaining lines as findings.
findings="$(awk '
  NF == 0 { next }
  /^== / { next }
  { printf "%d:%s\n", NR, $0 }
' "$tmp_out")"

if [ -n "$findings" ]; then
  echo
  echo "splint findings:"
  echo "$findings"
  echo
  echo "Full splint output saved at: $tmp_out"
  exit 1
fi

echo "splint: clean."
