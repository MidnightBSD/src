---
name: cppcheck-clang-format-precommit
description: Run clang-format (prefer clang-format19), clang-tidy19, and cppcheck on staged C/C header changes before committing; auto-format then fail on serious diagnostics.
---

# C pre-commit sanity (clang-format + clang-tidy + cppcheck)

Use this skill when you’re about to `git commit` C/C header changes and want an automated preflight for serious bugs/vulns and formatting consistency.

## Workflow

1. Stage your changes (`git add ...`).
2. Run the pre-commit script:
   - `./skills/cppcheck-clang-format-precommit/scripts/precommit_c_sanity.sh`
3. If it exits non-zero, fix findings and re-run until clean.
4. Commit.

## What it does

- Formats staged `*.c`/`*.h` with `clang-format` (prefers `clang-format19` when present, otherwise picks the highest `clang-formatNN` available, otherwise `clang-format`).
- Re-stages any files changed by formatting.
- Runs `clang-tidy` on the staged `*.c` files (prefers `clang-tidy19`) with a security/bug-focused check set, using a minimal compile flag set (`-std=c11` + repo include dirs).
- Runs `cppcheck` on the staged C/C headers with:
  - `--enable=warning,style,performance,portability`
  - `--inconclusive --force`
  - `--std=c11`
- Fails if `clang-tidy` or `cppcheck` report any `error:` or `warning:` lines (style-only findings do not fail the run).

## Notes

- Uses repo `.clang-format` automatically.
- If your project needs extra include paths/defines for fewer false positives, edit the script’s `CPPFLAGS`/`INCLUDE_DIRS`.
