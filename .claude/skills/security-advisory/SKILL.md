---
name: security-advisory
description: Handle a security fix end to end for MidnightBSD src - triage a FreeBSD security advisory (FreeBSD-SA-*) or CVE against this tree, port the fix to master and both stable branches, add the UPDATING entry, and publish the matching MNBSD-YYYY-N advisory on the GitHub security advisories page. Use this whenever the user pastes or mentions a FreeBSD SA, a CVE, "does this affect us", "patch this on master/stable", or asks for a security advisory, even if they only ask for one of the steps.
---

# Security advisory workflow

A security fix here is only done when four things are true: master and both
supported stable branches carry the fix, each branch has an UPDATING entry,
every push has been verified by re-fetching, and a GitHub advisory with the
next MNBSD number is published. Users read the advisory page and UPDATING,
not git log, so the last two steps matter as much as the code.

Supported branches are listed in `SECURITY.md`. At the time of writing that
is `master` (current), `stable/4.1` and `stable/4.0`.

## 1. Triage: does it affect us?

Start from the FreeBSD advisory text. The "Correction details" table gives
the commit hash on each FreeBSD branch; prefer the `stable/14` hash because
our tree descends from FreeBSD 13/14 and that diff applies most cleanly.

Get the diff from the local FreeBSD checkout rather than fetching patches:

```sh
cd /home/laffer1/git/freebsd && git fetch -q origin stable/14
git show <hash> -- <files> > "$SCRATCH/fix.diff"
```

Then look at the same code in our tree. Don't assume we are affected because
the file exists, and don't assume we are safe because a previous fix touched
the same area: check that the vulnerable construct is actually present. In
the sysvsem case an earlier advisory had been fixed in `semctl()` while the
identical check in `semop()` was still vulnerable.

Also check whether a third-party component (`contrib/`, `crypto/`) is at a
version that already has the fix; for those, the fix usually arrives via a
vendor update instead of a patch (see the vendor-update notes in memory).

If we are not affected, say so with the evidence and stop.

## 2. Port the fix on master

Work in a throwaway worktree off `origin/master`; the main checkout is often
on a feature branch and must not be disturbed:

```sh
cd /usr/src && git fetch -q origin master stable/4.1 stable/4.0
git worktree add -q /tmp/src-<topic> origin/master --detach
```

Worktree creation on this tree takes a couple of minutes; run it in the
background if other work can proceed.

Apply with `patch -p1` (BSD `git apply --3way` fails here because the
FreeBSD blobs are not in our repo). Hand-merge rejected hunks; they are
usually caused by our own earlier fixes or a different comment. Keep the
FreeBSD semantics (error codes, KASSERTs) unless there is a reason not to,
and prefer their follow-up hardening when it is in the same hunk.

C changes are restricted under `AI_POLICY.md`: audit every line for
allocation/free symmetry and bounds. Do not touch `contrib/mksh` or assembly.

### Compile check without a full build

A GENERIC kernel object tree usually exists at
`/usr/obj/usr/src/amd64.amd64/sys/GENERIC`. Its `*.o.meta` files record the
exact compile command, so one file can be checked in seconds:

```sh
cd /usr/obj/usr/src/amd64.amd64/sys/GENERIC
CMD=$(grep -m1 '^CMD' <file>.o.meta | sed 's/^CMD //; s# /usr/src/sys/<path>##')
PATH=/usr/obj/usr/src/amd64.amd64/tmp/usr/bin:$PATH
eval "$CMD -Werror -o $SCRATCH/<file>.o /tmp/src-<topic>/sys/<path>"
```

Without the `-o` override the object in the obj tree is overwritten, which
is harmless but worth mentioning. Headers come from `/usr/src/sys`, so this
is a syntax and type check, not proof the stable branches build; it is still
far better than nothing. Also run the style checker on the diff:

```sh
git diff | perl tools/build/checkstyle9.pl --terse --patch -
```

### Commit message

Describe the bug and the fix in our own words (the advisory and the FreeBSD
commit message are good sources), then these trailers, in this order:

```
Security:       CVE-YYYY-NNNNN
Security:       FreeBSD-SA-YY:NN.module
Obtained from:  FreeBSD <hash>
AI-Assisted-by: <model actually used, e.g. Claude Fable 5.1>

Co-Authored-By: <model> <noreply@anthropic.com>
Signed-off-by: (from git commit -s)
```

Commit with `git commit -s`. The DCO sign-off is mandatory.

