---
name: vendor-update
description: Import a new upstream release of third-party software into a MidnightBSD vendor branch and merge it into master (and optionally stable branches). Handles vendor branch naming, Xlist pruning, tagging, git subtree merge, FreeBSD patch refresh, and stable branch merges — with mandatory user approval before every push, merge, or commit.
---

# Vendor Branch Update

Use this skill when you need to import a new upstream release of a third-party
software package that lives in a MidnightBSD vendor branch.

## Conventions

### Vendor branch naming

| Format | Example | Notes |
|---|---|---|
| `vendor/softwarename` | `vendor/openssh`, `vendor/bc` | Preferred for all new imports |
| `vendor/organization/softwarename` | `vendor/NetBSD/bmake`, `vendor/OpenBSD/ksh` | For software sourced from another BSD project |
| `vendor/softwarename/dist` | `vendor/libarchive/dist`, `vendor/unbound/dist` | Legacy only — from SVN migration; do not create new branches in this format |

### In-tree locations

| Location | Contents |
|---|---|
| `contrib/` | Userland software (default for most packages) |
| `crypto/` | Cryptographic software (openssh, openssl, netpgp) |
| `sys/contrib/` | Kernel-only software (openzfs) |

### Tags

Every vendor import is tagged: `vendor/<branch-path>/<version>`

Examples: `vendor/openssh/10.3p1`, `vendor/bc/7.1.0`, `vendor/unbound/dist/1.24.2`

---

## Workflow

### Step 0 — Gather information

Ask the user for:
1. **Software name** (e.g., `unbound`, `libarchive`, `openssh`)
2. **New version** (e.g., `1.24.2`)
3. **Location of the new upstream sources** — a local directory, tarball path, or git clone the user will prepare

Then determine the following by inspecting the repo:

```bash
# List vendor branches matching the software name
git branch -r | grep vendor | grep -i <name>

# Identify the in-tree prefix
git log --oneline --all -- contrib/<name>/ crypto/<name>/ sys/contrib/<name>/
```

Confirm with the user:
- The vendor branch name (e.g., `vendor/unbound/dist`)
- The in-tree prefix (e.g., `contrib/unbound`)
- The tag to create (e.g., `vendor/unbound/dist/1.24.2`)

### Step 1 — Read upgrade instructions and Xlist

Check for per-package metadata files at `<prefix>/MIDNIGHTBSD-upgrade` and
`<prefix>/MIDNIGHTBSD-Xlist` (some older packages use `FREEBSD-upgrade` /
`FREEBSD-Xlist`).

```bash
cat <prefix>/MIDNIGHTBSD-upgrade 2>/dev/null || cat <prefix>/FREEBSD-upgrade 2>/dev/null
cat <prefix>/MIDNIGHTBSD-Xlist  2>/dev/null || cat <prefix>/FREEBSD-Xlist  2>/dev/null
```

Read and display these files to the user. The upgrade file may contain
important notes about the import procedure. The Xlist is the authoritative list
of files and directories that must be pruned from the vendor branch — they are
not carried in the in-tree copy (e.g., Windows build files, CI configs, test
data not needed on MidnightBSD).

### Step 2 — Ask about FreeBSD patches and local modifications

Ask the user:

> "Does `<prefix>` carry any local patches or modifications on top of the
> upstream sources (e.g., FreeBSD-specific fixes, MidnightBSD-specific fixes)?
> If so, these will need to be reviewed and possibly refreshed after the merge."

If yes, note it — you will prompt the user to check them in Step 8.

### Step 3 — Create a worktree for the vendor branch

**Do not work on the vendor branch in the main checkout.** Use a worktree:

```bash
WORKTREE=$(mktemp -d)
git worktree add "$WORKTREE" <vendor-branch>
# Example: git worktree add /tmp/vendor-unbound vendor/unbound/dist
```

Tell the user the worktree path.

### Step 4 — Sync new upstream sources into the worktree

Tell the user:
> "Please copy (or rsync) the new upstream sources into `$WORKTREE`, replacing
> the existing files. Do not copy `.git`. Use rsync or cp -r from an unpacked
> tarball or git clone."

Suggested command (adapt as needed):
```bash
rsync -av --delete --exclude='.git' /path/to/new-upstream-sources/ "$WORKTREE/"
```

Wait for the user to confirm they have copied the sources.

### Step 5 — Apply Xlist pruning

