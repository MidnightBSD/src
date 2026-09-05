# Packages on MidnightBSD installation media

MidnightBSD DVD images can carry a selected subset of a normal, blessed mport
repository.  The DVD contains the complete repository index but only the
package bundles needed for the installer choices and their runtime dependency
closure.  This keeps the image bounded while leaving the installed system with
a complete package catalog for later network use.

## Repository input

Build and bless the packages with Magus before building the release.  The
directory passed as `MPORT_REPOSITORY` must contain the uncompressed
`index.db` produced by `magus-bless` and every `.mport` bundle referenced by
that index.  The index and bundles must all come from the same Magus run and
match the release architecture and OS version.

The default repository location is:

```text
/usr/mports/Packages/${TARGET_ARCH}/All
```

`/usr/mports` is the canonical MidnightBSD mports path.  A release
configuration can override the repository and top-level choices:

```sh
MPORT_REPOSITORY="/path/to/blessed/repository"
DVD_PACKAGE_ROOTS="midnightbsd-desktop bash zsh sudo screen tmux nano vim rsync git"
```

`NOPKG` continues to disable third-party package staging.  Package payloads
are currently added only to `dvd1.iso`.

## Staging

`release/scripts/pkg-stage.sh` validates every configured root against the
blessed index, follows the `depends` table recursively, and copies the exact
bundle selected by each package name and version.  It rejects ambiguous or
missing index entries, missing bundles, unsafe filenames, and SHA-256
mismatches.  A staging failure stops the DVD build rather than producing
incomplete offline media.

The resulting `/packages` directory contains:

- `index.db`: the complete blessed repository index, including remote entries
  and mirrors.
- `installer.tsv`: curated application roots and their default checklist
  state.  `midnightbsd-desktop` is selected by default.
- `catalog.tsv`: a sanitized, searchable catalog of application packages.
- `.mport` files: only the configured roots and their recursive dependencies.

Only application roots (`packages.type = 0`) are offered.  The type stored in
the full index is not changed.

## Installer behavior

bsdinstall mounts the DVD package directory read-only at `/packages` inside
the target system.  Its package screen provides a curated checklist plus a
name-and-description search of the full catalog.  Search results are marked
as either present on the DVD or requiring the network.

The complete index is copied to `/var/db/mport/index.db` if the target does not
already have one.  Selected packages are installed with:

```sh
mport -U -c "$BSDINSTALL_CHROOT" install -y package-name
```

`mport install` is preferable to `mport add` here.  It uses the blessed index,
checks package hashes, resolves exact dependency versions, marks dependencies
automatic, uses `/packages` before attempting a download, and supports the
advanced network search.  `-U` prevents an index refresh during installation
without disabling downloads for bundles absent from the DVD.

Local curated packages must have their complete dependency closure on the
DVD.  Network-only installation failures are reported and return the user to
the package menu; they do not invalidate an otherwise completed base-system
installation.

## Future system packages

Installing the base operating system as mport packages is deliberately out of
scope.  The media layout and full index preserve `packages.type`, so a future
pkgbase-like installer can use system packages (`type = 1`) without changing
the third-party application menu.  Base-system extraction and application
selection should remain separate until that work is designed and implemented.

## Validation checklist

- Exercise dependency cycles, shared dependencies, missing bundles, duplicate
  roots, invalid types, and incorrect hashes with a synthetic index.
- Confirm that a disconnected installation of `midnightbsd-desktop` uses only
  DVD bundles and records its dependencies as automatic.
- Confirm that remote search results install when networking and DNS are
  configured and fail cleanly otherwise.
- Boot-test `dvd1.iso`, revisit Packages from Final Configuration, and verify
  package-media unmounting.
- Verify that disc1, bootonly, and memory-stick images do not gain the DVD
  package payload.
