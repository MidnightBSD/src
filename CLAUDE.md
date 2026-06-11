# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the MidnightBSD operating system source tree — a BSD-derived desktop OS. The repository contains the full OS source including the kernel (`sys/`), userland utilities (`bin/`, `sbin/`, `usr.bin/`, `usr.sbin/`), libraries (`lib/`, `libexec/`), and contributed third-party software (`contrib/`).

## AI Policy (Required Reading)

See `AI_POLICY.md` for the full policy. Key rules:

- **C/C++**: Restricted. Every line must be manually audited for malloc/free symmetry and bounds checking.
- **`contrib/mksh`**: AI assistance is **prohibited** — the shell author does not want it indexed or scanned.
- **Assembly**: AI assistance is **prohibited**.
- **Shell (sh)**: Permitted, but must be POSIX-compliant. Avoid Bash-isms.
- **Documentation**: Encouraged.

When AI assists in a commit, add the trailer: `AI-Assisted-by: Claude Sonnet 4.6`

## Contribution Requirements

- All contributions require a DCO sign-off (see `DCO.md`).
- Code must conform to the **style(9)** style guide (BSD kernel coding style).
- PRs use the template in `.github/pull_request_template.md`.

## Build System

MidnightBSD uses `bmake` (BSD make). The build system is driven by `Makefile` and `Makefile.inc1` at the root.

### Common Build Targets

```sh
# Build and install everything except the kernel
make buildworld
make installworld

# Build and install the kernel (run buildworld first)
make buildkernel KERNCONF=GENERIC
make installkernel KERNCONF=GENERIC

# Combined shorthand
make world   # buildworld + installworld
make kernel  # buildkernel + installkernel

# Clean up obsolete files after upgrade
make check-old
make delete-old
make delete-old-libs
```

### Cross-building (Linux/macOS)

The CI uses `tools/build/make.py` for cross-building from non-BSD hosts:

```sh
# Bootstrap bmake
./tools/build/make.py --debug TARGET=amd64 TARGET_ARCH=amd64 -n

# Build kernel toolchain then kernel
./tools/build/make.py TARGET=amd64 TARGET_ARCH=amd64 kernel-toolchain -j$(nproc)
./tools/build/make.py TARGET=amd64 TARGET_ARCH=amd64 KERNCONF=GENERIC NO_MODULES=yes buildkernel -j$(nproc)
```

### Build Variables

Key variables to set in `/etc/src.conf` or on the command line:
- `WITH_FOO` / `WITHOUT_FOO` — enable/disable build components
- `TARGET` / `TARGET_ARCH` — cross-compilation target
- `KERNCONF` — kernel configuration (default: `GENERIC`)
- `MAKEOBJDIRPREFIX` — where build artifacts go (needs ~6 GB minimum)

## Kernel Configuration

Kernel config files live in `sys/<arch>/conf/`:
- `GENERIC` — release configuration
- `LINT` — compile-only config for maximum build coverage
- `NOTES` — documentation of all possible config entries
- `MINIMAL` — minimal kernel configuration

## Code Style

- **Style guide**: style(9) — BSD kernel style with 8-space tabs
- **Formatter**: `.clang-format` is present (WebKit-based, 8-space tabs, 80 col limit, `UseTab: Always`)
- **Style CI check**: `tools/build/checkstyle9.pl` runs on PRs against C/C++/header files

Run the style checker manually:
```sh
tools/build/checkstyle9.pl --github <base-sha>..<head-sha>
```

## Testing

Tests use the **Kyua** test framework and are installed to `/usr/tests/`:

```sh
# Run the full test suite (after installworld)
kyua test -k /usr/tests/Kyuafile

# View results
kyua report
```

Tests live next to the source they test (e.g., `lib/libcrypt/tests/`). The `tests/` directory provides cross-functional tests and infrastructure.

To build with tests: ensure `MK_TESTS` is not disabled (it's on by default). Disable with `WITHOUT_TESTS` in `src.conf`.

## Repository Structure

```
bin/          # Basic user commands (/bin)
sbin/         # System administration (/sbin)
usr.bin/      # User commands (/usr/bin)
usr.sbin/     # System administration (/usr/sbin)
lib/          # System libraries
libexec/      # System daemons
sys/          # Kernel source
  kern/       # Core kernel
  dev/        # Device drivers
  net*/       # Networking subsystems
  vm/         # Virtual memory
  fs/         # Filesystems (non-UFS/NFS/ZFS)
  ufs/        # UFS/FFS filesystem
  <arch>/     # Architecture-specific (amd64, arm64, i386, etc.)
contrib/      # Third-party contributed software
crypto/       # Cryptographic libraries and commands
etc/          # Template files for /etc
share/        # Shared resources, man pages, mk files
  mk/         # BSD make include files
  man/man9/   # Kernel developer documentation (section 9 man pages)
tools/        # Build utilities and regression test helpers
tests/        # Cross-functional test suite and Kyua infrastructure
```

## Kernel Documentation

Kernel internals are documented in section 9 man pages under `share/man/man9/`. Start with `intro(9)` for an overview.
