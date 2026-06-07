---
name: update-updating
description: Add an entry to the UPDATING file at the repository root. Use whenever there is a security fix, vendor/third-party software update, behavioral change requiring user action, new stable branch creation, or a release. The UPDATING commit must be separate from any code changes so it can be cleanly cherry-picked to other branches.
---

# Update UPDATING

Use this skill when any of the following occur and need to be recorded for
users who track -current or a stable branch:

- Security vulnerability fixes (include CVE numbers when available)
- Vendor/third-party software version bumps
- Behavioral changes that require user action or awareness
- Creation of a new stable branch (e.g., `stable/4.1`)
- A release off a stable branch

## Format rules

- Date header: `YYYYMMDD:` on its own line, no leading whitespace
- Each entry: one leading **tab** character (not spaces), then the text
- Maximum line length: **80 characters** (including the leading tab)
- Wrap long lines: continuation lines use a leading tab plus enough spaces
  to align naturally with the text above
- Newer dates appear **near the top** of the file, below the header line
- Multiple entries under the same date are separated by a blank line
- CVE numbers should be included when applicable: `CVE-YYYY-NNNNN`
- For vendor updates, just state the package name and version number
- For branch creation: `stable/X.Y branch created.`
- For a release: `MidnightBSD X.Y released off stable/X.Y.`

## Example entries

```
20260607:
	libarchive 3.8.7

	expat 2.8.1: CVE-2026-45186

20260522:
	libcasper: CVE-2026-39461  select(2) file descriptor set overflow
	causes stack overflow

	unbound 1.24.2: CVE-2025-11411

20250929:
	stable/4.0 branch created.
```

---

## Workflow

### Step 1 — Gather information

Ask the user what needs to be recorded. Determine:

1. **Type**: security fix / vendor update / behavioral change /
   branch creation / release
2. **Date**: use today's date (`date +%Y%m%d`) unless the user specifies
   a different date (e.g., the date a change actually landed)
3. **Content**: the entry text — package name, version, CVE, or
   description

```bash
date +%Y%m%d
```

### Step 2 — Check for an existing entry for this date

```bash
head -20 UPDATING
```

If a `YYYYMMDD:` block for today already exists, the new entry will be
added to it (separated by a blank line from existing entries). If not, a
new date block is inserted at the top.

### Step 3 — Verify the current branch and working tree

```bash
git branch --show-current
git status
```

Warn the user if there are uncommitted code changes — the UPDATING commit
must be separate. If uncommitted changes are present, ask whether to stash
them first or whether to proceed anyway.

### Step 4 — Draft the entry

Draft the entry text following the format rules:
- Leading tab on every line
- No line exceeds 80 characters (tab counts as 1 character for this check)
- CVE numbers included if applicable
- Terse but complete — users need to know what changed and why it matters

Show the draft to the user:

> "Here is the proposed UPDATING entry:
>
> ```
> 20260607:
> \tlibarchive 3.8.7
> ```
>
> Does this look correct, or would you like to change the wording?"

Wait for confirmation or corrections before proceeding.

### Step 5 — Write the entry

Insert the entry at the top of UPDATING (after the file header line).

If today's date block already exists: append the new entry (with a
separating blank line) inside that block.

If today's date block does not exist: prepend a new block above the
previous newest entry.

Use this bash pattern to check and insert:

```bash
TODAY=$(date +%Y%m%d)

# Check if today's block exists
if grep -q "^${TODAY}:" UPDATING; then
    # Find the line number of today's header and insert after its last entry
    # (manual edit is safer — show the user the exact change needed)
    echo "Today's block exists — appending entry."
else
    echo "No block for today — creating new date header."
fi
```

Since precise multi-line insertion is error-prone in shell, **edit the
file directly** using the Edit tool: read the current top of UPDATING,
construct the new content, and write it back with the new entry in place.

Always read the file first, then use the Edit tool to make the change.

### Step 6 — Show the diff and ask for approval

```bash
git diff UPDATING
```

**STOP. Show the diff to the user and ask:**
> "The diff above shows the proposed UPDATING entry. Shall I commit this
> as a separate commit?"

### Step 7 — Commit separately

**STOP before committing.** If there are staged or unstaged code changes
unrelated to UPDATING, remind the user:

> "There are other uncommitted changes in the working tree. The UPDATING
> entry should be committed separately so it can be cleanly cherry-picked
> to stable branches. I will only stage and commit UPDATING."

Then commit only UPDATING:

```bash
git add UPDATING
git commit -m "UPDATING: note <brief description>"
```

Commit message format: `UPDATING: note <what changed>`. Examples:
- `UPDATING: note libarchive 3.8.7`
- `UPDATING: note expat 2.8.1 CVE-2026-45186`
- `UPDATING: note stable/4.1 branch creation`
- `UPDATING: note iconv fix and mmap security fix`

### Step 8 — Cherry-pick to stable branches (optional)

Ask the user:
> "Should this UPDATING entry also be cherry-picked to any stable
> branches (e.g., `stable/4.1`, `stable/4.0`)?"

If yes, for each branch:

```bash
git checkout stable/X.Y
git cherry-pick <commit-hash>
```

**STOP. Ask for approval before pushing each branch.**

```bash
git push origin stable/X.Y
```

Then return to the original branch:

```bash
git checkout <original-branch>
```

---

## When to add UPDATING entries

| Event | Example entry |
|---|---|
| Security fix (with CVE) | `libarchive 3.8.7: CVE-2026-45123` |
| Security fix (no CVE) | `dhcp: fix remote code execution via BOOTP field` |
| Vendor update (no security) | `mport 2.7.9` |
| Behavioral change | `rtld: honor FreeBSD no-init notes` |
| Kernel driver update | `Update ice(4) driver to ice-1.3.41.0` |
| New stable branch | `stable/4.1 branch created.` |
| Release | `MidnightBSD 4.1 released off stable/4.1.` |
| Certificate update | `caroot: update trusted/blacklisted certificates` |
| Timezone update | `tzdata 2025c` |

## What does NOT need an UPDATING entry

- Internal refactors with no user-visible effect
- Test-only changes
- CI/workflow changes
- Documentation-only changes
- Changes to `skills/` or agent config files
