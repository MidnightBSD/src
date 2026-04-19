# prowld: MidnightBSD Service Management Daemon

**Specification Draft v0.1**

## 1. Overview

`prowld` is a service management daemon for MidnightBSD inspired by Apple's
`launchd` and Linux's `systemd`. It supersedes the traditional sequential
`rc.d` startup model with a dependency-aware, parallel-execution service
manager while maintaining full backward compatibility with existing `rc.d`
scripts and the `service(8)` command.

The companion utility `prowlctl(8)` provides a command-line interface for
querying and controlling services under `prowld`'s management.

### 1.1 Goals

- Parallel service startup for faster boot times
- Declarative service configuration in UCL/JSON (parsed via `libucl`)
- First-class dependency resolution (requires, wants, before, after, conflicts)
- Integrated periodic task scheduling (cron-like functionality)
- Service supervision: restart on failure, keep-alive policies
- Backward compatibility with existing `/etc/rc.d` and `/usr/local/etc/rc.d` scripts
- Preserve `rcorder(8)` semantics (REQUIRE/PROVIDE/BEFORE/KEYWORD)
- Preserve `service(8)` command behavior
- Preserve `rc.conf` variable semantics
- Structured logging with per-service streams
- On-demand service activation via socket activation
- Clean separation of policy (configuration) from mechanism (the daemon)

### 1.2 Non-Goals

- Replacing `init(8)` as PID 1 in the initial release (see §3.1)
- Replacing the kernel boot process or loader
- Managing per-user agents in v1.0 — scheduled for v1.1
- Managing services running inside jails — each jail has its own init and
  service manager; prowld treats jails as opaque units from the host side
- Cgroup-style resource isolation (BSD has jails/rctl; integration is future work)
- A binary journal format (structured text logs via syslog initially)

### 1.3 Design Principles

1. **Do one thing well.** prowld manages service lifecycles. It does not reinvent
   logging, networking, or device management.
2. **Fail safe.** A misconfigured service must not prevent the system from booting.
3. **Backward compatibility is non-negotiable.** Existing rc.d scripts must continue
   to work without modification.
4. **Configuration is declarative.** Imperative shell logic belongs in rc.d scripts,
   not in prowld configuration.
5. **Observability by default.** Every state transition should be loggable and queryable.
6. **No binary caches.** Unit files are parsed from disk on every boot. Parsing
   is cheap, correctness is not — a stale cache is a class of bug prowld will
   not have.

---

## 2. Terminology

| Term | Definition |
|------|-----------|
| **Job** | A managed unit of work (daemon, oneshot task, or timer) |
| **Service** | A long-running job (daemon) |
| **Oneshot** | A job that runs to completion and exits |
| **Timer** | A job activated on a schedule |
| **Socket job** | A job activated when a listening socket receives a connection |
| **Label** | Unique reverse-DNS identifier for a job (e.g., `org.midnightbsd.sshd`) |
| **Unit file** | A configuration file defining one job |
| **rc-shim** | The compatibility layer that wraps legacy rc.d scripts as prowld jobs |

---

## 3. Architecture

### 3.1 Process Model

For the initial release, `init(8)` remains PID 1. `prowld` is started by init
early in the boot process (replacing the bulk of `/etc/rc`) and becomes the
parent of all managed services.

```
init (PID 1)
 └── prowld (PID 2)
      ├── service A
      ├── service B
      ├── rc-shim for legacy rc.d script
      │    └── legacy daemon (reparented to prowld)
      └── prowlctl handler threads
```

A future version may migrate prowld to PID 1 once the design is proven. This
migration is explicitly deferred and out of scope for v1.0.

**Rationale:** Keeping init as PID 1 minimizes the blast radius of prowld bugs
during early adoption. Users can fall back to traditional rc.d by editing a
single line in `/etc/rc` if prowld fails.

### 3.2 Components

| Component | Purpose |
|-----------|---------|
| `prowld(8)` | The service management daemon |
| `prowlctl(8)` | CLI for querying and controlling prowld |
| `libprowl` | Shared library exposing the IPC protocol |
| `rc-shim` | Wrapper that presents legacy rc.d scripts as jobs |
| `prowl-rcorder` | Parses rc.d REQUIRE/PROVIDE/BEFORE for dependency graph |
| `prowl-generator(8)` | Optional: generates native unit files from rc.d scripts |

### 3.3 Startup Sequence

1. Kernel hands off to init
2. init mounts root, runs early `/etc/rc.d` entries required before prowld
   (e.g., `mountcritlocal`, `hostid`, `random`) — configurable whitelist
3. init execs `prowld`
4. prowld reads `/etc/rc.conf` and `/etc/prowld/prowld.conf`
5. prowld scans unit directories (§6.1) and builds the dependency graph
6. prowld scans `/etc/rc.d` and `/usr/local/etc/rc.d`, wrapping each as an
   rc-shim job unless a native unit file with the same PROVIDE exists
7. prowld computes a topological sort and launches jobs in parallel waves
   respecting dependencies
8. prowld enters its main loop: event-driven supervision

---

## 4. Configuration Format

### 4.1 File Format

Unit files are parsed by `libucl`, which accepts UCL, JSON, and YAML-like
syntax. JSON is the recommended canonical format; UCL is acceptable for
hand-edited files.

File extension: `.unit` (language-neutral; libucl auto-detects syntax).

### 4.2 Minimal Unit File

```json
{
  "label": "org.midnightbsd.sshd",
  "description": "OpenSSH Daemon",
  "program": "/usr/sbin/sshd",
  "arguments": ["-D"],
  "run_at_load": true,
  "keep_alive": true
}
```

### 4.3 Full Schema

