#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

die() { echo "error: $*" >&2; exit 2; }

if ! command -v splint >/dev/null 2>&1; then
  die "splint not found in PATH"
fi

mapfile -t staged_c_files < <(git diff --cached --name-only --diff-filter=ACMR -- '*.c' ':!external/*')
if ((${#staged_c_files[@]} == 0)); then
  echo "No staged *.c files; skipping splint."
  exit 0
fi

INCLUDE_DIRS=(
  -I.
  -Ilibmport
  -Ilibexec
  -Iexternal/tllist
  -I/usr/include/private/ucl
  -I/usr/include/private/zstd
)

# Focus on memory/ownership, API misuse, and basic contracts; avoid ultra-strict noise.
SPLINT_FLAGS=(
  -standard
  +posixlib -warnposixheaders
  "-D__aligned(x)="
  "-D_Alignof(x)=sizeof(x)"
  "-D__va_list=void *"
  +boundsread +boundswrite +likelybounds
  +nullpass +nullret
  +bufferoverflowhigh
  +branchstate +evalorder
  +mustfreefresh +freshtrans +onlytrans +temptrans
  +usereleased
  +compdestroy
  -showcolumn
)

tmp_out="${TMPDIR:-/tmp}/splint.$$.txt"
keep_tmp=0
trap 'if [[ "$keep_tmp" -eq 0 ]]; then rm -f "$tmp_out"; fi' EXIT

printf 'Running splint on %d staged .c file(s)...\n' "${#staged_c_files[@]}"
: >"$tmp_out"

for f in "${staged_c_files[@]}"; do
  [[ -f "$f" ]] || continue
  echo "== $f ==" >>"$tmp_out"
  splint "${SPLINT_FLAGS[@]}" "${INCLUDE_DIRS[@]}" "$f" >>"$tmp_out" 2>&1 || true
done

# Splint output isn't always labeled as warning/error; fail if it reports any file diagnostics.
# Heuristic: ignore file headers and blank lines; treat remaining lines as findings.
findings="$(
  awk '
    NF == 0 { next }
    /^== / { next }
    /^Splint [0-9]/ { next }
    /^Command Line: Setting .* redundant with current value$/ { next }
    { printf "%d:%s\n", NR, $0 }
  ' "$tmp_out"
)"

if [[ -n "$findings" ]]; then
  keep_tmp=1
  echo
  echo "splint findings:"
  echo "$findings"
  echo
  echo "Full splint output saved at: $tmp_out"
  exit 1
fi

echo "splint: clean."
