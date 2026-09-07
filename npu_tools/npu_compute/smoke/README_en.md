# NPU Compute dav3510 Smoke Baseline

This directory contains the immutable daily smoke baseline for `npu-compute` on dav3510. It verifies Ascend C examples, tool installation, runtime collection, report export, and result validation. The baseline contains exactly the following six cases. Do not add or remove cases based on local directory contents.

## Requirements

- Install CANN 9.2 with the Bisheng compiler and Driver that match dav3510. Run the suite on a host with an available dav3510 device.
- Source `set_env.sh` for the target CANN installation so that `ASCEND_HOME_PATH` is set.
- Use an account that can build the asc-tools package and install it into the CANN directory that contains `${ASCEND_HOME_PATH}`.

The default entry point builds and installs the asc-tools package before it builds the six cases. You do not need to build the tool or cases separately. The target architecture is fixed to `dav-3510`.
Each example launches eight blocks. Each block uses independent input and output slices, rather than using repeated writes as multi-block coverage.

## Fixed Cases

| Case | Scenario |
| --- | --- |
| `vector_add` | Vector addition on a Vector Core. Verifies basic Vector computation and data movement. |
| `cube_mmad` | A 16 x 16 matrix multiplication on a Cube Core. Verifies `Mmad` and `Fixpipe`. |
| `mix_1_1` | Matrix multiplication and LeakyReLU with one Cube Core and one Vector Core. |
| `mix_1_2` | Matrix multiplication and split LeakyReLU with one Cube Core and two Vector Cores. |
| `reg_add` | Register-programming addition with `__simd_vf__` on a Vector Core. |
| `simt_hello` | Basic thread-index writeback with the SIMT compiler option enabled. |

## Run

Run the full smoke suite from the repository root:

```bash
bash npu_tools/npu_compute/smoke/run_smoke.sh
```

By default, the entry point builds and installs asc-tools from the current workspace. To reuse an installed `npu-compute` and skip that step, run:

```bash
bash npu_tools/npu_compute/smoke/run_smoke.sh --skip-asc-tools-build
```

When you skip the build and installation, `NPU_COMPUTE_SMOKE_CLI` can identify an executable by absolute path. If it is unset, the runner uses `npu-compute` from PATH. Both modes rebuild and run all six examples.

After installing `npu-compute`, you can also run individual cases:

```bash
bash npu_tools/npu_compute/smoke/examples/vector_add/run.sh
bash npu_tools/npu_compute/smoke/examples/cube_mmad/run.sh
bash npu_tools/npu_compute/smoke/examples/mix_1_1/run.sh
bash npu_tools/npu_compute/smoke/examples/mix_1_2/run.sh
bash npu_tools/npu_compute/smoke/examples/reg_add/run.sh
bash npu_tools/npu_compute/smoke/examples/simt_hello/run.sh
```

## Msprof Interface Sample

`examples/mix_1_2_msprof` is based on `mix_1_2` and independently verifies `MsprofStart`, `MsprofStop`, and the raw-data callback. It is not one of the six fixed cases and `run_smoke.sh` does not run it. In addition to sourcing the target CANN `set_env.sh`, install the current asc-tools package into that CANN before running it, so that `${ASCEND_HOME_PATH}/<host>-linux/tools/npu_tools/lib64/libacl_tool_injection.so` is available, where `<host>` is the host architecture.

```bash
bash npu_tools/npu_compute/smoke/examples/mix_1_2_msprof/run.sh
```

The sample configures `PROF_TASK_TIME_MASK | PROF_AICORE_METRICS_MASK` without log collection, writes 10 PMU events into all PMU slots, and requests block-level data with `PROF_COMPUTE_ALL_BLOCK`. The callback separately prints `msprof PMU task:` and `msprof PMU block:` records, including task/stream, core, block/sub-block, cycles, and PMU counter values. The sample requires both task and block PMU records before it passes.

## npu-compute and msopprof Comparison

