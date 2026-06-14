# SCHED_MIC — a hybrid-core-aware scheduler

`SCHED_MIC` is a new scheduler derived from ULE (`sys/kern/sched_ule.c`). It is
identical to ULE except that, on x86 CPUs with heterogeneous ("hybrid") cores,
it weighs four core priority classes when choosing where to run a thread. On
homogeneous hardware — and on non-x86 architectures — it behaves like ULE, and
is byte-for-byte identical to ULE placement with `kern.sched.smt_busy_penalty=0`
(the default 192 primarily changes choices within an SMT-threaded group, though
it can tip cross-group placement in edge cases since it feeds the group total).

ULE is unchanged and remains the default scheduler. `SCHED_MIC` is opt-in.

## Why

Modern CPUs are no longer uniform:

- **Intel 12th–14th gen and Core Ultra** mix fast P-cores, efficient E-cores,
  and (on Core Ultra) very-low-power LP-E cores on the SoC tile.
- **AMD 7950X3D / 7900X3D / 9950X3D / 9900X3D** pair a 3D V-Cache CCD (huge L3,
  great for latency-sensitive/gaming work) with a higher-clocked compute CCD.
- **AMD hybrid mobile** parts add dense Zen4c/5c "C" efficiency cores.

ULE treats all of these as interchangeable (apart from SMT), so latency-
sensitive work can land on a slow core while a fast one sits idle. `SCHED_MIC`
adds awareness of these classes.

## Core priority classes

