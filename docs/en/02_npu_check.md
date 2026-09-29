# npu-check

## Overview

npu-check is a runtime correctness checking tool for Ascend NPUs. It helps users check operator correctness during development. Run an application with npu-check to view error types, locations, and check results.

This guide is intended for Ascend C operator developers. It covers environment setup, command-line usage, checking features, report reading, and usage examples.

When using this tool through an Agent, refer to the [tool-npu-check Skill](../../skills/tool-npu-check/SKILL.md) for applicability checks, checker selection, controlled execution, report interpretation, and operator retesting.

### Checking Features

| Feature | Purpose | Issues Checked |
| --- | --- | --- |
| `memcheck` | Checks GM reads and writes and supported L1, L0A, L0B, and L0C accesses | GM allocation bounds, invalid addresses, use after free, and on-chip memory capacity bounds |
| `synccheck` | Checks whether device synchronization operations are correctly paired | Repeated event notifications, waits for events that have not been notified, notifications without corresponding waits, and mismatched notification/wait pairs |
| `initcheck` | Checks whether persistent registers are set before their first dependent use | Data-transfer or compute instructions using persistent registers before they are set, or registers set only after use |

The three features can be used separately or enabled together for the same application run.

### Usage Scenarios

Typical uses of npu-check include:

- Checking normal inputs, boundary inputs, and tail-block handling after developing an operator.
- Checking memory accesses after changing tiling, copy lengths, or strides.
- Checking synchronization after modifying inter-pipeline synchronization logic.
- Investigating intermittent incorrect results or device execution errors.

Correct numerical results do not necessarily mean the code is correct. For example, a program may read beyond an allocation while still producing correct output because the extra data does not contribute to the final computation.

## Environment Preparation

Follow the [Quick Start](00_quick_start.md) to prepare the environment. Before using npu-check, install a CANN package compatible with the target NPU and driver, then load the CANN environment variables. Replace `<CANN-install-directory>` in the following command with the actual installation directory:

```bash
source <CANN-install-directory>/cann/set_env.sh
```

Run the following command to check that `npu-check` is available:

```bash
npu-check --help
```

The target program must be able to run independently in the current environment and execute its kernels successfully.

### Source Location Information

To locate source files and line numbers directly from a report, preserve line information when compiling the operator.

## Command Format

```text
npu-check [--tool <name>]... [--log-file <filepath>] [--] <application> [args...]
```

### Options

| Option | Input | Default Behavior | Description |
| --- | --- | --- | --- |
| `--tool <name>` | `memcheck`, `synccheck`, `initcheck` | Enables `memcheck` when omitted | Can be specified multiple times to enable multiple tools; repeating the same tool does not repeat the check |
| `--log-file <filepath>` | A file path | Displays the report in the terminal | Saves diagnostic information |
| `-h`, `--help` | None | Does not display help | Displays command help |
| `--` | None | Optional | Separates tool options from application arguments |

## Usage Examples

Check device memory accesses:

```bash
npu-check --tool memcheck ./my_app
```

When no feature is specified, memory access checking (`memcheck`) is enabled by default:

```bash
npu-check ./my_app
```

Run synchronization checking only:

```bash
npu-check --tool synccheck ./my_app
```

Enable all checks:

```bash
npu-check --tool memcheck --tool synccheck --tool initcheck ./my_app
```

### Specifying the Report Location

Save to a specified file:

```bash
mkdir -p reports
npu-check --tool memcheck --log-file reports/memcheck.log ./my_app
```

When `--log-file` is specified, the report is written to the file and is not also displayed in the terminal. The application's console output is not affected.

## Constraints

- Device checking currently covers Ascend 950 (dav-3510).
- This tool can only be used in the environment of the CANN package version that contains it.
- A single invocation of a single operator through either `<<<>>>` or ACLNN is currently supported.
- `memcheck` checks GM accesses from supported instructions and accesses beyond buffer boundaries in L1, L0A, L0B, and L0C.
- `synccheck` checks intra-core synchronization pairing issues and repeated synchronization setting issues.
- `initcheck` checks only currently supported data-transfer and Fixpipe instructions and their explicit persistent-register dependencies.
- The target program must be able to run independently without `npu-check`.
- The target program must be an executable file.
- Multi-process and multi-threaded execution are not supported.