The following entry point builds the six fixed cases twice and compares their common performance data; it excludes `mix_1_2_msprof`. Pass each CANN `set_env.sh` through `--npu-compute-env` and `--msopprof-env`. The two CANN installations do not share build directories, executables, or runtime libraries.

```bash
bash npu_tools/npu_compute/smoke/compare_tools.sh \
  --npu-compute-env /path/to/npu-compute/cann/set_env.sh \
  --msopprof-env /path/to/msopprof/cann/set_env.sh \
  --output /tmp/npu-compute-msopprof-comparison
```

Both tools collect `PipeUtilization`, `Memory`, `MemoryL0`, `MemoryUB`, and `L2Cache`. Each `comparison/<case>/comparison.csv` reports source values, absolute deltas, and relative deltas by `section`, `block_id`, `sub_block_id`, and common field. `coverage.csv` and `summary.md` report missing sections, fields, or rows. This is report-only mode: metric differences do not fail the command. Build or collection failures still return a nonzero status and retain tool logs under `logs/`.

For each case, npu-compute first writes raw CSV data to the directory reported by `npu-compute: data-directory=`; the script then copies it to `npu-compute/<case>/raw/`. After a successful collection, the script unpacks the sibling `.npu-rep` with `npu-compute --import --export` into `npu-compute/<case>/imported/` and compares the imported data first. If application result validation fails and no `.npu-rep` is published, comparison still uses the collected CSV files in `raw/`.

To make the counters more visible, all fixed scenarios keep eight blocks while vector/reg use 4096 elements per block, cube/mix process 32 verified `16 x 16 x 16` tiles per block, and SIMT uses 256 threads per block with 64 values written per thread.

Each case first runs `npu-compute --list-sections`, then passes one `--section <Section>` argument for every returned Section. The runner does not collect only a fixed baseline. The following five baseline Sections must be available:

- `PipeUtilization`
- `Memory`
- `MemoryL0`
- `MemoryUB`
- `L2Cache`

Full-suite logs are stored in `npu_tools/npu_compute/smoke/build/logs/<case>.log`. Each case stores its executable, `npu_compute.log`, and `result.npu-rep` under `examples/<case>/build/`. The `npu-compute: data-directory=<absolute-path>` diagnostic identifies the collection data directory for that run.

## Pass Criteria

A case prints `[PASSED] <case>` only when all of the following conditions are met:

- Compilation and `npu-compute` collection succeed. The application prints the exact `result verification passed: <case>` result marker. It also prints `result verification passed: <case> block=<n>` for each of eight blocks. Each block uses an independent input magnitude and independent expected-value validation.
- `PipeUtilization.csv` contains the expected core rows: `vector_add` and `reg_add` contain exactly `{vector0}`; `cube_mmad` contains exactly `{cube0}`; `mix_1_1` contains exactly `{cube0,vector0}`; and `mix_1_2` contains exactly `{cube0,vector0,vector1}`. The same core can appear in multiple rows, but no expected `sub_block_id` can be missing and no extra value is allowed. Every row for these constrained cases has a nonempty `block_id`. `simt_hello` does not constrain the core set.
- `HardwareInfo.jsonl` contains exactly the `Host Info`, `Device Info`, `CPU Information`, `AI Core Information`, and `Memory Information` records in that order. The device architecture is `3510`.
- Every Section returned by `--list-sections` has a nonempty CSV file. Each CSV uses strict CSV syntax, includes a header and at least one data row, has nonempty and unique header names, and has the same field count in every data row as in the header. Metric cells for unavailable counters can be empty.
- `result.npu-rep` is a nonempty regular file. The data-directory and report diagnostics match the paths requested for the run.

The full suite continues after failures and prints `PASS` or `FAIL` for every case. A successful suite ends with:

```text
total=6 passed=6 failed=0
```

## Maintenance

Do not change fixed smoke scenarios during ordinary feature work. When an example, runner, test, or this document needs an update, review the behavior change separately.

Run the layout check before submitting changes:

```bash
bash npu_tools/npu_compute/smoke/tests/check_smoke_layout.sh
```
