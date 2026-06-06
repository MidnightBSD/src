---
allowed-tools: Bash(./skills/splint-post-c-sanity/scripts/run_splint_on_staged.sh:*), Bash(git diff:*), Bash(git status:*)
description: Run splint on staged C files after cppcheck/clang-format precommit to catch memory ownership, API misuse, and contract violations.
---

Run the Splint pass on staged C changes (use after `/cppcheck-clang-format-precommit`).

1. Make sure the cppcheck/clang-format precommit skill has already been run and passed.
2. Run `./skills/splint-post-c-sanity/scripts/run_splint_on_staged.sh` from the repo root.
3. Report any Splint findings and fix them before committing.

The script:
- Runs `splint` on staged `*.c` files (headers are pulled in via includes).
- Uses a conservative flag set targeting memory/ownership issues (`mustfreefresh`, `usereleased`, `freshtrans`), bounds/null misuse (`boundsread/write`, `nullpass/nullret`), and contract annotation checking.
- Because Splint's parser is older, the script is intentionally conservative — local suppressions or annotations may be needed in tricky areas.