If a `MIDNIGHTBSD-Xlist` (or `FREEBSD-Xlist`) exists, prune the files it lists
from the worktree **before staging**. The Xlist entries may use glob patterns.

```bash
cd "$WORKTREE"
XLIST="$(git show HEAD:<prefix>/MIDNIGHTBSD-Xlist 2>/dev/null || \
         git show HEAD:<prefix>/FREEBSD-Xlist 2>/dev/null)"
if [ -n "$XLIST" ]; then
    echo "$XLIST" | grep -v '^#' | grep -v '^$' | grep -v 'MidnightBSD\|FreeBSD' | \
    while IFS= read -r pattern; do
        # shellcheck disable=SC2086
        rm -rf $pattern
    done
fi
```

Show the user which files were removed.

### Step 6 — Stage, review, and commit on the vendor branch

```bash
cd "$WORKTREE"
git add -A
git status
git diff --stat HEAD
```

Show the user the diff summary.

**STOP. Show the user the staged changes and ask:**
> "The staged changes on the vendor branch are shown above. Shall I commit
> these with the message: `Vendor import of <name> <version>`?"

If approved:
```bash
cd "$WORKTREE"
git commit -m "Vendor import of <name> <version>"
```

### Step 7 — Create an annotated tag

```bash
cd "$WORKTREE"
git tag -a "vendor/<branch-path>/<version>" -m "<name> <version>"
# Example: git tag -a vendor/unbound/dist/1.24.2 -m "unbound 1.24.2"
```

Show the user the tag that will be created.

### Step 8 — Push vendor branch and tag

**STOP. Ask the user:**
> "Ready to push the vendor branch `<vendor-branch>` and tag
> `vendor/<branch-path>/<version>` to origin. Shall I proceed?"

If approved:
```bash
cd "$WORKTREE"
git push origin <vendor-branch> --follow-tags
```

Then clean up the worktree:
```bash
git worktree remove "$WORKTREE"
```

### Step 9 — Merge into master with git subtree

Switch to the master branch and verify it is clean:
```bash
git checkout master
git status
```

Preview the merge:
```bash
git subtree merge --dry-run -P <prefix> <vendor-branch> 2>/dev/null || \
    echo "(dry-run not supported; review vendor branch diff manually)"
git log --oneline HEAD..<vendor-branch> -- 2>/dev/null | head -20
```

**STOP. Ask the user:**
> "Ready to merge vendor branch `<vendor-branch>` into master at prefix
> `<prefix>` using `git subtree merge`. This will create a merge commit.
> Shall I proceed?"

If approved:
```bash
git subtree merge -P <prefix> <vendor-branch>
```

The resulting merge commit message will be `Merge commit '<hash>'`. This is
the expected format — do not change it.

### Step 10 — Handle FreeBSD / local patches (if applicable)

If the user indicated in Step 2 that local patches exist:

> "The merge is complete. Please review whether any local patches in
> `<prefix>` need to be refreshed against the new upstream. Common indicators:
> - Patch hunks that fail to apply
> - Behaviour regressions in MidnightBSD-specific code paths
> - Files in `<prefix>` that are not in the upstream Xlist and differ from
>   the vendor branch
>
> Once you have refreshed any patches, stage and commit them separately."

**STOP. Ask the user if any patch commits are needed before proceeding.**

### Step 11 — Push master

**STOP. Ask the user:**
> "Ready to push master to origin. Shall I proceed?"

If approved:
```bash
git push origin master
```

### Step 12 — Optional: merge into stable branches

Ask the user:
> "Should this vendor update also be merged into any stable branches (e.g.,
> `stable/4.1`, `stable/4.0`)? This is typically done for security-relevant
> updates."

If yes, for each stable branch:

```bash
git checkout stable/X.Y
git status
```

**STOP. Ask the user:**
> "Ready to merge the vendor update into `stable/X.Y`. This will use
> `git subtree merge -P <prefix> <vendor-branch>`. Shall I proceed?"

If approved:
```bash
git subtree merge -P <prefix> <vendor-branch>
```

Then:

**STOP. Ask the user:**
> "Ready to push `stable/X.Y` to origin. Shall I proceed?"

If approved:
```bash
git push origin stable/X.Y
```

Repeat for each additional stable branch.

### Step 13 — Update the GitHub wiki

After all merges and pushes are complete, update the vendor branch table on the
GitHub wiki at https://github.com/MidnightBSD/src/wiki/Vendor-Branches.