Each logical CPU is assigned a static class. The "priority 3" tier is a dynamic
state (a busy core's free SMT thread), so it is computed at placement time and
never stored.

| Priority | Meaning | Class constant |
|---|---|---|
| 1 | P-core / AMD cache (V-Cache) CCD core — physical | `CPU_CLASS_PERF` (1) |
| 2 | E-core / AMD compute CCD core / AMD C-core — physical | `CPU_CLASS_EFF` (2) |
| 3 | Free SMT thread of a *busy* core ("hyperthreaded") | dynamic, not stored |
| 4 | Intel LP-E core — scheduled last | `CPU_CLASS_LP` (4) |

The global fill rule is **class-1 physical → class-2 physical → SMT siblings →
LP**. The preference is *soft and tunable*: higher classes fill first, but work
spills to lower classes once the higher ones carry real load. Nothing is ever
hard-fenced off, so the machine is never artificially underused.

## How placement works

ULE's `cpu_search_lowest()` walks the `cpu_group` topology tree, summing each
subtree's load into a `total` and descending the least-loaded path, then picks
the least-loaded CPU on that path. `SCHED_MIC` folds a per-candidate **class
cost** into each CPU's load *before* it is summed into `total`
(`sched_class_cost()` in `sys/kern/sched_mic.c`), so the bias steers both the
subtree choice and the final CPU pick:

```
load = l * 256 + sched_class_cost(cg, c, l);
```

The cost is, on the 256-points-per-thread scale ULE already uses:

- class 2 (efficiency): `+ kern.sched.class_weight_eff`  (default 160)
- class 4 (LP-E):       `+ kern.sched.class_weight_lp`   (default 512)
- free thread whose SMT sibling is busy: `+ kern.sched.smt_busy_penalty` (192)

This yields the per-candidate ordering required by the priority list:

```
idle perf (0) < idle eff (64) < busy-sibling perf (192) < idle LP (512)
```

The class cost is applied only on the placement path: `sched_lowest()` takes a
`class_aware` argument that is set by `sched_pickcpu()` but cleared by the
periodic load balancer (`sched_balance_group()`), and `cpu_search_highest()`
(the work-steal *source* search) is unmodified. So the balancer and work
stealing stay class-blind and move work purely by real load; only thread
*placement* steers by core class. This keeps cache-CCD packing (driven by
placement) without risking class-driven migration ping-pong.

Because the cost is summed into group totals, a lower-class cluster of N cores
carries an `N × weight` baseline — which is what keeps LP genuinely near-last
and packs the AMD cache CCD under load. All crossovers are controlled by the
sysctls below.

### Worked examples

- **Intel 13900K, all idle**: P-cores (cost 0) fill first, then E-cores (160),
  LP-E (512) last.
- **Intel, all P physical busy (1 thread each)**: a half-busy P SMT group costs
  `256 + 192`; an idle E core costs `160`, so an acceptable E core is cheaper
  than a P SMT sibling (priority 2 before 3).
- **AMD 7950X3D, all idle**: the cache CCD (class 1) fills first; the 9th thread
  spills to a compute-CCD **physical** core before any cache-CCD SMT sibling.
  `kern.sched.prefer_compute=1` swaps the classes so the compute/frequency CCD
  is preferred instead.
- **AMD asymmetric non-X3D, all idle**: the smaller-cache compute/C-core group is
  preferred by default (`kern.sched.prefer_compute=1`).
- **Intel power-saving mode**: Intel defaults to
  `kern.sched.prefer_compute=1` for P-core preference.  Setting it to `0`
  swaps class 1 and class 2, so E-cores are preferred over P-cores.
- **Homogeneous box**: detection leaves every CPU class 1, so all cost terms are
  zero except the SMT-sibling penalty; set `kern.sched.smt_busy_penalty=0` for
  byte-identical ULE placement.

## How cores are detected

Detection is x86 machine-dependent, in `sys/x86/x86/mp_x86.c`, guarded by
`#ifdef SCHED_MIC` (so ULE/4BSD kernels gain no code). A `SYSINIT` at
`SI_SUB_SMP` uses `smp_rendezvous()` to have **each CPU probe its own CPUID**
(core type and the size of the L3 it shares), then derives `cpu_core_class[]`.
Until that runs, the array defaults to all class 1, i.e. plain ULE behavior.

| Case | Method | Reliability |
|---|---|---|
| Intel P vs E | CPUID `0x1A` EAX[31:24] (`0x40`=P, `0x20`=E) | architectural |
| Intel LP-E | small core with **no L3** (SoC-tile LP-E lack L3, unlike compute-tile E-cores); gated by `kern.sched.detect_lpe` | heuristic |
| AMD X3D CCD | per-CCD L3 size via CPUID `0x8000001D`; the larger-L3 die is the cache die (class 1), the other compute (class 2) | heuristic (method sound) |
| AMD Zen4c/5c | same larger-L3-is-preferred rule (full CCX has larger L3 than a C CCX) | heuristic |

Graceful fallbacks: non-hybrid Intel, single-CCD AMD, and AMD parts with
symmetric L3 (e.g. a plain 7950X) leave every CPU class 1 → identical to ULE.

The CPUID cache-size field macros live in `sys/x86/include/specialreg.h`
(`CPUID_CACHE_*`).

## Tunables (`kern.sched.*`)

| sysctl | default | meaning |
|---|---|---|
| `class_weight_eff` | 160 | load penalty for class-2 cores |
| `class_weight_lp` | 512 | load penalty for class-4 (LP-E) cores |
| `smt_busy_penalty` | 192 | penalty for a free thread whose SMT sibling is busy; `0` restores ULE SMT behavior |
| `prefer_compute` | vendor policy | platform compute preference; Intel defaults to `1` for P-cores and `0` favors E-cores, AMD X3D defaults to `0` for the V-Cache die, AMD asymmetric non-X3D defaults to `1` for smaller-cache compute/C-core groups |
| `detect_lpe` | 1 | (boot tunable) detect LP-E cores as class 4; `0` leaves them class 2 |
| `core_class` | (read-only) | debug dump of per-CPU class, `cpuid:class` pairs |

## Files

| File | Change |
|---|---|
| `sys/kern/sched_mic.c` | new; copy of `sched_ule.c` with the cost function, helpers, `cpu_core_class[]`, and new sysctls |
| `sys/sys/smp.h` | `CPU_CLASS_*` constants and `cpu_core_class[]` extern |
| `sys/x86/include/specialreg.h` | `CPUID_CACHE_*` macros; `CPUID_HYBRID_NATIVE_MODEL_MASK` |
| `sys/x86/x86/mp_x86.c` | `SCHED_MIC`-guarded detection (`mic_probe_cpu`, `mic_classify` SYSINIT) |
| `sys/conf/options`, `sys/conf/files`, `sys/conf/NOTES` | `SCHED_MIC` option/build wiring and documentation |
| `sys/amd64/conf/MIC` | sample kernel config selecting `SCHED_MIC` |

## Building and trying it

```
cd /usr/src
make buildkernel KERNCONF=MIC
make installkernel KERNCONF=MIC
```

After booting:

```
sysctl kern.sched.name          # -> MIC
sysctl kern.sched.core_class    # per-CPU class dump
sysctl kern.sched.topology_spec # cross-reference CPU -> cache/CCD
```

Expected `core_class` counts: 13900K = 8×1 + 16×2; 7950X3D = 8×1 + 8×2; Core
Ultra includes a cluster of class 4. On homogeneous hardware every CPU is 1.

The Intel detection path has been validated on real Alder Lake silicon (Core
i7-1260P) by reading the same CPUID leaves per-core from userspace: 8 P-core
threads → class 1, 8 E-cores → class 2, 0 LP-E. The E-cores report an 18 MB L3,
so the "small core with no L3 ⇒ LP-E" heuristic correctly leaves them class 2.
The LP-E, AMD X3D, and AMD C-core paths, and actual placement behavior, still
need their respective hardware / a booted MIC kernel.

## Limitations / future work

- LP-E detection is heuristic (no architectural bit); `detect_lpe=0` disables it.
- AMD CPPC "highest performance" is not yet used as a corroborating signal for
  C-core detection; the L3-size heuristic covers the current parts.
- Work-stealing (`cpu_search_highest`) is class-blind; it does not yet avoid
  pulling work onto LP/efficiency cores.
- The long-term load balancer (`sched_balance_group`) is class-blind by default
  (the `class_aware` argument to `sched_lowest()` at the balancer call site is
  `0`). It evens load by real `tdq_load`, so under *sustained* saturation it will
  migrate work off the preferred cores (cache CCD / P-cores) to equalize,
  partially eroding the packing that placement establishes. Placement preference
  therefore holds firmly at light/moderate load and softens at saturation. To
  test the opposite tradeoff on real hardware — balancer *reinforces* packing —
  flip that single argument to `1`; which is better is hardware-dependent.
- `prefer_compute` is global (fine for single-socket consumer X3D and hybrid mobile parts).
