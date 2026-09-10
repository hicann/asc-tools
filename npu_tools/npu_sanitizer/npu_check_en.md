# npu-check User Guide

npu-check is a runtime correctness checking tool for Ascend NPUs. It helps users check operator correctness during development. Run an application with npu-check to view error types, locations, and check results.

This guide is intended for Ascend C operator developers. It covers environment setup, command-line usage, checking features, report reading, and usage examples.

## Contents

- [1. Overview](#1-overview)
- [2. Getting Started](#2-getting-started)
- [3. Command-Line Options](#3-command-line-options)
- [4. Limitations of the Current Version](#4-limitations-of-the-current-version)

## 1. Overview

### 1.1 Checking Features

| Feature | Purpose | Issues Checked |
| --- | --- | --- |
| `memcheck` | Checks reads and writes to device global memory (GM) | Out-of-bounds accesses, accesses to addresses not recognized as valid allocations, and accesses to memory recorded as freed |
| `synccheck` | Checks whether device synchronization operations are correctly paired | Repeated event notifications, waits for events that have not been notified, notifications without corresponding waits, and mismatched notification/wait pairs |

The two features can be used separately or enabled together for the same application run.

### 1.2 Usage Scenarios

Typical uses of npu-check include:

- Checking normal inputs, boundary inputs, and tail-block handling after developing an operator.
- Checking memory accesses after changing tiling, copy lengths, or strides.
- Checking synchronization after modifying inter-pipeline synchronization logic.
- Investigating intermittent incorrect results or device execution errors.

Correct numerical results do not necessarily mean the code is correct. For example, a program may read beyond an allocation while still producing correct output because the extra data does not contribute to the final computation.

## 2. Getting Started

### 2.1 Environment Setup

Follow the [Quick Start](../../docs/00_quick_start.md) to prepare the environment. Before using npu-check, install a CANN package compatible with the target NPU and driver, then load the CANN environment variables. Replace `<CANN-install-directory>` in the following command with the actual installation directory:

```bash
source <CANN-install-directory>/cann/set_env.sh
```

Run the following command to check that `npu-check` is available:

```bash
npu-check --help
```

The target program must be able to run independently in the current environment and execute its kernels successfully.

### 2.2 Usage Examples

Check device memory accesses:

```bash
npu-check --tool memcheck ./my_app
```

When no feature is specified, memory access checking (`memcheck`) is enabled by default:

```bash
npu-check ./my_app
```

### 2.3 Source Location Information

To locate source files and line numbers directly from a report, preserve line information when compiling the operator.

## 3. Command-Line Options

### 3.1 Syntax

```text
npu-check [--tool <name>]... [--log-file <path>] [--] <application> [args...]
```

### 3.2 Option Reference

| Option | Input | Default Behavior | Description |
| --- | --- | --- | --- |
| `--tool <name>` | `memcheck`, `synccheck` | Enables `memcheck` when omitted | Can be specified multiple times to enable both tools; repeating the same tool does not repeat the check |
| `--log-file <path>` | A file path or an existing directory | Displays the report in the terminal | Saves diagnostic information |
| `-h`, `--help` | None | Does not display help | Displays command help |
| `--` | None | Optional | Separates tool options from application arguments |

Run synchronization checking only:

```bash
npu-check --tool synccheck ./my_app
```

Enable both checks:

```bash
npu-check --tool memcheck --tool synccheck ./my_app
```

### 3.3 Saving to a File or Directory

Save to a specified file:

```bash
mkdir -p reports
npu-check --tool memcheck --log-file reports/memcheck.log ./my_app
```

When `--log-file` is specified, the report is written to the file and is not also displayed in the terminal. The application's own output is still displayed in the terminal and recorded in the file.

## 4. Limitations of the Current Version

- Device checking currently covers Ascend 950 (dav-3510).
- This tool can only be used in the environment of the CANN package version that contains it.
- Only a single invocation of a single operator using `<<<>>>` is currently supported.
- `memcheck` currently checks only GM accesses made by supported data transfer instructions. Scalar single-element memory access checking is not currently supported.
- `synccheck` checks intra-core synchronization pairing issues and repeated synchronization setting issues.
- The target program must be able to run independently without `npu-check`.
- The target program must be an executable file.
- Multi-process and multi-threaded execution are not supported.
