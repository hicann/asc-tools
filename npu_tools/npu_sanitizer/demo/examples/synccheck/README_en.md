# Synccheck Smoke Tests

This directory contains end-to-end synccheck smoke tests. They quickly verify the
basic path from sample compilation and device instrumentation through callback
delivery, synchronization-pair analysis, diagnostic rendering, and session
completion. Each same-named subdirectory contains its scenario-specific `.asc`
file, `CMakeLists.txt`, `run.sh`, and `verify.py`. The parent directory owns batch
execution and verification; `demo/build.sh` packages and installs asc-tools and
the online DBI sources into `ASCEND_HOME_PATH`.

The `.asc` file retains only the device instructions needed by the synchronization
scenario, plus ACL initialization, kernel launch, stream management, and cleanup.
Both normal paths and expected error paths are guarded by
the smoke suite. An error-case smoke test succeeds when synccheck reliably reports
the expected error.

## Running Smoke Tests

Package and install the shared tools from the repository root:

```bash
bash npu_tools/npu_sanitizer/demo/build.sh
```

Then run and summarize the complete smoke suite:

```bash
bash npu_tools/npu_sanitizer/demo/examples/synccheck/run_all.sh
```

`run_all.sh` uses `${ASCEND_HOME_PATH}/$(uname -m)-linux/bin/npu-check` and runs every case in order. It
preserves each case's original output and prints per-case `PASS/FAIL` results plus
`total/passed/failed` counts. The script returns 1 if any case fails. An error case
prints its diagnostic; when its `verify.py`, summary, and session checks succeed,
its runner returns 0 and prints
`example verification passed: synccheck/<case-name>`.

Each runner loads the CANN environment from `ASCEND_HOME_PATH`. DBI discovers the
public architecture and matching toolchain from the CANN Runtime actually loaded by
the application; runners no longer provide DBI environment variables.

Run one target smoke test directly with its local `run.sh`:

```bash
bash npu_tools/npu_sanitizer/demo/examples/synccheck/multi_launch_pairs/run.sh
bash npu_tools/npu_sanitizer/demo/examples/synccheck/flag_set_set_wait_wait/run.sh
```

## Pass Criteria

- Normal cases: the summary matches, `verify.py` prints `synccheck verification passed`,
  and the runner returns 0.
- Error cases: the specified diagnostic and summary match, `verify.py` prints
  `synccheck verification passed`, and the runner returns 0.
- Build failures, handshake failures, incomplete sessions, and diagnostic or count
  mismatches all fail the smoke test.

## Smoke Matrix

Excluding the synchronization-free `no_sync` control, the suite covers pure-AIC,
pure-AIV, and 1:1 and 1:2 mix synchronization scenarios.
The multi-launch cases enqueue two kernels consecutively on one stream without an
intermediate synchronization; one `aclrtSynchronizeStreamWithTimeout` settles the
synchronization events from both launches.

| Smoke Test | Expected Result | Guarded Scenario |
| --- | --- | --- |
| `no_sync/` | Normal | No false synccheck report when the kernel has no synchronization instructions. |
| `multi_launch_unconsumed/` | Error | Two AIV kernels are launched consecutively on one stream and synchronized once; the unconsumed `SET_FLAG` values for `EVENT_ID0` and `EVENT_ID1` are both reported. |
| `multi_launch_pairs/` | Normal | Two AIV kernels are launched consecutively on one stream and synchronized once; both event-specific `SET_FLAG -> WAIT_FLAG` pairs are consumed correctly. |
| `flag_set_set_wait_wait/` | Error | An AIC M `SET -> SET -> WAIT -> WAIT` sequence reports both a duplicate `SET_FLAG` and an unmatched `WAIT_FLAG`. |
| `flag_mutex_error_bundle/` | Error | A single AIC block synchronizes once and aggregates two primitive error reports: a duplicate flag `SET_FLAG` and an unmatched mutex `RLS_BUF`. |
| `mix_wait_without_set/` | Error | Both the AIC M and AIV V branches of `__mix__(1, 1)` issue a WAIT without SET; the case passes only when both unmatched closes are captured. |
| `mutex_pair/` | Normal | `GET_BUF -> RLS_BUF` pairs on the MTE2 and V pipelines are consumed correctly. |
| `mix_mutex_unreleased/` | Error | Both the AIC M and AIV V branches of `__mix__(1, 2)` issue an unreleased `GET_BUF`; the case passes only when all three unconsumed opens are captured. |
| `split_wrong_side_mutex_noop/` | Normal | On dav-3510, wrong-side mutex API calls in the AIC/AIV branches of a split kernel are filtered and produce no callback or false positive. |
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
- `mix_wait_without_set` and `flag_set_set_wait_wait` block device instructions and must use bounded stream
  synchronization and `aclrtDestroyStreamForce` so the smoke process can terminate.
- `demo/tests/check_synccheck_examples_layout.sh` guards the package contract,
  shell syntax, and documentation matrix for every smoke test. After changes to
  synchronization-event translation, the checker, renderer, or runtime path, run
  at least the affected smoke tests and use `multi_launch_pairs` as the basic normal-path
  smoke test.
