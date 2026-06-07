#!/usr/bin/env bash
set -euo pipefail

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

  local candidates=()
  while IFS= read -r path; do
    candidates+=("$path")
  done < <(command -v -a clang-format 2>/dev/null || true)

  if ((${#candidates[@]})); then
    echo "clang-format"
    return 0
  fi

  # Try version-suffixed binaries (clang-formatNN) and pick the highest NN.
  local best=""
  local best_ver=-1
  while IFS= read -r cmd; do
    local base="${cmd##*/}"
    if [[ "$base" =~ ^clang-format([0-9]+)$ ]]; then
      local ver="${BASH_REMATCH[1]}"
      if (( ver > best_ver )); then
        best_ver=$ver
        best="$base"
      fi
    fi
  done < <(compgen -c clang-format | sort -u)

  if [[ -n "$best" ]] && command -v "$best" >/dev/null 2>&1; then
    echo "$best"
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

  local best=""
  local best_ver=-1
  while IFS= read -r cmd; do
    local base="${cmd##*/}"
    if [[ "$base" =~ ^clang-tidy([0-9]+)$ ]]; then
      local ver="${BASH_REMATCH[1]}"
      if (( ver > best_ver )); then
        best_ver=$ver
        best="$base"
      fi
    fi
  done < <(compgen -c clang-tidy | sort -u)

  if [[ -n "$best" ]] && command -v "$best" >/dev/null 2>&1; then
    echo "$best"
    return 0
  fi

  die "no clang-tidy found (expected clang-tidy19, clang-tidyNN, or clang-tidy)"
}

clang_tidy_bin="$(pick_clang_tidy)"

mapfile -t staged_files < <(git diff --cached --name-only --diff-filter=ACMR -- '*.c' '*.h' || true)
if ((${#staged_files[@]} == 0)); then
  echo "No staged *.c/*.h files; skipping clang-format/clang-tidy/cppcheck."
  exit 0
fi

echo "clang-format: $clang_format_bin"
echo "clang-tidy: $clang_tidy_bin"
echo "Formatting staged files..."

for f in "${staged_files[@]}"; do
  [[ -f "$f" ]] || continue
  "$clang_format_bin" -i "$f"
done

git add -- "${staged_files[@]}"

INCLUDE_DIRS=(-I. -Ilibmport -Ilibexec)
CPPFLAGS=()

tmp_tidy="${TMPDIR:-/tmp}/clang-tidy.$$.txt"
tmp_cppcheck="${TMPDIR:-/tmp}/cppcheck.$$.txt"
trap 'rm -f "$tmp_tidy" "$tmp_cppcheck"' EXIT

mapfile -t staged_c_files < <(printf '%s\n' "${staged_files[@]}" | rg -N '\\.c$' || true)

if ((${#staged_c_files[@]})); then
  TIDY_CHECKS='clang-analyzer-*,bugprone-*,cert-*,security-*,-cert-err34-c'
  echo "Running clang-tidy on staged .c files..."
  : > "$tmp_tidy"
  for f in "${staged_c_files[@]}"; do
    [[ -f "$f" ]] || continue
    "$clang_tidy_bin" \
      -checks="$TIDY_CHECKS" \
      -warnings-as-errors='clang-analyzer-*,cert-*,security-*' \
      -quiet \
      "$f" -- -std=c11 "${INCLUDE_DIRS[@]}" "${CPPFLAGS[@]}" \
      >>"$tmp_tidy" 2>&1 || true
  done

  tidy_errors="$(rg -n '(^|: )error:' "$tmp_tidy" || true)"
  tidy_warnings="$(rg -n '(^|: )warning:' "$tmp_tidy" || true)"
  if [[ -n "$tidy_errors" || -n "$tidy_warnings" ]]; then
    echo
    echo "clang-tidy findings (errors/warnings):"
    if [[ -n "$tidy_errors" ]]; then
      echo "$tidy_errors"
    fi
    if [[ -n "$tidy_warnings" ]]; then
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
cppcheck \
  --std=c11 \
  --enable=warning,style,performance,portability \
  --inconclusive \
  --force \
  --inline-suppr \
  --suppress=missingIncludeSystem \
  "${INCLUDE_DIRS[@]}" \
  "${CPPFLAGS[@]}" \
  "${staged_files[@]}" \
  2> "$tmp_cppcheck" || true

errors="$(rg -n ': error:' "$tmp_cppcheck" || true)"
warnings="$(rg -n ': warning:' "$tmp_cppcheck" || true)"

if [[ -n "$errors" || -n "$warnings" ]]; then
  echo
  echo "cppcheck findings (errors/warnings):"
  if [[ -n "$errors" ]]; then
    echo "$errors"
  fi
  if [[ -n "$warnings" ]]; then
    echo "$warnings"
  fi
  echo
  echo "Full cppcheck output saved at: $tmp_cppcheck"
  exit 1
fi

echo "cppcheck: clean (no errors/warnings)."
