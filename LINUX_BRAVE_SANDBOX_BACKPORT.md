# Linux Brave Sandbox Backport Plan

## Goal

Backport FreeBSD's Linuxulator memory protection key support so
Chromium-based Linux applications, including Brave, can use the V8 heap and
JIT sandbox on MidnightBSD/amd64.

## Upstream changes

1. Backport FreeBSD commit `7bcaff05223e`, which exposes XSAVE feature and
   save-area layout information.
2. Backport FreeBSD commit `b9951017bab3`, which extends the XSAVE helpers to
   account for supervisor-state components.
3. Adapt FreeBSD commit `bdb561843e86`, which implements Linux
   `pkey_alloc(2)`, `pkey_free(2)`, and `pkey_mprotect(2)` using native amd64
   PKU support.
4. Treat FreeBSD commit `34718e01869b`, which maps `IFF_LOWER_UP` through
   Linux `NETLINK_ROUTE`, as a separate follow-up because it fixes Brave
   network detection rather than sandboxing.

## Integration approach

- Preserve the upstream split between common Linux syscall validation and
  machine-dependent PKU operations.
- Keep protection-key allocation state in Linux per-process emulation data,
  inherited on fork and reset on exec.
- Initialize PKRU to Linux's `0x55555554` default during Linux exec.
- Retain Linux-compatible no-PKU behavior on unsupported architectures.
- Adapt source and module Makefiles to MidnightBSD's current tree instead of
  applying conflicting upstream hunks mechanically.
- Preserve existing syscall numbers and replace only their ENOSYS stubs.

## Validation

1. Run the repository C static-analysis scripts on staged C and header files.
2. Build the affected `linux_common`, Linux ABI modules, and amd64 kernel.
3. Exercise allocation, protection changes, access-right changes, fork
   inheritance, exec reset, key exhaustion, and protection-key faults with a
   small Linux test program.
4. Confirm protection-key faults translate to Linux `SEGV_PKUERR`.
5. Start Linux Brave on PKU-capable amd64 hardware and inspect its sandbox
   status.
6. Test the no-PKU fallback where suitable hardware is available.

## Commit structure

- XSAVE query helpers.
- Linuxulator protection-key syscall support.
- Tests, if kept separate by the existing test layout.
- `UPDATING` entry as an independently reviewable commit, after approval.
- Optional `IFF_LOWER_UP` compatibility fix as a separate change.