```json
{
  "label": "org.example.myservice",
  "description": "Human-readable description",
  "type": "daemon",
  "rc_name": "myservice",

  "program": "/usr/local/bin/myservice",
  "arguments": ["--flag", "value"],
  "environment": {
    "PATH": "/usr/local/bin:/usr/bin:/bin",
    "MYSERVICE_MODE": "production"
  },
  "working_directory": "/var/db/myservice",
  "root_directory": null,

  "user": "myservice",
  "group": "myservice",
  "umask": "0022",

  "run_at_load": true,
  "keep_alive": {
    "successful_exit": false,
    "crashed": true,
    "after_initial_demand": false
  },
  "throttle_interval": 10,
  "exit_timeout": 20,

  "requires": ["network", "syslogd"],
  "wants": ["ntpd"],
  "before": [],
  "after": ["local-fs"],
  "conflicts": [],
  "provides": ["myservice"],

  "standard_in_path": "/dev/null",
  "standard_out_path": "/var/log/myservice.log",
  "standard_error_path": "/var/log/myservice.err",

  "resource_limits": {
    "max_open_files": 4096,
    "max_processes": 128,
    "max_memory": "512M"
  },

  "nice": 0,
  "jail": null,

  "sockets": {
    "listener": {
      "sock_type": "stream",
      "sock_family": "inet",
      "sock_node_name": "0.0.0.0",
      "sock_service_name": "myservice"
    }
  },

  "notify_type": "none",
  "watchdog_sec": 0,

  "mdns": {
    "register": false,
    "name": "%l",
    "type": "_http._tcp",
    "port": 8080,
    "txt_record": {}
  },

  "schedule": null,

  "watch_paths": [],
  "queue_directories": []
}
```

### 4.4 Field Reference

**Identity**
- `label` *(required, string)*: Unique reverse-DNS identifier
- `description` *(string)*: Human-readable description
- `type` *(string)*: One of `daemon`, `oneshot`, `timer`, `socket`. Default: `daemon`
- `rc_name` *(string, optional)*: rc.conf variable prefix for enable state.
  Defaults to the last dot-separated component of `label` (e.g.,
  `org.midnightbsd.sshd` → `sshd`, producing `sshd_enable`). See §7.6.

**Execution**
- `program` *(required unless `arguments[0]` is a full path)*: Path to executable
- `arguments` *(array of strings)*: argv (argv[0] is `program` basename by default)
- `environment` *(object)*: Environment variables
- `working_directory` *(string)*: Working directory
- `root_directory` *(string)*: chroot target (optional)

**Privilege**
- `user`, `group` *(string)*: Drop privileges to this user/group
- `umask` *(string)*: Octal umask

**Lifecycle**
- `run_at_load` *(bool)*: Start immediately when loaded
- `keep_alive` *(bool or object)*: Restart policy
- `throttle_interval` *(int, seconds)*: Minimum time between restart attempts
- `exit_timeout` *(int, seconds)*: Time to wait after SIGTERM before SIGKILL

**Dependencies** — see §5
- `requires`, `wants`, `before`, `after`, `conflicts`, `provides`

**I/O**
- `standard_in_path`, `standard_out_path`, `standard_error_path`

**Resources**
- `resource_limits`: maps to `setrlimit(2)`
- `nice`: nice value
- `jail`: jail name. Sets an implicit `requires` on the named jail and
  execs the program via `jexec(8)` inside it. In v1.0 the jail must be
  started by the rc-shim'd `/etc/rc.d/jail` (see §7.5); per-jail units
  land in v1.1.

**Activation**
- `sockets`: inetd-style socket activation (§9)
- `schedule`: calendar/periodic trigger (§12)
- `watch_paths`, `queue_directories`: filesystem-triggered activation

**Readiness** — see §10
- `notify_type`: `none` (default) or `notify` (sd_notify-compatible)
- `watchdog_sec`: watchdog interval in seconds; 0 disables

**Service discovery** — see §11
- `mdns`: optional mDNSResponder registration block

---

## 5. Dependency Resolution

### 5.1 Relationship Types

| Relationship | Semantics |
|--------------|-----------|
| `requires: [X]` | X must be running before this job starts. If X fails, this job fails. |
| `wants: [X]` | Start X if not running, but do not fail if X fails. |
| `before: [X]` | This job must be started before X. |
| `after: [X]` | This job must be started after X. |
| `conflicts: [X]` | This job cannot run while X is running. Starting this stops X. |
| `provides: [name]` | This job satisfies the named capability. |

Note: `requires` and `wants` imply `after` unless `before` is explicitly set.

### 5.2 Dependency Graph

prowld builds a directed acyclic graph (DAG) from all loaded unit files plus
rc-shim jobs. Each node is a job; edges represent `after` relationships
(derived from `requires`, `wants`, `after`, and the inverse of `before`).

Cycle detection is performed at load time. A cycle is a fatal configuration
error for the services involved; the system continues booting without them
and logs the error.

### 5.3 Topological Scheduling

prowld does not compute rigid "waves." Instead, it uses an event-driven
scheduler: a job becomes eligible to start when all of its `requires` and
`after` dependencies have reached the `running` state (or `completed` for
oneshots). Eligible jobs are launched immediately in parallel, bounded by a
configurable concurrency limit (default: `ncpu * 2`).

### 5.4 Provides/Virtual Services

A job may declare `provides: ["network"]`. Other jobs depending on `network`
will be satisfied when any job providing it reaches `running`. Multiple
providers may exist; the first to become ready satisfies the dependency.

Standard virtual provides (aligned with existing rc.d conventions):

- `early` — very early boot (mounts, random)
- `local-fs` — local filesystems mounted
- `network` — network stack up
- `network-online` — at least one non-loopback route available
- `syslog` — syslog available
- `dns` — name resolution available
- `multi-user` — system ready for login

---

## 6. Filesystem Layout

### 6.1 Unit File Directories

Searched in this order (later overrides earlier):

1. `/etc/prowld/units.d/` — base system unit files (package-owned, do not edit)
2. `/usr/local/etc/prowld/units.d/` — ports/packages unit files
3. `/etc/prowld/overrides.d/` — local administrator overrides
4. `/var/run/prowld/generated.d/` — runtime-generated (e.g., rc-shim)

A unit file at a higher-priority path with the same `label` fully replaces
the lower-priority file. Partial overrides are supported via drop-in files:
`/etc/prowld/overrides.d/org.example.myservice.d/local.unit`.

### 6.2 Runtime State

`/var/run/prowld/` lives on tmpfs; everything in it is lost on reboot by design.

- `/var/run/prowld/prowld.sock` — control socket
- `/var/run/prowld/prowld.pid` — pid file
- `/var/run/prowld/reexec.state` — handoff file written during `daemon-reexec`,
  consumed and unlinked by the successor process. Never present between boots.
- `/var/log/prowld/` — prowld's own logs
- `/var/log/prowld/jobs/<label>.log` — per-job logs (if configured)

### 6.3 Persistent State

