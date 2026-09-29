# CPU Debug

## Overview

Before deploying operators to the NPU, the CPU Debug tool helps users perform basic functional and accuracy verification on the CPU. Developers write operator kernel-side source code using Ascend C, compile it with the bisheng compiler to generate CPU-domain executables, and then use standard debugging tools such as gdb to debug the operators.

## Environment Preparation

Please refer to [Quick Start](00_quick_start.md#Environment Preparation) to complete the environment preparation.

## Usage

Using the [cpudebug](../../examples/02_cpudebug/) example, you can start CPU debugging in just two steps.

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

## CPU Debug Checks and Log Analysis

### Overview

The twin debugging provided by Ascend C Tools includes the debug function and the runtime checking function. The debug function covers aspects such as interface usage validation and parameter verification. On top of that, runtime checking provides memory checking, memory lifecycle management, memory address dependency management, and synchronization event management. Note that runtime checking only outputs complete verification logs and analysis when the debug phase exits normally (i.e., no ASSERT failures).

### Environment Preparation

Please refer to [Quick Start](00_quick_start.md) to complete the environment preparation.

### Usage

When operators developed with the Ascend C programming language are executed in the CPU domain via [CPU debug usage](#usage), the CPU debug runtime simultaneously checks the operator implementation. The execution process and detected errors are saved as *_npuchk.log files in the npuchk folder under the execution path of the CPU-domain operator executable. Run the following command to generate the check results in one step:

  ``` bash
  # Without specifying a log file, the script automatically searches for log files in the current directory. git_clone_path is the clone path of this repository.
  python3 ${git_clone_path}/asc-tools/npuchk/ascendc_npuchk_report.py

  # Specify a log file
  python3 ${git_clone_path}/asc-tools/npuchk/ascendc_npuchk_report.py npuchk/xxx_npuchk.log
  ```

- Errors detected: After the command finishes, failure results are displayed on screen. For example, error code ErrorRead3 and related failure information:

  ``` bash
  [V] [ErrorRead3] on read 0x7f328c11b010 0x800B
  Rule: Read out of bounds, length exceeds the actual valid data (start/end) allocated via Ascend C framework's alloc_buf
  ### vadd((__ubuf__ half*)7f328c11b810, (__ubuf__ half*)0xf328c11b010, (__ubuf__*)0x7f328c11b410, (uint8_t)1, (uint8_t)1, (uint8_t)1, (uint8_t)1, (uint8_t)8, (uint8_t)8, (uint8_t)8);

  ---------------------- ERROR STATISTICS ----------------------
  1, ErrorRead3, Read out of bounds, length exceeds the actual valid data (start/end) allocated via Ascend C framework's alloc_buf
  ```

- No errors detected: Command completes with no screen output.

If errors are detected, you can view the detailed execution process in the log. Based on the log information, the following functional areas are covered.

#### Anomaly Detection

Runtime checking validates the legality of memory reads/writes, instruction synchronization, and tensor operations. Common failure types and their corresponding fields are as follows:

- **ErrorRead1:**
    Illegal memory read: The entire memory region was not allocated via Ascend C framework's AllocTensor or has already been freed by FreeTensor.
- **ErrorRead2:**
    [Suspicious] Reading invalid data: The memory being read was partially/entirely never written to, so the data may be invalid.
- **ErrorRead3:**
    Read out of bounds: The length exceeds the actual valid data (start/end) allocated via Ascend C framework's AllocTensor.
- **ErrorRead4:**
    Read address is not 32-byte aligned.
- **ErrorWrite1:**
    Illegal memory write: The memory was not allocated via Ascend C framework's AllocTensor or has already been freed by FreeTensor.
- **ErrorWrite2:**
    Write out of bounds: The length exceeds the actual valid data (start/end) allocated via Ascend C framework's AllocTensor.
- **ErrorWrite3:**
    [Suspicious] Duplicate write: The previously written memory has not been consumed, and is being overwritten.
- **ErrorWrite4:**
    Write address is not 32-byte aligned.
- **ErrorSync1:**
    Write synchronization issue: Missing pipe barrier within a pipe or missing set/wait between pipes.
- **ErrorSync2:**
    Read synchronization issue: Missing pipe barrier within a pipe or missing set/wait between pipes.
- **ErrorSync3:**
    set/wait pairing mismatch: Missing either set or wait.
- **ErrorSync4:**
    Duplicate eventID in set/wait operations, e.g., mte2:set0/set0, vector:set0/wait0.
- **ErrorLeak:**
    Memory leak: Memory was allocated but not freed.
- **ErrorFree:**
    Double free: Memory was already freed via free_buf, and free_buf is called again.
- **ErrorBuffer0:**
    Tensor memory was not initialized using Ascend C framework's InitBuffer.
- **ErrorBuffer1:**
    Tensor queue type is inconsistent with the type used during initialization.
- **ErrorBuffer2:**
    VECIN/VECOUT/VECCALC operations are non-compliant.
- **ErrorBuffer3:**
    Tensor operation memory is invalid. Possible causes: memory not allocated / memory out of bounds.
- **ErrorBuffer4:**
    TBufPool resource pool was not initialized using Ascend C framework's InitBufPool interface.

#### EnQue/DeQue Error Scenario Check

For VECIN/VECOUT/VECCALC type Tensors, the tool checks whether a Tensor is in the correct state when it appears in a data transfer/compute instruction, to ensure synchronization correctness. Abnormal states are recorded in the log.

#### GM Memory Multi-Core Conflict Check

Based on the GM global memory management mechanism, the tool records the GM address range operated by each core. If overlapping write address ranges are detected across multiple cores, an error is recorded. In Atomic add scenarios, overlapping addresses are not flagged as errors.

### Usage Example

Using the [add](https://gitcode.com/cann/asc-devkit/blob/master/examples/01_simd_cpp_api/00_introduction/01_add/add_tpipe_tque/add_tpipe_tque.asc) example, after calling the CPU debugging API and using gdb/printf to debug the operator kernel function, developers can use the log analysis script to check the kernel source code implementation logic based on the generated log file.

**Step 1**: Construct an error case

Add the following FreeTensor operation in the CopyIn function of the add_custom code.

``` cpp
AscendC::LocalTensor<float> xLocal = inQueueX.AllocTensor<float>();
AscendC::LocalTensor<float> yLocal = inQueueY.AllocTensor<float>();
// Add the following line here to construct an error example
inQueueX.FreeTensor(xLocal);
// The remaining code stays unchanged
AscendC::DataCopy(xLocal, xGm, blockLength);
AscendC::DataCopy(yLocal, yGm, blockLength);
inQueueX.EnQue(xLocal);
inQueueY.EnQue(yLocal);
```

Performing FreeTensor here will result in an illegal memory write.

**Step 2**: Use cpu debug to generate the log file

Refer to [CPU debug usage](#usage) and run the following commands to compile and generate the CPU-domain operator executable. The add_custom_x_x_npuchk.log file is saved in the npuchk folder under the newly created build folder in the execution path.

```bash
mkdir -p build && cd build;
cmake ..  -DCMAKE_ASC_RUN_MODE=cpu -DCMAKE_ASC_ARCHITECTURES=${SOC_VERSION}; make -j
python3 ../scripts/gen_data.py
./demo
python3 ../scripts/verify_result.py output/output.bin output/golden.bin
```

**Step 3**: Find the corresponding log file and run the check

Since this is a multi-core example, each core generates a separate log file. Taking core 0 as an example, the generated log file is add_custom_0_0_vec_npuchk.log. Run the following command to perform the check:

``` shell
python3 ${git_clone_path}/asc-tools/npuchk/ascendc_npuchk_report.py npuchk/add_custom_0_0_vec_npuchk.log
```

  - If no xxx_npuchk.log file is specified, the script will automatically search for files with the "_npuchk.log" suffix in the current directory.

You can then view the stack trace information recorded during CPU debugging in the log file.

**Step 4**: Determine the error type based on the screen output

When the example case has errors, the following information will be displayed:

``` shell
----------------------ERROR STATISTICS----------------------
1, ErrorBuffer2, VECIN/VECOUT/VECCALC operations are non-compliant
1, ErrorWrite1, Illegal memory write: Memory was not allocated via Ascend C framework's alloc_buf or has already been freed
```

You can then determine the error type based on the anomaly detection section above.
