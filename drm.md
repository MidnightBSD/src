# Porting drm-515-kmod to MidnightBSD — Gap Analysis & Plan

## Goal

Make `graphics/drm-515-kmod` (FreeBSD's backport of the Linux 5.15 DRM stack:
`drm.ko`, `i915kms`, `amdgpu`, `radeonkms`, `ttm`) **compile and link** against
MidnightBSD master. The port officially requires FreeBSD **14.x** and does not
support 13.x.

**Committed scope: build + link only. Do not `kldload`.** Runtime bring-up
(vt/newcons KMS console handoff, interrupts, suspend/resume, modesetting) is
follow-on work, out of scope here.

## Current state

| Tree | Version | LinuxKPI level |
|------|---------|----------------|
| MidnightBSD master (`/usr/src`) | `__MidnightBSD_version 401002`, `__FreeBSD_version 1305500` | ad-hoc fork, ~FreeBSD **13.5-STABLE** with selective forward-ports |
| FreeBSD 14-STABLE (`/home/laffer1/git/freebsd`, `stable/14`) | `__FreeBSD_version 1404501` | reference target |

MidnightBSD's `sys/compat/linuxkpi` is **not** a clean FreeBSD checkout. Its git
history is a sequence of partial manual imports (FreeBSD 12-stable → 13-stable →
13.5), including a DRM-motivated `THIS_MODULE` fix — so it already carries some
14-era symbols (`pci_domain_nr`, `lkpi_pci_get_domain_bus_and_slot`) but lacks a
coherent 14-level baseline.

## Key finding that reframes the gap

`dma-buf.h`, `dma-fence.h`, `dma-resv.h` are **absent from both** MidnightBSD and
FreeBSD 14 base LinuxKPI. They are **shipped by the drm-kmod port itself** (its
bundled GPLv2 headers). They are therefore **not** base-kernel gaps — an initial
read that flagged them as "blocking" was wrong. The real gap is the LinuxKPI
delta between MidnightBSD (~13.5) and FreeBSD 14-STABLE.

The file-level diff below is a **floor**, not the full gap: the port also relies
on intra-file API additions inside headers MidnightBSD already has.

### Missing headers (`sys/compat/linuxkpi/common/include/linux/`)

`aperture.h`, `iosys-map.h`, `hdmi.h`, `iommu.h`, `irqdomain.h`, `ioport.h`,
`io-64-nonatomic-lo-hi.h`, `cleanup.h`, `container_of.h`, `build_bug.h`,
`minmax.h`, `math.h`, `limits.h`, `kstrtox.h`, `string_helpers.h`, `nodemask.h`,
`dynamic_debug.h`, `of.h`, `agp_backend.h`, `apple-gmux.h`, the `device/`
subdir, plus the `kunit/` and `xen/` include trees.

### Missing sources (`sys/compat/linuxkpi/common/src/`)

`linux_aperture.c`, `linux_cmdline.c`, `linux_hdmi.c`, `linux_kobject.c`,
`linuxkpi_hdmikmod.c`, `linuxkpi_videokmod.c` (HDMI infoframe + ACPI
video/backlight modules the DRM drivers link against).

### Intra-file deltas to re-sync to 14-STABLE

`pci.h` / `linux_pci.c` (the port's `extra-patch-linuxkpi-pci`, gated
`>=1403508`, assumes the 14-era `to_pci_dev(dev->dev)->bus` + `pci_domain_nr`
shape), `iosys-map.h` (replaces the older `dma-buf-map.h`), `mm.h`, `device.h`,
`shrinker.h`.

### Already present — no work needed

`sys/dev/vt` (newcons + `hw/fb` `vt_fb` + `hw/efifb`), `sys/dev/drm2/ttm`,
`sys/dev/acpica`, and the build glue in `sys/conf/kmod.mk`
(`LINUXKPI_GENSRCS` / `LINUXKPI_INCLUDES` already match FreeBSD 14).

## Plan

Two independent workstreams.

### A. LinuxKPI uplift to FreeBSD 14-STABLE (in `/usr/src`)

**Wholesale vendor import** of FreeBSD 14-STABLE's `sys/compat/linuxkpi` via the
repo's vendor-import workflow, establishing a clean 14-level baseline, then fix
base-kernel build breaks. Chosen over cherry-picking the ~28 missing files
because the port also depends on intra-file API additions, and the existing
ad-hoc state is harder to reason about than a clean re-baseline.

**Base-kernel dependency reconciliation** (where the real effort/risk is). The
14-level LinuxKPI calls base facilities that evolved 13.5→14. Iterate until the
modules link:

1. Build kernel + modules; collect undefined-symbol / signature errors.
2. For each, grep the FreeBSD 14 implementation for the base call
   (`vm_page`/pctrie, `bus_dma`, `taskqueue`, `sysctl`, `kobject`/sysfs, fbio)
   and confirm it exists at the right signature in `/usr/src/sys`.
3. Backport the minimal base KPI (or shim it) as separate, reviewable commits.

### B. Make the port recognize MidnightBSD (in `/usr/ports/graphics/`)

Patch the port Makefile locally rather than bumping `__FreeBSD_version`
(version bump = repo-wide blast radius). In
`/usr/ports/graphics/drm-515-kmod/Makefile` (and the `drm-kmod` meta-port as
needed):

- Drop/adjust the `OPSYS != FreeBSD` → `IGNORE` guard so MidnightBSD is allowed.
- Gate the `OSVERSION` / `extra-patch-linuxkpi-pci` branches on a MidnightBSD
  capability condition (e.g. `__MidnightBSD_version`) instead of `1403508` /
  `1500065`, so the PCI patch applies given our 14-level `pci.h`.
- Confirm `USES=kmod` finds `SRC_BASE=/usr/src/sys/Makefile`; build
  `gpu-firmware-kmod` (firmware-only, low risk).

## Critical files

- `/usr/src/sys/compat/linuxkpi/common/{include,src}/` — import target (A)
- `/usr/src/sys/sys/param.h` — version constants (read; bump only if route B changes)
- `/usr/src/sys/conf/kmod.mk` — build glue (verify; already 14-aligned)
- `/home/laffer1/git/freebsd/sys/compat/linuxkpi/` — FreeBSD 14-STABLE source of truth
- `/usr/ports/graphics/drm-515-kmod/{Makefile,Makefile.version,files/extra-patch-linuxkpi-pci}`
- `/usr/ports/graphics/drm-kmod/Makefile`, `/usr/ports/graphics/gpu-firmware-kmod/`

## Verification (build-only; do NOT kldload)

1. `cd /usr/src && make buildkernel` — kernel + in-tree LinuxKPI compile clean
   after import + dependency fixes.
2. `cd /usr/ports/graphics/drm-515-kmod && make` — port no longer `IGNORE`s and
   compiles; confirm `work/.../{drm,i915kms,amdgpu,radeonkms,ttm}.ko` produced.
3. Static link sanity: `nm -u` on the `.ko`s to confirm no unresolved LinuxKPI
   symbols against the built kernel. **No `kldload`.**
4. Run the C precommit sanity (cppcheck / clang-format) on base-kernel shim
   commits.

## Out of scope (follow-on)

`kldload`, vt/newcons KMS console handoff, interrupt routing, suspend/resume,
on-hardware modesetting.
