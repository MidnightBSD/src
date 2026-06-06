# Agent instructions (mport)

## Pre-commit checks (C)

- Run `./skills/cppcheck-clang-format-precommit/scripts/precommit_c_sanity.sh` before committing C changes.
- After that, run `./skills/splint-post-c-sanity/scripts/run_splint_on_staged.sh` to catch additional memory/contract issues with Splint.
- These checks should ignore `external/` (third-party code); do not run precommit formatting/linting/splint over that directory.

## Splint annotations (security-sensitive areas)

When modifying code that handles untrusted inputs or privileged actions (network fetch, archive/bundle parsing, SQL/DB I/O, filesystem paths, privilege changes), add/maintain Splint annotations to make ownership and nullability explicit:

- **Nullability**: `/*@null@*/`, `/*@notnull@*/`
- **Ownership/allocators**: `/*@only@*/` for owned pointers, `/*@owned@*/` when returning owned memory, `/*@observer@*/` for borrowed pointers
- **Out params**: annotate pointer parameters as `/*@out@*/` / `/*@in@*/` where appropriate
- **Pre/post contracts**: use `/*@requires ... @*/` / `/*@ensures ... @*/` for important invariants (e.g., validated path, verified hash)
- **Intentional exceptions**: use Splint control comments sparingly to document why a warning is safe