## 3. UPDATING entry

Add a dated entry at the top of `UPDATING` as a separate commit
(`UPDATING: <module> fix (CVE-...)`). Format: `YYYYMMDD:` line, body
indented with one tab, wrapped at 72 columns, one blank line after. Say
what was wrong, who can exploit it, what changed, and what the user must do
(usually "Rebuild and install the kernel, then reboot" or "rebuild and
install world, then restart daemons"). Example:

```
20261001:
	sysvsem: fix a heap out-of-bounds access in semop(2)
	(CVE-2026-58098, FreeBSD-SA-26:64.sysvsem).  When semop(2) slept
	waiting on a semaphore, the check that the set still existed after
	waking relied on a 15-bit sequence number that wraps, so a local
	user ... potentially leading to privilege escalation.  The kernel
	now keeps a 64-bit sequence number per slot.  Rebuild and install
	the kernel, then reboot.
```

## 4. Push master, then the stable branches

```sh
git push origin HEAD:master
```

The remote prints "Changes must be made through a pull request" and then
succeeds anyway; never trust the message, confirm with
`git fetch origin master && git log --oneline -1 origin/master`.

For each stable branch make another detached worktree off
`origin/stable/4.1` / `origin/stable/4.0`, then
`git cherry-pick -x <fix> <updating>`. Before pushing, confirm the result
with `git diff <master-commit> HEAD -- <file>` (empty when the file ends up
identical to master's) and repeat the compile check. If the branch's code
differs enough that a cherry-pick needs real merging, say so in the commit.
Push with `HEAD:stable/4.1` and verify by re-fetching as above. Remove the
worktrees when done (`git worktree remove`).

## 5. Publish the GitHub advisory

Advisories live at https://github.com/MidnightBSD/src/security/advisories.
Naming is `MNBSD-YYYY-N <title> (CVE-...)` where N is the next free number
for the year in publication order, not in order of the underlying issue.
List existing ones with `gh api --paginate repos/MidnightBSD/src/security-advisories`
and also confirm no advisory already exists for the CVE.

Use the bundled script, which finds the next number, works around the API
quirks and publishes in one go:

```sh
python3 .claude/skills/security-advisory/scripts/publish_advisory.py \
    --title "Heap out-of-bounds access in semop(2)" \
    --cve CVE-2026-58098 \
    --cvss "CVSS:3.1/AV:L/AC:H/PR:L/UI:N/S:U/C:H/I:H/A:H" \
    --cwe CWE-362 --cwe CWE-787 \
    --package kernel/sysvsem --patched 4.0.8 \
    --description "$SCRATCH/advisory.md"
```

Add `--draft` to review first. The quirks it handles: `ecosystem` must be
`other` (the API rejects `midnightbsd`), `severity` cannot be given together
with a CVSS vector (the vector is preferred, GitHub derives severity), and
`state` is not accepted on create, so it creates a draft and then PATCHes
`state=published`.

The `--patched` version is the next stable/4.0 release that will contain
the fix: read `REVISION` from `sys/conf/newvers.sh` on `stable/4.0`, and
check `git tag` to see whether that version has already been tagged (if it
has, the fix goes into the one after and newvers.sh may need bumping).

### Description template

Write the description in Markdown with these sections; look at a recent
published advisory for tone:

```
## Overview        - what the subsystem is, two or three sentences
## Vulnerability   - the defect, naming the file and function, in our own words
## Impact          - who can exploit it and what they get
## Affected Versions
All MidnightBSD releases prior to <patched> and the corresponding
stable/4.1 and current (master) builds before the fix.
## Solution        - what changed, then:
Fixed in:
- current (master): `<hash>`
- stable/4.1: `<hash>`
- stable/4.0 (<patched>): `<hash>`
Rebuild and install the kernel, then reboot.
## Credits
Reported by <names from the SA Credits field>; upstream fix by <FreeBSD
commit author> (FreeBSD-SA-YY:NN.module). Ported to MidnightBSD by Lucas Holt.
```

If the fix supersedes or relates to an earlier MNBSD advisory, cross-reference
it. If an earlier fix in the same area never got an advisory, point that out
to the user; the numbering lets a late advisory be added at any time.

## 6. Report

Finish with a table of fix and UPDATING hashes per branch, the advisory
URL, what was verified (compile check, style check, re-fetch), and anything
left undone, such as a branch that needed a different patch or a related
issue that still lacks an advisory.
