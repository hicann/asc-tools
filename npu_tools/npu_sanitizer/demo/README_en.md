# ACLSan Demo

This directory directly reuses the production `npu_sanitizer/npu_check_cli` and
`npu_sanitizer/npu_check` components. `npu-check` launches an example program,
loads `libnpu_check.so` through CANN API Injection, and then subscribes to and
enables the callbacks for the selected tool through `libacl_san.so`.

## Example Directories

| Directory | Source File | Executable | Description |
| --- | --- | --- | --- |
| `examples/memcheck/add` | `add.asc` | `demo` | AscendC vector addition and basic GM access example. |
| `examples/memcheck/datacopy_stride` | `datacopy_stride.asc` | `demo` | DataCopy stride out-of-bounds example. |
| `examples/memcheck/matmul_basic_api` | `matmul_basic_api.asc` | `demo` | Basic matrix multiplication example. |
| `examples/memcheck/matmul_leakyrelu_basic_api` | `matmul_leakyrelu_basic_api.asc` | `demo` | Fused Matmul and LeakyReLU example. |
| `examples/basic_func/multi_kernel` | `multi_kernel.asc` | `demo` | Basic multi-kernel loading and instrumentation example. |
| `examples/basic_func/padding_register_state` | `padding_register_state.asc` | `demo` | Basic `SET_PADDING` register-manager example. |
| `examples/basic_func/dual_tool_multi_launch_aggregate` | `dual_tool_multi_launch_aggregate.asc` | `demo` | Dual-tool, multi-launch aggregation example. |
| `examples/synccheck` | One standalone directory per scenario | `demo` | Normal and abnormal synchronization pairing examples. |

Each example directory under `examples/synccheck` contains its own `.asc` file,
CMake configuration, runner, and result verifier. `verify_common.py` provides common
result checks. Each case's `run.sh` builds and runs its `.asc` in its own
`build/` directory. The
`.asc` file contains the complete AscendC kernel, ACL initialization, kernel
launch, and cleanup logic. The retained suite balances normal pairing, duplicate
opens, unmatched closes, unconsumed opens, multi-launch aggregation, and
multi-block isolation across SET_FLAG/WAIT_FLAG and GET_BUF/RLS_BUF.
`mix_wait_without_set` and `flag_set_set_wait_wait` cover blocking failure
scenarios; they use `aclrtSynchronizeStreamWithTimeout` to bound the wait and
`aclrtDestroyStreamForce` to destroy a stream that still contains a blocked kernel.

## Build and Artifacts

The top-level `build.sh` loads `${ASCEND_HOME_PATH}/set_env.sh`, runs
`bash build.sh --pkg` at the repository root, and installs the generated `.run`
package into the current `ASCEND_HOME_PATH`. Runners use:

```text
${ASCEND_HOME_PATH}/<arch>-linux/bin/npu-check
```

Every example directory is a complete standalone CMake project. Its `run.sh`
configures and builds it under:

```text
npu_sanitizer/demo/examples/<category>/<case-name>/build
```

The example executable is written directly to that build directory, for example:

```text
npu_sanitizer/demo/examples/memcheck/add/build/demo
```

`build.sh` installs `npu-check` and its shared libraries. The first kernel binary
load creates and caches `probe.o` and `ctrl.bin` at runtime. Each case stores
its log and other run files in its own `build/` directory. Synccheck logs are written
to `demo/examples/synccheck/<case-name>/build/npu_check.log`, while Matmul inputs and
outputs are stored in the case-local `build/run/` directory.

### Shared Libraries in the Runtime Chain

