# AI Tools Compatibility: MidnightBSD vs GNU Coreutils

This document describes the differences between MidnightBSD's BSD utilities and GNU coreutils, focusing on features that AI agents commonly need for automation tasks.

---

## Table of Contents

1. [Completely Missing Utilities](#completely-missing-utilities)
2. [Missing Features from Existing Utilities](#missing-features-from-existing-utilities)
3. [Present Utilities with Notes](#present-utilities-with-notes)
4. [Top Missing Features for AI Agents](#top-missing-features-for-ai-agents)
5. [Summary](#summary)

---

## Completely Missing Utilities

Utilities that exist in GNU coreutils but are not present in MidnightBSD's base system:

| Utility | Purpose | BSD Alternative |
|---------|---------|-----------------|
| `base32` | Base32 encoding/decoding | `b64encode`/`b64decode` (Base64 only) |
| `base64` | Base64 encoding/decoding | `b64encode`/`b64decode` |
| `basenc` | General base encoding/decoding | None |
| `dir` | Brief directory listing | `ls -C -b` or `echo *` |
| `dircolors` | Color configuration for ls | None (use `ls -G` or `CLICOLOR=1`) |
| `hashsum` | Generic hash sum computation | `/sbin/md5sum`, `/sbin/sha256sum`, etc. |
| `numfmt` | Number formatting/conversion | None |
| `parallel` | Parallel execution | None (GNU parallel, not part of coreutils) |
| `pinky` | Lightweight finger | None |
| `shred` | Secure file deletion | None |
| `shuf` | Random permutation generator | ✅ **Imported** from OpenBSD |
| `tac` | Reverse concatenate files | None |
| `tempfile` | Create temporary files | `mktemp` |
| `vdir` | Verbose directory listing | `ls -l` |

---

## Missing Features from Existing Utilities

### `find`
- ❌ `-printf` - Custom output formatting (not supported)
- ✅ `-regex` / `-iregex` - Regular expression matching (supported)
- ✅ `-E` - Extended regular expressions (supported)

### `grep`
- ❌ `-P` / `--perl-regexp` - Perl-compatible regular expressions (not supported)
- ✅ `-E` - Extended regular expressions (supported)
- ✅ `-z` - Null-separated input (supported)
- ✅ `--include` / `--exclude` - File pattern filtering (supported)
- ✅ `-r` / `-R` - Recursive search (supported)

### `ls`
- ❌ `--color` - Colorized output (not supported)
- ✅ `-G` - BSD equivalent for colorized output
- ✅ `-h` - Human-readable sizes (supported)
- Note: Use `CLICOLOR=1` environment variable for color with `-G`

### `sed`
- ❌ `-i` - In-place file editing (not supported)
- ❌ `-i.bak` - In-place editing with backup (not supported)
- ✅ Standard stream editing (fully supported)

### `sort`
- ✅ `-V` / `--version-sort` - Natural/version sorting (supported)
- ✅ `-n` - Numeric sorting (supported)
- ✅ `-h` - Human-readable sorting (supported)
- ❓ `--parallel` - Parallel sorting (needs verification)

### `stat`
- ❌ `-c` / `--format` - Custom output format (not supported)
- ✅ `-f` - File system stat with format string (limited support)
- ✅ Standard stat output (supported)

### `date`
- ✅ `-I` / `--iso-8601` - ISO 8601 format (supported)
- ✅ `-Iseconds` - ISO format with seconds (supported)
- ❌ Some GNU-specific format extensions (needs verification)

### `xargs`
- ✅ `-P` / `--max-procs` - Parallel processing (supported)
- ✅ `-I` - Replace strings (supported)
- ✅ `-n` - Max args per command (supported)

### `tar`
- ✅ `--exclude` - Exclude patterns (supported)
- ✅ Standard tar operations (fully supported)

### `du` / `df`
- ✅ `-h` - Human-readable sizes (supported)
- ✅ `-s` - Summarize (supported)

---

## Present Utilities with Notes

The following utilities exist in MidnightBSD, sometimes in different locations:

| Utility | Location | Notes |
|---------|----------|-------|
| `b64encode` | `/usr/bin/` | Base64 encoding |
| `b64decode` | `/usr/bin/` | Base64 decoding |
| `checksums` | `/sbin/` | `md5sum`, `sha1sum`, `sha224sum`, `sha256sum`, `sha384sum`, `sha512sum` |
| `cksum` | `/usr/bin/` | Checksum utility |
| `install` | `/usr/bin/` | File installation |
| `xinstall` | `/usr/bin/` | BSD alternative to GNU install |
| `join` | `/usr/bin/` | Join lines from two files |
| `link` | `/bin/` | Create hard links |
| `mknod` | `/sbin/` | Create special files |
| `nl` | `/usr/bin/` | Number lines of files |
| `nohup` | `/usr/bin/` | Run command immune to hangups |
| `od` | `/usr/bin/` | Octal dump |
| `paste` | `/usr/bin/` | Merge lines of files |
| `pathchk` | `/usr/bin/` | Check file name validity |
| `printf` | `/usr/bin/` | Format and print data |
| `pr` | `/usr/bin/` | Convert text files for printing |
| `readlink` | `/usr/bin/` | Print resolved symbolic links |
| `realpath` | `/bin/` | Print resolved paths |
| `split` | `/usr/bin/` | Split file into pieces |
| `stdbuf` | `/usr/bin/` | Run command with modified I/O buffering |
| `sum` | `/usr/bin/` | Print checksum and block count |
| `timeout` | `/bin/` | Run command with timeout |
| `truncate` | `/usr/bin/` | Truncate files to specified size |
| `uuencode` | `/usr/bin/` | UU encoding |
| `uudecode` | `/usr/bin/` | UU decoding |

---

## Top Missing Features for AI Agents

For AI agents performing automation tasks, the most impactful missing features are:

1. **`sed -i`** - In-place file editing
   - **Impact**: Very High - Commonly used for batch file modifications
   - **Workaround**: Use `sed 's/old/new/' file > tmp && mv tmp file`

2. **`grep -P`** - Perl-compatible regular expressions
   - **Impact**: High - Needed for advanced pattern matching
   - **Workaround**: Use `perl -ne 'print if /pattern/'` or Python

3. **`find -printf`** - Custom output formatting
   - **Impact**: High - Powerful for data extraction and transformation
   - **Workaround**: Use `find ... -exec stat -f format {} \;` or post-process with awk

4. **`base64` / `base32`** - Standard encoding utilities
   - **Impact**: Medium - Commonly used in data processing
   - **Workaround**: Use `b64encode`/`b64decode` for Base64

5. **`shuf`** - Random shuffling of lines
   - **Impact**: Medium - Useful for generating test data
   - **Status**: ✅ **Imported** - Now available in `/usr/bin/shuf`

6. **`shred`** - Secure file deletion
   - **Impact**: Medium - Security-sensitive operations
   - **Workaround**: Use `rm -P` (if available) or overwrite then delete

7. **`tac`** - Reverse file printing
   - **Impact**: Medium - Useful for log processing
   - **Workaround**: Use `tail -r` (if available) or `awk '{a[i++]=$0} END {for (j=i-1; j>=0;) print a[j--] }'`

8. **`numfmt`** - Human-readable number conversion
   - **Impact**: Medium - Useful for parsing human-readable data
   - **Workaround**: Use `awk` or `perl`

9. **`parallel`** - Parallel execution
   - **Impact**: High for performance - Not part of coreutils but widely used
   - **Workaround**: Use `xargs -P` for parallel processing

---

## Summary

### Statistics
- **Total BSD utilities in source**: ~340 in `/usr/src/bin/` and `/usr/src/usr.bin/`
- **Completely missing coreutils**: ~14-19 (shuf now imported)
- **Utilities with feature differences**: ~10-15

### Key Differences
1. **Naming**: Some utilities have BSD-specific names (e.g., `b64encode` vs `base64`)
2. **Locations**: Some utilities are in `/sbin/` instead of `/usr/bin/` (e.g., checksum tools)
3. **Flags**: BSD uses different flags (e.g., `ls -G` vs `ls --color`)
4. **Features**: Some GNU extensions are not implemented (e.g., `sed -i`, `grep -P`)

### Recommendations for AI Agents
1. Use BSD-native alternatives where available
2. For missing features, use shell scripts, `perl`, or `awk` as workarounds
3. Check `/sbin/` for checksum utilities (`md5sum`, `sha256sum`, etc.)
4. Use `ls -G` instead of `ls --color`
5. Use `find -regex` instead of `find -printf` with post-processing

### Testing Notes
- Verified on: MidnightBSD (based on source tree at `/usr/src`)
- Date: 2026-07-09
- Method: Manual testing of utilities and feature flags

---

*This file was generated by Mistral Vibe to document compatibility differences for AI agent tooling.*
