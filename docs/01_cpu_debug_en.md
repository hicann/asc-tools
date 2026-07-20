# CPU Debug

## Overview

Before deploying operators to the NPU, the CPU Debug tool helps users perform basic functional and accuracy verification on the CPU. Developers write operator kernel-side source code using Ascend C, compile it with the bisheng compiler to generate CPU-domain executables, and then use standard debugging tools such as gdb to debug the operators.

## Environment Preparation

Please refer to [Quick Start](00_quick_start_en.md#Environment Preparation) to complete the environment preparation.

## Usage

Using the [cpudebug](../examples/02_cpudebug/) example, you can start CPU debugging in just two steps.

### Step 1: Add Header File Reference

In the source file that calls the kernel function via `<<<>>>`, add the following code:

```c
#ifdef ASCENDC_CPU_DEBUG
#include "cpu_debug_launch.h"
#endif
```

In CPU debug mode, the bisheng compiler translates `<<<>>>` kernel function calls through this header file to execute the kernel function on the CPU. This modification does not affect compilation or execution in NPU mode.

### Step 2: Build and Run

Using the dav-2201 architecture NPU (e.g., Ascend910B1) as an example:

```bash
mkdir -p build && cd build;
cmake -DCMAKE_ASC_RUN_MODE=cpu -DCMAKE_ASC_ARCHITECTURES=dav-2201 ..;make -j;
./add
```

- Build Options

    | Option | Description |
    |------|------|
    | `CMAKE_ASC_RUN_MODE` | Set to `cpu` to enable CPU-domain compilation |
    | `CMAKE_ASC_ARCHITECTURES` | Specify the NPU architecture version. CMake will configure the corresponding CPU debug dependency libraries based on this value.<br>`dav-2201` corresponds to Atlas A2/A3 series, `dav-3510` corresponds to Ascend 950PR/Ascend 950DT |

## Debugging Methods

The generated CPU-domain executable supports debugging via gdb. gdb supports common debugging operations such as setting breakpoints, inspecting registers and memory state, single-stepping, and viewing call stacks.

CPU Debug launches a separate child process for each kernel function to simulate NPU execution logic. Therefore, when debugging with gdb, you need to set `follow-fork-mode` to have gdb follow the child process in order to set breakpoints inside the kernel function.

Basic usage:

```bash
gdb ./build/add
```

After entering gdb, first set the child process follow mode:

```text
(gdb) set follow-fork-mode child
```

Then proceed with debugging as needed. Common operations:

```text
# Set a breakpoint at the kernel function entry
(gdb) break Compute

# Run the program
(gdb) run

# Single-step execution
(gdb) next

# Continue to the next breakpoint
(gdb) continue
```

> **Note**: `set follow-fork-mode child` tells gdb to switch to the child process when a fork creates a new process. Without this option, gdb follows the parent process by default and will not be able to enter the kernel function.

## Switch Back to NPU Mode

After CPU debugging is complete, clear the build directory and reconfigure cmake to switch back to NPU mode. The `#ifdef` code added in Step 1 does not need to be removed — it has no effect in NPU mode.

```bash
rm -r build;
mkdir -p build && cd build;
cmake -DCMAKE_ASC_ARCHITECTURES=dav-2201 ..;make -j;
./add
```
