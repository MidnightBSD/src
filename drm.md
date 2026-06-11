# Porting drm-515-kmod to MidnightBSD — Gap Analysis & Plan

## Goal

Make `graphics/drm-515-kmod` (FreeBSD's backport of the Linux 5.15 DRM stack:
`drm.ko`, `i915kms`, `amdgpu`, `radeonkms`, `ttm`) **compile and link** against
MidnightBSD master. The port officially requires FreeBSD **14.x**.

**Committed scope: build + link only. Do not `kldload`.** Runtime bring-up
(vt/newcons KMS console handoff, interrupts, suspend/resume, modesetting) is
out of scope.

## Current state

| Tree | Version | LinuxKPI level |
|------|---------|----------------|
| MidnightBSD master (`/usr/src`) | `__MidnightBSD_version 401002`, `__FreeBSD_version 1305500` | ad-hoc fork, ~FreeBSD **13.5-STABLE** + selective forward-ports |
| FreeBSD 14-STABLE (`/home/laffer1/git/freebsd`, `stable/14`) | `__FreeBSD_version 1404501` | reference target |

MidnightBSD's `sys/compat/linuxkpi` is a series of ad-hoc partial imports from
FreeBSD 12 → 13 → 13.5, already partly forward-ported (e.g. `pci_domain_nr`).

## Key findings

1. **`dma-buf.h` / `dma-fence.h` / `dma-resv.h` are NOT base gaps.** They are
   absent from *both* MidnightBSD and FreeBSD 14 base LinuxKPI — the drm-kmod
   **port ships them itself** (bundled GPLv2 headers). Not our problem to add.

2. **A wholesale FreeBSD 14 LinuxKPI import is the WRONG approach.** Empirically
   verified: overlaying FreeBSD 14's `common/{include,src}` and building fails on
   **base-kernel API deltas** that recur in nearly every file —
   - `sched.h` → `td_ast_pending` / `TDA_SCHED` (FreeBSD 14 **AST framework**)
   - `net.h` → `pr_peeraddr` / `pr_sockaddr` (FreeBSD 14 **protosw refactor**)
   - `file.h` / `linux_compat.c` → `fget_unlocked`, `fo_stat_t`, const `fileops`
   - `rbtree.h` (`sys/tree.h` macro arity), `kmem_free(void*)`, `qsort_r`
   MidnightBSD's base has **none** of these. Wholesale import would require
   backporting the AST framework, the protosw refactor, and the file/fileops
   API into MidnightBSD's base kernel — a larger, riskier project than the drm
   port itself, destabilizing unrelated subsystems.

3. **MidnightBSD's existing LinuxKPI already correctly targets its 13.5 base.**
   The real gap is the *additive* FreeBSD 14 files (new headers + new modules),
   not re-importing existing files.

## Approach: hybrid (additive import + targeted backports)

Keep MidnightBSD's existing, base-compatible LinuxKPI files. Add only the
genuinely-new FreeBSD 14 pieces drm needs, and backport individual new symbols
into existing headers only where they don't drag base-incompatible changes.

### Workstream A — LinuxKPI uplift (in `/usr/src`)  — IN PROGRESS

- [x] Import additive FreeBSD 14 headers (`aperture.h`, `iosys-map.h`, `hdmi.h`,
      `iommu.h`, `irqdomain.h`, `ioport.h`, `io-64-nonatomic-lo-hi.h`,
      `cleanup.h`, `container_of.h`, `build_bug.h`, `minmax.h`, `math.h`,
      `limits.h`, `kstrtox.h`, `string_helpers.h`, `nodemask.h`, `of.h`,
      `agp_backend.h`, `apple-gmux.h`, `device/`, `video/`, dummy `sysfb.h`).
- [x] Add `linuxkpi_hdmi` and `linuxkpi_video` modules (HDMI infoframe,
      ACPI-video/backlight, framebuffer aperture) + register in
      `sys/modules/Makefile`. **Both build clean.**
- [x] Backport `devm_add_action()/devm_add_action_or_reset()` (needed by
      `linux_aperture.c`).
- [ ] Backport further individual symbols as the drm port build demands them
      (iterative, localized — keep MidnightBSD base semantics, do NOT pull the
      AST/protosw/file base reworks).
- Note: `linux_kobject.c` is intentionally NOT imported — FreeBSD 14 split it
  out of `linux_compat.c`, which MidnightBSD still carries inline (would dup).

### Workstream B — Create drm-515-kmod in mports (in `/usr/mports`) — TODO

NOTE: `/usr/ports` here is the upstream **FreeBSD** ports tree and is not
operational under MidnightBSD make (`bsd.port.options.mk` not found,
`${OPSYS}` conditionals malformed). MidnightBSD uses **mports** at
`/usr/mports`, which already ships a MidnightBSD-adapted `graphics/drm-510-kmod`
(uses `bsd.mport.options.mk`, `IGNORE_MidnightBSD_3.x`, already
`CONFLICTS_INSTALL` with `drm-515-kmod`) and `gpu-firmware-kmod`.

- Create `graphics/drm-515-kmod` by copying mports `drm-510-kmod` and bumping
  `Makefile.version` to 5.15.160 / `drm_v5.15.160_6`; refresh `distinfo`,
  `pkg-plist`, and the `extra-patch-linuxkpi-pci` (gate decided by build).
- Build via mports against `/usr/src` (our uplifted LinuxKPI); iterate
  base-compat backports (Workstream A) as drm source demands them.
- mports is a separate git repo (MidnightBSD/mports) → a second PR.

## Verification (build-only; do NOT kldload)

1. `cd /usr/src/sys/modules/linuxkpi{,_hdmi,_video} && make` — clean. (done)
2. `cd /usr/ports/graphics/drm-515-kmod && make` — no `IGNORE`; produces
   `drm/i915kms/amdgpu/radeonkms/ttm .ko`.
3. `nm -u` the `.ko`s for unresolved LinuxKPI symbols against the built kernel.
4. cppcheck/clang-format sanity on any base-compat backport commits.

## Out of scope (follow-on)
`kldload`, vt/newcons KMS console handoff, interrupts, suspend/resume,
on-hardware modesetting.
