---
name: bhyve-test
description: Boot-test a MidnightBSD src change in a VM on the build host - build world/kernel from a branch, install into a disk image (never the host), boot it in bhyve (new userboot and/or UEFI loader) or in QEMU (legacy BIOS via SeaBIOS: pmbr, gptboot, gptzfsboot, BIOS loader), run a scripted test inside the guest, and collect the serial log. Use whenever a change needs a real boot or runtime check that cannot be done on the host (loader, boot blocks, ZFS/UFS, kernel modules, rc scripts, installworld), or when the user asks to "test it in a VM", "spin up bhyve", "use QEMU" or "test BIOS boot".
---

# VM boot testing (bhyve and QEMU)

The goal is a reproducible, non-interactive boot test whose evidence is a
serial-console log. Everything the test creates lives in the scratchpad; the
host's root filesystem, `/boot`, and its pools are never modified.

## Ask first

These change host state, so get the user's approval for each before doing it
(an approval for one test run does not carry over to the next task):

- `kldload vmm` (check first with `kldstat | grep vmm`).
- Installing packages (firmware, expect).
- `installworld`/`installkernel`/`distribution` into a `DESTDIR`. This is not
  an install to the host, but say so explicitly; never run them without
  `DESTDIR`.
- `mdconfig` and creating/importing a test pool on the host.

## Prerequisites

| Need | Check | Notes |
|---|---|---|
| vmm.ko | `kldstat \| grep vmm` | `/dev/vmm/` stays empty until a VM exists; that is normal |
| UEFI firmware | `ls /usr/local/share/uefi-firmware/BHYVE_UEFI.fd` | mports `sysutils/edk2` FLAVOR=bhyve (`edk2-bhyve`). The 4.2 package was missing from the mirror once; building from mports works, and the 4.1 package's firmware works too |
| expect | `/usr/local/bin/expect` | only for interactive steps (single-user, loader prompt) |
| new loader | obj tree | `stand/userboot/userboot_lua/userboot_lua.so`, `stand/efi/loader_lua/loader_lua.efi` |
| QEMU (BIOS only) | `qemu-system-x86_64 --version` | mports `emulators/qemu` (no 4.2 package was available; build from mports). SeaBIOS is `/usr/local/share/qemu/bios-256k.bin` |

bhyve cannot run BIOS boot code (`pmbr`, `gptboot`, `gptzfsboot`, `zfsboot`,
the BIOS `/boot/loader`). Use QEMU for those (section 5); install the BIOS
boot blocks into every image anyway.

## 1. Build

Build from a plain export of the branch with a separate objdir, e.g.

```sh
git -C /usr/src archive <branch> | tar -x -C "${S}/tgt"
cd "${S}/tgt" && env MAKEOBJDIRPREFIX=/tmp/llvm21-obj make -j16 buildworld
env MAKEOBJDIRPREFIX=/tmp/llvm21-obj make -j16 buildkernel KERNCONF=GENERIC
```

- To refresh an existing export, copy only changed files
  (`git diff --name-only --diff-filter=AM <old> <new>` + `git archive ... |
  tar -x`) and delete removed ones; re-extracting everything resets mtimes
  and turns `-DNO_CLEAN` into a full rebuild.
- With `-DNO_CLEAN`, stale `.depend` files can resolve a renamed source to the
  wrong file (e.g. after a vendor layout change). Delete the affected objdirs.
- Hosts where `echo hi | cat /dev/stdin` fails (fdescfs) break the vi
  catalog's `gencat /dev/stdin`: add `WITHOUT_VI=yes` to build *and* install.
- Use the same `WITHOUT_*` options for installworld as for buildworld.

## 2. Disk image

Use a sparse file on an md(4) device. **Use `mnbsd-*` partition types** (all
examples below do). The loader built from this tree (`stand/common/part.c`,
`stand/libsa/zfs/zfs.c`, `stand/libsa/gpt.c`) also recognizes `freebsd-ufs`
and `freebsd-zfs` on GPT disks as a fallback, and prefers `mnbsd-*` when a
disk has both. Loaders older than that change (for example the host's
`/boot/userboot.so` on older systems) show `freebsd-*` partitions as
"Unknown" and will not find the root, so they require `mnbsd-*`. For BIOS
boot, `pmbr` only loads an `mnbsd-boot` partition; a `freebsd-boot`
partition is not booted. `boot1.efi` from this tree also prefers `mnbsd-*`
over `freebsd-ufs`/`freebsd-zfs`; older ones boot the first partition with
`/boot/loader.efi`, whatever its type.