| Artifact | CMake Target or Source | Source Code | How It Enters the Demo | Purpose |
| --- | --- | --- | --- | --- |
| `libnpu_check.so` | `npu_check` | `npu_sanitizer/npu_check/src/` | `npu_check` writes its absolute path to `ACL_API_INJECTION`; CANN loads it during `aclInit()` and calls `acltoolInitialize()` | Establishes the UDS session, subscribes to and enables callbacks for the selected tool, and produces diagnostics and the summary. |
| `libacl_san.so` | `acl_san` | `npu_sanitizer/sanitizer_api/`, with sources explicitly listed by its `CMakeLists.txt` | A `DT_NEEDED` dependency of `libnpu_check.so` | Provides the public ACLSan API, callback routing, callback data construction, and Runtime hook replacements. |
| `libacl_tool_injection.so` | `acl_tool_injection` | `npu_compute/src/injection_hook/injection_hook.cpp` | A `DT_NEEDED` dependency of `libacl_san.so` | Installs fixed trampolines and stores and switches the `orig/hook/custom` Runtime entry points. |
| `libacl_rt.so` | CANN installation library | CANN 9.2.0 installation | A `DT_NEEDED` dependency of the examples and `libacl_tool_injection.so` | Exports `aclrt*` and `aclrtApiInjectionGetFunc/SetFunc`, then invokes the underlying RTS implementation. |
| `libruntime.so` | CANN Runtime | CANN 9.2.0 installation | A `DT_NEEDED` dependency of `libacl_rt.so` | Provides the underlying `rt*` and RTS implementation; it does not directly export `aclrtMalloc`. |
| `libprofapi.so` | `CANN::profapi` | CANN 9.2.0 installation | A `DT_NEEDED` dependency of `libacl_tool_injection.so` and `libruntime.so` | Provides CANN profiling and tool injection support. |

The repository packaging flow builds the production asc-tools package without the
test-only Runtime/Profiling stub. Explicitly building other
regular `npu_compute` targets can also produce `libacl_pti.so` (target `acl_pti`)
and `libnpu-compute.so` (target `npu_compute`), but these libraries are not part of
the demo runtime chain. System libraries such as `libstdc++.so`, `libc.so`, and
`libdl.so` are not generated by this repository.

In addition to the shared libraries, the package contains `npu-check`
from the `npu_check_cli` target. Case runners build their example targets on demand
in standalone build directories. `libnpu_check.so` in the shared build tree links
directly against `acl_san`, while the CLI specifies
`libnpu_check.so` through `ACL_API_INJECTION`. The demo directory no longer contains
source copies of `npu_check` or `npu_check_exec`, and it no longer generates the
legacy `npucheck` executable.

## Running Examples

Load the CANN environment, then build the shared tools from the repository root:

```bash
source /home/cty/cann_0829/cann/set_env.sh
bash ./npu_sanitizer/demo/build.sh
```

Then run the script in any case directory:

```bash
bash ./npu_tools/npu_sanitizer/demo/examples/memcheck/add/run.sh
bash ./npu_tools/npu_sanitizer/demo/examples/memcheck/matmul_basic_api/run.sh
bash ./npu_tools/npu_sanitizer/demo/examples/basic_func/padding_register_state/run.sh
bash ./npu_tools/npu_sanitizer/demo/examples/basic_func/dual_tool_multi_launch_aggregate/run.sh
bash ./npu_tools/npu_sanitizer/demo/examples/synccheck/multi_launch_pairs/run.sh
```

Run all 17 basic-capability and Synccheck cases with:

```bash
bash ./npu_sanitizer/demo/run_smoke.sh
```

`run_smoke.sh` is the self-contained entry point for the full smoke suite; no
separate `build.sh` invocation is required. It uses `build.sh` to package and install
asc-tools, then runs
every case in a fixed order. Each console log is saved under
`demo/build/smoke/<category>/<case-name>.log`. A package build or install failure stops
the script immediately. A case failure does not stop the remaining cases, and
the script returns 1 after the complete run when any case failed.

Run the complete Synccheck suite with:

```bash
bash ./npu_sanitizer/demo/examples/synccheck/run_all.sh
```

The top-level `build.sh` requires `ASCEND_HOME_PATH` and loads
`${ASCEND_HOME_PATH}/set_env.sh`. Because the `.run` installer writes to
`<install-path>/cann`, the script creates a temporary sibling `cann` symlink when
the target directory has another name. An occupied or mismatched bridge path is a
fatal error. Cleanup removes only a symlink created by the current invocation and
only while it still points to the target CANN tree; it never removes that tree or
its contents. Each case `run.sh` requires
the current shell to have loaded `set_env.sh`, which provides `ASCEND_HOME_PATH` and
the CANN toolchain environment. If CANN is installed elsewhere, load that environment
before running:

