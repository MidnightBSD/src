#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

die() { echo "error: $*" >&2; exit 2; }

if ! command -v splint >/dev/null 2>&1; then
  die "splint not found in PATH"
fi

if ! command -v rg >/dev/null 2>&1; then
  die "rg (ripgrep) not found in PATH"
fi

mapfile -t staged_c_files < <(git diff --cached --name-only --diff-filter=ACMR -- '*.c' | rg -N '\\.c$' || true)
if ((${#staged_c_files[@]} == 0)); then
  echo "No staged *.c files; skipping splint."
  exit 0
fi

INCLUDE_DIRS=(-I. -Ilibmport -Ilibexec)

# Focus on memory/ownership, API misuse, and basic contracts; avoid ultra-strict noise.
SPLINT_FLAGS=(
  -standard
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
trap 'rm -f "$tmp_out"' EXIT

echo "Running splint on staged .c files..."
: >"$tmp_out"

for f in "${staged_c_files[@]}"; do
  [[ -f "$f" ]] || continue
  echo "== $f ==" >>"$tmp_out"
  splint "${SPLINT_FLAGS[@]}" "${INCLUDE_DIRS[@]}" "$f" >>"$tmp_out" 2>&1 || true
done

# Splint output isn't always labeled as warning/error; fail if it reports any file diagnostics.
# Heuristic: ignore file headers and blank lines; treat remaining lines as findings.
findings="$(rg -n '^(?!== ).\\S' "$tmp_out" || true)"

if [[ -n "$findings" ]]; then
  echo
  echo "splint findings:"
  echo "$findings"
  echo
  echo "Full splint output saved at: $tmp_out"
  exit 1
fi

echo "splint: clean."

