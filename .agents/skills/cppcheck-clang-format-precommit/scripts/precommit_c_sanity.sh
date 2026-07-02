#!/bin/sh
set -eu

repo_root="$(git rev-parse --show-toplevel)"
cd "$repo_root"

die() { echo "error: $*" >&2; exit 2; }

if ! command -v cppcheck >/dev/null 2>&1; then
  die "cppcheck not found in PATH"
fi

if ! command -v rg >/dev/null 2>&1; then
  die "rg (ripgrep) not found in PATH"
fi

pick_clang_format() {
  if command -v clang-format19 >/dev/null 2>&1; then
    echo "clang-format19"
    return 0
  fi

  n=30
  while [ "$n" -ge 10 ]; do
    if command -v "clang-format$n" >/dev/null 2>&1; then
      echo "clang-format$n"
      return 0
    fi
    n=$((n - 1))
  done

  if command -v clang-format >/dev/null 2>&1; then
    echo "clang-format"
    return 0
  fi

  die "no clang-format found (expected clang-format19, clang-formatNN, or clang-format)"
}

clang_format_bin="$(pick_clang_format)"

pick_clang_tidy() {
  if command -v clang-tidy19 >/dev/null 2>&1; then
    echo "clang-tidy19"
    return 0
  fi

  if command -v clang-tidy >/dev/null 2>&1; then
    echo "clang-tidy"
    return 0
  fi

  n=30
  while [ "$n" -ge 10 ]; do
    if command -v "clang-tidy$n" >/dev/null 2>&1; then
      echo "clang-tidy$n"
      return 0
    fi
    n=$((n - 1))
  done

  die "no clang-tidy found (expected clang-tidy19, clang-tidyNN, or clang-tidy)"
}

clang_tidy_bin="$(pick_clang_tidy)"

tmp_staged="${TMPDIR:-/tmp}/c-sanity-files.$$.txt"
tmp_candidates="${TMPDIR:-/tmp}/c-sanity-candidates.$$.txt"
tmp_c="${TMPDIR:-/tmp}/c-sanity-c-files.$$.txt"
tmp_tidy="${TMPDIR:-/tmp}/clang-tidy.$$.txt"
tmp_cppcheck="${TMPDIR:-/tmp}/cppcheck.$$.txt"
trap 'rm -f "$tmp_staged" "$tmp_candidates" "$tmp_c" "$tmp_tidy" "$tmp_cppcheck"' EXIT

git diff --cached --name-only --diff-filter=ACMR -- '*.c' '*.h' >"$tmp_candidates" || true
if ! [ -s "$tmp_candidates" ]; then
  echo "No staged *.c/*.h files; skipping clang-format/clang-tidy/cppcheck."
  exit 0
fi

grep -Ev '^(contrib|crypto)/' "$tmp_candidates" >"$tmp_staged" || true
if ! [ -s "$tmp_staged" ]; then
  echo "Only vendor C/C header files under contrib/ or crypto/ are staged; skipping clang-format/clang-tidy/cppcheck."
  exit 0
fi

echo "clang-format: $clang_format_bin"
echo "clang-tidy: $clang_tidy_bin"
echo "Formatting staged files..."

while IFS= read -r f; do
  [ -f "$f" ] || continue
  "$clang_format_bin" -i "$f"
  git add -- "$f"
done <"$tmp_staged"

rg -N '\\.c$' "$tmp_staged" >"$tmp_c" || true

if [ -s "$tmp_c" ]; then
  TIDY_CHECKS='clang-analyzer-*,bugprone-*,cert-*,security-*,-cert-err34-c'
  echo "Running clang-tidy on staged .c files..."
  : > "$tmp_tidy"
  while IFS= read -r f; do
    [ -f "$f" ] || continue
    "$clang_tidy_bin" \
      -checks="$TIDY_CHECKS" \
      -warnings-as-errors='clang-analyzer-*,cert-*,security-*' \
      -quiet \
      "$f" -- -std=c11 -I. -Ilibmport -Ilibexec \
      >>"$tmp_tidy" 2>&1 || true
  done <"$tmp_c"

  tidy_errors="$(rg -n '(^|: )error:' "$tmp_tidy" || true)"
  tidy_warnings="$(rg -n '(^|: )warning:' "$tmp_tidy" || true)"
  if [ -n "$tidy_errors" ] || [ -n "$tidy_warnings" ]; then
    echo
    echo "clang-tidy findings (errors/warnings):"
    if [ -n "$tidy_errors" ]; then
      echo "$tidy_errors"
    fi
    if [ -n "$tidy_warnings" ]; then
      echo "$tidy_warnings"
    fi
    echo
    echo "Full clang-tidy output saved at: $tmp_tidy"
    exit 1
  fi

  echo "clang-tidy: clean (no errors/warnings)."
else
  echo "No staged *.c files; skipping clang-tidy."
fi

echo "Running cppcheck on staged files..."
: >"$tmp_cppcheck"
while IFS= read -r f; do
  [ -f "$f" ] || continue
  cppcheck \
    --std=c11 \
    --enable=warning,style,performance,portability \
    --inconclusive \
    --force \
    --inline-suppr \
    --suppress=missingIncludeSystem \
    -I. -Ilibmport -Ilibexec \
    "$f" \
    2>> "$tmp_cppcheck" || true
done <"$tmp_staged"

errors="$(rg -n ': error:' "$tmp_cppcheck" || true)"
warnings="$(rg -n ': warning:' "$tmp_cppcheck" || true)"

if [ -n "$errors" ] || [ -n "$warnings" ]; then
  echo
  echo "cppcheck findings (errors/warnings):"
  if [ -n "$errors" ]; then
    echo "$errors"
  fi
  if [ -n "$warnings" ]; then
    echo "$warnings"
  fi
  echo
  echo "Full cppcheck output saved at: $tmp_cppcheck"
  exit 1
fi

echo "cppcheck: clean (no errors/warnings)."
