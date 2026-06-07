---
allowed-tools: Bash(./skills/cppcheck-clang-format-precommit/scripts/precommit_c_sanity.sh:*), Bash(git add:*), Bash(git diff:*), Bash(git status:*)
description: Run clang-format, clang-tidy, and cppcheck on staged C/header changes before committing; auto-format then fail on serious diagnostics.
---

Run the C pre-commit sanity check on staged changes.

1. Make sure changes are staged with `git add`.
2. Run `./skills/cppcheck-clang-format-precommit/scripts/precommit_c_sanity.sh` from the repo root.
3. If it exits non-zero, report the findings and fix them before committing.
4. If it exits zero, confirm the staged files are ready to commit.

The script:
- Formats staged `*.c`/`*.h` files with `clang-format` (prefers `clang-format19`).
- Re-stages any files changed by formatting.
- Runs `clang-tidy` on staged `*.c` files with a security/bug-focused check set.
- Runs `cppcheck` with `--enable=warning,style,performance,portability --inconclusive --force --std=c11`.
- Fails if `clang-tidy` or `cppcheck` report any `error:` or `warning:` lines.
