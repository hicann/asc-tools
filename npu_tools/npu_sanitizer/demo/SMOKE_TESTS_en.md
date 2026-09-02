# Demo Smoke Tests

This document describes the minimum smoke-test workflow for the examples under
`npu_tools/npu_sanitizer/demo/examples`. The tests quickly verify example compilation,
`npu-check` startup, Device execution, and result validation. Run all commands from the repository root.

## Environment

Load the CANN environment where asc-tools will be installed:

```bash
source /path/to/cann/set_env.sh
```

Running the examples also requires an available Ascend Device and a Driver compatible with the CANN installation.
Package and install the tools and shared libraries before running any case:

```bash
bash npu_tools/npu_sanitizer/demo/build.sh
```

## Full Smoke Suite

```bash
bash npu_tools/npu_sanitizer/demo/run_smoke.sh
```

No separate `build.sh` invocation is required. The script uses `build.sh` to package
and install asc-tools, then runs all 18 cases in a fixed order. A package build or
install failure stops the script
immediately. Case failures do not stop the remaining cases. Each case output is
saved to `demo/build/smoke/<category>/<case-name>.log`; the final summary prints
`total/passed/failed`, and the script returns 1 when any case failed.

## Add

```bash
bash npu_tools/npu_sanitizer/demo/examples/memcheck/add/run.sh
```

This example runs vector addition under `memcheck` and intentionally triggers one
8256-byte GM out-of-bounds read. The runner checks `errors=1` and a complete session
before returning 0.

## DataCopy Stride

```bash
bash npu_tools/npu_sanitizer/demo/examples/memcheck/datacopy_stride/run.sh
```

This example intentionally triggers four 32-byte GM out-of-bounds reads. Its runner
checks the stride parameter field, `errors=4`, and session completion.

## GM Memory Access

```bash
bash npu_tools/npu_sanitizer/demo/examples/memcheck/memory_access/run.sh
```

This runner executes 18 representative dav-3510 GM transfer cases covering Vector/Cube DMA,
Multi ND/DN2NZ, Fixpipe, NDDMA, LoadData 2DV2, and public API lowering. It validates CCE instructions,
SET state, cbdata layouts, out-of-bounds diagnostics, and session completion. Run the complete supported
230-case matrix with:

```bash
bash npu_tools/npu_sanitizer/demo/tests/check_memory_access_end_to_end.sh all
```

Every matrix example uses ACL APIs directly on the host and AscendC APIs in the kernel, without a demo runtime helper.

## Basic Capability Cases

```bash
bash npu_tools/npu_sanitizer/demo/examples/basic_func/multi_kernel/run.sh
bash npu_tools/npu_sanitizer/demo/examples/basic_func/padding_register_state/run.sh
bash npu_tools/npu_sanitizer/demo/examples/basic_func/dual_tool_multi_launch_aggregate/run.sh
```

`multi_kernel` checks one 32-byte out-of-bounds read per kernel and distinct
launch IDs. `padding_register_state` checks an error-free summary, a shared
register-state key, and `SET_PADDING` state propagation for `0x12` and `0x34`.
`dual_tool_multi_launch_aggregate` triggers a Memcheck GM out-of-bounds error and a
Synccheck unconsumed `SET_FLAG` across two launches, then verifies both summaries after one synchronization.

## Matmul Basic API

```bash
bash npu_tools/npu_sanitizer/demo/examples/memcheck/matmul_basic_api/run.sh
```

The runner builds the basic matrix multiplication example, generates input and golden data, and launches it under
`memcheck`. In addition to a complete tool session, the result verifier must print `test pass!`.

## Matmul LeakyReLU Basic API

```bash
bash npu_tools/npu_sanitizer/demo/examples/memcheck/matmul_leakyrelu_basic_api/run.sh
```

This example covers coordinated Cube and Vector execution for Matmul and LeakyReLU. It runs under `memcheck` and
validates the output against golden data. The result verifier must print `test pass!`.

## Synccheck Smoke Tests

Synccheck has an independent smoke suite. Run and summarize the complete suite with:

```bash
bash npu_tools/npu_sanitizer/demo/examples/synccheck/run_all.sh
```

To run one case, invoke the case-local `run.sh` directly:

```bash
bash npu_tools/npu_sanitizer/demo/examples/synccheck/multi_launch_pairs/run.sh
```

The retained suite balances normal and abnormal pairing, multi-launch aggregation,
and multi-block isolation across SET_FLAG/WAIT_FLAG and GET_BUF/RLS_BUF.
`mix_wait_without_set` and `flag_set_set_wait_wait` cover blocking scenarios.

Every Synccheck `RunSample` returns 0, and `npu_check` forwards that status. Each
runner captures the real status for `verify.py`; `has_errors`, the summary, and
diagnostics carry the checker result. Both normal and negative cases return 0
after verification and session completion pass, and print
`example verification passed: synccheck/<case-name>`; `run_all.sh` and
`run_smoke.sh` use that result for `PASS/FAIL` summaries.

See [synccheck/README_en.md](examples/synccheck/README_en.md) for the full scenario matrix, environment variables,
and pass criteria.

## Result Criteria

- A runner build failure, abnormal application exit, failed tool handshake, or incomplete session is a failure.
- `add`, `datacopy_stride`, and `multi_kernel` are expected memcheck-error cases; each runner checks its fixed
  error count. `dual_tool_multi_launch_aggregate` is an expected dual-tool error case and checks both error counts.
  Both matmul examples and `padding_register_state` require `errors=0`; matmul must also pass
  golden-data validation.
- An expected synccheck diagnostic is not a smoke-test failure. A case fails only when its diagnostics or summary do
  not match the expectations in its `verify.py`.