```sh
truncate -s 12G "${V}/disk.img"
md=$(mdconfig -a -t vnode -f "${V}/disk.img")
gpart create -s gpt ${md}
gpart add -t efi -s 260M ${md}
gpart add -t mnbsd-boot -s 512K ${md}
gpart add -t mnbsd-swap -s 1G -l vmswap ${md}
gpart add -t mnbsd-zfs ${md}            # or mnbsd-ufs
gpart bootcode -b ${OBJ}/stand/i386/pmbr/pmbr \
    -p ${OBJ}/stand/i386/gptzfsboot/gptzfsboot -i 2 ${md}
newfs_msdos -F 32 -c 1 -L EFISYS /dev/${md}p1
```

ZFS root (`makefs -t zfs` is not available here):

```sh
zpool create -o altroot="${V}/mnt" -O mountpoint=none -O compression=lz4 \
    -O atime=off vmtest /dev/${md}p4
zfs create -o mountpoint=none vmtest/ROOT
zfs create -o mountpoint=/ -o canmount=noauto vmtest/ROOT/default
zfs mount vmtest/ROOT/default
zpool set bootfs=vmtest/ROOT/default vmtest
```

- Give the test pool a unique name (`vmtest`, `vmnew`). Never name it `zroot`
  and never run `zpool import` without an explicit pool name and `-d`.
- If the host's ZFS is older than the guest's, the host cannot import the
  pool once the guest upgrades it. Don't try; inspect it from inside a VM.
- Long altroot paths get truncated in the mount table, so `zfs unmount` can
  fail; `umount <path>` works.

Install and configure:

```sh
cd "${S}/tgt" && env MAKEOBJDIRPREFIX=/tmp/llvm21-obj make installworld \
    installkernel distribution KERNCONF=GENERIC DESTDIR="${V}/mnt"
mount -t msdosfs /dev/${md}p1 "${V}/esp"
mkdir -p "${V}/esp/EFI/BOOT"
cp "${V}/mnt/boot/loader.efi" "${V}/esp/EFI/BOOT/BOOTX64.efi"
umount "${V}/esp"
```

Guest files:

- `/boot/loader.conf`: `console="comconsole"`, `boot_serial="YES"`,
  `autoboot_delay="2"`, `beastie_disable="YES"`, plus `zfs_load="YES"` and
  `vfs.root.mountfrom="zfs:vmtest/ROOT/default"` for ZFS.
- `/etc/rc.conf`: hostname, `zfs_enable`, `sendmail_enable="NONE"`,
  `dumpdev="NO"`, `cron_enable="NO"`, no networking.
- `/etc/fstab`: swap only (`/dev/gpt/vmswap`) for ZFS root.
- `/etc/rc.local`: run the test script, tee to `/dev/console` and a log.

Then detach from the host before any VM touches the disk:
`zfs unmount`/`umount`, `zpool export vmtest`, `mdconfig -d -u N`. A pool
must never be imported by the host and a guest at the same time.

## 3. Test script in the guest

Drive multi-boot tests with a stage machine in `/root/<name>test.sh` keyed
on a state file such as `/var/db/<name>test.state`:

- Print every step with a fixed prefix (`ZFSTEST: ...`) and an explicit
  `RESULT PASS <name>` / `RESULT FAIL <name> (rc=N)` line, so the host can
  grep the serial log.
- Start each boot with `uname -a`, `sysctl machdep.bootmethod` (proves
  UEFI vs userboot) and the relevant `kenv` lines (`currdev`, `loaddev`).
- End a stage with `shutdown -r now` (bhyve exits 0) or `poweroff`
  (bhyve exits 1), after writing the next stage number.
- Re-check your own test logic: a FAIL can be a script bug (e.g. `zfs
  rollback` across newer snapshots needs `-r`). Say which in the report.

## 4. Running VMs non-interactively

Use `scripts/runvm.sh`:

