# Synccheck Smoke Tests

This directory contains end-to-end synccheck smoke tests. They quickly verify the
basic path from sample compilation and device instrumentation through callback
delivery, synchronization-pair analysis, diagnostic rendering, and session
completion. Each same-named subdirectory contains its scenario-specific `.asc`
file, `CMakeLists.txt`, `run.sh`, and `verify.py`. The parent directory owns batch
execution and verification; `demo/build.sh` prepares the shared tools and online
DBI sources.

The `.asc` file retains only the device instructions needed by the synchronization
scenario, plus ACL initialization, kernel launch, stream management, and cleanup.
Both normal paths and expected error paths are guarded by
the smoke suite. An error-case smoke test succeeds when synccheck reliably reports
the expected error.

## Running Smoke Tests

Build the shared tools from the repository root:

```bash
bash npu_sanitizer/demo/build.sh
```

Then run and summarize the complete smoke suite:

```bash
bash npu_sanitizer/demo/examples/synccheck/run_all.sh
```

`run_all.sh` uses the existing `demo/build` tree and runs every case in order. It
preserves each case's original output and prints per-case `PASS/FAIL` results plus
`total/passed/failed` counts. The script returns 1 if any case fails. An error case
prints its diagnostic; when its `verify.py`, summary, and session checks succeed,
its runner returns 0 and prints
`example verification passed: synccheck/<case-name>`.

Run one target smoke test directly with its local `run.sh`:

```bash
bash npu_sanitizer/demo/examples/synccheck/single_pair/run.sh
bash npu_sanitizer/demo/examples/synccheck/single_unconsumed/run.sh
```

## Pass Criteria

- Normal cases: the summary matches, `verify.py` prints `synccheck verification passed`,
  and the runner returns 0.
- Error cases: the specified diagnostic and summary match, `verify.py` prints
  `synccheck verification passed`, and the runner returns 0.
- Build failures, handshake failures, incomplete sessions, and diagnostic or count
  mismatches all fail the smoke test.

## Smoke Matrix

Excluding the synchronization-free `no_sync` control, the suite contains six
pure-AIC and six pure-AIV synchronization scenarios. It also retains 1:1, 1:2,
and 2:1 mix ratios for future capability regression.

| Smoke Test | Expected Result | Guarded Scenario |
| --- | --- | --- |
| `no_sync/` | Normal | No false synccheck report when the kernel has no synchronization instructions. |
| `single_pair/` | Normal | A matching `SET_FLAG -> WAIT_FLAG` pair on the AIC M pipeline is consumed correctly. |
| `single_unconsumed/` | Error | An unconsumed `SET_FLAG` is reported at session completion. |
| `duplicate_set/` | Error | A repeated `SET_FLAG` with the same key on the AIC M pipeline is reported. |
| `multi_block_isolation/` | Error | SET/WAIT state is isolated by block and only the unwaited block is reported. |
| `wait_without_set/` | Error | A `WAIT_FLAG` without a corresponding `SET_FLAG` is reported. |
| `aic_wait_without_set/` | Error | A `WAIT_FLAG` without a corresponding `SET_FLAG` is reported on the AIC M pipeline. |
| `mix_wait_without_set/` | Expected FAIL | Both the AIC M and AIV V branches of `__mix__(1, 1)` issue a WAIT without SET. |
| `mutex_pair/` | Normal | `GET_BUF -> RLS_BUF` pairs on the MTE2 and V pipelines are consumed correctly. |
| `mutex_unreleased/` | Error | An unreleased `GET_BUF` is reported at session completion. |
| `aic_mutex_unreleased/` | Error | An unreleased `GET_BUF` on the AIC M pipeline is reported at session completion. |
| `mix_mutex_unreleased/` | Expected FAIL | Both the AIC M and AIV V branches of `__mix__(1, 2)` issue an unreleased `GET_BUF`. |
| `mutex_unlock_without_lock/` | Error | An `RLS_BUF` without a corresponding `GET_BUF` is reported on the AIC M pipeline. |
| `mutex_duplicate_lock/` | Error | A repeated blocking `GET_BUF` with the same mutex ID is reported. |
| `mutex_multi_block_isolation/` | Error | AIC M GET/RLS state is isolated by block and only the unreleased block is reported. |

## Guardrails

- Every smoke test must retain a same-named `.asc` file, independent
  `CMakeLists.txt`, executable `run.sh`, and `verify.py`. The `run.sh` builds its
  target, stores its log, and runs in the case-local `build/` directory. The parent
  directory owns batch execution and verification logic.
- Each `.asc` file must describe the guarded condition with a `// Scenario:`
  comment immediately before the trigger and include complete host-side context.
- Each `verify.py` must declare the expected summary and diagnostic types;
  checking only for an arbitrary log substring is not sufficient.
- `wait_without_set`, `aic_wait_without_set`, `mix_wait_without_set`, and
  `mutex_duplicate_lock` block device instructions and must use bounded stream
  synchronization and `aclrtDestroyStreamForce` so the smoke process can terminate.
- `demo/tests/check_synccheck_examples_layout.sh` guards the package contract,
  shell syntax, and documentation matrix for every smoke test. After changes to
  synchronization-event translation, the checker, renderer, or runtime path, run
  at least the affected smoke tests and use `single_pair` as the basic normal-path
  smoke test.
