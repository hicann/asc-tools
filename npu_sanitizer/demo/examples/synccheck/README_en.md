# Synccheck Samples

Each same-named subdirectory is an independently buildable and verifiable
synccheck sample package containing an `.asc` file, `CMakeLists.txt`, `run.sh`,
and `verify.py`. The `.asc` file directly contains the synchronization instruction
sequence, related data-movement context, ACL initialization, device memory
management, kernel launch, stream management, and cleanup logic.

## Running Samples

Invoke the top-level demo runner from any directory. Use
`synccheck/<file-name>` without the `.asc` suffix:

```bash
bash npu_sanitizer/demo/run.sh synccheck/single_pair
bash npu_sanitizer/demo/run.sh synccheck/single_unconsumed
```

The runner first invokes the `run.sh` in the corresponding subdirectory to build
the `aclsan_demo_synccheck_<file-name>` target independently. It instruments the
target with `examples/synccheck/symbol_ordering.txt` and then launches it through
`npu_check --tool synccheck`. The original `npu_check` output remains visible in
the terminal and is also written to the example build directory. Finally, the
example's `verify.py` checks the exit status, summary, diagnostic type, and session
lifecycle.

Normal samples return 0. Abnormal samples detected by synccheck print diagnostics,
and `npu_check` returns a nonzero status. `wait_without_set` and
`mutex_duplicate_lock` leave device instructions blocked, so they use bounded
stream synchronization and forced stream destruction to allow the example process
to exit.

## Samples

| Directory | Expected Result | Scenario |
| --- | --- | --- |
| `no_sync/` | Normal | Executes only non-synchronization memory instructions. |
| `single_pair/` | Normal | One matching `SET_FLAG -> WAIT_FLAG` pair. |
| `single_unconsumed/` | Error | One unconsumed `SET_FLAG`. |
| `duplicate_set/` | Error | Executes `SET_FLAG` twice consecutively with the same key. |
| `multi_block_isolation/` | Error | Two blocks use the same key, but only block 0 waits. |
| `wait_without_set/` | Error | A `WAIT_FLAG` without a matching `SET_FLAG`; requires timed synchronization and forced destruction of the blocked stream. |
| `mutex_pair/` | Normal | The MTE2 and V pipelines each execute a matching `GET_BUF -> RLS_BUF` pair. |
| `mutex_unreleased/` | Error | Executes `GET_BUF` without a following `RLS_BUF`. |
| `mutex_unlock_without_lock/` | Error | Executes `RLS_BUF` without a matching `GET_BUF`. |
| `mutex_duplicate_lock/` | Error | Executes blocking `GET_BUF` twice consecutively with the same mutex ID. |
| `mutex_multi_block_isolation/` | Error | Two blocks use the same mutex ID, but only block 0 releases it. |
