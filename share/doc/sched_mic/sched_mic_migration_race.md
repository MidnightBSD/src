# SMP thread-migration handoff race: a thread can run on two CPUs at once (shared kernel stack → stack-canary panic)

## Summary

Under heavy fork/exec/scheduler load, a runnable thread can be left **dispatched on two
CPUs simultaneously** — the same `struct thread` is `pc_curthread` on two cores, sharing one
kernel stack, with `td_lock` stuck at `&blocked_lock` and `td_oncpu == -1`. The two cores
trample the shared kernel stack, which smashes the stack-protector canary and panics with
`stack overflow detected; backtrace may be corrupted` (often preceded on another core by
`kernel trap 12 with interrupts disabled` and `timeout stopping cpus`).

The fault is in the **shared scheduler migration-handoff path** (`sched_switch` /
`sched_switch_migrate` / `tdq_move` / `thread_lock_block`/`thread_lock_unblock` /
`cpu_switch`), code common to ULE. It is reproduced reliably by an experimental
hybrid-core-aware scheduler (SCHED_MIC, a derivative of ULE) whose only functional change is
*which* CPU it picks for placement — i.e. it perturbs SMP timing/placement and exposes the
latent race. Stock ULE has not been observed to hit it in a 46-minute soak of the identical
workload, so on ULE the race is (at minimum) far rarer; this report does **not** claim ULE
is provably immune.

## Environment

- MidnightBSD 4.1, `master-n14327-f6a601a264` (amd64).
- CPU: 12th Gen Intel Core i7-1260P (Alder Lake, 4 P-cores w/ SMT + 8 E-cores = 16 logical).
- Crashing kernel: `#2 master-n14327-f6a601a264-dirty`, config `MIC` (SCHED_MIC instead of
  SCHED_ULE). Uptime at panic: 1m37s.

## Evidence (vmcore, analyzed with kgdb)

`blocked_lock` is at `0xffffffff8204b1f0`. Iterating every CPU's `__pcpu[c].pc_curthread`:

```
cpu  7: td=0xfffff8030160c000 tid=100763 name=spin state=TDS_RUNNING oncpu=-1
        td_lock=0xffffffff8204b1f0 (==&blocked_lock) kstack=0xfffffe0177972000
cpu 15: td=0xfffff8030160c000 tid=100763 name=spin state=TDS_RUNNING oncpu=-1
        td_lock=0xffffffff8204b1f0 (==&blocked_lock) kstack=0xfffffe0177972000
```

- **Identical `struct thread` pointer** (`0xfffff8030160c000`) and **identical `td_kstack`**
  (`0xfffffe0177972000`) on both CPUs — one thread, one stack, two cores.
- This is not a stale post-mortem snapshot: **cpu 7 is in `stopped_cpus`** (it answered the
  stop-IPI and was frozen cleanly) and was running the `spin` thread, *not* its idle thread;
  **cpu 15 is the panicking CPU** (`cpuid = 15`), whose live context is the same thread.
- The panic frame's `ap` is `0xfffffe0177975870`, i.e. inside that kstack
  (`0xfffffe0177972000 + 0x3870`, within the 0x4000 stack) — confirming cpu 15 was executing
  on tid 100763's stack while cpu 7 was also running it. The two cores corrupting one stack
  is what trips `__stack_chk_fail`.
- `td_lock == &blocked_lock` with `td_oncpu == -1` means the migration handoff never
  completed the final unblock step (`cpu_switch`'s atomic store of
  `td->td_lock = &target_tdq->tdq_lock`). The thread is permanently in the
  "being migrated" transient state, yet is concurrently the running thread on two CPUs.

Message buffer (two CPUs interleaving, hence the garbled `ekrern`/`elkrernnel`):

```
kernel trap 12 with interrupts disabled
timeout stopping cpus
panic: stack overflow detected; backtrace may be corrupted
cpuid = 15
```

`timeout stopping cpus` indicates a CPU spinning with interrupts disabled that never
answered the stop-IPI — consistent with a core wedged in the broken handoff.

## How to reproduce

1. Build a kernel with a placement-perturbing scheduler. We used SCHED_MIC (ULE + a
   read-only per-candidate "core class" cost folded into `cpu_search_lowest`), but the point
   is only that placement decisions and their timing differ from stock; the added code does
   no writes.
2. Boot it on a multi-core SMP box (16 logical here).
3. Run a sustained scheduler/fork-heavy soak: looped `make -j16 buildkernel` reproduces in
   ~13–19 min; a tighter fork/exec/exit storm reproduces faster (here: ~1m37s).
4. Panic: `stack overflow detected; backtrace may be corrupted`.

## Analysis / why it is shared-code, not the derivative scheduler

- The SCHED_MIC additions are strictly **read-only**: the cost function only reads a
  per-CPU class array (indexed by a valid cpuid) and `TDQ_LOAD()` of SMT siblings (bounded by
  `cg_first..cg_last`, short-circuited behind `CPU_ISSET`). There is **no out-of-bounds
  write** in the added code, so it cannot corrupt a stack canary directly.
- A mitigation that made the *migration* path (`sched_switch` → `sched_pickcpu`) byte-for-byte
  identical to ULE (class-aware placement applied only on the wakeup/fork `sched_add` path)
  **did not prevent the crash** — the double-run still occurred. That localizes the trigger to
  timing/placement perturbation on the wakeup/fork path and the fault to the shared migration
  handoff, not to any code executed inside the blocked-lock window.

The net hypothesis: a window in the migration handoff
(`thread_lock_block` → `sched_switch_migrate`/`tdq_add` on the target + `tdq_notify` IPI →
`td_oncpu = NOCPU` → `cpu_switch` final unblock) can race such that the target CPU begins
running the thread before/without the handoff being completed on the source, leaving the
thread `TDS_RUNNING` on two CPUs with `td_lock` still `&blocked_lock`.

## kgdb recipe (for whoever picks this up)

`gdb` cannot open the MidnightBSD vmcore ("file format not recognized"); use `kgdb`:

```
kgdb -n <dumpnr> /usr/lib/debug/boot/<kernel>/kernel.debug <<'EOF'
set pagination off
python
import gdb
for c in range(0, int(gdb.parse_and_eval("mp_maxid"))+1):
    td = gdb.parse_and_eval("__pcpu[%d].pc_curthread" % c)
    print(c, hex(int(td)), int(td['td_tid']), td['td_name'].string(),
          int(td['td_state']), int(td['td_oncpu']), hex(int(td['td_lock'])),
          hex(int(td['td_kstack'])))
end
EOF
```

Look for the same `td` pointer / same `td_kstack` appearing on two CPUs with
`td_lock == &blocked_lock`. (`info threads` does not surface on-CPU threads by lwpid, so
iterate `__pcpu[]` directly.)

## Disposition on our side

SCHED_MIC remains opt-in/experimental and is **not** shipped as default; ULE stays the
GENERIC default and is unaffected in normal use. Filing this so the underlying handoff race
can be examined upstream.