```sh
sh scripts/runvm.sh <vmname> <userboot|olduserboot|uefi> <timeout> <log> <disk1> [disk2]
```

What it handles and why:

- stdin comes from a FIFO held open by a process that never writes, so the
  loader sees no keypresses and nothing blocks on a closed stdin.
- `timeout` wraps both `bhyveload` and `bhyve` so a hung guest cannot run
  forever.
- bhyve drops `O_APPEND` on its stdout and overwrites the start of the log,
  so the bhyve output goes to a separate file that is appended afterwards.
- `bhyvectl --destroy` runs before and after every boot.
- bhyve exit codes: 0 reboot, 1 poweroff, 2 halt, 3 triple fault, 4 error,
  124 = killed by timeout.

Loaders:

- **userboot**: `bhyveload -l <obj>/userboot_lua.so -m 4G -d disk.img -c stdio
  <name>`, then `bhyve` without a bootrom. The host's `/boot/userboot.so` is
  the *old* loader; use it as a negative test when a change adds loader
  support (it should fail with a clear message, which proves the test
  exercises the new code).
- **UEFI**: `bhyve ... -l bootrom,/usr/local/share/uefi-firmware/BHYVE_UEFI.fd`,
  booting the image's ESP `EFI/BOOT/BOOTX64.efi`.
- Cover each loader at least once against the final on-disk state.

For interactive steps (boot single-user, run a command at the loader `OK`
prompt), spawn `bhyveload`/`bhyve` under expect: wait for the autoboot
countdown, send a space, then `boot -s` at `OK`, wait for the single-user
shell prompt, run commands, then `halt -p`.

## 5. BIOS boot with QEMU

Use `scripts/qrun.sh`:

```sh
sh scripts/qrun.sh <tag> <disk> <log> [snapshot]
```

- It runs `qemu-system-x86_64 -machine pc` with SeaBIOS and the disk on IDE,
  `-display none -monitor none -serial file:<log> -no-reboot`, wrapped in
  `timeout`. QEMU exits when the guest powers off or reboots.
- Pass `snapshot` to add `-snapshot`: writes go to a temporary overlay, so
  one image can be booted repeatedly without changing it. Without it the
  guest's stage machine advances as usual.
- There is no hardware acceleration here (pure TCG emulation): a boot to
  multi-user takes a few minutes. Use a generous timeout (600-900 s).
- QEMU has no VM state to destroy and needs no `vmm.ko`.
- To test a boot block from a build, put it on a copy of an image:
  `gpart bootcode -b <obj>/stand/i386/pmbr/pmbr -p <obj>/stand/i386/gptzfsboot/gptzfsboot -i <bootpart> mdN`
  (or `gptboot` for UFS). Copy sparse images with
  `dd if=... of=... bs=1m conv=sparse`.
- `pmbr`, `gptboot` and `gptzfsboot` print to the VGA screen, not to
  the serial port, unless `/boot.config` contains `-h`. A boot-block failure
  therefore shows as an empty serial log and a timeout (rc 124). To see the
  message, add `-monitor unix:<sock>,server,nowait` and use the monitor's
  `screendump <file.ppm>`, or put `-h` in `/boot.config`.
- From `/boot/loader` on, `console="comconsole"` in loader.conf sends
  everything to the log; `sysctl machdep.bootmethod` prints `BIOS`.
- `pmbr` only loads a partition of type `mnbsd-boot`; an image partitioned
  with `freebsd-boot` will not boot under BIOS.

A loader or boot-block change needs both: bhyve for userboot and UEFI, QEMU
for BIOS. Report them in one matrix.

## 6. Report and cleanup

Report: per run, the VM (bhyve/QEMU), the loader or boot block, the
disk/pool state at boot, the exit code and log path; PASS/FAIL lines with
excerpts; what was not tested and why (interactive-only paths, anything
that needs real hardware); problems found, separating regressions from
pre-existing issues.

Cleanup:

- `mdconfig -l` is empty, `bhyvectl --destroy` done, no `bhyve`/`qemu`/
  `expect`/sleeper processes left.
- Host `zpool list` shows only the host's own pools.
- Keep disk images in the scratchpad and tell the user their size
  (`du -h`), so they can delete them. Installed trees have schg flags:
  `chflags -R noschg` before `rm -rf` (literal scratchpad paths only).