prowld deliberately does not use a database. Persistent state is split across
a small number of simple filesystem objects plus the existing `rc.conf`, each
chosen to match the nature of the data it holds. See §6.5 for the rationale.

**Enable state — `/etc/rc.conf`**

Service enable state lives in `rc.conf` as the `<rc_name>_enable` variable,
for both rc-shim'd and native units. `prowlctl enable` and `sysrc` write the
same variable and are genuinely equivalent (§7.6). No separate prowld-managed
store exists for enabled/disabled state.

**Masking — symlinks**

- `/var/db/prowld/masked.d/<label>` — symlink to `/dev/null`. Presence means
  masked (cannot be started, even as a dependency). Created by `prowlctl mask`,
  removed by `prowlctl unmask`.

Masking is a prowld-specific concept with no rc.conf equivalent, so it has
its own storage. Symlink creation and removal are atomic single syscalls,
so the state cannot be observed in a torn form — even across a crash
mid-operation. An administrator can inspect or repair masking state with
`ls`, `ln`, and `rm` when prowld is not running.

**Persistent timer state — flat UCL file**

- `/var/db/prowld/timers.state` — UCL-formatted file recording the last
  successful run of each `schedule.persistent: true` timer. Written atomically
  via `write-tempfile → fsync → rename(2)`.

Example contents:

```ucl
"org.midnightbsd.periodic.daily" {
    last_run = 1713312060
    last_exit = 0
}
"org.midnightbsd.periodic.weekly" {
    last_run = 1712707260
    last_exit = 0
}
```

Write frequency is bounded by the timer firing frequency (at most once per
scheduled event), so write amplification is not a concern in practice.

### 6.4 Configuration

- `/etc/prowld/prowld.conf` — daemon-level configuration
- `/etc/rc.conf` — **still honored** for backward compatibility

### 6.5 State Design Rationale

A database was considered and rejected. The persistent state prowld needs to
track is small (kilobytes), infrequently written, and naturally decomposable
into independent per-service flags plus a bounded set of timer timestamps.
SQL query power, indexing, and transactions offer no benefit at this volume.

The chosen approach has several properties that matter in an init-system
context:

- **No external library dependency.** libucl is already required for unit
  files; no additional code is pulled in for state.
- **Inspectable and repairable with standard tools.** If prowld fails to
  start, an administrator can see and fix the state with `ls`, `ln`, `rm`,
  and a text editor.
- **Atomic by construction.** `symlink(2)` and `rename(2)` are atomic; there
  is no window in which state can be observed half-updated.
- **Consistent with the "no binary caches" principle** (§1.3): the same
  reasoning that keeps unit files as text keeps state as text.