```bash
source /path/to/cann/set_env.sh
bash ./npu_sanitizer/demo/build.sh
```

Each case runner configures and builds its target under
`demo/examples/<category>/<case-name>/build`, then launches it with memcheck,
synccheck, or both tools. Matmul runners also generate input and golden data and
run `verify_result.py`. On success, the verifier prints `test pass!`. Every runner
checks result validation, application status, UDS handshake, sanitizer summary, and
session completion. A case with fully verified expected diagnostics returns 0 and
prints `example verification passed: <category>/<case-name>`; a log mismatch returns
nonzero.

`padding_register_state` uses a single cube block to execute
`asc_set_l13d_padding(0x12)` followed by `asc_set_l13d_padding(0x34)`. Device logs prove
that the `0x12` and `0x34` `SET_PADDING` raw values are decoded and delivered to the
same register-state key. `register_state_manager_test` independently calls `Get()` to
verify that the same key retains only the latest value.
`dual_tool_multi_launch_aggregate` first triggers a GM out-of-bounds read and then
leaves an unconsumed `SET_FLAG` on the same stream before one synchronization, proving
that Memcheck and Synccheck aggregate both launches together.

## Runtime Flow

```text
npu-check
  -> creates a private UDS session and sets ACL_API_INJECTION to the absolute path of libnpu_check.so
  -> launches the selected example
  -> the example calls aclInit() from libacl_rt.so
  -> the CANN profiling/injection mechanism loads libnpu_check.so and calls acltoolInitialize()
  -> libnpu_check.so receives the tool configuration over UDS and calls libacl_san.so to Subscribe/Enable
  -> libacl_san.so calls aclrtApiInjectionGetFunc/SetFunc through libacl_tool_injection.so
  -> the binary-load hook compiles the callback-selected probe online, then links and instruments the kernel binary
  -> subsequent aclrt* calls enter the hook; replacements call the original aclrt* functions and reach the RTS implementation in libruntime.so
  -> callback data is passed to the selected checker; diagnostics, the summary, and session end are returned through UDS to the CLI
```

The only source definition of `acltoolInitialize` is in
`npu_sanitizer/npu_check/src/tool_manager/entry.cpp`.

## Verification Scope

- `tests/check_product_npu_check_layout.sh` checks production targets, entry-point
  symbol spelling, and that the demo does not reference the legacy launcher or
  unsupported tools.
- `tests/check_examples_layout.sh` checks all example categories, runner
  self-validation contracts, and `run_smoke.sh` aggregation behavior, and confirms
  that the removed `examples/test` directory has not re-entered the build or runtime
  chain.
- `tests/check_end_to_end.sh` runs add on a real Device and checks the UDS lifecycle,
  application exit status, the `acltoolInitialize@@NPU_CHECK_1.0` export, and key
  ELF `DT_NEEDED` relationships.
- `tests/check_probe_device_end_to_end.sh` checks probe record readback for add.
  `tests/check_matmul_end_to_end.sh` checks the record count, AIV block coverage,
  and calculation results of both matmul examples.

In the current CANN/Device environment, all three examples have been verified for
Device probe instrumentation, record readback, memcheck analysis, and calculation
results. Add, basic matmul, and fused matmul read back 24, 12, and 36 records,
respectively. This is real Device end-to-end evidence, but it does not imply
coverage of the scenarios listed below.

## Current Limitations and TODOs

Building the examples requires the ASC compiler from CANN 9.2.0. Running them also
requires an available Ascend Device and a matching Driver. If no device is available,
`aclInit` may fail; this does not indicate a failure in the example's CMake
configuration or binary linkage.

The following items remain incomplete:

- The current probe end-to-end tests cover only the CCE instructions triggered by
  the three examples. Claiming broader instruction coverage requires additional
  CCE instruction cases, record parsing assertions, and real Device end-to-end tests.
- Concurrency, callback reentrancy, and shared-library unload safety have not been
  verified. If these scenarios are within the supported product scope, additional
  lifecycle and stress tests are required.