Fetch the current wiki page content:
```bash
gh api repos/MidnightBSD/src/pages 2>/dev/null || true
# The wiki is a separate git repo:
# git clone https://github.com/MidnightBSD/src.wiki.git /tmp/mbsd-wiki
```

The wiki is maintained as a separate git repository. Clone or update it:
```bash
WIKI_DIR=$(mktemp -d)
git clone https://github.com/MidnightBSD/src.wiki.git "$WIKI_DIR"
```

Open `$WIKI_DIR/Vendor-Branches.md` and locate the row for `<name>` in the
table. Update the **current version** column to `<new-version>`.

Show the user the diff:
```bash
git -C "$WIKI_DIR" diff
```

**STOP. Ask the user:**
> "Ready to commit and push the wiki update for `<name>` to version
> `<new-version>`. Shall I proceed?"

If approved:
```bash
git -C "$WIKI_DIR" add Vendor-Branches.md
git -C "$WIKI_DIR" commit -m "Update <name> to <new-version>"
git -C "$WIKI_DIR" push origin master
rm -rf "$WIKI_DIR"
```

---

## Vendor branch table reference

Key entries from the wiki (may not be exhaustive — always verify with
`git branch -r | grep vendor`):

| Software | Vendor branch | In-tree prefix | Notes |
|---|---|---|---|
| atf | `vendor/atf/dist` | `contrib/atf` | |
| bc | `vendor/bc` | `contrib/bc` | |
| bearssl | `vendor/bearssl` | `contrib/bearssl` | |
| bzip2 | `vendor/bzip2/dist` | `contrib/bzip2` | |
| expat | `vendor/expat/dist` | `contrib/expat` | |
| file | `vendor/file/dist` | `contrib/file` | |
| jemalloc | `vendor/jemalloc/dist` | `contrib/jemalloc` | |
| kyua | `vendor/kyua` | `contrib/kyua` | |
| less | `vendor/less/dist` | `contrib/less` | |
| libarchive | `vendor/libarchive/dist` | `contrib/libarchive` | |
| libfido2 | `vendor/libfido2` | `contrib/libfido2` | |
| libpcap | `vendor/libpcap` | `contrib/libpcap` | |
| llvm | `vendor/llvm/dist` | `contrib/llvm` | large — take care |
| mandoc | `vendor/mandoc/dist` | `contrib/mandoc` | |
| mksh | `vendor/MirOS/mksh` | `contrib/mksh` | |
| ncurses | `vendor/ncurses/dist` | `contrib/ncurses` | |
| one-true-awk | `vendor/one-true-awk/dist` | `contrib/one-true-awk` | |
| openbsm | `vendor/openbsm/dist` | `contrib/openbsm` | |
| openntpd | `vendor/OpenBSD/openntpd` | `contrib/openntpd` | |
| openssh | `vendor/openssh` | `crypto/openssh` | crypto prefix |
| openssl | `vendor/openssl` | `crypto/openssl` | crypto prefix |
| openzfs | `vendor/openzfs` | `sys/contrib/openzfs` | sys/contrib prefix |
| sendmail | `vendor/sendmail/dist` | `contrib/sendmail` | |
| sqlite3 | `vendor/sqlite3/dist` | `contrib/sqlite3` | |
| tcsh | `vendor/tcsh/dist` | `contrib/tcsh` | |
| tzcode | `vendor/tzcode/dist` | `contrib/tzcode` | |
| tzdata | `vendor/tzdata/dist` | `contrib/tzdata` | |
| unbound | `vendor/unbound/dist` | `contrib/unbound` | |
| wpa | `vendor/wpa/dist` | `contrib/wpa` | |
| xz | `vendor/xz/dist` | `contrib/xz` | |
| zstd | `vendor/zstd` | `contrib/zstd` | |
| bmake | `vendor/NetBSD/bmake` | `contrib/bmake` | |
| googletest | `vendor/google/googletest` | `contrib/googletest` | |

---

## Key rules

- **Never make local modifications on the vendor branch.** All MidnightBSD-specific changes go on master or stable branches after the subtree merge.
- **Always prune Xlist files before committing to the vendor branch.**
- **Always tag every vendor import** with the version, using the format above, and push with `--follow-tags`.
- **Use `git subtree merge -P <prefix>`** — not `git merge` — so that the subtree history is preserved correctly.
- **All pushes, commits, and merges require explicit user approval** before execution.
- **Do not rebase or amend commits on vendor branches** — they are shared history.