The one scenario that would motivate reconsidering is a future need for
historical analytics over state transitions (e.g., "which services have
restarted most often across the last 30 boots"). If that arrives, the
timer-state file can migrate to SQLite independently without disturbing
the symlink-based enable/mask state.

---

## 7. Backward Compatibility

### 7.1 rc.d Compatibility

All existing scripts in `/etc/rc.d` and `/usr/local/etc/rc.d` are discovered
at startup. For each script, prowld:

1. Parses REQUIRE, PROVIDE, BEFORE, KEYWORD headers using `prowl-rcorder`
2. Synthesizes an ephemeral unit file mapping rc.d headers to prowld fields:
   - `REQUIRE:` → `requires:`
   - `BEFORE:`  → `before:`
   - `PROVIDE:` → `provides:`
   - `KEYWORD: nojail` → sets a `conditions` predicate
3. Registers the script as a `type: rcshim` job
4. To start: invokes `/etc/rc.d/script start` with the standard environment
5. To stop: invokes `/etc/rc.d/script stop`

rc.conf variables (`foo_enable`, `foo_flags`, `foo_user`, etc.) are read and
honored exactly as the existing rc.subr does. A service with
`foo_enable="NO"` is loaded but not started.

### 7.2 service(8) Compatibility

The `service(8)` command is rewritten as a thin wrapper:

- `service foo start` → `prowlctl start foo` (which resolves to either a native
  unit or an rc-shim)
- `service foo status` → `prowlctl status foo`
- `service -e` → `prowlctl list --enabled`
- `service -l` → `prowlctl list`
- `service -r` → startup ordering (preserved for scripts that parse this)

The existing `service(8)` flags and output format are preserved. Scripts in
the ports tree that invoke `service` continue to work.

### 7.3 Conflict Resolution

If a native unit file and an rc.d script both claim the same `provides`, the
native unit wins and the rc.d script is skipped. This allows incremental
migration: a port can ship a native unit file, and the rc.d script is
automatically retired.

### 7.4 Migration Tool

`prowl-generator(8)` parses an rc.d script and emits a native unit file as a
starting point for porting. Manual review is required — the generator
handles the common case (simple daemon with rc.conf variables) but cannot
translate arbitrary shell logic.

### 7.5 Jail Subsystem

In v1.0, the jail subsystem is handled entirely via rc-shim wrapping
`/etc/rc.d/jail`. Services that require a jail declare `requires: [jail]`,
which resolves to the rc-shim'd jail unit and thus the full jail subsystem.
All jails start and stop as a single group, preserving exact compatibility
with the existing rc.d model. `jail.conf` remains the authoritative
configuration; prowld does not parse it in v1.0.

**Per-jail units — one prowld unit per jail, synthesized from `jail.conf` —
are scheduled for v1.1** (§19). This enables fine-grained dependencies
(e.g., `requires: [web-jail]` rather than depending on every jail), parallel
startup of independent jails, and per-jail restart policies. The migration
is source-compatible: units that depend on `jail` (the rc-shim aggregate)
continue to work unchanged, and hosts with many independent jails will see
measurable boot-time improvements automatically.

**Services inside jails remain out of scope in both v1.0 and v1.1.** Each
jail has its own init and service manager. A unit file's `jail: <name>`
field causes the program to be `jexec`'d into the named jail from the host,
but prowld does not cross the jail boundary to manage the jail's internal
service tree. Admins who want prowld inside a jail can run a separate
prowld instance there, managed by that jail's init — this is no different
from running any other service manager in a jail.

### 7.6 rc.conf and sysrc Integration

prowld treats `/etc/rc.conf` as authoritative for service enable state, for
both rc-shim'd and native units. This preserves the existing ecosystem of
automation — Ansible's `sysrc` module, Puppet's `sysrc` provider, port
post-install scripts, and admin workflows — without modification.

#### 7.6.1 Enable State

`prowlctl enable <label>` is implemented as a call to `sysrc(8)`. The
following pairs are genuinely equivalent — they write the same variable
to the same file and produce identical prowld behavior:

```
prowlctl enable sshd        ≡  sysrc sshd_enable=YES
prowlctl disable sshd       ≡  sysrc sshd_enable=NO
```

There is no dual state to reconcile. An admin who edits `rc.conf` directly,
runs `sysrc`, or runs `prowlctl enable` reaches the same result by any path.
On startup, prowld reads `rc.conf` to determine which services to launch, in
the same way `rc(8)` does today.

#### 7.6.2 Variable Name Derivation

For rc-shim'd services, the variable name is the rc.d script's name (as
today: `/etc/rc.d/sshd` → `sshd_enable`).

For native units, the variable name comes from the unit's optional `rc_name`
field. If omitted, it is derived from the last dot-separated component of
the `label`: `org.midnightbsd.sshd` → `sshd_enable`. Unit authors can set
`rc_name` explicitly to override the derivation — e.g., to avoid collisions
between two units whose labels would derive to the same name, or to match
an existing port's established variable name.

#### 7.6.3 Flags and Other Per-Service Variables

rc.conf variables beyond the `_enable` flag (e.g., `<name>_flags`,
`<name>_user`, `<name>_chdir`, `<name>_program`) are handled differently
depending on the service's origin:

- **rc-shim'd services**: all such variables are consumed by `rc.subr`
  exactly as today. No change from current MidnightBSD behavior.
- **Native units**: the unit file itself is authoritative for arguments,
  user, working directory, and similar fields. Local customization is done
  via drop-in files in `/etc/prowld/overrides.d/<label>.d/`. prowld does
  **not** read `<name>_flags`, `<name>_user`, etc. for native units.

This asymmetry is deliberate. Enable state is the one rc.conf concept prowld
preserves end-to-end, because the value of keeping `sysrc`-based automation
working outweighs the cost of the exception. Splitting an individual service's
configuration across rc.conf variables and a unit file would reintroduce the
dual-sourcing problem that native unit files exist to eliminate.

Porters converting an rc.d script to a native unit should move per-service
flag variables from `rc.conf` defaults into the unit file itself. The
`prowl-generator(8)` tool (§7.4) does this automatically for common cases.

#### 7.6.4 Non-Service rc.conf Variables

`rc.conf` is also the configuration file for many system-level settings that
are not services at all: `hostname`, `ifconfig_*`, `kld_list`, `defaultrouter`,
`sshd_keys`, and many others. These are **entirely outside prowld's scope**.

The early rc scripts that run before prowld starts (§3.3) continue to process
these variables exactly as today. prowld never reads them and has no opinion
about them.

---

## 8. Parallel Execution

### 8.1 Scheduler

The scheduler maintains three sets:

- **Ready**: dependencies satisfied, eligible to start
- **Running**: currently starting or running
- **Waiting**: blocked on dependencies

On each state transition of a job, the scheduler re-evaluates the Waiting set
and promotes newly-eligible jobs to Ready. The Ready set is drained into
Running subject to the concurrency limit.

### 8.2 Concurrency Limit

Configurable in `prowld.conf`:

```
max_concurrent_starts = auto   # auto = ncpu * 2
```

This bounds the number of simultaneously-starting jobs, not the number of
running jobs. Once a job reaches `running`, its slot is released.

### 8.3 Startup Completion

A daemon is considered `running` when:

- **Default**: its process is alive after `throttle_interval` seconds
- **With `notify_socket`**: it sends a ready notification (similar to
  sd_notify). Recommended for v1.1.
- **With `sockets`**: immediately after prowld has bound the socket

Oneshot jobs are considered `completed` when they exit with status 0.

---

## 9. Socket Activation

For `type: socket` jobs (or any job with a `sockets` block), prowld:

1. Binds the socket(s) at startup
2. Does NOT start the program
3. On first connection/datagram, starts the program and passes the socket(s)
   as inherited file descriptors starting at fd 3

This enables parallel startup of services that would otherwise need to wait
for each other: consumers can connect immediately; prowld queues the
connection until the provider is ready.

### 9.1 Environment Protocol

prowld uses the systemd-compatible `LISTEN_FDS` protocol so that daemons
built for Linux work without modification and `sd_listen_fds()` in the
libsystemd shim (§10.3) is a trivial thin wrapper:

| Variable | Meaning |
|----------|---------|
| `LISTEN_FDS` | Number of file descriptors passed, starting at fd 3 |
| `LISTEN_PID` | PID the fds were intended for; daemons verify this matches their own PID |
| `LISTEN_FDNAMES` | Colon-separated list of logical names matching the order of the fds; derived from the keys in the unit's `sockets` block |

File descriptors are passed starting at fd 3 (i.e., the first socket is
fd 3, the next is fd 4, and so on). `LISTEN_FDS` holds the count;
`LISTEN_FDNAMES` maps positions to names for daemons that manage multiple
listeners.

This matches systemd's documented `sd_listen_fds_with_names(3)` protocol
exactly; the same daemon binary consumes sockets identically on Linux and
MidnightBSD.

---

## 10. Ready Notification (sd_notify Compatibility)

prowld implements a native ready-notification protocol that is wire-compatible
with systemd's `sd_notify(3)`. This allows MidnightBSD to run Linux-origin
software using `sd_notify` without modification, and lets native MidnightBSD
software use the same mechanism for consistency.

### 10.1 Protocol

When a job is configured with `notify_type: "notify"`, prowld:

1. Creates a Unix datagram socket before `fork(2)`/`execve(2)`
2. Sets `NOTIFY_SOCKET=/var/run/prowld/notify/<label>` in the job's environment
3. Waits for the job to send `READY=1` on that socket before marking it `running`

Messages are newline-separated `KEY=value` pairs in a single datagram.
Supported keys (subset of the sd_notify protocol):

| Key | Meaning |
|-----|---------|
| `READY=1` | Service finished startup; mark as running |
| `RELOADING=1` | Service is reloading configuration |
| `STOPPING=1` | Service is shutting down |
| `STATUS=<text>` | Human-readable status line (shown by `prowlctl status`) |
| `ERRNO=<n>` | Error code if startup failed |
| `MAINPID=<pid>` | Main PID (for services that fork) |
| `WATCHDOG=1` | Watchdog keepalive |
| `EXTEND_TIMEOUT_USEC=<n>` | Request more time for current operation |
| `FDSTORE=1` | Store passed file descriptors across restarts (future) |

Unknown keys are silently ignored for forward compatibility.

### 10.2 Watchdog

If the unit specifies `watchdog_sec: N`, prowld sets `WATCHDOG_USEC=<N*1000000>`
and `WATCHDOG_PID=<pid>` in the environment. The service must send `WATCHDOG=1`
at least every N seconds or prowld will treat the service as hung, send
`SIGTERM` (then `SIGKILL` after `exit_timeout`), and restart according to the
`keep_alive` policy.

### 10.3 libsystemd Compatibility Shim

A stub `libsystemd.so` (and, for legacy consumers, `libsystemd-daemon.so`)
ships as part of the MidnightBSD base system. It implements a deliberately
narrow subset of the libsystemd API — the client-facing entry points that
unmodified Linux daemons actually depend on — against prowld's native
mechanisms. Because the underlying wire formats are compatible, this is
genuinely a thin shim rather than a reimplementation.

Shipping in base (rather than as a port) means:

- Unmodified Linux daemons (CUPS, NetworkManager, etc.) that link against
  libsystemd work out of the box with zero user configuration
- Ports that hard-depend on libsystemd at build time need no special
  handling in their port Makefiles
- Native MidnightBSD daemons can link the shim directly or use `libprowl`
  for a smaller dependency footprint on targeted builds

#### 10.3.1 APIs Implemented

**Readiness notification** — maps directly to the protocol in §10.1:

- `sd_notify(3)`, `sd_notifyf(3)`, `sd_pid_notify(3)`
- `sd_watchdog_enabled(3)`

**Socket activation consumers** — reads the environment variables defined
in §9.1:

- `sd_listen_fds(3)`, `sd_listen_fds_with_names(3)`
- `sd_is_socket(3)`, `sd_is_socket_inet(3)`, `sd_is_socket_unix(3)`,
  `sd_is_fifo(3)`, `sd_is_mq(3)`, `sd_is_special(3)`

These are pure client-side `fstat(2)`/`getsockopt(2)` checks; the shim
implementation is line-for-line identical to systemd's.

**Environment check:**

- `sd_booted(3)` — returns `1` under prowld so that software branching on
  "is the service manager available" takes the sd_notify code path rather
  than falling back to dumber behavior.

**Structured logging — lossy redirect to syslog:**

- `sd_journal_print(3)`, `sd_journal_printv(3)`
- `sd_journal_send(3)`, `sd_journal_sendv(3)`

These are redirected to `syslog(3)` with priority mapping preserved.
**Structured fields are discarded**; only the `MESSAGE=` field and priority
survive the translation. Daemons that need full structured logging should
use `syslog(3)` directly or log to a file. This lossy behavior is
documented in the shim's man page so porters aren't surprised.

#### 10.3.2 Deferred to v1.1

- `sd_notify` FDSTORE — file descriptor preservation across service
  restarts. Requires prowld-side state management of handed-back
  descriptors. Tracked in §20 as a Phase 4 item.

#### 10.3.3 Explicitly Out of Scope

The following systemd surface area is **not** provided by the shim and
will not be added. Software that requires these interfaces must be
adapted to prowld's native mechanisms or used only on Linux.

- **D-Bus control interface** (`org.freedesktop.systemd1`). Implementing
  systemd's D-Bus object tree is a reimplementation of its control plane,
  not a stub. Management tools should use `prowlctl` or the IPC protocol
  in §14.
- **`systemd-logind`** — session tracking, seat management, inhibitors,
  power-management integration. An entire subsystem on par with prowld
  itself in scope. A MidnightBSD logind equivalent would be a separate
  project.
- **`systemd-resolved`, `systemd-networkd`, `systemd-timesyncd`,
  `systemd-homed`, and similar sibling daemons.** These are daemons
  that happen to live in the systemd source tree, not systemd features.
  The corresponding MidnightBSD facilities (`resolvconf`, the base
  network stack, `ntpd`, normal user databases) remain authoritative.
- **`systemctl` CLI compatibility.** A `systemctl → prowlctl` wrapper
  looks attractive but would map only a narrow subset faithfully; the
  long tail of flags and verbs would silently misbehave or no-op in
  ways indistinguishable from success. Porters and documentation
  should call `prowlctl` directly.
- **`journalctl` CLI compatibility.** Logs go through syslog; the
  syslog ecosystem has its own tooling.
- **Cgroup-based resource management APIs** (`sd_bus_*`,
  `sd_cgroup_*`). BSD uses jails and `rctl(8)` for resource isolation;
  the conceptual model doesn't map.

The boundary is deliberately drawn at "client APIs a daemon calls to
cooperate with its service manager" — those get shimmed. "Tools for
administering the service manager" and "other daemons that happen to
ship with systemd" are outside the scope of compatibility work.

### 10.4 Unit File Example

```json
{
  "label": "org.example.myservice",
  "program": "/usr/local/sbin/myservice",
  "notify_type": "notify",
  "exit_timeout": 60,
  "watchdog_sec": 30,
  "keep_alive": { "crashed": true }
}
```

### 10.5 Interaction with Startup Completion

`notify_type: notify` overrides the default "alive after throttle_interval"
readiness check (§8.3). If the service never sends `READY=1`, prowld fails the
start after `exit_timeout` seconds and acts per `keep_alive`.

---

## 11. Service Discovery (mDNS Registration)

prowld can optionally register running services with the system's
mDNSResponder, providing zero-configuration network service discovery without
requiring each daemon to implement DNS-SD registration itself.

### 11.1 Requirements and Interface

prowld registers services by invoking `dns-sd(1)` as a long-running child
process, one per registered service. When the service stops, prowld terminates
the corresponding `dns-sd` process, which deregisters the record.

This approach was chosen over direct use of the mDNSResponder Unix socket
API or the libdns_sd C API because:

- `dns-sd(1)` is a stable, documented interface that ships with every BSD
  mDNSResponder implementation (Apple's reference and Avahi's compat shim both
  provide it)
- No additional link-time dependency on libdns_sd
- Process-level isolation: a misbehaving mDNSResponder cannot crash prowld
- The `dns-sd` process itself signals success or failure on its stdout/stderr,
  which prowld can capture and log per service

If `dns-sd(1)` is not available in `$PATH`, `mdns` blocks in unit files are
silently ignored and prowld logs a single warning per boot. Service startup
is **not** affected — mDNS registration is strictly additive.

### 11.2 Unit File Fields

```json
{
  "label": "org.midnightbsd.sshd",
  "program": "/usr/sbin/sshd",
  "arguments": ["-D"],
  "mdns": {
    "register": true,
    "name": "%h SSH",
    "type": "_ssh._tcp",
    "port": 22,
    "txt_record": {
      "u": "admin"
    },
    "subtypes": [],
    "domain": "local"
  }
}
```

### 11.3 Field Reference

- `register` *(bool)*: If true, register with mDNSResponder when the service
  reaches `running`. Default: `false`.
- `name` *(string)*: Service instance name. Supports `%h` (hostname) and `%l`
  (job label) substitution. Default: `%l`.
- `type` *(string, required if `register` is true)*: DNS-SD service type
  (e.g., `_http._tcp`, `_ipp._tcp`, `_ssh._tcp`).
- `port` *(int)*: Port number. If omitted and the job has a `sockets` block,
  the socket's port is used.
- `txt_record` *(object)*: Key/value pairs published as TXT records.
- `subtypes` *(array of strings)*: Additional DNS-SD subtypes.
- `domain` *(string)*: Registration domain. Default: `local`.

### 11.4 Lifecycle

- After the service reaches `running` (either via `notify_type: notify` or the
  default readiness rule in §8.3), prowld spawns a `dns-sd -R` child process
  with arguments derived from the `mdns` block.
- The `dns-sd` child is supervised by prowld: if it exits unexpectedly (for
  example because mDNSResponder itself restarted), prowld respawns it after
  a short throttle interval.
- On service transition to `stopping`, `stopped`, or `failed`, prowld sends
  `SIGTERM` to the corresponding `dns-sd` child. This causes mDNSResponder to
  deregister the record cleanly.
- Deregistration is best-effort. If `dns-sd` has already exited or cannot
  reach mDNSResponder, the registration is dropped; mDNSResponder will
  expire the stale record on its own schedule.

### 11.5 Example Use Cases

- Advertise SSH, HTTP, Samba, printing services to local-network browsers
- Make self-hosted services discoverable from macOS/iOS clients using Bonjour
- Replace hand-written Avahi service XML files with inline unit-file declarations

---

## 12. Scheduling (Cron-like)

Timer jobs replace cron for prowld-managed tasks. Legacy cron continues to
work unchanged for user crontabs.

### 12.1 Calendar Triggers

```json
{
  "label": "org.midnightbsd.periodic.daily",
  "type": "timer",
  "program": "/etc/periodic/daily",
  "schedule": {
    "calendar": { "hour": 3, "minute": 1 }
  }
}
```

Fields in `calendar`: `minute`, `hour`, `day`, `weekday`, `month`. Omitted
fields match any value. Supports lists and ranges (TBD syntax: likely
cron-compatible for familiarity).

### 12.2 Periodic Triggers

```json
{
  "schedule": {
    "interval": 3600,
    "on_boot_delay": 60
  }
}
```

### 12.3 Catch-Up Behavior

If the system was off when a calendar-scheduled job should have run, prowld
runs it once at next startup if `schedule.persistent: true` is set.

---

## 13. Control Utility: prowlctl

### 13.1 Commands

```
prowlctl list [--enabled] [--failed] [--type=daemon|oneshot|timer]
prowlctl status <label>
prowlctl start <label>
prowlctl stop <label>
prowlctl restart <label>
prowlctl reload <label>                 # SIGHUP or custom reload command
prowlctl enable <label>                 # set <rc_name>_enable=YES in rc.conf (wraps sysrc)
prowlctl disable <label>                # set <rc_name>_enable=NO in rc.conf (wraps sysrc)
prowlctl mask <label>                   # prevent from ever starting
prowlctl unmask <label>
prowlctl show <label>                   # dump effective unit file
prowlctl logs <label> [--follow] [--lines=N]
prowlctl reload-config                  # rescan unit directories
prowlctl daemon-reexec                  # re-exec prowld (for upgrades)
prowlctl dependencies <label>           # show dependency tree
prowlctl graph [--format=dot]           # full dependency graph
```

### 13.2 Output

Default output is human-readable. `--json` produces machine-readable output
for scripts and monitoring tools.

### 13.3 Exit Codes

- `0` — success
- `1` — job not found
- `2` — job in failed state
- `3` — operation not permitted
- `4` — daemon not responding

---

## 14. IPC Protocol

### 14.1 Transport

Unix domain socket at `/var/run/prowld/prowld.sock`. Permissions `0660`, owned
by `root:wheel`. Members of the `wheel` group can query; only `root` can
modify state.

### 14.2 Wire Format

Length-prefixed JSON messages:

```
[4-byte big-endian length][JSON payload]
```

Request:
```json
{
  "id": "req-1234",
  "verb": "start",
  "target": "org.midnightbsd.sshd"
}
```

Response:
```json
{
  "id": "req-1234",
  "status": "ok",
  "result": { "state": "running", "pid": 4823 }
}
```

### 14.3 Events

Clients may subscribe to state-change events via a persistent connection:

```json
{
  "verb": "subscribe",
  "filter": { "labels": ["org.midnightbsd.*"] }
}
```

Events are pushed as they occur. Useful for monitoring and dashboards.

---

## 15. Service Lifecycle States

```
                  ┌─────────┐
                  │ unknown │
                  └────┬────┘
                       │ load
                       ▼
                  ┌─────────┐      disable     ┌──────────┐
                  │ loaded  │ ────────────────▶│ disabled │
                  └────┬────┘                  └──────────┘
          start        │
                       ▼
                  ┌──────────┐
                  │ starting │
                  └────┬─────┘
                       │ ready
                       ▼
            ┌─────▶┌─────────┐
            │      │ running │
            │      └────┬────┘
            │           │ stop / exit
         keep_alive     ▼
            │      ┌──────────┐
            └──────│ stopping │
                   └────┬─────┘
                        │
                        ▼
                   ┌─────────┐
                   │ stopped │
                   └─────────┘

  Any state ───on error──▶ ┌────────┐
                           │ failed │
                           └────────┘
```

---

## 16. Shutdown Ordering

When the system is brought down via `shutdown(8)` with `-r` (reboot) or `-p`
(power off), prowld performs an orderly stop of managed services before
returning control to init. Faster shutdown paths — `reboot(8)` and `halt(8)`
— skip orderly stop; see §16.2.

### 16.1 Orderly Shutdown (`shutdown -r`, `shutdown -p`)

prowld computes a reverse topological order of the dependency graph: services
are stopped in the reverse of the order they were started, so dependencies
outlive their dependents. Independent branches are stopped in parallel,
mirroring startup behavior and bounded by the same concurrency limit.

Services declared with `KEYWORD: shutdown` (via rc.d compatibility) or
`shutdown_wait: true` (native units) receive graceful-shutdown handling:

1. prowld sends `SIGTERM` and waits for the service to exit cleanly.
2. If the service does not exit within its configured `exit_timeout`, prowld
   sends `SIGKILL`.
3. **prowld waits for all services marked for graceful shutdown to complete**
   before returning to init, up to a system-wide bound
   (`shutdown_timeout` in `prowld.conf`, default **300 seconds**).
4. If the system-wide bound is reached, any remaining graceful-shutdown
   services are force-killed and shutdown proceeds.

The per-service `exit_timeout` and the system-wide `shutdown_timeout` are
independent: a slow service consumes the system-wide budget but cannot
prevent shutdown indefinitely.

Services without the shutdown keyword are sent `SIGTERM`, then `SIGKILL`
after their individual `exit_timeout`, but prowld does not wait beyond that
for them.

### 16.2 Fast Shutdown (`reboot`, `halt`)

`reboot(8)` and `halt(8)` bypass orderly stop. When init signals prowld
during these operations, prowld:

1. Sends `SIGTERM` to all managed services as a courtesy.
2. Does not wait for any service, including those marked `KEYWORD: shutdown`.
3. Exits immediately, allowing init to proceed with the fast path.

This is the correct behavior for emergency shutdown, kernel panics, or
administrator-requested immediate reboots. Services that need to flush state
cleanly should be stopped with `shutdown(8)` instead of `reboot(8)`/`halt(8)`.

### 16.3 Configuration

```ucl
# /etc/prowld/prowld.conf

shutdown_timeout = 300   # seconds, system-wide budget for graceful shutdown
```

### 16.4 rc.d Compatibility

Existing rc.d scripts with `KEYWORD: shutdown` are recognized by the rc-shim
(§7.1) and treated as graceful-shutdown services. Their stop command is
`/etc/rc.d/<script> stop`, invoked and awaited up to `exit_timeout`
individually and `shutdown_timeout` system-wide.

Scripts without `KEYWORD: shutdown` are stopped without waiting, matching the
historical behavior of `rc.shutdown` for non-shutdown-keyword scripts.

### 16.5 Remote System Considerations

The 5-minute default (`shutdown_timeout = 300`) is deliberately chosen so
that remote systems — machines where a hung shutdown requires physical
intervention — cannot be indefinitely blocked by a misbehaving service. If a
critical service legitimately needs more than 5 minutes to flush state,
raise the value explicitly; the default errs on the side of actually
shutting down.

---

## 17. Security Considerations

- prowld runs as root (required to drop privileges and bind privileged ports).
- The control socket is protected by filesystem permissions.
- Unit files in `/etc/prowld/units.d/` must be owned by root and not
  world-writable; prowld refuses to load world-writable unit files.
- Environment variable inheritance is explicit: prowld starts jobs with a
  minimal environment unless `environment` is specified.
- prowld does not parse untrusted input from the network; all input comes
  from local filesystem or local socket.
- Resource limits are enforced via `setrlimit(2)` before execve.
- Integration with MAC frameworks (mac_portacl, etc.) is preserved because
  prowld is a normal process tree.

---

## 18. Open Questions

These require further discussion before implementation begins.

1. **Behavior of `prowlctl daemon-reexec` for an in-flight startup wave**:
   Does the successor process resume in-flight starts from the handoff file,
   or does it wait for the wave to settle before re-exec begins?

---

## 19. Implementation Language

prowld will be implemented in C.

### 19.1 Rationale for C

The language choice is constrained by two factors specific to this project:

1. **Static recovery binary.** prowld must be buildable as a fully static
   binary for use on rescue media. Every statically-linked library becomes
   code that has to work in a degraded recovery environment.
2. **Ecosystem fit.** MidnightBSD's base system — `init(8)`, `sshd`,
   `syslogd`, `rc.subr`, `libucl`, `jail(8)`, `sysrc(8)`, `kqueue(2)` — is C.
   prowld's interoperation surface is entirely C APIs.

Within these constraints, C is the natural choice. A minimal static C prowld
is expected to fall in the 200–400 KB range; the library dependencies are
limited to `libc`, `libutil`, and `libucl`. No C++ runtime library, no
exception unwinder, and no language runtime need to be included in the
recovery environment.

### 19.2 Standard and Toolchain

- Target **C17** (`-std=c17`) with GNU extensions permitted where they
  improve clarity (designated initializers, `__attribute__((cleanup))`,
  etc.).
- Must build with both Clang and GCC on all MidnightBSD-supported
  architectures.
- Must build `-static` against base-system libraries without depending on
  any port.

### 19.3 Safety Discipline

C does not provide the memory-safety guarantees of Rust or the RAII-based
resource management of C++. The following practices are mandatory, not
optional:

- Compile with `-Wall -Wextra -Wpedantic -Wconversion -Wshadow
  -Wstrict-prototypes -Werror`.
- Run Clang's static analyzer and `scan-build` in CI on every change.
- Build and test with Address Sanitizer (`-fsanitize=address`) and
  Undefined Behavior Sanitizer (`-fsanitize=undefined`). These catch the
  majority of memory-safety bugs before they ship and are free with modern
  Clang.
- Fuzz-test the libucl parser entrypoints and the IPC protocol from day
  one using libFuzzer or AFL++.
- Use BSD-safe string APIs: `strlcpy(3)`, `strlcat(3)`, `snprintf(3)`.
  Never use `strcpy`, `strcat`, or `sprintf`.
- Prefer `calloc(3)` over `malloc(3)` to default-zero allocations.
- Pair resource acquisition and release in clearly matched functions;
  consider `__attribute__((cleanup))` for complex error paths.

These practices, applied rigorously, close most of the gap between C and
safer languages for a codebase of prowld's size and scope.

### 19.4 Rust Considered and Rejected

Rust was seriously considered. Its memory-safety guarantees are stronger
than anything achievable in C, and unlike C++ it produces binaries without
a runtime-library dependency comparable to `libstdc++` — making static
linking of a Rust prowld technically viable.

**Rust is rejected on project-scope grounds, not technical merit:**

- **MidnightBSD does not ship a Rust compiler in its base system.**
  The only Rust toolchains available are in ports. A base-system service
  manager cannot depend on a ports-provided toolchain to build, because
  ports require a functional service manager to install.
- Bringing `rustc` into base would require multi-year work on
  bootstrapping, cross-compilation, rescue-media sizing, and ongoing
  toolchain maintenance — vastly out of scope for an init-system
  project.
- Adopting Rust for a core base component is a project-wide decision
  with implications well beyond prowld. That conversation belongs in a
  separate MidnightBSD RFC, not this specification.

If MidnightBSD adopts Rust in base at some future point, a v2.0 rewrite
could be reconsidered on its merits. Until then, C is the only language
that meets the static-recovery and base-build requirements.

### 19.5 C++ Considered and Rejected

Modern C++ (C++17 or C++20) offers RAII, smart pointers, and container
types that eliminate whole classes of C bugs. It is rejected for the
following reasons:

- **Static-linking cost.** A statically-linked C++ binary typically pulls
  in 1–2 MB of `libstdc++` or `libc++` code even with `-fno-exceptions
  -fno-rtti`. This inflates the recovery binary and adds surface area
  for issues in degraded environments.
- **Exception-handling complexity.** C++ exception unwinding has
  historical interactions with `fork(2)`, shared-library boundaries, and
  destructor-during-unwind scenarios. Disabling exceptions eliminates
  these risks but also eliminates RAII's main advantage for error
  propagation, leaving a C-with-classes dialect.
- **Post-fork async-signal-safety.** The window between `fork(2)` and
  `execve(2)` is extensive in a service manager. Almost no C++ standard
  library operations are safe in this window, requiring careful manual
  partitioning between "rich C++" and "C-style syscall code" throughout
  the codebase. The discipline required is comparable to writing C
  throughout, with a less clear mental model.
- **Ecosystem fit.** Introducing C++ as the only C++ component in base
  would impose a permanent maintenance cost and raise the bar for
  contributions from BSD developers who work primarily in C.

The memory-safety benefits of C++ are real, but they are smaller in a
fork/exec/kqueue-dominated workload than in code that does heavy in-memory
data-structure manipulation. The safety discipline in §19.3 closes most
of the practical gap for this specific application.

---

## 20. Implementation Phases

### Phase 1 — Minimum Viable prowld (3–4 months)
- Core daemon: load unit files, build DAG, launch services
- prowlctl with list/start/stop/status/restart/logs
- rc-shim for backward compatibility
- service(8) wrapper
- Parallel startup with dependency ordering
- Orderly shutdown with `KEYWORD: shutdown` handling and `shutdown_timeout`
- Basic logging to syslog

### Phase 2 — Feature Parity (2–3 months)
- Timer jobs (cron replacement for system jobs)
- Socket activation
- Keep-alive and restart policies
- Resource limits
- sd_notify-compatible ready notification + libsystemd shim in base
- Watchdog support
- prowl-generator for rc.d → unit migration
- Comprehensive man pages

### Phase 3 — Polish (2 months)
- Event subscription / monitoring API
- Persistent calendar timers with catch-up
- Drop-in override support
- mDNSResponder registration via `dns-sd(1)`
- Documentation, migration guide, porter's handbook

### Phase 4 — v1.1 (post v1.0 release)
- Per-user agents (`prowld --user` or equivalent; per-user control socket,
  unit directories under `~/.config/prowld/`, session lifecycle integration)
- Per-jail units: parse `jail.conf`, synthesize one unit per jail, enable
  fine-grained dependencies and parallel jail startup (§7.5)
- Ready-notification FDSTORE (descriptor preservation across service restarts)

### Phase 5 — v1.2 (Power-Aware Service Management)

Power-aware service management: conditionally stop, throttle, or avoid
starting services based on the system's current power source. Targeted at
laptops, UPS-backed servers, and energy-conscious deployments.

Design sketch:

- New unit-file fields: `run_on_battery: true|false` (default `true`),
  `run_on_ups: true|false` (default `true`), `stop_after_battery_minutes: N`
  (optional graceful-stop delay when transitioning to battery).
- prowld subscribes to power-source transitions via `devd(8)` events
  and/or `sysctl` polling (`hw.acpi.acline`, `hw.acpi.battery.*`).
- On transition to battery/UPS, services with `run_on_battery: false`
  receive `SIGTERM` after their `stop_after_battery_minutes` grace period
  (or immediately if unset). On transition back to AC, services that were
  auto-stopped are restarted if they are otherwise enabled.
- Timer jobs gain a `defer_on_battery: true` option that postpones firing
  until AC is restored (useful for backups, periodic indexers, etc.).
- Native sd_notify extension: services can send `POWER_STATE=battery` to
  learn about transitions and adapt internally (e.g., a search indexer
  throttling itself) rather than being killed.
- Explicit out-of-scope: prowld does not manage screen dimming, CPU
  frequency scaling, suspend/hibernate, or device power gating. Those
  belong to `powerd(8)` and the kernel. prowld only decides which
  services run.

Open questions to be resolved before implementation:
- Integration model with `powerd(8)` — cooperate, coexist, or ignore?
- Whether UPS-on-battery (via `nut` or `apcupsd`) is surfaced through
  the same mechanism as laptop battery, or a separate concept.
- Hysteresis policy to prevent flapping when AC is intermittent.

### Phase 6 — Future (post v1.2)
- PID 1 migration

---

## 21. References

- launchd design: `launchd.plist(5)`, WWDC 2005 Session 420
- systemd design: `systemd.unit(5)`, `systemd.service(5)`
- OpenRC: https://github.com/OpenRC/openrc
- s6 / s6-rc: https://skarnet.org/software/s6-rc/
- rcorder(8), rc.subr(8), rc(8) — existing MidnightBSD/FreeBSD manpages
- libucl: https://github.com/vstakhov/libucl

---

*End of specification draft. Comments, corrections, and alternative proposals
welcome on the midnightbsd-users list.*
