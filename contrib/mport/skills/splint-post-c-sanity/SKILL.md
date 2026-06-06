---
name: splint-post-c-sanity
description: Run splint after cppcheck/clang-format/clang-tidy precommit sanity to find memory issues, API misuse, and contract violations in staged C code (tolerates older C parser limitations).
---

# Splint pass (after C sanity)

Use this skill after running `cppcheck-clang-format-precommit` when you want an additional Splint pass focused on memory ownership, API misuse, and contract/annotation checking.

## Workflow

1. Stage changes (`git add ...`).
2. Run the C sanity skill first:
   - `./skills/cppcheck-clang-format-precommit/scripts/precommit_c_sanity.sh`
3. Then run Splint:
   - `./skills/splint-post-c-sanity/scripts/run_splint_on_staged.sh`
4. If Splint reports problems, fix and repeat.

## What it does

- Runs `splint` on staged `*.c` files (headers are pulled in via includes).
- Uses a conservative flag set aimed at:
  - memory/ownership issues (`mustfreefresh`, `usereleased`, `freshtrans`, etc.)
  - bounds/null misuse (`boundsread/write`, `nullpass/nullret`)
  - contract checking via annotations where present
- Because Splint’s parser is older, the script is intentionally conservative and may require local suppressions or annotations in tricky areas.

## Tuning

- Edit `SPLINT_FLAGS` / include dirs inside the script if you need to reduce noise or add `-D...` defines.

